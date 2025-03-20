#include "eval.hpp"

namespace chess {

namespace {

// Piece-square tables from Tomasz Michniewski's "Simplified Evaluation
// Function". Written from White's view with rank 8 on the first row, so a
// white piece on sq reads index sq ^ 56 and a black piece reads index sq.
constexpr int PST[6][64] = {
    {  0,  0,  0,  0,  0,  0,  0,  0,
      50, 50, 50, 50, 50, 50, 50, 50,
      10, 10, 20, 30, 30, 20, 10, 10,
       5,  5, 10, 25, 25, 10,  5,  5,
       0,  0,  0, 20, 20,  0,  0,  0,
       5, -5,-10,  0,  0,-10, -5,  5,
       5, 10, 10,-20,-20, 10, 10,  5,
       0,  0,  0,  0,  0,  0,  0,  0},
    {-50,-40,-30,-30,-30,-30,-40,-50,
     -40,-20,  0,  0,  0,  0,-20,-40,
     -30,  0, 10, 15, 15, 10,  0,-30,
     -30,  5, 15, 20, 20, 15,  5,-30,
     -30,  0, 15, 20, 20, 15,  0,-30,
     -30,  5, 10, 15, 15, 10,  5,-30,
     -40,-20,  0,  5,  5,  0,-20,-40,
     -50,-40,-30,-30,-30,-30,-40,-50},
    {-20,-10,-10,-10,-10,-10,-10,-20,
     -10,  0,  0,  0,  0,  0,  0,-10,
     -10,  0,  5, 10, 10,  5,  0,-10,
     -10,  5,  5, 10, 10,  5,  5,-10,
     -10,  0, 10, 10, 10, 10,  0,-10,
     -10, 10, 10, 10, 10, 10, 10,-10,
     -10,  5,  0,  0,  0,  0,  5,-10,
     -20,-10,-10,-10,-10,-10,-10,-20},
    {  0,  0,  0,  0,  0,  0,  0,  0,
       5, 10, 10, 10, 10, 10, 10,  5,
      -5,  0,  0,  0,  0,  0,  0, -5,
      -5,  0,  0,  0,  0,  0,  0, -5,
      -5,  0,  0,  0,  0,  0,  0, -5,
      -5,  0,  0,  0,  0,  0,  0, -5,
      -5,  0,  0,  0,  0,  0,  0, -5,
       0,  0,  0,  5,  5,  0,  0,  0},
    {-20,-10,-10, -5, -5,-10,-10,-20,
     -10,  0,  0,  0,  0,  0,  0,-10,
     -10,  0,  5,  5,  5,  5,  0,-10,
      -5,  0,  5,  5,  5,  5,  0, -5,
       0,  0,  5,  5,  5,  5,  0, -5,
     -10,  5,  5,  5,  5,  5,  0,-10,
     -10,  0,  5,  0,  0,  0,  0,-10,
     -20,-10,-10, -5, -5,-10,-10,-20},
    {-30,-40,-40,-50,-50,-40,-40,-30,
     -30,-40,-40,-50,-50,-40,-40,-30,
     -30,-40,-40,-50,-50,-40,-40,-30,
     -30,-40,-40,-50,-50,-40,-40,-30,
     -20,-30,-30,-40,-40,-30,-30,-20,
     -10,-20,-20,-20,-20,-20,-20,-10,
      20, 20,  0,  0,  0,  0, 20, 20,
      20, 30, 10,  0,  0, 10, 30, 20},
};

constexpr int KING_ENDGAME[64] = {
    -50,-40,-30,-20,-20,-30,-40,-50,
    -30,-20,-10,  0,  0,-10,-20,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-30,  0,  0,  0,  0,-30,-30,
    -50,-30,-30,-30,-30,-30,-30,-50};

constexpr int PHASE_WEIGHT[6] = {0, 1, 1, 2, 4, 0};
constexpr int MAX_PHASE = 24;

}  // namespace

int evaluate(const Position& pos) {
    int score[2] = {0, 0}, king_mg[2] = {0, 0}, king_eg[2] = {0, 0};
    int phase = 0;

    for (int p = 0; p < 12; ++p) {
        const Color c = color_of(Piece(p));
        const PieceType t = type_of(Piece(p));
        Bitboard b = pos.pieces[p];
        while (b) {
            int sq = pop_lsb(b);
            int idx = c == WHITE ? sq ^ 56 : sq;
            phase += PHASE_WEIGHT[t];
            if (t == KING) {
                king_mg[c] = PST[KING][idx];
                king_eg[c] = KING_ENDGAME[idx];
            } else {
                score[c] += PIECE_VALUE[t] + PST[t][idx];
            }
        }
    }
    if (phase > MAX_PHASE) phase = MAX_PHASE;

    // Taper the king between hiding (middlegame) and centralizing (endgame).
    for (int c = 0; c < 2; ++c) {
        score[c] += (king_mg[c] * phase + king_eg[c] * (MAX_PHASE - phase)) / MAX_PHASE;
        if (popcount(pos.pieces[make_piece(Color(c), BISHOP)]) >= 2) score[c] += 30;
    }

    int white_view = score[WHITE] - score[BLACK];
    return pos.side == WHITE ? white_view : -white_view;
}

}  // namespace chess
