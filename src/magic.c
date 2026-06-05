#include "../include/magic.h"
#include "../include/bitboard.h"
#include <stdio.h>

// magic numbers
U64 rook_magics[64];
U64 bishop_magics[64];

// lookup tables
U64 rook_attacks[64][4096];
U64 bishop_attacks[64][512];
U64 rook_masks[64];
U64 bishop_masks[64];

// ------ magic numbers computing --------

// random 64 bit number generating:

// A deterministic PRNG state so the engine boots the exact same way every time
unsigned int random_state = 1804289383;

// 32-bit XorShift
unsigned int get_random_U32() {
    unsigned int number = random_state;
    number ^= number << 13;
    number ^= number >> 17;
    number ^= number << 5;
    random_state = number;
    return number;
}

// stitch 4 16 bit chunks into a 64 bit integer
U64 get_random_U64() {
    U64 n1 = (U64)(get_random_U32()) & 0xFFFF;
    U64 n2 = (U64)(get_random_U32()) & 0xFFFF;
    U64 n3 = (U64)(get_random_U32()) & 0xFFFF;
    U64 n4 = (U64)(get_random_U32()) & 0xFFFF;
    return n1 | (n2 << 16) | (n3 << 32) | (n4 << 48);
}

U64 get_magic_candidate() {
    // bitwise and 3 times to reduce number of 1s
    return get_random_U64() & get_random_U64() & get_random_U64(); 
}


U64 find_magic_number(int square, int relevant_bits, int is_bishop) {
    U64 occupancies[4096];
    U64 attacks[4096];
    U64 used_attacks[4096];

    U64 attack_mask = is_bishop ? mask_bishop_attacks(square) : mask_rook_attacks(square);
    int occupancy_indices = 1 << relevant_bits;

    for (int index = 0; index < occupancy_indices; index++) {
        occupancies[index] = set_occupancy(index, relevant_bits, attack_mask);
        attacks[index] = is_bishop ? bishop_attacks_cast(square, occupancies[index]) : rook_attacks_cast(square, occupancies[index]);
    }

    for (int k = 0; k < 100000000; k++) {
        U64 candidate = get_magic_candidate();
        if (__builtin_popcountll((attack_mask * candidate) & 0xFF00000000000000ULL) < 6) {
            continue;
        }

        // clear used_attacks array
        for (int i = 0; i < 4096; i++) {
            used_attacks[i] = 0ULL;
        }

        int fail = 0;

        // Test the magic number against every permutation
        for (int index = 0; index < occupancy_indices; index++) {
            // The magic hashing formula!
            int magic_index = (occupancies[index] * candidate) >> (64 - relevant_bits);

            // If the slot is completely empty, claim it
            if (used_attacks[magic_index] == 0ULL) {
                used_attacks[magic_index] = attacks[index];
            } 
            // If the slot is taken, check if it's a destructive collision
            else if (used_attacks[magic_index] != attacks[index]) {
                fail = 1;
                break;
            }
        }

        // If it didn't fail, we found a perfect magic number!
        if (!fail) {
            return candidate;
        }

    }

    printf("Failed to find Magic Number for Square %d\n", square);
    return 0ULL;
}


// ------ lookup tables initializer for sliders ------
// runs once when the engine boots up to build the tables
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


// ------ super fast O(1) table lookups ------

U64 get_rook_attacks(int square, U64 occupancy) {

    U64 relevant_occupancy = occupancy & rook_masks[square];

    int r_bits = __builtin_popcountll(rook_masks[square]);
    int magic_index = (relevant_occupancy * rook_magics[square]) >> (64 - r_bits);

    return rook_attacks[square][magic_index];

    //return rook_attacks_cast(square, occupancy);
}

U64 get_bishop_attacks(int square, U64 occupancy) {
    
    U64 relevant_occupancy = occupancy & bishop_masks[square];

    int b_bits = __builtin_popcountll(bishop_masks[square]);
    int magic_index = (relevant_occupancy * bishop_magics[square]) >> (64 - b_bits);
    
    return bishop_attacks[square][magic_index];
    
    //return bishop_attacks_cast(square, occupancy);
}

U64 get_queen_attacks(int square, U64 occupancy) {
    return get_rook_attacks(square, occupancy) | get_bishop_attacks(square, occupancy);
}