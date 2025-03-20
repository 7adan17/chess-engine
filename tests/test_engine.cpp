// Perft against published node counts (chessprogramming.org/Perft_Results),
// plus some tactical sanity checks for the search.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "../src/bitboard.hpp"
#include "../src/position.hpp"
#include "../src/search.hpp"

using namespace chess;

static int failures = 0;

static void check(bool ok, const std::string& what) {
    std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) ++failures;
}

struct PerftCase {
    const char* name;
    const char* fen;
    int depth;
    std::uint64_t expected;
};

static void perft_suite() {
    const PerftCase cases[] = {
        {"startpos", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5, 4865609},
        {"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4, 4085603},
        {"position 3", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 5, 674624},
        {"position 4", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 4, 422333},
        {"position 5", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 4, 2103487},
        {"position 6", "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 4, 3894594},
    };
    for (const auto& c : cases) {
        auto t0 = std::chrono::steady_clock::now();
        std::uint64_t got = perft(Position::from_fen(c.fen), c.depth);
        double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        char buf[160];
        std::snprintf(buf, sizeof buf, "perft %-10s depth %d = %llu (expected %llu, %.2fs)", c.name, c.depth,
                      (unsigned long long)got, (unsigned long long)c.expected, s);
        check(got == c.expected, buf);
    }
}

static void fen_roundtrip() {
    const char* fen = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";
    check(Position::from_fen(fen).fen() == fen, "FEN round trip");
}

static void incremental_hash_matches() {
    // Playing moves incrementally must give the same hash as parsing the resulting FEN.
    // Covers double pushes, captures, checks, castling-rights loss and a capture-promotion.
    Position pos = Position::startpos(), next;
    bool ok = true;
    for (const char* m : {"e2e4", "d7d5", "e4d5", "g8f6", "f1b5", "c7c6", "d5c6", "d8d2", "b1d2", "e7e5", "c6b7", "c8d7", "b7a8q"}) {
        Move mv = pos.parse_uci(m);
        ok = ok && mv && pos.make(mv, next);
        pos = next;
        ok = ok && pos.hash == Position::from_fen(pos.fen()).hash;
    }
    check(ok, "incremental Zobrist hash");
}

static std::string best_move(const char* fen, int depth) {
    Searcher s(16);
    SearchLimits limits;
    limits.depth = depth;
    return move_to_uci(s.search(Position::from_fen(fen), {}, limits).best);
}

static void tactics() {
    check(best_move("6k1/5ppp/8/8/8/8/5PPP/R5K1 w - - 0 1", 4) == "a1a8", "finds back-rank mate in 1");
    check(best_move("r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 4 4", 4) == "h5f7",
          "finds scholar's mate");
    check(best_move("4k3/8/8/3q4/8/8/3R4/3K4 w - - 0 1", 4) == "d2d5", "wins hanging queen");
    // Mate in 2: 1. Qb8+ Nxb8 2. Rd8#
    check(best_move("1n4k1/5ppp/8/8/8/8/1Q3PPP/3R2K1 w - - 0 1", 5) == "b2b8", "finds queen-sacrifice mate in 2");
}

int main() {
    init_bitboards();
    init_zobrist();
    fen_roundtrip();
    incremental_hash_matches();
    perft_suite();
    tactics();
    std::printf("\n%s\n", failures ? "SOME TESTS FAILED" : "all tests passed");
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
