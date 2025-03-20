#include "bitboard.hpp"

namespace chess {

Bitboard KNIGHT_ATTACKS[64];
Bitboard KING_ATTACKS[64];
Bitboard PAWN_ATTACKS[2][64];

namespace {

// Directions 0-3 increase the square index (N, NE, E, NW), so the nearest
// blocker is the LSB. Directions 4-7 decrease it (S, SW, W, SE): use the MSB.
enum Dir { N, NE, E, NW, S, SW, W, SE };
constexpr int DF[8] = {0, 1, 1, -1, 0, -1, -1, 1};
constexpr int DR[8] = {1, 1, 0, 1, -1, -1, 0, -1};

Bitboard RAYS[8][64];

Bitboard step_mask(int sq, const int (*deltas)[2], int n) {
    Bitboard b = 0;
    for (int i = 0; i < n; ++i) {
        int f = file_of(sq) + deltas[i][0], r = rank_of(sq) + deltas[i][1];
        if (f >= 0 && f < 8 && r >= 0 && r < 8) b |= bit(r * 8 + f);
    }
    return b;
}

inline Bitboard ray_attack(int dir, int sq, Bitboard occ) {
    Bitboard ray = RAYS[dir][sq];
    Bitboard blockers = ray & occ;
    if (!blockers) return ray;
    int b = dir < 4 ? lsb(blockers) : msb(blockers);
    return ray ^ RAYS[dir][b];
}

}  // namespace

Bitboard rook_attacks(int sq, Bitboard occ) {
    return ray_attack(N, sq, occ) | ray_attack(E, sq, occ) | ray_attack(S, sq, occ) | ray_attack(W, sq, occ);
}

Bitboard bishop_attacks(int sq, Bitboard occ) {
    return ray_attack(NE, sq, occ) | ray_attack(NW, sq, occ) | ray_attack(SE, sq, occ) | ray_attack(SW, sq, occ);
}

void init_bitboards() {
    static const int knight[8][2] = {{1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};
    static const int king[8][2] = {{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
    static const int wpawn[2][2] = {{-1, 1}, {1, 1}};
    static const int bpawn[2][2] = {{-1, -1}, {1, -1}};

    for (int sq = 0; sq < 64; ++sq) {
        KNIGHT_ATTACKS[sq] = step_mask(sq, knight, 8);
        KING_ATTACKS[sq] = step_mask(sq, king, 8);
        PAWN_ATTACKS[WHITE][sq] = step_mask(sq, wpawn, 2);
        PAWN_ATTACKS[BLACK][sq] = step_mask(sq, bpawn, 2);

        for (int d = 0; d < 8; ++d) {
            Bitboard ray = 0;
            int f = file_of(sq) + DF[d], r = rank_of(sq) + DR[d];
            while (f >= 0 && f < 8 && r >= 0 && r < 8) {
                ray |= bit(r * 8 + f);
                f += DF[d];
                r += DR[d];
            }
            RAYS[d][sq] = ray;
        }
    }
}

}  // namespace chess
