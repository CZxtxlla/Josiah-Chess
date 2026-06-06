#ifndef SEARCH_H
#define SEARCH_H

#include "position.h"
#include "movegen.h"

extern int search_time_limit;

// helper to give a move a very quick preliminary score value
int score_move(Position* pos, int move);

// sort the moves in the list to be ordered according to their preliminary score
void order_moves(Position* pos, MoveList* moves);

// recursively perform alpha beta pruning 
int negamax(Position* pos, int depth, int distance, int alpha, int beta); 

// function to call negamax and format the output
void search_position(Position* pos, int depth);

// base case for negamax that continues until there are no captures
int quiescence(Position* pos, int alpha, int beta);



#endif