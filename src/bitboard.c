#include "../include/bitboard.h"
#include <stdio.h>

U64 knight_attacks[64];
U64 king_attacks[64];

void init_leapers() {
    // initialize king and knight move arrays
    for (int square = 0; square < 64; square++) {
        U64 bitboard = 0ULL;
        set_bit(bitboard, square);
    }
}

void print_bitboard(U64 bitboard) {
    for (int rank = 7; rank >= 0; rank--) {
        printf(" %d  ", rank + 1); // print rank number

        for (int file = 0; file < 8; file++) {
            int square = rank * 8 + file;
            printf(" %llu ", get_bit(bitboard, square));
        }
        printf("\n");
    }

    // Print file labels
    printf("\n     a  b  c  d  e  f  g  h\n\n");
}