#ifndef SEARCH_H
#define SEARCH_H

#include "position.h"
#include "movegen.h"

extern int killer_moves[2][64];
extern int history_moves[2][64][64];

extern int search_time_limit;
extern int time_over;
extern long long nodes_evaluated;
extern long long search_start_time;
extern int best_move;

long long get_time_ms();

// helper to give a move a very quick preliminary score value
int score_move(Position* pos, int move, int distance, int hash_move);

// sort the moves in the list to be ordered according to their preliminary score
void order_moves(Position* pos, MoveList* moves, int distance, int hash_move);

// recursively perform alpha beta pruning 
int negamax(Position* pos, int depth, int distance, int alpha, int beta); 

// function to call negamax and format the output
void search_position(Position* pos, int depth);

// base case for negamax that continues until there are no captures
int quiescence(Position* pos, int alpha, int beta);



#endif