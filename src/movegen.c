#include "../include/movegen.h"
#include "../include/bitboard.h"
#include <stdio.h>

// debugging helper to print encoded moves
void print_move(int move) {
    printf("%c%d%c%d ", 
           'a' + (get_move_from(move) % 8), (get_move_from(move) / 8) + 1,
           'a' + (get_move_to(move) % 8), (get_move_to(move) / 8) + 1);
}

void addMove(MoveList* list, int move) {
    list->moves[list->count] = move;
    list->count++;
}

void generate_moves(const Position* pos, MoveList* list) {
    list->count = 0;
    U64 bitboard, pushes;
    int to_sq, from_sq;

    U64 a_file = 0x0101010101010101ULL;
    U64 h_file = 0x8080808080808080ULL;

    if (pos->side == WHITE) {
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

        // double pushes
        U64 double_pushes = (pushes << 8) & 0x00000000FF000000ULL & ~pos->occupancy[BOTH];
        while (double_pushes) {
            to_sq = __builtin_ctzll(double_pushes);

            from_sq = to_sq - 16;
            addMove(list, encode_move(from_sq, to_sq, 0, 0, 1, 0));
            pop_bit(double_pushes, to_sq);
        }

        //captures 

        U64 captures_left = (bitboard << 7) & ~h_file & pos->occupancy[BLACK];
        while (captures_left) {
            to_sq = __builtin_ctzll(captures_left);

            from_sq = to_sq - 7;
            if (to_sq >= A8 && to_sq <= H8) {
                addMove(list, encode_move(from_sq, to_sq, Q, 0, 0, 0));
                addMove(list, encode_move(from_sq, to_sq, R, 0, 0, 0));
                addMove(list, encode_move(from_sq, to_sq, B, 0, 0, 0));
                addMove(list, encode_move(from_sq, to_sq, N, 0, 0, 0));
            } else {
                addMove(list, encode_move(from_sq, to_sq, 0, 0, 0, 0));
            }
            pop_bit(captures_left, to_sq);
        }

        U64 captures_right = (bitboard << 9) & ~a_file & pos->occupancy[BLACK];
        while (captures_right) {
            to_sq = __builtin_ctzll(captures_right);

            from_sq = to_sq - 9;
            if (to_sq >= A8 && to_sq <= H8) {
                addMove(list, encode_move(from_sq, to_sq, Q, 0, 0, 0));
                addMove(list, encode_move(from_sq, to_sq, R, 0, 0, 0));
                addMove(list, encode_move(from_sq, to_sq, B, 0, 0, 0));
                addMove(list, encode_move(from_sq, to_sq, N, 0, 0, 0));
            } else {
                addMove(list, encode_move(from_sq, to_sq, 0, 0, 0, 0));
            }
            pop_bit(captures_right, to_sq);
        }

        // en passant

        //check if en passant even possible
        if (pos->en_passant != -1) {
            U64 ep_square = 1ULL << pos->en_passant;

            U64 ep_left = (bitboard << 7) & ~h_file & ep_square;
            if (ep_left) {
                int to_sq = pos->en_passant;
                int from_sq = to_sq - 7;
                addMove(list, encode_move(from_sq, to_sq, 0, 1, 0, 0)); // ep flag set to 1
            }

            U64 ep_right = (bitboard << 9) & ~a_file & ep_square;
            if (ep_right) {
                int to_sq = pos->en_passant;
                int from_sq = to_sq - 9;
                addMove(list, encode_move(from_sq, to_sq, 0, 1, 0, 0)); // ep flag set to 1
            }
        }

    } else {
        // black pawns
        bitboard = pos->pieces[p];

        pushes = (bitboard >> 8) & ~pos->occupancy[BOTH]; // set move

        // single pushes
        U64 single_pushes = pushes;
        while (single_pushes) {
            to_sq = __builtin_ctzll(single_pushes);

            from_sq = to_sq + 8;

            if (to_sq >= A1 && to_sq <= H1) {
                addMove(list, encode_move(from_sq, to_sq, q, 0, 0, 0));
                addMove(list, encode_move(from_sq, to_sq, r, 0, 0, 0));
                addMove(list, encode_move(from_sq, to_sq, b, 0, 0, 0));
                addMove(list, encode_move(from_sq, to_sq, n, 0, 0, 0));
            } else {
                addMove(list, encode_move(from_sq, to_sq, 0, 0, 0, 0));
            }
            pop_bit(single_pushes, to_sq);
        }

        // double pushes on start
        U64 double_pushes = (pushes >> 8) & 0x000000FF00000000ULL & ~pos->occupancy[BOTH];
        while (double_pushes) {
            to_sq = __builtin_ctzll(double_pushes);

            from_sq = to_sq + 16;
            addMove(list, encode_move(from_sq, to_sq, 0, 0, 1, 0));
            pop_bit(double_pushes, to_sq);
        }

        // captures
        U64 captures_left = (bitboard >> 9) & ~h_file & pos->occupancy[WHITE];
        while (captures_left) {
            to_sq = __builtin_ctzll(captures_left);

            from_sq = to_sq + 9;
            if (to_sq >= A1 && to_sq <= H1) {
                addMove(list, encode_move(from_sq, to_sq, q, 0, 0, 0));
                addMove(list, encode_move(from_sq, to_sq, r, 0, 0, 0));
                addMove(list, encode_move(from_sq, to_sq, b, 0, 0, 0));
                addMove(list, encode_move(from_sq, to_sq, n, 0, 0, 0));
            } else {
                addMove(list, encode_move(from_sq, to_sq, 0, 0, 0, 0));
            }
            pop_bit(captures_left, to_sq);
        }

        U64 captures_right = (bitboard >> 7) & ~a_file & pos->occupancy[WHITE];
        while (captures_right) {
            to_sq = __builtin_ctzll(captures_right);

            from_sq = to_sq + 7;
            if (to_sq >= A1 && to_sq <= H1) {
                addMove(list, encode_move(from_sq, to_sq, q, 0, 0, 0));
                addMove(list, encode_move(from_sq, to_sq, r, 0, 0, 0));
                addMove(list, encode_move(from_sq, to_sq, b, 0, 0, 0));
                addMove(list, encode_move(from_sq, to_sq, n, 0, 0, 0));
            } else {
                addMove(list, encode_move(from_sq, to_sq, 0, 0, 0, 0));
            }
            pop_bit(captures_right, to_sq);
        }

        // en passant

        //check if en passant even possible
        if (pos->en_passant != -1) {
            U64 ep_square = 1ULL << pos->en_passant;

            U64 ep_left = (bitboard >> 9) & ~h_file & ep_square;
            if (ep_left) {
                int to_sq = pos->en_passant;
                int from_sq = to_sq + 9;
                addMove(list, encode_move(from_sq, to_sq, 0, 1, 0, 0)); // ep flag set to 1
            }

            U64 ep_right = (bitboard >> 7) & ~a_file & ep_square;
            if (ep_right) {
                int to_sq = pos->en_passant;
                int from_sq = to_sq + 7;
                addMove(list, encode_move(from_sq, to_sq, 0, 1, 0, 0)); // ep flag set to 1
            }
        }
    }
}