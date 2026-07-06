#include "../include/search.h"
#include "../include/evaluate.h"
#include "../include/movegen.h"
#include "../include/position.h"
#include "../include/zobrist.h"
#include "../syzygy/tbprobe.h"
#include <stdio.h>
#include <sys/time.h>
#include <string.h>

// used for 3-fold repetition
U64 game_history[2048]; // Stores the hash of every position played
int game_ply = 0;       // How many moves deep into the game we are

int killer_moves[2][64]; // stores 2 killer moves for up to 64 depth plys
int history_moves[2][64][64]; // [color][from_sq][to_sq]

int pv_length[64]; // Stores the length of the PV for each ply
int pv_table[64][64]; // stores the actual PV moves: [ply][move index]

// used for iterative deepening
int search_time_limit = 2000; // Stop searching after 2000ms (2 seconds)
long long search_start_time = 0;
int time_over = 0;

// used for negamax
int best_move = 0;
long long nodes_evaluated = 0;

int syzygy_enabled = 0; // used for enabling endgame tablebases

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

// helper to check if there's a pawn to prevent zugzwang
int has_non_pawn_material(Position* pos) {
    if (pos->side == WHITE) {
        return pos->pieces[N] || pos->pieces[B] || pos->pieces[R] || pos->pieces[Q];
    } else {
        return pos->pieces[n] || pos->pieces[b] || pos->pieces[r] || pos->pieces[q];
    }
}

// helper to check if move is legal
int is_move_valid(Position* pos, int move) {
    if (move == 0) {
        return 1; // safe to evaluate
    }
    MoveList list;
    generate_moves(pos, &list);

    for (int i = 0; i < list.count; i++) {
        if (list.moves[i] == move) {
            Position next_state = *pos;
            if (make_move(&next_state, move)) {
                return 1; // legal move
            }
            return 0; // psuedo legal, but not legal
        }
    }

    return 0; // illegal move (hash collision)
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
    if (distance < 64) {
        if (move == killer_moves[0][distance]) return 19000;
        if (move == killer_moves[1][distance]) return 18000;
    }

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

    // initialize pv length table
    if (distance < 64) {
        pv_length[distance] = distance;
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
        // check if there's a hash collision
        if (!is_move_valid(pos, hash_move)) {
            hash_move = 0;
        } else {
            if (distance == 0 && hash_move != 0) {
                best_move = hash_move;
            }
            return tt_score;  // perfect, cut the search
        }
    }

    // Syzygy tablebase probing (WDL)

    int piece_count = __builtin_popcountll(pos->occupancy[WHITE] | pos->occupancy[BLACK]);

    if (syzygy_enabled && piece_count <= TB_LARGEST && distance > 0 && pos->castling_rights == 0) {
        
        unsigned wdl = tb_probe_wdl(
            pos->occupancy[WHITE], 
            pos->occupancy[BLACK],
            pos->pieces[K] | pos->pieces[k], // Combine White and Black Kings
            pos->pieces[Q] | pos->pieces[q], // Queens
            pos->pieces[R] | pos->pieces[r], // Rooks
            pos->pieces[B] | pos->pieces[b], // Bishops
            pos->pieces[N] | pos->pieces[n], // Knights
            pos->pieces[P] | pos->pieces[p], // Pawns
            0, // 50-move rule counter
            pos->castling_rights,   // Castling rights (should be 0)
            pos->en_passant == -1 ? 0 : pos->en_passant, // Fathom expects 0 if no EP
            pos->side == WHITE
        );

        if (wdl != TB_RESULT_FAILED) {
            int tb_score = 0;
            
            // Translate Fathom's win/loss/draw into C-hess scores
            if (wdl == TB_WIN) {
                tb_score = 49000 - distance;  // Exact mate score formula from your engine
            } else if (wdl == TB_LOSS) {
                tb_score = -49000 + distance;
            } else if (wdl == TB_DRAW || wdl == TB_CURSED_WIN || wdl == TB_BLESSED_LOSS) {
                tb_score = 0; // Draw
            }

            // Save this tablebase evaluation to our TT so we don't have to probe it again
            write_hash(pos->hash_key, depth, tb_score, HASH_EXACT, 0);
            return tb_score;
        }
    }

    // NMP implementation

    int R = 2; // reduction for NMP

    int king_type = (pos->side == WHITE) ? K : k;
    int king_sq = __builtin_ctzll(pos->pieces[king_type]);
    int in_check = is_square_attacked(king_sq, pos->side^1, pos);

    if (depth >= R + 1 && distance > 0 && !in_check && has_non_pawn_material(pos)) {

        // backup the en passant
        int ep_backup = pos->en_passant; 

        if (ep_backup != -1) {
            pos->hash_key ^= en_passant_keys[ep_backup % 8]; // remove en passant in the hash
        }
        pos->hash_key ^= side_key; // flip turn in the hash
        
        pos->en_passant = -1; 
        pos->side ^= 1;

        // (-beta, -beta + 1) window
        int null_score = -negamax(pos, depth - 1 - R, distance + 1, -beta, -beta + 1);

        pos->side ^= 1;
        pos->en_passant = ep_backup;

        // hash back in the restored state
        pos->hash_key ^= side_key;
        if (ep_backup != -1) {
            pos->hash_key ^= en_passant_keys[ep_backup % 8];
        }

        if (time_over) return 0;

        // cutoff, move so good opponent couldn't beat it with a free move
        if (null_score >= beta) {
            return beta; 
        }
    }


    // back to regular search
    MoveList list;
    generate_moves(pos, &list); // get all moves

    order_moves(pos, &list, distance, hash_move);

    int legal_moves = 0;
    int moves_searched = 0; // track the num of legal moves we have evaluated

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

            int score;

            int move = list.moves[i];
            int to_sq = get_move_to(move);
            int is_capture = pos->occupancy[pos->side ^ 1] & (1ULL << to_sq) || get_move_ep(move);
            int is_promotion = get_move_promoted(move);

            // principle variation search and LMR
            if (moves_searched == 0) {
                score = -negamax(&next_state, depth - 1, distance + 1, -beta, -alpha); // full window
            } else {
                int pvs = 1;

                // only reduce quiet moves at greater than 3 depth late in the list
                if (depth >= 3 && moves_searched >= 3 && !is_capture && !is_promotion && !in_check) {
                    int reduction = 1; // reduction amount
                    
                    // If it's really deep and really late, reduce by 2
                    if (depth >= 4 && moves_searched >= 6) {
                        reduction = 2; 
                    }

                    score = -negamax(&next_state, depth - 1 - reduction, distance + 1, -alpha - 1, -alpha);

                    // reduction worked
                    if (score <= alpha) {
                        pvs = 0; // Skip the normal search
                    }
                }


                // run if didn't reduce or reduction search failed high (move might be good)
                if (pvs) {
                    score = -negamax(&next_state, depth - 1, distance + 1, -alpha - 1, -alpha);
                }

                // failsafe if move was actually better
                if (score > alpha && score < beta) {
                    score = -negamax(&next_state, depth - 1, distance + 1, -beta, -alpha);
                }
            }

            game_ply--;

            moves_searched++;

            if (time_over) return 0;

            if (score > alpha) {
                alpha = score;

                local_best_move = list.moves[i]; // track locally for TT

                if (distance == 0) {
                    best_move = list.moves[i];
                }

                if (distance < 64) {
                    pv_table[distance][distance] = list.moves[i];
                    
                    // Copy the PV from the deeper ply to the current ply
                    for (int next_ply = distance + 1; next_ply < pv_length[distance + 1]; next_ply++) {
                        pv_table[distance][next_ply] = pv_table[distance + 1][next_ply];
                    }
                    
                    // Update the length of the PV line
                    pv_length[distance] = pv_length[distance + 1];
                }
            }

            // prune the branch
            if (alpha >= beta) {
                // only save as killer move if it is a quiet move (captures already handled)
                int to_sq = get_move_to(list.moves[i]);
                int is_capture = pos->occupancy[pos->side ^ 1] & (1ULL << to_sq) || get_move_ep(list.moves[i]);
                if (!is_capture) {
                    // shift down the killer moves, most recent in index 0
                    if (distance < 64) {
                        killer_moves[1][distance] = killer_moves[0][distance];
                        killer_moves[0][distance] = list.moves[i];
                    }

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
    memset(pv_table, 0, sizeof(pv_table));
    memset(pv_length, 0, sizeof(pv_length));

    //svygzy root probing
    int piece_count = __builtin_popcountll(pos->occupancy[WHITE] | pos->occupancy[BLACK]);

    // only probe if the piece count is low enough, and castling is no longer possible
    if (syzygy_enabled && piece_count <= TB_LARGEST && pos->castling_rights == 0) {
        
        unsigned root_result = tb_probe_root(
            pos->occupancy[WHITE], 
            pos->occupancy[BLACK],
            pos->pieces[K] | pos->pieces[k],
            pos->pieces[Q] | pos->pieces[q],
            pos->pieces[R] | pos->pieces[r],
            pos->pieces[B] | pos->pieces[b],
            pos->pieces[N] | pos->pieces[n],
            pos->pieces[P] | pos->pieces[p],
            0,
            pos->castling_rights, 
            pos->en_passant == -1 ? 0 : pos->en_passant, 
            pos->side == WHITE,
            NULL
        );

        if (root_result != TB_RESULT_FAILED) {
            unsigned tb_from = TB_GET_FROM(root_result);
            unsigned tb_to   = TB_GET_TO(root_result);
            unsigned tb_prom = TB_GET_PROMOTES(root_result); 

            // Convert Fathom's internal promotion integer to UCI chars
            char prom_char = ' ';
            if (tb_prom == 1) prom_char = 'q';
            else if (tb_prom == 2) prom_char = 'r';
            else if (tb_prom == 3) prom_char = 'b';
            else if (tb_prom == 4) prom_char = 'n';

            // Let the GUI know we are playing from the tablebase
            printf("info string Syzygy Tablebase Hit!\n");

            // Convert A1=0, H8=63 format to UCI string (e.g., e7e8q)
            if (prom_char != ' ') {
                printf("bestmove %c%c%c%c%c\n", 
                    (tb_from % 8) + 'a', (tb_from / 8) + '1',
                    (tb_to % 8) + 'a', (tb_to / 8) + '1', prom_char);
            } else {
                printf("bestmove %c%c%c%c\n", 
                    (tb_from % 8) + 'a', (tb_from / 8) + '1',
                    (tb_to % 8) + 'a', (tb_to / 8) + '1');
            }
            
            return; // exit search, found best move
        }
    }

    // iterative deepening
    for (int current_depth = 1; current_depth <= depth; current_depth++) {
        int final_score = negamax(pos, current_depth, 0, -50000, 50000);

        if (time_over) {
            break;
        }

        best_move_so_far = best_move;

        long long duration = get_time_ms() - search_start_time;

        printf("info depth %d score cp %d time %lld nodes %lld pv ", current_depth, final_score, duration, nodes_evaluated);
        for (int count = 0; count < pv_length[0]; count++) {
            print_move(pv_table[0][count]);
            printf(" ");
        }
        printf("\n");
    }

    // Output the results in the official UCI format
    printf("bestmove ");
    print_move(best_move_so_far);
    printf("\n");
}