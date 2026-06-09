#include "../include/search.h"
#include "../include/evaluate.h"
#include "../include/movegen.h"
#include "../include/position.h"
#include "../include/zobrist.h"
#include <stdio.h>
#include <sys/time.h>
#include <string.h>

// used for 3-fold repetition
U64 game_history[2048]; // Stores the hash of every position played
int game_ply = 0;       // How many moves deep into the game we are

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

// helper to check if repetition
int is_repetition(Position* pos) {
    for (int i = 0; i < game_ply - 1; i++) {
        if (game_history[i] == pos->hash_key) {
            return 1;
        }
    }
    return 0;
}

int score_move(Position* pos, int move, int distance, int hash_move) {
    if (move == hash_move) {
        return 40000; // hashe table moves are really good
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

void order_moves(Position* pos, MoveList* list, int distance, int hash_move) {
    int scores[256];

    // initialize scores array
    for (int i = 0; i < list->count; i++) {
        scores[i] = score_move(pos, list->moves[i], distance, hash_move);
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

    order_moves(pos, &list, 0, 0); // 0 for the distance as a dummy since the score will be overidden by the fact it's a capture

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

    if (distance > 0 && is_repetition(pos)) {
        return 0; // it's a draw, 3 fold repetition
    }

    // original alpha before changed by search
    int old_alpha = alpha; 
    
    // track best move for this specific node
    int local_best_move = 0; 

    int hash_move = 0; 
    int tt_score = 0;  

    if (read_hash(pos->hash_key, depth, alpha, beta, &tt_score, &hash_move)) {
        if (distance == 0 && hash_move != 0) {
            best_move = hash_move;
        }
        return tt_score;  // perfect, cut the search
    }

    MoveList list;
    generate_moves(pos, &list); // get all moves

    order_moves(pos, &list, distance, hash_move);

    int legal_moves = 0;

    // go through the tree, check every move
    for (int i = 0; i < list.count; i++) {
        Position next_state = *pos;

        if (make_move(&next_state, list.moves[i])) {
            legal_moves++;

            if (distance == 0 && best_move == 0) {
                best_move = list.moves[i]; // fix the not finding a move in time
            }
            game_history[game_ply] = next_state.hash_key;
            game_ply++;

            int score = -negamax(&next_state, depth - 1, distance + 1, -beta, -alpha);

            game_ply--;

            if (time_over) return 0;

            if (score > alpha) {
                alpha = score;

                local_best_move = list.moves[i]; // track locally for TT

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

    int hash_flag = HASH_EXACT;
    if (alpha <= old_alpha) {
        hash_flag = HASH_ALPHA; // We failed low; this score is an upper bound
    } else if (alpha >= beta) {
        hash_flag = HASH_BETA;  // We failed high; this score is a lower bound (cutoff)
    }

    // save this position so we never have to search it at this depth again
    // Use the local best move for this node (may be different from the global root best_move)
    write_hash(pos->hash_key, depth, alpha, hash_flag, local_best_move);

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