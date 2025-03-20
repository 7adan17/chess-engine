#pragma once

#include <cstdint>
#include <string>

#include "bitboard.hpp"

namespace chess {

// Move layout: bits 0-5 from, 6-11 to, 12-15 promotion piece type (0 = none),
// 16-19 flags.
using Move = std::uint32_t;
enum MoveFlag : std::uint32_t { FLAG_CAPTURE = 1, FLAG_DOUBLE = 2, FLAG_EP = 4, FLAG_CASTLE = 8 };

constexpr Move make_move(int from, int to, int promo = 0, std::uint32_t flags = 0) {
    return Move(from | (to << 6) | (promo << 12) | (flags << 16));
}
constexpr int move_from(Move m) { return m & 63; }
constexpr int move_to(Move m) { return (m >> 6) & 63; }
constexpr int move_promo(Move m) { return (m >> 12) & 15; }
constexpr std::uint32_t move_flags(Move m) { return m >> 16; }
constexpr bool is_capture(Move m) { return move_flags(m) & FLAG_CAPTURE; }

std::string move_to_uci(Move m);

struct MoveList {
    Move moves[256];
    int size = 0;
    void add(Move m) { moves[size++] = m; }
    Move& operator[](int i) { return moves[i]; }
};

enum Castling { WK = 1, WQ = 2, BK = 4, BQ = 8 };

// Copy-make position: make() writes the child into a separate object, so
// there is no unmake and no undo stack to keep consistent.
struct Position {
    Bitboard pieces[12] = {};
    Bitboard occ[2] = {};
    Bitboard all = 0;
    Piece board[64];
    Color side = WHITE;
    int castling = 0;
    int ep = -1;
    int halfmove = 0;
    int fullmove = 1;
    std::uint64_t hash = 0;

    static Position from_fen(const std::string& fen);
    static Position startpos();
    std::string fen() const;

    bool attacked(int sq, Color by) const;
    int king_sq(Color c) const { return lsb(pieces[make_piece(c, KING)]); }
    bool in_check() const { return attacked(king_sq(side), Color(side ^ 1)); }
    bool has_non_pawn_material(Color c) const;

    // Pseudo-legal moves. make() returns false if the move leaves our king in check.
    void generate(MoveList& list, bool captures_only = false) const;
    bool make(Move m, Position& out) const;
    void make_null(Position& out) const;
    void legal_moves(MoveList& list) const;
    Move parse_uci(const std::string& s) const;  // 0 if not legal

    void put(Piece p, int sq);
    void remove(int sq);
};

std::uint64_t perft(const Position& pos, int depth);
void init_zobrist();

}  // namespace chess
