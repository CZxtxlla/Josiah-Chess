#ifndef ZOBRIST_H
#define ZOBRIST_H

#include "types.h"
#include "position.h"

extern U64 piece_keys[12][64]; // [piece_type][square]
extern U64 side_key;
extern U64 castling_keys[16]; // 16 possible castling rights combinations
extern U64 en_passant_keys[8]; // one for each file


// return pseudorandom 64 bit integer using George Marsaglia's Xorshift64 algorithm
U64 get_random_U64_xorshift();

// initialize 12 * 64 zobrist hashing numbers
void init_zobrist();

// generate the zobrist hash of a board position
U64 generate_hash(Position* pos);


#define HASH_EXACT 0 // found exact score
#define HASH_ALPHA 1 // failed low, score is an upper bound
#define HASH_BETA 2 // failed high, score is a lower bound 

typedef struct {
    U64 key; // the zobrist hash
    int score; // the evaluation score
    int move; // the best move found 
    uint8_t depth; // how deep the search was
    uint8_t flag; // exact, alpha, or beta
} TTentry;
// 24 bytes per entry

void init_tt(int megabytes); // initialize transposition table of a given size

void clear_tt(); // clear all transposition table values between games

// add an element to the transposition table
void write_hash(U64 key, int depth, int score, int flag, int best_move);

// read an element fronm the table at a specific hash
int read_hash(U64 key, int depth, int alpha, int beta, int* return_score, int* return_move);


#endif