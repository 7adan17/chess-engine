#pragma once

#include <cstdint>

namespace chess {

using Bitboard = std::uint64_t;

// Squares are numbered a1 = 0, b1 = 1, ..., h8 = 63.
enum Color : int { WHITE, BLACK };
enum PieceType : int { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, NO_TYPE };
enum Piece : int {
    W_PAWN, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
    B_PAWN, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING,
    NO_PIECE
};

constexpr Piece make_piece(Color c, PieceType t) { return Piece(c * 6 + t); }
constexpr PieceType type_of(Piece p) { return PieceType(p % 6); }
constexpr Color color_of(Piece p) { return Color(p / 6); }

constexpr Bitboard bit(int sq) { return 1ULL << sq; }
constexpr int rank_of(int sq) { return sq >> 3; }
constexpr int file_of(int sq) { return sq & 7; }

inline int lsb(Bitboard b) { return __builtin_ctzll(b); }
inline int msb(Bitboard b) { return 63 - __builtin_clzll(b); }
inline int popcount(Bitboard b) { return __builtin_popcountll(b); }
inline int pop_lsb(Bitboard& b) {
    int sq = lsb(b);
    b &= b - 1;
    return sq;
}

extern Bitboard KNIGHT_ATTACKS[64];
extern Bitboard KING_ATTACKS[64];
extern Bitboard PAWN_ATTACKS[2][64];

// Sliding attacks use precomputed rays: the ray beyond the first blocker
// is XORed away, so blockers are included (they may be captures).
Bitboard rook_attacks(int sq, Bitboard occ);
Bitboard bishop_attacks(int sq, Bitboard occ);
inline Bitboard queen_attacks(int sq, Bitboard occ) { return rook_attacks(sq, occ) | bishop_attacks(sq, occ); }

void init_bitboards();

}  // namespace chess
