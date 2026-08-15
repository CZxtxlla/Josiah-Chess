#ifndef SEARCH_H
#define SEARCH_H

#include "position.h"
#include "movegen.h"
#include <stdio.h>

extern int THREAD_COUNT; // used for lazy smp

typedef struct {
    int thread_id;
    Position pos; // local copy of position
    int target_depth;
} ThreadData;

void* search_worker(void* arg);

#define MAX_SEARCH_PLY 128

extern U64 game_history[2048];
extern int game_ply;

extern int search_time_limit;
extern volatile int time_over;
extern long long search_start_time;
extern int search_node_limit;

extern int syzygy_enabled;

typedef struct {
    int id;
    int best_move;
    int best_move_so_far;
    long long nodes_evaluated;
    
    int killer_moves[2][MAX_SEARCH_PLY];
    int history_moves[2][64][64];
    
    int pv_length[MAX_SEARCH_PLY];
    int pv_table[MAX_SEARCH_PLY][MAX_SEARCH_PLY];

    // Thread-local history for repetition detection
    U64 search_history[2048];
    int search_ply;
} ThreadState;

long long get_time_ms();

// helper to give a move a very quick preliminary score value
int score_move(Position* pos, int move, int distance, int hash_move, ThreadState* ts);

// sort the moves in the list to be ordered according to their preliminary score
void order_moves(Position* pos, MoveList* moves, int distance, int hash_move, ThreadState* ts);

// recursively perform alpha beta pruning 
int negamax(Position* pos, int depth, int distance, int alpha, int beta, ThreadState* ts); 

// base case for negamax that continues until there are no captures
int quiescence(Position* pos, int alpha, int beta, int qdepth, ThreadState* ts);

// for datagen
void play_datagen_game(char* starting_fen, FILE* output_file);

#endif