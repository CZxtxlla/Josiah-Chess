#include "../include/movegen.h"
#include "../include/bitboard.h"
#include "../include/magic.h"
#include "../include/position.h"
#include <stdio.h>

// debugging helper to print encoded moves
void print_move(int move) {
    int from = get_move_from(move);
    int to = get_move_to(move);
    int promoted = get_move_promoted(move);

    printf("%c%d%c%d", 'a' + (from % 8), (from / 8) + 1, 'a' + (to % 8), (to / 8) + 1);

    // If it's a promotion, append the correct letter
    if (promoted) {
        if (promoted == Q || promoted == q) printf("q");
        else if (promoted == R || promoted == r) printf("r");
        else if (promoted == B || promoted == b) printf("b");
        else if (promoted == N || promoted == n) printf("n");
    }
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

    U64 friendly_occupancy = (pos->side == WHITE) ? pos->occupancy[WHITE] : pos->occupancy[BLACK];

    // pawns
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

    // knights

    U64 knights = (pos->side == WHITE) ? pos->pieces[N] : pos->pieces[n];
    while(knights) {
        int from_sq = __builtin_ctzll(knights);

        U64 attacks = knight_attacks[from_sq] & ~friendly_occupancy;

        while (attacks) {
            int to_sq = __builtin_ctzll(attacks);
            addMove(list, encode_move(from_sq, to_sq, 0, 0, 0, 0));
            pop_bit(attacks, to_sq);
        }
        pop_bit(knights, from_sq);
    }

    // kings

    U64 kings = (pos->side == WHITE) ? pos->pieces[K] : pos->pieces[k];
    while(kings) {
        int from_sq = __builtin_ctzll(kings);

        U64 attacks = king_attacks[from_sq] & ~friendly_occupancy;

        while (attacks) {
            int to_sq = __builtin_ctzll(attacks);
            addMove(list, encode_move(from_sq, to_sq, 0, 0, 0, 0));
            pop_bit(attacks, to_sq);
        }
        pop_bit(kings, from_sq);
    }

    // castling

    if (pos->side == WHITE) {
        if (pos->castling_rights & WK) {
            // check if F1, G1 empty
            if (!(pos->occupancy[BOTH] & 0x60ULL)) {
                // check if F1, E1 are not attacked
                if (!is_square_attacked(E1, BLACK, pos) && !is_square_attacked(F1, BLACK, pos)) {
                    addMove(list, encode_move(E1, G1, 0, 0, 0, 1));
                }
            }
        }
        if (pos->castling_rights & WQ) {
            // check if B1, C1, D1 empty
            if (!(pos->occupancy[BOTH] & 0x0EULL)) {
                // check if E1, D1 are not attacked
                if (!is_square_attacked(E1, BLACK, pos) && !is_square_attacked(D1, BLACK, pos)) {
                    addMove(list, encode_move(E1, C1, 0, 0, 0, 1));
                }
            }
        }
    } else {
        if (pos->castling_rights & BK) {
            // check if F8 and G8 empty
            if (!(pos->occupancy[BOTH] & 0x6000000000000000ULL)) {
                if (!is_square_attacked(E8, WHITE, pos) && !is_square_attacked(F8, WHITE, pos)) {
                    addMove(list, encode_move(E8, G8, 0, 0, 0, 1));
                }
            }
        }
        if (pos->castling_rights & BQ) {
            // check if B8, C8, and D8 empty
            if (!(pos->occupancy[BOTH] & 0x0E00000000000000ULL)) {
                if (!is_square_attacked(E8, WHITE, pos) && !is_square_attacked(D8, WHITE, pos)) {
                    addMove(list, encode_move(E8, C8, 0, 0, 0, 1));
                }
            }
        }

    }

    // bishops

    U64 bishops = (pos->side == WHITE) ? pos->pieces[B] : pos->pieces[b];
    while(bishops) {
        int from_sq = __builtin_ctzll(bishops);

        U64 attacks = get_bishop_attacks(from_sq, pos->occupancy[BOTH]) & ~friendly_occupancy;

        while (attacks) {
            int to_sq = __builtin_ctzll(attacks);
            addMove(list, encode_move(from_sq, to_sq, 0, 0, 0, 0));
            pop_bit(attacks, to_sq);
        }
        pop_bit(bishops, from_sq);
    }

    // rooks

    U64 rooks = (pos->side == WHITE) ? pos->pieces[R] : pos->pieces[r];
    while(rooks) {
        int from_sq = __builtin_ctzll(rooks);

        U64 attacks = get_rook_attacks(from_sq, pos->occupancy[BOTH]) & ~friendly_occupancy;

        while (attacks) {
            int to_sq = __builtin_ctzll(attacks);
            addMove(list, encode_move(from_sq, to_sq, 0, 0, 0, 0));
            pop_bit(attacks, to_sq);
        }
        pop_bit(rooks, from_sq);
    }

    // queens

    U64 queens = (pos->side == WHITE) ? pos->pieces[Q] : pos->pieces[q];
    while(queens) {
        int from_sq = __builtin_ctzll(queens);

        U64 attacks = get_queen_attacks(from_sq, pos->occupancy[BOTH]) & ~friendly_occupancy;

        while (attacks) {
            int to_sq = __builtin_ctzll(attacks);
            addMove(list, encode_move(from_sq, to_sq, 0, 0, 0, 0));
            pop_bit(attacks, to_sq);
        }
        pop_bit(queens, from_sq);
    }
}

void generate_captures(const Position* pos, MoveList* list) {
    list->count = 0;
    U64 bitboard, attacks;
    int to_sq, from_sq;

    U64 a_file = 0x0101010101010101ULL;
    U64 h_file = 0x8080808080808080ULL;

    U64 enemy_occupancy = (pos->side == WHITE) ? pos->occupancy[BLACK] : pos->occupancy[WHITE];

    // pawns
    if (pos->side == WHITE) {
        bitboard = pos->pieces[P];

        // left right captures
        U64 captures_left = (bitboard << 7) & ~h_file & enemy_occupancy;
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

        U64 captures_right = (bitboard << 9) & ~a_file & enemy_occupancy;
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

        // promotions
        U64 promo_pushes = (bitboard << 8) & ~pos->occupancy[BOTH] & 0xFF00000000000000ULL;
        while (promo_pushes) {
            to_sq = __builtin_ctzll(promo_pushes);

            from_sq = to_sq - 8;
            addMove(list, encode_move(from_sq, to_sq, Q, 0, 0, 0));
            addMove(list, encode_move(from_sq, to_sq, R, 0, 0, 0));
            addMove(list, encode_move(from_sq, to_sq, B, 0, 0, 0));
            addMove(list, encode_move(from_sq, to_sq, N, 0, 0, 0));
            pop_bit(promo_pushes, to_sq);
        }

    } else {
        bitboard = pos->pieces[p];

        // captures
        U64 captures_left = (bitboard >> 9) & ~h_file & enemy_occupancy;
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

        U64 captures_right = (bitboard >> 7) & ~a_file & enemy_occupancy;
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

        // promotions
        U64 promo_pushes = (bitboard >> 8) & ~pos->occupancy[BOTH] & 0x00000000000000FFULL;
        while (promo_pushes) {
            to_sq = __builtin_ctzll(promo_pushes);

            from_sq = to_sq + 8;
            addMove(list, encode_move(from_sq, to_sq, q, 0, 0, 0));
            addMove(list, encode_move(from_sq, to_sq, r, 0, 0, 0));
            addMove(list, encode_move(from_sq, to_sq, b, 0, 0, 0));
            addMove(list, encode_move(from_sq, to_sq, n, 0, 0, 0));
            pop_bit(promo_pushes, to_sq);
        }
    }

    // other pieces

    // knights

    U64 knights = (pos->side == WHITE) ? pos->pieces[N] : pos->pieces[n];
    while(knights) {
        int from_sq = __builtin_ctzll(knights);

        U64 attacks = knight_attacks[from_sq] & enemy_occupancy;

        while (attacks) {
            int to_sq = __builtin_ctzll(attacks);
            addMove(list, encode_move(from_sq, to_sq, 0, 0, 0, 0));
            pop_bit(attacks, to_sq);
        }
        pop_bit(knights, from_sq);
    }

    // kings

    U64 kings = (pos->side == WHITE) ? pos->pieces[K] : pos->pieces[k];
    while(kings) {
        int from_sq = __builtin_ctzll(kings);

        U64 attacks = king_attacks[from_sq] & enemy_occupancy;

        while (attacks) {
            int to_sq = __builtin_ctzll(attacks);
            addMove(list, encode_move(from_sq, to_sq, 0, 0, 0, 0));
            pop_bit(attacks, to_sq);
        }
        pop_bit(kings, from_sq);
    }

    // bishops

    U64 bishops = (pos->side == WHITE) ? pos->pieces[B] : pos->pieces[b];
    while(bishops) {
        int from_sq = __builtin_ctzll(bishops);

        U64 attacks = get_bishop_attacks(from_sq, pos->occupancy[BOTH]) & enemy_occupancy;

        while (attacks) {
            int to_sq = __builtin_ctzll(attacks);
            addMove(list, encode_move(from_sq, to_sq, 0, 0, 0, 0));
            pop_bit(attacks, to_sq);
        }
        pop_bit(bishops, from_sq);
    }

    // rooks

    U64 rooks = (pos->side == WHITE) ? pos->pieces[R] : pos->pieces[r];
    while(rooks) {
        int from_sq = __builtin_ctzll(rooks);

        U64 attacks = get_rook_attacks(from_sq, pos->occupancy[BOTH]) & enemy_occupancy;

        while (attacks) {
            int to_sq = __builtin_ctzll(attacks);
            addMove(list, encode_move(from_sq, to_sq, 0, 0, 0, 0));
            pop_bit(attacks, to_sq);
        }
        pop_bit(rooks, from_sq);
    }

    // queens

    U64 queens = (pos->side == WHITE) ? pos->pieces[Q] : pos->pieces[q];
    while(queens) {
        int from_sq = __builtin_ctzll(queens);

        U64 attacks = get_queen_attacks(from_sq, pos->occupancy[BOTH]) & enemy_occupancy;

        while (attacks) {
            int to_sq = __builtin_ctzll(attacks);
            addMove(list, encode_move(from_sq, to_sq, 0, 0, 0, 0));
            pop_bit(attacks, to_sq);
        }
        pop_bit(queens, from_sq);
    }

}