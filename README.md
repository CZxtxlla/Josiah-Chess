# Josiah Chess Engine

Josiah is a C chess engine with bitboard move generation, NNUE evaluation, UCI support, and optional Syzygy endgame tablebases.

## Features

- Bitboard-based move generation
- Magic bitboards for sliding pieces
- UCI protocol support for chess GUIs
- Quantized NNUE evaluation
- Optional Syzygy tablebase probing
- Opening book support

## Building

### Build with CMake

```bash
cd /path/to/C-hess
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The executable is written to `build/bin/josiah_engine`. The build also stages the NNUE model in `build/bin/nnue/`.

### Manual build

```bash
cd /path/to/C-hess/src
gcc -O3 -march=native -flto main.c bitboard.c position.c movegen.c magic.c \
    search.c evaluate.c uci.c zobrist.c ../nnue/inference.c ../syzygy/tbprobe.c \
    -o josiah_engine -lm
```

## Running

Start the engine directly:

```bash
./build/bin/josiah_engine
```

The engine speaks UCI. Common working commands are:

- `uci` to print engine identity and options
- `isready` to confirm the engine is ready
- `position startpos` to load the initial chess position
- `position fen <fen-string>` to load a custom position
- `setoption name Hash value 128` to change transposition table size
- `setoption name SyzygyPath value /path/to/tables` to enable optional Syzygy tablebases (path is absolute)
- `go wtime 300000 btime 300000` to start searching
- `quit` to exit

Example session:

```text
uci
setoption name SyzygyPath value /absolute/path/to/tables
position startpos
go wtime 60000 btime 60000
quit
```

## Notes

- The NNUE loader expects the quantized model `nnue/768_quant_9_18_50_1024.nnue`.
- Syzygy tablebases are optional and only used after the UCI `SyzygyPath` option is set.
- If you move the binary outside the build tree, keep the `nnue/` directory next to it otherwise the engine will crash.

## Layout

```text
src/       Engine implementation
include/   Public headers
nnue/      NNUE inference code and models
syzygy/    Tablebase probing code
tables/    Optional Syzygy tablebase files
```

## Contributing

To extend or modify the engine:
1. Add source files to `src/` and headers to `include/`
2. Update `CMakeLists.txt` with new source files
3. Rebuild with `cmake` and `make`

Some weak points include the handcrafted evaluation function that is being used in tandem with the nnue, as well as the opening book which is little more than a text file lookup.

## References
Obviously the chess programming wiki but definitely also for the nnue stuff the stockfish documentation is a great resource. Here are several key resources used.

- [UCI Protocol Specification](https://gist.github.com/DOBRO/2592c6dad754ba67e6dcaec8c90165bf)
- [Bitboards](https://www.chessprogramming.org/Bitboards)
- [Alpha-Beta Pruning](https://www.chessprogramming.org/Alpha-Beta)
- [NNUE Evaluation](https://official-stockfish.github.io/docs/nnue-pytorch-wiki/docs/nnue.html)

Additionally llms are very useful tools for understanding some of the more difficult concepts (like the nnue or alpha beta optimizations).

---

**Last Updated**: 2026-07-08
