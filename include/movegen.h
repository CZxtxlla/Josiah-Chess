#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "position.h"

/*
  Move representation in a single 32-bit integer:
  0000 0000 0000 0000 0000 0011 1111 -> From square (Bits 0-5)
  0000 0000 0000 0000 1111 1100 0000 -> To square (Bits 6-11)
  0000 0000 0000 1111 0000 0000 0000 -> Promoted piece flags (Bits 12-15)
  0000 0000 0001 0000 0000 0000 0000 -> En Passant flag (Bit 16)
  0000 0000 0010 0000 0000 0000 0000 -> Double Pawn Push flag (Bit 17)
  0000 0000 0100 0000 0000 0000 0000 -> Castling flag (Bit 18)
*/

typedef struct {
    int moves[256]; // only needs to be size 256 since we are never gonna gave more moves than that from a single position
    int count;
} MoveList;

#define encode_move(from, to, piece, ep, double_push, castling) ((from) | ((to) << 6) | ((piece) << 12) | ((ep) << 16) | ((double_push) << 17) | ((castling) << 18))

#define get_move_from(move) ((move) & 0x3F)
#define get_move_to(move) (((move) >> 6) & 0x3F)
#define get_move_promoted(move) (((move) >> 12) & 0xF)
#define get_move_ep(move) (((move) >> 16) & 1)
#define get_move_double(move) (((move) >> 17) & 1)
#define get_move_castle(move) ((move) >> 18 & 1)


void generate_moves(const Position* pos, MoveList* list);

void print_move(int move);

#endif