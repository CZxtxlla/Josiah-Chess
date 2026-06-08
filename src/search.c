#include "../include/search.h"
#include "../include/evaluate.h"
#include "../include/movegen.h"
#include "../include/position.h"
#include <stdio.h>
#include <sys/time.h>
#include <string.h>

int killer_moves[2][64]; // stores 2 killer moves for up to 64 depth plys
int history_moves[2][64][64]; // [color][from_sq][to_sq]

// used for iterative deepening
int search_time_limit = 2000; // Stop searching after 2000ms (2 seconds)
long long search_start_time = 0;
int time_over = 0;

// used for negamax
int best_move = 0;
long long nodes_evaluated = 0;

// helper to get current time in ms for iterative deepening
long long get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000LL) + (tv.tv_usec / 1000LL);
}

// helper to get piecetype at a position
int get_piece_at(Position* pos, int square) {
    for (int piece_type = P; piece_type <= k; piece_type++) {
        if (pos->pieces[piece_type] & (1ULL << square)) {
            return piece_type;
        }
    }
    return -1; // empty square
}

int score_move(Position* pos, int move, int distance) {
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

void order_moves(Position* pos, MoveList* list, int distance) {
    int scores[256];

    // initialize scores array
    for (int i = 0; i < list->count; i++) {
        scores[i] = score_move(pos, list->moves[i], distance);
    }

    for (int i = 0; i < list->count - 1; i++) {
        int max_idx = i;
        for (int j = i + 1; j < list->count; j++) {
            if (scores[j] > scores[max_idx]) {
                max_idx = j;
            }
        }
        int temp = scores[max_idx];
        scores[max_idx] = scores[i];
        scores[i] = temp;

        int temp_move = list->moves[max_idx];
        list->moves[max_idx] = list->moves[i];
        list->moves[i] = temp_move;
    }
}

int quiescence(Position* pos, int alpha, int beta) {
    nodes_evaluated++;

    // Every 2048 nodes, check if we are out of time
    if ((nodes_evaluated % 2048) == 0) {
        if (get_time_ms() - search_start_time >= search_time_limit) {
            time_over = 1;
        }
    }

    // If time is up, return immediately
    if (time_over) return 0;

    int current_score = evaluate(pos);

    if (current_score >= beta) {
        return beta;
    }

    if (current_score > alpha) {
        alpha = current_score;
    }

    MoveList list;
    generate_moves(pos, &list);

    order_moves(pos, &list, 0); // 0 for the distance as a dummy since the score will be overidden by the fact it's a capture

    for (int i = 0; i < list.count; i++) {
        int move = list.moves[i];
        int to_sq = get_move_to(move);

        int is_capture = 0;
        if (pos->occupancy[pos->side ^ 1] & (1ULL << to_sq)) {
            is_capture = 1;
        } else if (get_move_ep(move)) {
            is_capture = 1;
        }
        // only continue search if it is a capture
        if (is_capture) {
            Position next_state = *pos;

            if (make_move(&next_state, move)) {
                int score = -quiescence(&next_state, -beta, -alpha);

                if (time_over) return 0;

                if (score >= beta) {
                    return beta;
                }
                if (score > alpha) {
                    alpha = score;
                }
            }
        }
    }
    return alpha;
}

int negamax(Position* pos, int depth, int distance, int alpha, int beta) {
    // Every 2048 nodes, check if we are out of time
    if ((nodes_evaluated % 2048) == 0) {
        if (get_time_ms() - search_start_time >= search_time_limit) {
            time_over = 1;
        }
    }

    // If time is up, return immediately
    if (time_over) {
        return 0;
    }

    // base case
    if (depth == 0) {
        return quiescence(pos, alpha, beta);
    }

    MoveList list;
    generate_moves(pos, &list); // get all moves

    order_moves(pos, &list, distance);

    int legal_moves = 0;

    // go through the tree, check every move
    for (int i = 0; i < list.count; i++) {
        Position next_state = *pos;

        if (make_move(&next_state, list.moves[i])) {
            legal_moves++;

            int score = -negamax(&next_state, depth - 1, distance + 1, -beta, -alpha);

            if (time_over) return 0;

            if (score > alpha) {
                alpha = score;

                if (distance == 0) {
                    best_move = list.moves[i];
                }
            }

            // prune the branch
            if (alpha >= beta) {
                // only save as killer move if it is a quiet move (captures already handled)
                int to_sq = get_move_to(list.moves[i]);
                int is_capture = pos->occupancy[pos->side ^ 1] & (1ULL << to_sq) || get_move_ep(list.moves[i]);
                if (!is_capture) {
                    // shift down the killer moves, most recent in index 0
                    killer_moves[1][distance] = killer_moves[0][distance];
                    killer_moves[0][distance] = list.moves[i];

                    // reward this move globally based on how deep we searched
                    int from_sq = get_move_from(list.moves[i]);
                    history_moves[pos->side][from_sq][to_sq] += (depth * depth);

                    // never let History overflow into Killer territory
                    if (history_moves[pos->side][from_sq][to_sq] > 10000) {
                        history_moves[pos->side][from_sq][to_sq] = 10000;
                    }
                }
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

void search_position(Position* pos, int depth) {
    search_start_time = get_time_ms();
    time_over = 0;

    int best_move_so_far = 0;
    best_move = 0;
    nodes_evaluated = 0;

    memset(killer_moves, 0, sizeof(killer_moves));
    memset(history_moves, 0, sizeof(history_moves));

    // iterative deepening
    for (int current_depth = 1; current_depth <= depth; current_depth++) {
        int final_score = negamax(pos, current_depth, 0, -50000, 50000);

        if (time_over) {
            break;
        }

        best_move_so_far = best_move;

        long long duration = get_time_ms() - search_start_time;

        printf("info depth %d score cp %d nodes %lld\n", current_depth, final_score, nodes_evaluated);
    }

    // Output the results in the official UCI format
    printf("bestmove ");
    print_move(best_move_so_far);
    printf("\n");
}