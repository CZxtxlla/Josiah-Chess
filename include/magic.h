#ifndef MAGIC_H
#define MAGIC_H

#include "types.h"

// global lookup tables for attacks
extern U64 rook_attacks[64][4096];
extern U64 bishop_attacks[64][512];

//global lookup tables for occupancy masks
extern U64 rook_masks[64];
extern U64 bishop_masks[64];

//magic numbers
extern U64 rook_magics[64];
extern U64 bishop_magics[64];

void init_sliders(); // initialize lookup tables for sliders (rooks and bishops)
U64 get_rook_attacks(int square, U64 occupancy);
U64 get_bishop_attacks(int square, U64 occupancy);
U64 get_queen_attacks(int square, U64 occupancy);

#endif