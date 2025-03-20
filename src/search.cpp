#include "search.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include "eval.hpp"

namespace chess {

namespace {

int to_tt(int score, int ply) {
    if (score > MATE_BOUND) return score + ply;
    if (score < -MATE_BOUND) return score - ply;
    return score;
}

int from_tt(int score, int ply) {
    if (score > MATE_BOUND) return score - ply;
    if (score < -MATE_BOUND) return score + ply;
    return score;
}

// Selection sort step: swaps the best remaining move into slot i.
void pick_next(MoveList& list, int* scores, int i) {
    int best = i;
    for (int j = i + 1; j < list.size; ++j)
        if (scores[j] > scores[best]) best = j;
    std::swap(list[i], list[best]);
    std::swap(scores[i], scores[best]);
}

}  // namespace

Searcher::Searcher(std::size_t tt_mb) {
    std::size_t entries = 1;
    while (entries * 2 * sizeof(TTEntry) <= tt_mb * 1024 * 1024) entries *= 2;
    tt_.resize(entries);
    tt_mask_ = entries - 1;
}

void Searcher::clear() {
    std::fill(tt_.begin(), tt_.end(), TTEntry{});
    std::memset(killers_, 0, sizeof(killers_));
    std::memset(history_, 0, sizeof(history_));
}

Searcher::TTEntry* Searcher::probe(std::uint64_t key) {
    TTEntry& e = tt_[key & tt_mask_];
    return e.key == key ? &e : nullptr;
}

void Searcher::store(std::uint64_t key, Move m, int score, int depth, Bound b) {
    TTEntry& e = tt_[key & tt_mask_];
    if (e.key == key && e.depth > depth && b != EXACT) return;  // keep deeper info for this position
    if (!m && e.key == key) m = e.move;
    e = {key, m, std::int16_t(score), std::int8_t(depth), b};
}

void Searcher::check_time() {
    if (deadline_ms_ < 0) return;
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_).count();
    if (elapsed >= deadline_ms_) stopped_ = true;
}

bool Searcher::is_repetition(const Position& pos) const {
    // path_.back() is pos itself; only positions with the same side to move,
    // since the last irreversible move, can repeat.
    const int n = int(path_.size());
    for (int i = n - 3; i >= 0 && i >= n - 1 - pos.halfmove; i -= 2)
        if (path_[i] == pos.hash) return true;
    return false;
}

void Searcher::order_moves(const Position& pos, MoveList& list, int* scores, Move tt_move, int ply) const {
    for (int i = 0; i < list.size; ++i) {
        Move m = list[i];
        if (m == tt_move) scores[i] = 1'000'000;
        else if (is_capture(m)) {
            PieceType victim = (move_flags(m) & FLAG_EP) ? PAWN : type_of(pos.board[move_to(m)]);
            scores[i] = 100'000 + 10 * PIECE_VALUE[victim] - PIECE_VALUE[type_of(pos.board[move_from(m)])];
        } else if (move_promo(m)) scores[i] = 95'000 + move_promo(m);
        else if (m == killers_[ply][0]) scores[i] = 90'000;
        else if (m == killers_[ply][1]) scores[i] = 80'000;
        else scores[i] = history_[move_from(m)][move_to(m)];
    }
}

int Searcher::quiesce(const Position& pos, int alpha, int beta, int ply) {
    if ((++nodes_ & 2047) == 0) check_time();
    if (stopped_) return 0;

    int stand_pat = evaluate(pos);
    if (ply >= MAX_PLY - 1 || stand_pat >= beta) return stand_pat;
    if (stand_pat > alpha) alpha = stand_pat;

    MoveList list;
    pos.generate(list, true);
    int scores[256];
    order_moves(pos, list, scores, 0, ply);
    Position child;
    for (int i = 0; i < list.size; ++i) {
        pick_next(list, scores, i);
        if (!pos.make(list[i], child)) continue;
        int score = -quiesce(child, -beta, -alpha, ply + 1);
        if (stopped_) return 0;
        if (score >= beta) return score;
        if (score > alpha) alpha = score;
    }
    return alpha;
}

int Searcher::negamax(const Position& pos, int depth, int alpha, int beta, int ply, bool allow_null) {
    if ((++nodes_ & 2047) == 0) check_time();
    if (stopped_) return 0;

    const bool root = ply == 0;
    if (!root) {
        if (pos.halfmove >= 100 || is_repetition(pos)) return 0;
        // Mate-distance pruning: no line from here beats a mate already found closer to the root.
        alpha = std::max(alpha, -MATE + ply);
        beta = std::min(beta, MATE - ply - 1);
        if (alpha >= beta) return alpha;
    }

    const bool in_check = pos.in_check();
    if (in_check) ++depth;
    if (depth <= 0) return quiesce(pos, alpha, beta, ply);
    if (ply >= MAX_PLY - 1) return evaluate(pos);

    Move tt_move = 0;
    if (TTEntry* e = probe(pos.hash)) {
        tt_move = e->move;
        if (!root && e->depth >= depth) {
            int s = from_tt(e->score, ply);
            if (e->bound == EXACT) return s;
            if (e->bound == LOWER && s >= beta) return s;
            if (e->bound == UPPER && s <= alpha) return s;
        }
    }

    // Null-move pruning: if passing still fails high, a real move would too.
    // Skipped in check and without pieces (zugzwang risk).
    if (allow_null && !root && !in_check && depth >= 3 && beta < MATE_BOUND &&
        pos.has_non_pawn_material(pos.side)) {
        Position child;
        pos.make_null(child);
        path_.push_back(child.hash);
        int score = -negamax(child, depth - 3, -beta, -beta + 1, ply + 1, false);
        path_.pop_back();
        if (stopped_) return 0;
        if (score >= beta) return beta;
    }

    MoveList list;
    pos.generate(list);
    int scores[256];
    order_moves(pos, list, scores, tt_move, ply);

    const int orig_alpha = alpha;
    int best = -INF, legal = 0;
    Move best_move = 0;
    Position child;
    for (int i = 0; i < list.size; ++i) {
        pick_next(list, scores, i);
        const Move m = list[i];
        if (!pos.make(m, child)) continue;
        ++legal;

        path_.push_back(child.hash);
        int score;
        if (legal == 1) {
            score = -negamax(child, depth - 1, -beta, -alpha, ply + 1, true);
        } else {
            // Principal variation search: prove this move is worse with a null window.
            score = -negamax(child, depth - 1, -alpha - 1, -alpha, ply + 1, true);
            if (score > alpha && score < beta)
                score = -negamax(child, depth - 1, -beta, -alpha, ply + 1, true);
        }
        path_.pop_back();
        if (stopped_) return 0;

        if (score > best) {
            best = score;
            best_move = m;
            if (root) root_best_ = m;
        }
        if (score > alpha) alpha = score;
        if (alpha >= beta) {
            if (!is_capture(m) && !move_promo(m)) {
                if (killers_[ply][0] != m) {
                    killers_[ply][1] = killers_[ply][0];
                    killers_[ply][0] = m;
                }
                history_[move_from(m)][move_to(m)] += depth * depth;
            }
            break;
        }
    }

    if (!legal) return in_check ? -MATE + ply : 0;

    Bound bound = best <= orig_alpha ? UPPER : best >= beta ? LOWER : EXACT;
    store(pos.hash, best_move, to_tt(best, ply), depth, bound);
    return best;
}

std::string Searcher::pv_string(const Position& root, int max_len) const {
    std::ostringstream out;
    Position pos = root, next;
    std::vector<std::uint64_t> seen;
    for (int i = 0; i < max_len; ++i) {
        const TTEntry& e = tt_[pos.hash & tt_mask_];
        if (e.key != pos.hash || !e.move) break;
        if (std::find(seen.begin(), seen.end(), pos.hash) != seen.end()) break;
        MoveList legal;
        pos.legal_moves(legal);
        if (std::find(legal.moves, legal.moves + legal.size, e.move) == legal.moves + legal.size) break;
        seen.push_back(pos.hash);
        out << (i ? " " : "") << move_to_uci(e.move);
        pos.make(e.move, next);
        pos = next;
    }
    return out.str();
}

SearchResult Searcher::search(const Position& root, const std::vector<std::uint64_t>& history,
                              const SearchLimits& limits, const InfoCallback& info) {
    start_ = std::chrono::steady_clock::now();
    stopped_ = false;
    nodes_ = 0;
    path_ = history;
    if (path_.empty() || path_.back() != root.hash) path_.push_back(root.hash);
    std::memset(killers_, 0, sizeof(killers_));
    for (auto& row : history_) for (int& h : row) h /= 8;  // age history between searches

    deadline_ms_ = -1;
    if (!limits.infinite) {
        if (limits.movetime_ms > 0) {
            deadline_ms_ = limits.movetime_ms;
        } else if (limits.time_left_ms[root.side] > 0) {
            std::int64_t left = limits.time_left_ms[root.side];
            int moves = limits.movestogo > 0 ? limits.movestogo : 30;
            deadline_ms_ = left / moves + limits.increment_ms[root.side] * 3 / 4;
            deadline_ms_ = std::min(deadline_ms_, left / 2);
            deadline_ms_ = std::max<std::int64_t>(deadline_ms_ - 20, 5);
        }
    }

    MoveList legal;
    root.legal_moves(legal);
    SearchResult result;
    if (legal.size == 0) return result;
    result.best = legal[0];

    for (int depth = 1; depth <= limits.depth; ++depth) {
        root_best_ = 0;
        int score = negamax(root, depth, -INF, INF, 0, false);

        // A partly searched iteration still has a valid best move if it found one.
        if (root_best_) result.best = root_best_;
        if (stopped_) break;

        result.score = score;
        result.depth = depth;
        result.nodes = nodes_;

        if (info) {
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_).count();
            std::ostringstream line;
            line << "info depth " << depth << " score ";
            if (std::abs(score) > MATE_BOUND) {
                int plies = MATE - std::abs(score);
                line << "mate " << (score > 0 ? (plies + 1) / 2 : -(plies / 2));
            } else {
                line << "cp " << score;
            }
            line << " nodes " << nodes_ << " nps " << (ms > 0 ? nodes_ * 1000 / ms : nodes_)
                 << " time " << ms << " pv " << pv_string(root, depth);
            info(line.str());
        }

        if (std::abs(score) > MATE_BOUND && depth > 2 * (MATE - std::abs(score))) break;
        if (deadline_ms_ > 0) {
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_).count();
            if (ms > deadline_ms_ / 2) break;  // the next iteration would likely not finish
        }
    }
    result.nodes = nodes_;
    return result;
}

}  // namespace chess
