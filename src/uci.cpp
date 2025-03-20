// UCI protocol front end. Works with any UCI GUI (Cute Chess, Arena, BanksiaGUI)
// or by typing commands in a terminal.
#include <chrono>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>

#include "bitboard.hpp"
#include "eval.hpp"
#include "position.hpp"
#include "search.hpp"

using namespace chess;

namespace {

std::mutex out_mutex;

void send(const std::string& line) {
    std::lock_guard<std::mutex> lock(out_mutex);
    std::cout << line << std::endl;
}

struct Engine {
    Position pos = Position::startpos();
    std::vector<std::uint64_t> history{pos.hash};
    Searcher searcher{64};
    std::thread worker;

    void wait() {
        if (worker.joinable()) worker.join();
    }

    void stop() {
        searcher.stop();
        wait();
    }

    // position [startpos | fen <fen>] [moves m1 m2 ...]
    void set_position(std::istringstream& in) {
        std::string token, fen;
        in >> token;
        if (token == "startpos") {
            pos = Position::startpos();
            in >> token;
        } else if (token == "fen") {
            while (in >> token && token != "moves") fen += token + " ";
            pos = Position::from_fen(fen);
        }
        history.assign(1, pos.hash);
        if (token != "moves") return;
        Position next;
        while (in >> token) {
            Move m = pos.parse_uci(token);
            if (!m) {
                send("info string illegal move " + token);
                break;
            }
            pos.make(m, next);
            pos = next;
            history.push_back(pos.hash);
        }
    }

    void go(std::istringstream& in) {
        SearchLimits limits;
        std::string token;
        while (in >> token) {
            if (token == "depth") in >> limits.depth;
            else if (token == "movetime") in >> limits.movetime_ms;
            else if (token == "wtime") in >> limits.time_left_ms[WHITE];
            else if (token == "btime") in >> limits.time_left_ms[BLACK];
            else if (token == "winc") in >> limits.increment_ms[WHITE];
            else if (token == "binc") in >> limits.increment_ms[BLACK];
            else if (token == "movestogo") in >> limits.movestogo;
            else if (token == "infinite") limits.infinite = true;
        }
        stop();
        worker = std::thread([this, limits] {
            SearchResult r = searcher.search(pos, history, limits, send);
            send("bestmove " + move_to_uci(r.best));
        });
    }

    void perft_divide(int depth) {
        auto start = std::chrono::steady_clock::now();
        MoveList list;
        pos.legal_moves(list);
        std::uint64_t total = 0;
        Position child;
        for (int i = 0; i < list.size; ++i) {
            pos.make(list[i], child);
            std::uint64_t n = depth > 1 ? perft(child, depth - 1) : 1;
            total += n;
            send(move_to_uci(list[i]) + ": " + std::to_string(n));
        }
        double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::ostringstream s;
        s << "\nNodes: " << total << "  (" << secs << "s, " << std::uint64_t(total / std::max(secs, 1e-9)) << " nps)";
        send(s.str());
    }

    void display() const {
        std::ostringstream s;
        for (int r = 7; r >= 0; --r) {
            s << r + 1 << "  ";
            for (int f = 0; f < 8; ++f) {
                Piece p = pos.board[r * 8 + f];
                s << (p == NO_PIECE ? '.' : "PNBRQKpnbrqk"[p]) << ' ';
            }
            s << '\n';
        }
        s << "   a b c d e f g h\n\nFEN: " << pos.fen() << "\nEval: " << evaluate(pos) << " cp (side to move)";
        send(s.str());
    }
};

}  // namespace

int main() {
    init_bitboards();
    init_zobrist();
    std::ios::sync_with_stdio(false);

    Engine engine;
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        std::string cmd;
        in >> cmd;

        if (cmd == "uci") {
            send("id name Bitboard Engine\nid author 7adan17\nuciok");
        } else if (cmd == "isready") {
            send("readyok");
        } else if (cmd == "ucinewgame") {
            engine.stop();
            engine.searcher.clear();
        } else if (cmd == "position") {
            engine.stop();
            engine.set_position(in);
        } else if (cmd == "go") {
            engine.go(in);
        } else if (cmd == "stop") {
            engine.stop();
        } else if (cmd == "quit") {
            engine.stop();
            break;
        } else if (cmd == "perft") {
            int depth = 1;
            in >> depth;
            engine.perft_divide(depth);
        } else if (cmd == "d") {
            engine.display();
        } else if (!cmd.empty()) {
            send("info string unknown command " + cmd);
        }
    }
    engine.wait();
}
