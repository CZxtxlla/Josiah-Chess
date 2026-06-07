# C-hess Engine Documentation

A high-performance chess engine written in C utilizing bitboard representations, precomputed attack tables, and optimized move generation.

## 1. Code structure

The engine's core architecture is separated into distinct modular components:
* `src/bitboard.c` & `src/bitboard.h` – Core bitboard manipulation utilities.
* `src/magic.c` & `src/magic.h` – Magic bitboard generation for sliding pieces.
* `src/movegen.c` & `src/movegen.h` – Legal and pseudo-legal move generation loops.
* `src/position.c` & `src/position.h` – Board state parsing, tracking, and updates.

## 2. Representing the game

This section goes over the code contained in src/bitboard.c, src/magic.c, src/movegen.c, src/position.c

### Board Representation

The engine makes use of a **bitboard** representation. The board is essentially a series of 64-bit unsigned integers (`U64`), with each bit representing a square on the chess board. Each 64 bit integer corresponds to a piece type and colour, thus 12 of these 64 bit integers are needed to undertand the whole board. A board position is these 12 bitboards, along with some information about if en passant is available, whose turn it is to move, and castling rights. The code is seen here:

```c
typedef struct {
    U64 pieces[12]; // one bitboard for each piece type

    U64 occupancy[3]; // for fast checks of white pieces, black pieces, all pieces

    int side; // WHITE or BLACK
    int en_passant; // En passant square (-1 if there is none)
    int castling_rights; // 4 bit mask (WK | WQ | BK | BQ)

} Position;
```

Note each square is numbered as follows

```c
// (A1 = 0, H8 = 63)
enum {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8
};
```

thus in a 64-bit bitboard, the lowest order bit represents the A1 square and the highest order bit represents the H8 square.

### Move Generation

All moves are encoded as a 32 bit integer as follows:

```
  Move representation in a single 32-bit integer:
  0000 0000 0000 0000 0000 0011 1111 -> From square (Bits 0-5)
  0000 0000 0000 0000 1111 1100 0000 -> To square (Bits 6-11)
  0000 0000 0000 1111 0000 0000 0000 -> Promoted piece flags (Bits 12-15)
  0000 0000 0001 0000 0000 0000 0000 -> En Passant flag (Bit 16)
  0000 0000 0010 0000 0000 0000 0000 -> Double Pawn Push flag (Bit 17)
  0000 0000 0100 0000 0000 0000 0000 -> Castling flag (Bit 18)
```

The function `void generate_moves(const Position* pos, MoveList* list)` takes in a position, and a list, and populates the list with all the possible moves from the given position. Note in a realistic chess game, this will never exceed 256 so we can fix the moveList array size at 256.

To get all pawn moves, a bitwise shift of the pawn bitboard (left 8 bits if white is moving, right 8 bits if black is moving) simulates a single push for every pawn. We then serialize the moves by repeatedly finding the location of the lowest order bit and marking this location as the destination and the bit 8 bits prior to be the source. Here is this in code:

```c
// white pawns
bitboard = pos->pieces[P];

pushes = (bitboard << 8) & ~pos->occupancy[BOTH]; // set move

// single pushes
U64 single_pushes = pushes;
while (single_pushes) {
    to_sq = __builtin_ctzll(single_pushes);

    from_sq = to_sq - 8;

    if (to_sq >= A8 && to_sq <= H8) {
        addMove(list, encode_move(from_sq, to_sq, Q, 0, 0, 0));
        addMove(list, encode_move(from_sq, to_sq, R, 0, 0, 0));
        addMove(list, encode_move(from_sq, to_sq, B, 0, 0, 0));
        addMove(list, encode_move(from_sq, to_sq, N, 0, 0, 0));
    } else {
        addMove(list, encode_move(from_sq, to_sq, 0, 0, 0, 0));
    }
    pop_bit(single_pushes, to_sq);
}
```

I won't go through it all, but for captures and double pushes etc... it is very similar, like here is the code for double pushes. Note we apply a mask to the 4th row (since this is white) so that only pawns that were double pushed from the starting row will show up.

```c
// double pushes
U64 double_pushes = (pushes << 8) & 0x00000000FF000000ULL & ~pos->occupancy[BOTH];
while (double_pushes) {
    to_sq = __builtin_ctzll(double_pushes);

    from_sq = to_sq - 16;
    addMove(list, encode_move(from_sq, to_sq, 0, 0, 1, 0));
    pop_bit(double_pushes, to_sq);
}
```

For the knights and kings, we can do some precomputing for their movement. Since they can't be blocked by anything we can preemptively compute their movement positions from any given square and store them in an array, with each element in the array containing a 64-bit bitboard with all the potential attack squares from the square given by the index. To compute this attack bitboard we simply perform some more bit shifts, using bit masks to ensure no wraparound happens.

```c
void init_leapers() {
    // initialize king and knight move arrays
    for (int square = 0; square < 64; square++) {
        U64 bitboard = 0ULL;
        set_bit(bitboard, square);

        U64 knight = 0ULL;
        
        // up-left and up-right
        knight |= (bitboard << 15) & not_h_file;
        knight |= (bitboard << 17) & not_a_file;

        // down-left and down-right
        knight |= (bitboard >> 15) & not_a_file;
        knight |= (bitboard >> 17) & not_h_file;

        // right-up and right-down
        knight |= (bitboard << 10) & not_ab_file;
        knight |= (bitboard >> 6) & not_ab_file;

        // left-up and left-down
        knight |= (bitboard << 6) & not_gh_file;
        knight |= (bitboard >> 10) & not_gh_file;

        knight_attacks[square] = knight;

        U64 king = 0ULL;

        // up down left right
        king |= (bitboard << 8);
        king |= (bitboard >> 8);
        king |= (bitboard << 1) & not_a_file;
        king |= (bitboard >> 1) & not_h_file;

        // diagonals
        king |= (bitboard << 9) & not_a_file;
        king |= (bitboard >> 7) & not_a_file;
        king |= (bitboard << 7) & not_h_file;
        king |= (bitboard >> 9) & not_h_file;

        king_attacks[square] = king;
    }
}
```

Bishops and Rooks are different than the previous pieces, their movement depends on if there is a piece in blocking their path. If we were to store the moves for every square, for every possible occupancywe would have to store an array `attacks[64][2^64]`, which is much too large. Taking the example of a rook, first notice that the rook doesn't care about the whole board, only the occupancy in the same file and rank as it. That reduces the occupancies we care about to 12. Thus we only care about $2^{12}$ possible blocker configurations, down from $2^{64}$. (This number is $2^9$ for the bishop).

Now, if we get the mask of the rook for the specific square it is on, and AND it with the occupancy bitboard, we end up with a very sparse 64 bit integer. We need this integer to translate 1 to 1 with an index in the range $0$ to $2^{12}$. This is where the magic number comes in, and multiplying this masked occupancy with this special number, and shifting down by $2^{(64 - 12)}$, we obtain a unique index between $0$ and $2 ^{12} - 1$. Thus if we precompute the required bitboards we can reference them at any time using this magic number multiplication followed by a bit shift.

The following is a walkthrough of how this is implemented in code. Firstly, for now, assume we have these magic numbers already, one for each square and different for rooks and bishops. Here is how the attack arrays are initialized:

```c
void init_sliders() {
    for (int square = 0; square < 64; square++) {
        rook_masks[square] = mask_rook_attacks(square);
        int r_bits = __builtin_popcountll(rook_masks[square]);
        int r_perm = (1 << r_bits); // 2^r_bits;

        rook_magics[square] = find_magic_number(square, r_bits, 0);

        for (int i = 0; i < r_perm; i++) {
            U64 occ = set_occupancy(i, r_bits, rook_masks[square]);

            // The Magic Hash Formula
            int magic_index = (occ * rook_magics[square]) >> (64 - r_bits);

            rook_attacks[square][magic_index] = rook_attacks_cast(square, occ);
        }

        bishop_masks[square] = mask_bishop_attacks(square);
        int b_bits = __builtin_popcountll(bishop_masks[square]);
        int b_perm = (1 << b_bits); // 2^b_bits;

        bishop_magics[square] = find_magic_number(square, b_bits, 1);

        for (int i = 0; i < b_perm; i++) {
            U64 occ = set_occupancy(i, b_bits, bishop_masks[square]);

            // The Magic Hash Formula
            int magic_index = (occ * bishop_magics[square]) >> (64 - b_bits);

            bishop_attacks[square][magic_index] = bishop_attacks_cast(square, occ);
        }
    }
}
```

The function `set_occupancy(int index, int bits_in_mask, U64 attack_mask)` takes in the index from $0$ to $2^{12} - 1$ and computes it's respective permutation of the attack_mask. Here is the code

```c
U64 set_occupancy(int index, int bits_in_mask, U64 attack_mask) {
    // maps index number [0 to 2^(bits_in_mask)] to bitboard subset of mask bitboard

    U64 occupancy = 0ULL;

    for (int count = 0; count < bits_in_mask; count++) {
        // loop through each bit in the mask and add it to the subset if it is specified in the index
        int square = __builtin_ctzll(attack_mask);

        pop_bit(attack_mask, square);

        if (index & (1 << count)) {
            // put a piece on the square if it's bit is 1
            set_bit(occupancy, square);
        }
    }

    return occupancy;
}
```

Once the arrays are actually initialized, we can this generate the possible moves of a rook in any position in O(1) time. To do this, we simply apply the rook_mask and perform the magic hash formula to get the index, and we use that on the precomputed array. Here is it in code

```c
U64 get_rook_attacks(int square, U64 occupancy) {

    U64 relevant_occupancy = occupancy & rook_masks[square];

    int r_bits = __builtin_popcountll(rook_masks[square]);
    int magic_index = (relevant_occupancy * rook_magics[square]) >> (64 - r_bits);

    return rook_attacks[square][magic_index];
}
```

-------------

Using all of this, we an thus generate all the moves from any given position in O(1) time, and this is the core of what allows the chess bot to see into the future.

## 3. Chess Bot Architecture

### Negamax & Alpha Beta Pruning
At its core the chess bot is an implementation of alpha beta pruning minimax. The variant implemented here is negamax, but that is just a small simplification of minimax. It relies on the principle that min(a, b) = -max(-a, -b).

(I will come back to this later...)

## 4. Results


Mark_1 (quiescent search, basic move ordering, material only evaluation function)

...      CharlesEngine playing White: 18 - 4 - 3  [0.780] 25
...      CharlesEngine playing Black: 12 - 10 - 3  [0.540] 25
...      White vs Black: 28 - 16 - 6  [0.620] 50
Elo difference: 115.2 +/- 97.8, LOS: 99.2 %, DrawRatio: 12.0 %
SPRT: llr 0 (0.0%), lbound -inf, ubound inf

Player: CharlesEngine
   "Draw by 3-fold repetition": 4
   "Draw by insufficient mating material": 1
   "Draw by stalemate": 1
   "Loss: Black mates": 4
   "Loss: White mates": 10
   "Win: Black loses on time": 1
   "Win: Black mates": 11
   "Win: White loses on time": 1
   "Win: White mates": 17
Player: Stockfish
   "Draw by 3-fold repetition": 4
   "Draw by insufficient mating material": 1
   "Draw by stalemate": 1
   "Loss: Black loses on time": 1
   "Loss: Black mates": 11
   "Loss: White loses on time": 1
   "Loss: White mates": 17
   "Win: Black mates": 4
   "Win: White mates": 10
Finished match

Mark_2 (quiescent search, basic move ordering, material & piece square tables evaluation function)

The following is performance against stockfish skill level 4 and 10 seconds + 0.1 time control

...      Mark2 playing White: 8 - 15 - 2  [0.360] 25
...      Mark2 playing Black: 9 - 16 - 0  [0.360] 25
...      White vs Black: 24 - 24 - 2  [0.500] 50
Elo difference: -100.0 +/- 101.4, LOS: 2.2 %, DrawRatio: 4.0 %
SPRT: llr 0 (0.0%), lbound -inf, ubound inf

Player: Mark2
   "Draw by 3-fold repetition": 2
   "Loss: Black mates": 15
   "Loss: White mates": 16
   "Win: Black mates": 9
   "Win: White mates": 8
Player: Stockfish
   "Draw by 3-fold repetition": 2
   "Loss: Black mates": 9
   "Loss: White mates": 8
   "Win: Black mates": 15
   "Win: White mates": 16
Finished match

Mark_3 (quiescent search, basic move ordering, material & multiple piece square tables for opening/endgame evaluation function)

The following is performance against stockfish skill level 4 and 10 seconds + 0.1 time control

...      Mark3 playing White: 13 - 10 - 2  [0.560] 25
...      Mark3 playing Black: 10 - 12 - 3  [0.460] 25
...      White vs Black: 25 - 20 - 5  [0.550] 50
Elo difference: 6.9 +/- 93.4, LOS: 55.9 %, DrawRatio: 10.0 %
SPRT: llr 0 (0.0%), lbound -inf, ubound inf

Player: Mark3
   "Draw by 3-fold repetition": 4
   "Draw by insufficient mating material": 1
   "Loss: Black mates": 10
   "Loss: White mates": 12
   "Win: Black mates": 10
   "Win: White mates": 13
Player: Stockfish
   "Draw by 3-fold repetition": 4
   "Draw by insufficient mating material": 1
   "Loss: Black mates": 10
   "Loss: White mates": 13
   "Win: Black mates": 10
   "Win: White mates": 12
Finished match


Mark_4 (quiescent search, basic move ordering, material & multiple piece square tables for opening/endgame evaluation function, small opening book)

...      Mark4 playing White: 10 - 12 - 3  [0.460] 25
...      Mark4 playing Black: 8 - 15 - 2  [0.360] 25
...      White vs Black: 25 - 20 - 5  [0.550] 50
Elo difference: -63.2 +/- 95.1, LOS: 9.0 %, DrawRatio: 10.0 %
SPRT: llr 0 (0.0%), lbound -inf, ubound inf

Player: Mark4
   "Draw by 3-fold repetition": 5
   "Loss: Black mates": 12
   "Loss: White mates": 15
   "Win: Black mates": 8
   "Win: White mates": 10
Player: Stockfish
   "Draw by 3-fold repetition": 5
   "Loss: Black mates": 8
   "Loss: White mates": 10
   "Win: Black mates": 12
   "Win: White mates": 15
Finished match