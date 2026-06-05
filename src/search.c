#include "../include/search.h"
#include "../include/evaluate.h"
#include "../include/movegen.h"
#include "../include/position.h"
#include <stdio.h>

int best_move = 0;
long long nodes_evaluated = 0;

int negamax(Position* pos, int depth, int distance, int alpha, int beta) {
    // base case
    if (depth == 0) {
        nodes_evaluated++;
        return evaluate(pos);
    }

    MoveList list;
    generate_moves(pos, &list); // get all moves

    int legal_moves = 0;

    // go through the tree, check every move
    for (int i = 0; i < list.count; i++) {
        Position next_state = *pos;

        if (make_move(&next_state, list.moves[i])) {
            legal_moves++;

            int score = -negamax(&next_state, depth - 1, distance + 1, -beta, -alpha);

            if (score > alpha) {
                alpha = score;

                if (distance == 0) {
                    best_move = list.moves[i];
                }
            }

            // prune the branch
            if (alpha >= beta) {
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
    best_move = 0;
    nodes_evaluated = 0;

    int final_score = negamax(pos, depth, 0, -50000, 50000);

    // Output the results in the official UCI format
    printf("info depth %d score cp %d nodes %lld\n", depth, final_score, nodes_evaluated);
    printf("bestmove ");
    print_move(best_move);
    printf("\n");



}