#include "position.hpp"

#include <cstring>
#include <sstream>
#include <stdexcept>

namespace chess {

namespace {

std::uint64_t Z_PIECE[12][64], Z_CASTLE[16], Z_EP[8], Z_SIDE;

// Clears castling rights when a king or rook moves, or a rook is captured.
int CASTLE_MASK[64];

const char PIECE_CHARS[] = "PNBRQKpnbrqk";

void add_promos(MoveList& list, int from, int to, std::uint32_t flags) {
    for (int pt = QUEEN; pt >= KNIGHT; --pt) list.add(make_move(from, to, pt, flags));
}

}  // namespace

void init_zobrist() {
    std::uint64_t s = 0x9E3779B97F4A7C15ULL;
    auto next = [&] {  // xorshift64*
        s ^= s >> 12; s ^= s << 25; s ^= s >> 27;
        return s * 0x2545F4914F6CDD1DULL;
    };
    for (auto& p : Z_PIECE) for (auto& k : p) k = next();
    for (auto& k : Z_CASTLE) k = next();
    for (auto& k : Z_EP) k = next();
    Z_SIDE = next();

    for (int& m : CASTLE_MASK) m = 15;
    CASTLE_MASK[4] = ~(WK | WQ) & 15;
    CASTLE_MASK[7] = ~WK & 15;
    CASTLE_MASK[0] = ~WQ & 15;
    CASTLE_MASK[60] = ~(BK | BQ) & 15;
    CASTLE_MASK[63] = ~BK & 15;
    CASTLE_MASK[56] = ~BQ & 15;
}

std::string move_to_uci(Move m) {
    if (!m) return "0000";
    std::string s;
    for (int sq : {move_from(m), move_to(m)}) {
        s += char('a' + file_of(sq));
        s += char('1' + rank_of(sq));
    }
    if (move_promo(m)) s += "pnbrqk"[move_promo(m)];
    return s;
}

void Position::put(Piece p, int sq) {
    pieces[p] |= bit(sq);
    occ[color_of(p)] |= bit(sq);
    all |= bit(sq);
    board[sq] = p;
    hash ^= Z_PIECE[p][sq];
}

void Position::remove(int sq) {
    Piece p = board[sq];
    pieces[p] &= ~bit(sq);
    occ[color_of(p)] &= ~bit(sq);
    all &= ~bit(sq);
    board[sq] = NO_PIECE;
    hash ^= Z_PIECE[p][sq];
}

Position Position::startpos() {
    return from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

Position Position::from_fen(const std::string& fen) {
    Position p;
    for (auto& sq : p.board) sq = NO_PIECE;
    std::istringstream in(fen);
    std::string placement, side, castle, ep;
    in >> placement >> side >> castle >> ep;
    if (!(in >> p.halfmove)) p.halfmove = 0;
    if (!(in >> p.fullmove)) p.fullmove = 1;

    int rank = 7, file = 0;
    for (char c : placement) {
        if (c == '/') { --rank; file = 0; }
        else if (c >= '1' && c <= '8') file += c - '0';
        else {
            const char* found = std::strchr(PIECE_CHARS, c);
            if (!found || file > 7 || rank < 0) throw std::invalid_argument("bad FEN: " + fen);
            p.put(Piece(found - PIECE_CHARS), rank * 8 + file++);
        }
    }
    p.side = side == "b" ? BLACK : WHITE;
    for (char c : castle) {
        if (c == 'K') p.castling |= WK;
        if (c == 'Q') p.castling |= WQ;
        if (c == 'k') p.castling |= BK;
        if (c == 'q') p.castling |= BQ;
    }
    if (ep != "-" && ep.size() == 2) p.ep = (ep[1] - '1') * 8 + (ep[0] - 'a');

    if (p.side == BLACK) p.hash ^= Z_SIDE;
    p.hash ^= Z_CASTLE[p.castling];
    if (p.ep != -1) p.hash ^= Z_EP[file_of(p.ep)];
    return p;
}

std::string Position::fen() const {
    std::string s;
    for (int r = 7; r >= 0; --r) {
        int empty = 0;
        for (int f = 0; f < 8; ++f) {
            Piece p = board[r * 8 + f];
            if (p == NO_PIECE) { ++empty; continue; }
            if (empty) { s += char('0' + empty); empty = 0; }
            s += PIECE_CHARS[p];
        }
        if (empty) s += char('0' + empty);
        if (r) s += '/';
    }
    s += side == WHITE ? " w " : " b ";
    std::string c;
    if (castling & WK) c += 'K';
    if (castling & WQ) c += 'Q';
    if (castling & BK) c += 'k';
    if (castling & BQ) c += 'q';
    s += c.empty() ? "-" : c;
    s += ' ';
    s += ep == -1 ? std::string("-") : std::string{char('a' + file_of(ep)), char('1' + rank_of(ep))};
    return s + ' ' + std::to_string(halfmove) + ' ' + std::to_string(fullmove);
}

bool Position::attacked(int sq, Color by) const {
    if (PAWN_ATTACKS[by ^ 1][sq] & pieces[make_piece(by, PAWN)]) return true;
    if (KNIGHT_ATTACKS[sq] & pieces[make_piece(by, KNIGHT)]) return true;
    if (KING_ATTACKS[sq] & pieces[make_piece(by, KING)]) return true;
    Bitboard queens = pieces[make_piece(by, QUEEN)];
    if (bishop_attacks(sq, all) & (pieces[make_piece(by, BISHOP)] | queens)) return true;
    if (rook_attacks(sq, all) & (pieces[make_piece(by, ROOK)] | queens)) return true;
    return false;
}

bool Position::has_non_pawn_material(Color c) const {
    return pieces[make_piece(c, KNIGHT)] | pieces[make_piece(c, BISHOP)] |
           pieces[make_piece(c, ROOK)] | pieces[make_piece(c, QUEEN)];
}

void Position::generate(MoveList& list, bool captures_only) const {
    const Color us = side, them = Color(side ^ 1);
    const Bitboard own = occ[us], enemy = occ[them];

    const int push = us == WHITE ? 8 : -8;
    const int start_rank = us == WHITE ? 1 : 6;
    const int last_rank = us == WHITE ? 6 : 1;  // rank a pawn promotes from
    Bitboard pawns = pieces[make_piece(us, PAWN)];
    while (pawns) {
        int from = pop_lsb(pawns), r = rank_of(from), to = from + push;
        if (!(all & bit(to))) {
            if (r == last_rank) add_promos(list, from, to, 0);
            else if (!captures_only) {
                list.add(make_move(from, to));
                if (r == start_rank && !(all & bit(to + push)))
                    list.add(make_move(from, to + push, 0, FLAG_DOUBLE));
            }
        }
        Bitboard caps = PAWN_ATTACKS[us][from] & enemy;
        while (caps) {
            int t = pop_lsb(caps);
            if (r == last_rank) add_promos(list, from, t, FLAG_CAPTURE);
            else list.add(make_move(from, t, 0, FLAG_CAPTURE));
        }
        if (ep != -1 && (PAWN_ATTACKS[us][from] & bit(ep)))
            list.add(make_move(from, ep, 0, FLAG_EP | FLAG_CAPTURE));
    }

    for (int pt = KNIGHT; pt <= KING; ++pt) {
        Bitboard b = pieces[make_piece(us, PieceType(pt))];
        while (b) {
            int from = pop_lsb(b);
            Bitboard att = pt == KNIGHT ? KNIGHT_ATTACKS[from]
                         : pt == BISHOP ? bishop_attacks(from, all)
                         : pt == ROOK   ? rook_attacks(from, all)
                         : pt == QUEEN  ? queen_attacks(from, all)
                                        : KING_ATTACKS[from];
            att &= captures_only ? enemy : ~own;
            while (att) {
                int t = pop_lsb(att);
                list.add(make_move(from, t, 0, (enemy & bit(t)) ? FLAG_CAPTURE : 0));
            }
        }
    }

    if (captures_only) return;
    // The king may not castle out of, through, or into check.
    if (us == WHITE) {
        if ((castling & WK) && !(all & (bit(5) | bit(6))) &&
            !attacked(4, them) && !attacked(5, them) && !attacked(6, them))
            list.add(make_move(4, 6, 0, FLAG_CASTLE));
        if ((castling & WQ) && !(all & (bit(1) | bit(2) | bit(3))) &&
            !attacked(4, them) && !attacked(3, them) && !attacked(2, them))
            list.add(make_move(4, 2, 0, FLAG_CASTLE));
    } else {
        if ((castling & BK) && !(all & (bit(61) | bit(62))) &&
            !attacked(60, them) && !attacked(61, them) && !attacked(62, them))
            list.add(make_move(60, 62, 0, FLAG_CASTLE));
        if ((castling & BQ) && !(all & (bit(57) | bit(58) | bit(59))) &&
            !attacked(60, them) && !attacked(59, them) && !attacked(58, them))
            list.add(make_move(60, 58, 0, FLAG_CASTLE));
    }
}

bool Position::make(Move m, Position& n) const {
    n = *this;
    const int from = move_from(m), to = move_to(m), promo = move_promo(m);
    const std::uint32_t flags = move_flags(m);
    const Color us = side, them = Color(side ^ 1);
    const Piece p = board[from];

    if (n.ep != -1) n.hash ^= Z_EP[file_of(n.ep)];
    n.ep = -1;
    ++n.halfmove;

    if (flags & FLAG_EP) n.remove(us == WHITE ? to - 8 : to + 8);
    else if (n.board[to] != NO_PIECE) n.remove(to);
    n.remove(from);
    n.put(promo ? make_piece(us, PieceType(promo)) : p, to);

    if (flags & FLAG_CASTLE) {
        int rf = to > from ? to + 1 : to - 2;
        int rt = to > from ? to - 1 : to + 1;
        Piece rook = n.board[rf];
        n.remove(rf);
        n.put(rook, rt);
    }
    if (type_of(p) == PAWN || (flags & FLAG_CAPTURE)) n.halfmove = 0;
    if (flags & FLAG_DOUBLE) {
        n.ep = (from + to) / 2;
        n.hash ^= Z_EP[file_of(n.ep)];
    }

    n.hash ^= Z_CASTLE[n.castling];
    n.castling &= CASTLE_MASK[from] & CASTLE_MASK[to];
    n.hash ^= Z_CASTLE[n.castling];

    if (us == BLACK) ++n.fullmove;
    n.side = them;
    n.hash ^= Z_SIDE;
    return !n.attacked(n.king_sq(us), them);
}

void Position::make_null(Position& n) const {
    n = *this;
    if (n.ep != -1) n.hash ^= Z_EP[file_of(n.ep)];
    n.ep = -1;
    n.side = Color(side ^ 1);
    n.hash ^= Z_SIDE;
    ++n.halfmove;
}

void Position::legal_moves(MoveList& list) const {
    MoveList pseudo;
    generate(pseudo);
    Position tmp;
    for (int i = 0; i < pseudo.size; ++i)
        if (make(pseudo[i], tmp)) list.add(pseudo[i]);
}

Move Position::parse_uci(const std::string& s) const {
    MoveList list;
    legal_moves(list);
    for (int i = 0; i < list.size; ++i)
        if (move_to_uci(list[i]) == s) return list[i];
    return 0;
}

std::uint64_t perft(const Position& pos, int depth) {
    MoveList list;
    pos.generate(list);
    std::uint64_t nodes = 0;
    Position child;
    for (int i = 0; i < list.size; ++i) {
        if (!pos.make(list[i], child)) continue;
        nodes += depth <= 1 ? 1 : perft(child, depth - 1);
    }
    return nodes;
}

}  // namespace chess
