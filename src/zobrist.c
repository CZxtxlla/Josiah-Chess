#include "../include/zobrist.h"
#include <stdlib.h>
#include <stdio.h>

// ------ zobrust hash numbers stuff -------

U64 random_state_xor = 1804289383ULL; // random just non-zero

// zobrist tables
U64 piece_keys[12][64]; // [piece_type][square]
U64 side_key;
U64 castling_keys[16]; // 16 possible castling rights combinations
U64 en_passant_keys[8]; // one for each file


U64 get_random_U64_xorshift() {
    random_state_xor ^= random_state_xor << 13;
    random_state_xor ^= random_state_xor >> 7;
    random_state_xor ^= random_state_xor << 17;

    return random_state_xor;
}

// fill the zobrist tables
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


U64 generate_hash(Position* pos) {
    U64 final_hash = 0;

    // hash pieces (include all piece types up to `k`)
    for (int piece = P; piece <= k; piece++) {
        U64 bitboard = pos->pieces[piece];
        while (bitboard) {
            int sq = __builtin_ctzll(bitboard);
            final_hash ^= piece_keys[piece][sq];
            bitboard &= bitboard - 1; // clear lowest bit
        }
    }

    //hash en passant
    if (pos->en_passant != -1) {
        final_hash ^= en_passant_keys[pos->en_passant % 8];
    }

    // hash castling rights
    final_hash ^= castling_keys[pos->castling_rights];

    // hash side to move
    if (pos->side == BLACK) {
        final_hash ^= side_key;
    }

    return final_hash;
}

// ------- TT stuff -------

TTentry* hash_table = NULL;
int entries = 0;

void init_tt(int megabytes) {
    long long hash_size_bytes = megabytes * 1024 * 1024; // bytes

    entries = hash_size_bytes / sizeof(TTentry);

    if (hash_table != NULL) {
        free(hash_table);
    }

    hash_table = (TTentry*)malloc(entries * sizeof(TTentry));
    if (hash_table == NULL) {
        fprintf(stderr, "Error: could not allocate memory for transposition table.\n");
        exit(1);
    } else {
        printf("Transposition table initialized with %d MB and %d entries\n", megabytes, entries);
        clear_tt();
    }
}

void clear_tt() {
    for (int i = 0; i < entries; i++) {
        hash_table[i].key = 0;
        hash_table[i].score = 0;
        hash_table[i].depth = 0;
        hash_table[i].flag = 0;
        hash_table[i].move = 0;
    }
}


// add an element to the transposition table
void write_hash(U64 key, int depth, int score, int flag, int best_move) {
    int index = key % entries;

    // always replace
    hash_table[index].key = key;
    hash_table[index].score = score;
    hash_table[index].depth = depth;
    hash_table[index].flag = flag;
    // Only store a non-zero move to avoid recording 'null' a1a1 moves
    if (best_move != 0) {
        hash_table[index].move = best_move;
    }
}

// read an element fronm the table at a specific hash
int read_hash(U64 key, int depth, int alpha, int beta, int* return_score, int* return_move) {
    int index = key % entries;

    if (hash_table[index].key == key) {
        *return_move = hash_table[index].move;

        // only use if it contains a depth larger or equal to what we need
        if (hash_table[index].depth >= depth) {
            int score = hash_table[index].score;
            int flag = hash_table[index].flag;
            
            // 4. Does the memory fit inside our Alpha-Beta window?
            if (flag == HASH_EXACT) {
                *return_score = score;
                return 1;
            }
            if (flag == HASH_ALPHA && score <= alpha) { // Fail-low bound is worse than alpha
                *return_score = alpha;
                return 1;
            }
            if (flag == HASH_BETA && score >= beta) { // Fail-high bound caused a cutoff
                *return_score = beta;
                return 1;
            }
        }
    }
    return 0;
}