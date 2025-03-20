# Bitboard Chess Engine

A UCI chess engine in C++17.

**Board and move generation**
- 64-bit bitboards per piece, plus a mailbox array for O(1) piece lookup
- Precomputed knight, king and pawn attack tables; ray-based sliding attacks (the nearest blocker comes from LSB or MSB)
- Copy-make position updates with incremental Zobrist hashing
- Every rule: castling (no castling out of, through or into check), en passant, under-promotions, 50-move rule, repetition
- Checked with **perft** against six standard positions from chessprogramming.org (startpos d5, Kiwipete d4, and others)

**Search**
- Iterative deepening negamax with **alpha-beta pruning** and principal variation search
- Transposition table, null-move pruning, check extensions, mate-distance pruning
- Quiescence search over captures
- Move ordering: TT move, then captures by MVV-LVA, killer moves, and the history heuristic
- Time management from `wtime/btime/winc/binc/movestogo` or `movetime`

**Evaluation**: material, piece-square tables, a king table tapered from middlegame to endgame, and a bishop-pair bonus.

## Usage

```bash
make            # build/engine and build/test
make test       # perft and tactics tests
./build/engine  # speaks UCI on stdin/stdout
```

Load `build/engine` in any UCI GUI (Cute Chess, Arena, BanksiaGUI) to play against it. Extra commands for debugging:

```
position startpos moves e2e4
d                # print the board, FEN and static eval
perft 5          # per-move node counts
go movetime 2000
```
