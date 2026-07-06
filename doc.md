# C-hess Engine Documentation

A high-performance chess engine written in C utilizing bitboard representations, precomputed attack tables, and optimized move generation.

## 1. How To Use

Here I will mention requirements, etc... I will get around to it eventually once it is something I'm kind of happy with.

## 2. Code structure

The engine's core architecture is separated into distinct modular components:
* `src/bitboard.c` & `src/bitboard.h` – Core bitboard manipulation utilities.
* `src/magic.c` & `src/magic.h` – Magic bitboard generation for sliding pieces.
* `src/movegen.c` & `src/movegen.h` – Legal and pseudo-legal move generation loops.
* `src/position.c` & `src/position.h` – Board state parsing, tracking, and updates.

## 3. Representing the game

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

Bishops and Rooks are different than the previous pieces, as their movement depends on if there is a piece in blocking their path. If we were to store the moves for every square, for every possible occupancywe would have to store an array `attacks[64][2^64]`, which is much too large. Taking the example of a rook, first notice that the rook doesn't care about the whole board, only the occupancy in the same file and rank as it. That reduces the occupancies we care about to 12. Thus we only care about $2^{12}$ possible blocker configurations, down from $2^{64}$. (This number is $2^9$ for the bishop).

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

Using all of this, we can thus generate all the moves from any given position in O(1) time, and this is the core of what allows the chess bot to see into the future.

## 4. Chess Bot Architecture

The following will describe and explain the workings behind the chess bot, roughly in the order that they were implemented.

### Negamax & Alpha Beta Pruning
At its core the chess bot is an implementation of alpha beta pruning minimax. **Minimax** works by constructing a search tree with all possible move combinations until a certain depth. Once the depth is reached, the position is statically evaluated and assigned a score based on how good it is. Then, moving up the tree if the position had black to move, it will select the move with the minimum score of all possible children and if the position had white to move it will select the move with the maximum score of all possible children. The version implemented in the code is **negamax**, a simplification of minimax relying on the fact that $\min(a, b) = -\max(-a, -b)$. We can thus write one version of the function for the side maximizing and just negate the minimizer. Note when adding alpha beta pruning, which I am about to explain, we also must negate the alpha and beta when applying negamax over minimax for this reason.

**Alpha beta pruning** is an optimization of minimax/negamax that works to skip nodes that don't need to be evaluated. For example, if in a subtree there is a move that white can force that is so good for white, there is no point evaluating the other nodes in that subtree since black will never choose this subtree. The variable `beta` keeps track of the maximum score the opponent is guaranteed, so we know that if we find a score in this branch that exceeds beta, the opponent will never choose this branch and thus we can skip evaluating all the sibling nodes in this branch. The variable `alpha` keeps track of the minimum score the current player can force, which is why it is the best move for the current player.

The following is the base implementation of this. Note in the actual code several enhancements are applied like transposition tables that I will mention later.

```c
int negamax(Position* pos, int depth, int distance, int alpha, int beta) {
    // base case
    if (depth == 0) {
        nodes_evaluated++;
        return evaluate(pos);
    }

    MoveList list;
    generate_moves(pos, &list); // get all moves

    int legal_moves = 0;

    // go through the tree, check every move
    for (int i = 0; i < list.count; i++) {
        Position next_state = *pos;

        if (make_move(&next_state, list.moves[i])) {
            legal_moves++;

            int score = -negamax(&next_state, depth - 1, distance + 1, -beta, -alpha);

            if (score > alpha) {
                alpha = score;

                if (distance == 0) {
                    best_move = list.moves[i];
                }
            }

            // prune the branch
            if (alpha >= beta) {
                break; 
            }
        }
    }

    //checkmate or stalemate
    if (legal_moves == 0) {
        int king_type = (pos->side == WHITE) ? K : k;
        int king_sq = __builtin_ctzll(pos->pieces[king_type]);

        if (is_square_attacked(king_sq, pos->side^1, pos)) {
            return -49000 + distance;
        } else {
            return 0;
        }
    }

    return alpha;
}
```

### Evaluation Function
From the previous section, I mentioned that when we reach the bottom of the tree we must statically evaluate a position. This is no easy feat, as it's very difficult to actually quantify what is a winning position, and by how much. The current evaluation function makes use of two methods to assign a position a score. First is the material value on the board. Each piece is assigned a value, queens being the highest pawns being the lowest, and the total value of pieces on the board is tallied up, adding the values for the current player and subtracting them for the opponent. 

The other method of evaluating a position used is called **piece-square tables**. This is essentially a series of tables assigning each piece a bonus or penalty for what square it is on. This works to favor positions where knights are in the center of the board, pawns are pushed, etc... For kings and pawns, there are actually two tables, one for the endgame and one for the midgame. The scores given from these tables is smoothed between based on how many pieces are on the board. It aims to reflect the importance of hiding the king in the middle game versus getting the king involved in the endgame, and the importance of pushing pawns in the endgame.

### Quiescence

### Move Ordering
The goal of move ordering is to premeptively sort the moves before being visited in the alpha beta search tree such that the best looking moves are visited first, allowing for much more pruning of the search tree. Obviously we can't sort the moves by evaluation without actually performing the evaluation, but we can kind of guess what are good moves. For example, captures are a lot of the time a good move, thus it is good to assign a higher score to captures such that when we sort by score the captures come towards the front.

Firstly, we check if the current position exists in the transposition table. If it does, then the move stored there is ordered first since it previously proved to be the best move in this position and will likely cause a beta cutoff.

Secondly, we check if a move is a promotion and assign it the second highest score since these are generally the best possible move.

Captures are assigned the MVV-LVA (most valuable victim, least valuable attacker) heuristic. All captures are evaluated before quiet moves, but the better captures in terms of material value gain are evaluated first.

All that is left are "quiet moves", i.e. moves that we have no information about and are also not captures. For these moves, we employ two different heuristics to order the moves. Firstly we use something called the **killer heuristic**. A move is a *killer move* if it causes a beta cutoff in a previously evaluated branch at the same depth (ply). The killer heuristic relies on the fact that if a move is strong for one variation, it will likely be strong for its siblings. We track the last two killer moves at every depth level in an array and if we recognize a killer move we evaluate it before other quiet moves.

Lastly, our failsafe is the **history heuristic**. This is similar to the killer heuristic, but instead of being local to a certain depth, it is global for the whole tree. Anytime a move causes a beta cutoff, we increment the counter stored in the [from][to] location of the history array. Thus historically stronger positions are evaluated before others.

Below is the implementation for all this, it is pretty straightforward one just has to update the history_moves and killer_moves tables accordingly during the search.

```c
int score_move(Position* pos, int move, int distance, int hash_move) {
    if (move == hash_move) {
        return 40000; // hash table moves are really good
    }

    if (get_move_promoted(move)) {
        return 30000; // promotions really good
    }

    int from = get_move_from(move);
    int to = get_move_to(move);

    int attacker = get_piece_at(pos, from);
    int victim = get_piece_at(pos, to);

    // en passant, victim is pawn
    if (get_move_ep(move)) {
        victim = P; 
    }

    // if there is a victim
    if (victim != -1) {
        int attacker_val = piece_values[attacker];
        int victim_val = piece_values[victim];

        return victim_val - attacker_val + 20000;
    }

    // killer heuristic
    if (move == killer_moves[0][distance]) return 19000;
    if (move == killer_moves[1][distance]) return 18000;

    // history moves, baseline
    return history_moves[pos->side][from][to];
}
```

### Opening Book

We store an opening book for the engine to use in order to save computation time and ensure a strong line is played. Ideally I should reimplement it using hashing, but for now since the opening book is relatively small it is simply stored in a text file. The UCI protocol sends the game history everytime it asks for a new move, so we simply parse the game history and see if we recognize it in the stored openings. If it is recognized, then a random move is chosen that extends it. This just works to save computation in the early game and ensure a standard and proven opening line is played.

### Transposition Table

This is an extremely important optimization for the search. Chess has the property that many identical positions can be reached through entirely different move orders. These are called **transpositions**. Say we are in the start position, then the following two sequences of moves will result in the same board state: `1. e4 e5 2. Nf3 Nc6` and `1. Nf3 Nc6 2. e4 e5`. In a standard alpha beta search, these two positions would then be evaluated independently, duplicating an evaluation. This would happen many times over the whole search, wasting computation on positions already evaluated.

To solve this, a **transposition table** caches evaluated positions, allowing the engine to instantly look up the scores for previously evaluated positions. There are three steps to implementing this, zobrist hashing, the table entry structure, and the integration into the alpha beta search.

**Zobrist hashing** works to assign a unique identifier to every possible board state. It works as follows, on initialization a series of random 64 bit numbers are created for every piece on every square, as well as for the side to move, castling rights, and the en passant files. Here is that code:

```c
void init_zobrist() {
    for (int piece = 0; piece < 12; piece++) {
        for (int sq = 0; sq < 64; sq++) {
            piece_keys[piece][sq] = get_random_U64_xorshift();
        }
    }

    for (int file = 0; file < 8; file++) {
        en_passant_keys[file] = get_random_U64_xorshift();
    }

    for (int i = 0; i < 16; i++) {
        castling_keys[i] = get_random_U64_xorshift();
    }

    side_key = get_random_U64_xorshift();
} 
```

Note the random numbers are generated using an algorithm by George Marsaglia called [xorshift64](https://en.wikipedia.org/wiki/Xorshift). Here is the implementation:

```c
U64 get_random_U64_xorshift() {
    random_state_xor ^= random_state_xor << 13;
    random_state_xor ^= random_state_xor >> 7;
    random_state_xor ^= random_state_xor << 17;

    return random_state_xor;
}
```

Once we have these random numbers, we can compute the hash of any position by performing a bitwise XOR ($\oplus$) with the random numbers corresponding to the pieces currently on the board. For example, the hash for the starting position would be computed like:

$$
Hash = R_{WhiteRookA1} \oplus R_{WhiteKnightB1} \oplus \dots \oplus R_{BlackKingE8}
$$

What is unique and important about Zobrist hashing here is the fact that XOR is its own inverse. If we want to compute the hash of the starting position but with the move `e2e4`, we can simply XOR the hash with the e2 pawn random number to remove it from the hash and then XOR again with the e4 pawn random number to add it, like so:

```c
hash ^= zobrist_pieces[WHITE_PAWN][e2]; // remove piece from origin
hash ^= zobrist_pieces[WHITE_PAWN][e4]; // place piece on destination
```

### Null Move Pruning

[Null Move Pruning](https://www.chessprogramming.org/Null_Move_Pruning).

### Principle Variation Search

### Late Move Reduction

## 5. Results


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

The following is performance against stockfish skill level 4 and 10 seconds + 0.1 time control

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


Mark_5 (quiescent search, basic move ordering, material & multiple piece square tables for opening/endgame evaluation function, small opening book, killer/history move ordering heuristics)

The following is performance against stockfish skill level 4 and 10 seconds + 0.1 time control

...      Mark5 playing White: 11 - 11 - 3  [0.500] 25
...      Mark5 playing Black: 9 - 11 - 5  [0.460] 25
...      White vs Black: 22 - 20 - 8  [0.520] 50
Elo difference: -13.9 +/- 90.2, LOS: 37.9 %, DrawRatio: 16.0 %
SPRT: llr 0 (0.0%), lbound -inf, ubound inf

Player: Mark5
   "Draw by 3-fold repetition": 8
   "Loss: Black mates": 11
   "Loss: White mates": 11
   "Win: Black mates": 9
   "Win: White mates": 11
Player: Stockfish
   "Draw by 3-fold repetition": 8
   "Loss: Black mates": 9
   "Loss: White mates": 11
   "Win: Black mates": 11
   "Win: White mates": 11
Finished match

Mark_6 (quiescent search, basic move ordering, material & multiple piece square tables for opening/endgame evaluation function, small opening book, killer/history move ordering heuristics, transposition tables)

Note, this iteration also included a fix for a bug with the opening book thinking every fen loaded is the start position and trying to use book moves. Thus all versions before this that included an opening book (version 4 and 5) are non-functioning from any position other than the start position. Mark 1-3 still work since the opening book didn't exist then.

The following is performance against stockfish skill level 4 and 10 seconds + 0.1 time control

...      Mark6 playing White: 13 - 11 - 1  [0.540] 25
...      Mark6 playing Black: 12 - 10 - 3  [0.540] 25
...      White vs Black: 23 - 23 - 4  [0.500] 50
Elo difference: 27.9 +/- 94.8, LOS: 72.2 %, DrawRatio: 8.0 %
SPRT: llr 0 (0.0%), lbound -inf, ubound inf

Player: Mark6
   "Draw by 3-fold repetition": 4
   "Loss: Black mates": 11
   "Loss: White mates": 10
   "Win: Black mates": 12
   "Win: White mates": 13
Player: Stockfish
   "Draw by 3-fold repetition": 4
   "Loss: Black mates": 12
   "Loss: White mates": 13
   "Win: Black mates": 11
   "Win: White mates": 10
Finished match

Mark_7 (Mark_6 but with a fix for unnecesary 3 fold repetiton draws from a winning position)


The following is the result from SPRT with mark 6

...      Mark7 playing White: 149 - 71 - 107  [0.619] 327
...      Mark7 playing Black: 83 - 90 - 153  [0.489] 326
...      White vs Black: 239 - 154 - 260  [0.565] 653
Elo difference: 37.9 +/- 20.7, LOS: 100.0 %, DrawRatio: 39.8 %
SPRT: llr 2.97 (101.0%), lbound -2.94, ubound 2.94 - H1 was accepted

Player: Mark7
   "Draw by 3-fold repetition": 241
   "Draw by insufficient mating material": 19
   "Loss: Black mates": 71
   "Loss: White mates": 90
   "No result": 3
   "Win: Black mates": 83
   "Win: White mates": 149
Player: Mark6
   "Draw by 3-fold repetition": 241
   "Draw by insufficient mating material": 19
   "Loss: Black mates": 83
   "Loss: White mates": 149
   "No result": 3
   "Win: Black mates": 71
   "Win: White mates": 90
Finished match


Mark_8 (Mark_7 but with Null Move Pruning)

The following is the result from SPRT with mark 7

...      Mark8 playing White: 73 - 32 - 27  [0.655] 132
...      Mark8 playing Black: 73 - 31 - 27  [0.660] 131
...      White vs Black: 104 - 105 - 54  [0.498] 263
Elo difference: 113.5 +/- 39.0, LOS: 100.0 %, DrawRatio: 20.5 %
SPRT: llr 2.96 (100.6%), lbound -2.94, ubound 2.94 - H1 was accepted

Player: Mark8
   "Draw by 3-fold repetition": 37
   "Draw by fifty moves rule": 3
   "Draw by insufficient mating material": 14
   "Loss: Black mates": 32
   "Loss: White mates": 31
   "No result": 3
   "Win: Black mates": 73
   "Win: White mates": 73
Player: Mark7
   "Draw by 3-fold repetition": 37
   "Draw by fifty moves rule": 3
   "Draw by insufficient mating material": 14
   "Loss: Black mates": 73
   "Loss: White mates": 73
   "No result": 3
   "Win: Black mates": 32
   "Win: White mates": 31
Finished match


Mark_9 (Mark_8 but with principle variation search)

The following is the result from SPRT with mark 8

...      Mark9 playing White: 368 - 205 - 200  [0.605] 773
...      Mark9 playing Black: 261 - 325 - 186  [0.459] 772
...      White vs Black: 693 - 466 - 386  [0.573] 1545
Elo difference: 22.3 +/- 15.0, LOS: 99.8 %, DrawRatio: 25.0 %
SPRT: llr 2.95 (100.2%), lbound -2.94, ubound 2.94 - H1 was accepted

Player: Mark9
   "Draw by 3-fold repetition": 288
   "Draw by fifty moves rule": 25
   "Draw by insufficient mating material": 72
   "Draw by stalemate": 1
   "Loss: Black mates": 205
   "Loss: White mates": 325
   "No result": 3
   "Win: Black mates": 261
   "Win: White mates": 368
Player: Mark8
   "Draw by 3-fold repetition": 288
   "Draw by fifty moves rule": 25
   "Draw by insufficient mating material": 72
   "Draw by stalemate": 1
   "Loss: Black mates": 261
   "Loss: White mates": 368
   "No result": 3
   "Win: Black mates": 205
   "Win: White mates": 325
Finished match

Mark_10 (Mark_9 but with late move reductions and opening book fix)

The following is the result from SPRT with mark 9

...      Mark10 playing White: 146 - 70 - 63  [0.636] 279
...      Mark10 playing Black: 103 - 95 - 80  [0.514] 278
...      White vs Black: 241 - 173 - 143  [0.561] 557
Elo difference: 52.8 +/- 25.1, LOS: 100.0 %, DrawRatio: 25.7 %
SPRT: llr 2.97 (100.8%), lbound -2.94, ubound 2.94 - H1 was accepted

Player: Mark10
   "Draw by 3-fold repetition": 112
   "Draw by fifty moves rule": 6
   "Draw by insufficient mating material": 24
   "Draw by stalemate": 1
   "Loss: Black mates": 70
   "Loss: White mates": 95
   "No result": 3
   "Win: Black mates": 103
   "Win: White mates": 146
Player: Mark9
   "Draw by 3-fold repetition": 112
   "Draw by fifty moves rule": 6
   "Draw by insufficient mating material": 24
   "Draw by stalemate": 1
   "Loss: Black mates": 103
   "Loss: White mates": 146
   "No result": 3
   "Win: Black mates": 70
   "Win: White mates": 95
Finished match


Mark_9NNUE (Mark_9 but with blended 50 % NNUE evaluation, 50% standard evaluation)

The following is the result from SPRT with stockfish level 4

Player: Mark11
   "Draw by 3-fold repetition": 26
   "Draw by fifty moves rule": 14
   "Draw by insufficient mating material": 11
   "Draw by stalemate": 2
   "Loss: Black mates": 100
   "Loss: White makes an illegal move: h1g3": 1
   "Loss: White mates": 144
   "No result": 3
   "Win: Black mates": 157
   "Win: White loses on time": 1
   "Win: White mates": 190
Player: Stockfish_Level_4
   "Draw by 3-fold repetition": 26
   "Draw by fifty moves rule": 14
   "Draw by insufficient mating material": 11
   "Draw by stalemate": 2
   "Loss: Black mates": 157
   "Loss: White loses on time": 1
   "Loss: White mates": 190
   "No result": 3
   "Win: Black mates": 100
   "Win: White makes an illegal move: h1g3": 1
   "Win: White mates": 144
Finished match


Mark_10NNUEblended_V1 (Mark10Tables but with 768 feature NNUE blended evaluation (50% hc 50% nnue))

The following is the result of sprt with Mark_10Tables

Score of Mark10_NNUEblended vs Mark10Tables: 551 - 449 - 214  [0.542] 1214
...      Mark10_NNUEblended playing White: 337 - 161 - 110  [0.645] 608
...      Mark10_NNUEblended playing Black: 214 - 288 - 104  [0.439] 606
...      White vs Black: 625 - 375 - 214  [0.603] 1214
Elo difference: 29.3 +/- 17.8, LOS: 99.9 %, DrawRatio: 17.6 %
SPRT: llr 2.96 (100.5%), lbound -2.94, ubound 2.94 - H1 was accepted

Player: Mark10_NNUEblended
   "Draw by 3-fold repetition": 174
   "Draw by fifty moves rule": 21
   "Draw by insufficient mating material": 19
   "Loss: Black mates": 161
   "Loss: White mates": 288
   "No result": 3
   "Win: Black mates": 214
   "Win: White mates": 337
Player: Mark10Tables
   "Draw by 3-fold repetition": 174
   "Draw by fifty moves rule": 21
   "Draw by insufficient mating material": 19
   "Loss: Black mates": 214
   "Loss: White mates": 337
   "No result": 3
   "Win: Black mates": 161
   "Win: White mates": 288
Finished match


original network versus depth 9 18 million data points network, both blended

Score of Mark10_NNUE_v2_phaseblend2 vs Mark10_NNUE_v1_phaseblend2: 493 - 410 - 284  [0.535] 1187
...      Mark10_NNUE_v2_phaseblend2 playing White: 315 - 150 - 128  [0.639] 593
...      Mark10_NNUE_v2_phaseblend2 playing Black: 178 - 260 - 156  [0.431] 594
...      White vs Black: 575 - 328 - 284  [0.604] 1187
Elo difference: 24.3 +/- 17.3, LOS: 99.7 %, DrawRatio: 23.9 %
SPRT: llr 2.5 (84.8%), lbound -2.94, ubound 2.94

Player: Mark10_NNUE_v2_phaseblend2
   "Draw by 3-fold repetition": 244
   "Draw by fifty moves rule": 14
   "Draw by insufficient mating material": 23
   "Draw by stalemate": 3
   "Loss: Black mates": 150
   "Loss: White mates": 260
   "No result": 4
   "Win: Black mates": 178
   "Win: White mates": 315
Player: Mark10_NNUE_v1_phaseblend2
   "Draw by 3-fold repetition": 244
   "Draw by fifty moves rule": 14
   "Draw by insufficient mating material": 23
   "Draw by stalemate": 3
   "Loss: Black mates": 178
   "Loss: White mates": 315
   "No result": 4
   "Win: Black mates": 150
   "Win: White mates": 260
Finished match