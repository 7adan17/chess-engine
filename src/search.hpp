#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "position.hpp"

namespace chess {

constexpr int INF = 32001;
constexpr int MATE = 32000;
constexpr int MAX_PLY = 128;
constexpr int MATE_BOUND = MATE - MAX_PLY;

struct SearchLimits {
    int depth = MAX_PLY - 1;
    std::int64_t movetime_ms = -1;
    std::int64_t time_left_ms[2] = {-1, -1};
    std::int64_t increment_ms[2] = {0, 0};
    int movestogo = 0;
    bool infinite = false;
};

struct SearchResult {
    Move best = 0;
    int score = 0;
    int depth = 0;
    std::uint64_t nodes = 0;
};

// Iterative-deepening principal variation search (alpha-beta) with a
// transposition table, null-move pruning, check extensions, quiescence
// search, and MVV-LVA / killer / history move ordering.
class Searcher {
public:
    using InfoCallback = std::function<void(const std::string&)>;

    explicit Searcher(std::size_t tt_mb = 64);

    // history: hashes of the game's earlier positions, with the root last.
    SearchResult search(const Position& root, const std::vector<std::uint64_t>& history,
                        const SearchLimits& limits, const InfoCallback& info = nullptr);
    void stop() { stopped_ = true; }
    void clear();

private:
    enum Bound : std::uint8_t { EXACT, LOWER, UPPER };
    struct TTEntry {
        std::uint64_t key = 0;
        Move move = 0;
        std::int16_t score = 0;
        std::int8_t depth = -1;
        Bound bound = EXACT;
    };

    int negamax(const Position& pos, int depth, int alpha, int beta, int ply, bool allow_null);
    int quiesce(const Position& pos, int alpha, int beta, int ply);
    void order_moves(const Position& pos, MoveList& list, int* scores, Move tt_move, int ply) const;
    bool is_repetition(const Position& pos) const;
    void check_time();
    std::string pv_string(const Position& root, int max_len) const;

    TTEntry* probe(std::uint64_t key);
    void store(std::uint64_t key, Move m, int score, int depth, Bound b);

    std::vector<TTEntry> tt_;
    std::size_t tt_mask_ = 0;
    std::vector<std::uint64_t> path_;  // game history and current search path
    Move killers_[MAX_PLY][2] = {};
    int history_[64][64] = {};
    Move root_best_ = 0;
    std::uint64_t nodes_ = 0;
    std::atomic<bool> stopped_{false};
    std::chrono::steady_clock::time_point start_;
    std::int64_t deadline_ms_ = -1;
};

}  // namespace chess
