#include <stdio.h>
#include <sys/time.h>
#include "../include/types.h"
#include "../include/bitboard.h"
#include "../include/magic.h"
#include "../include/position.h"
#include "../include/movegen.h"

// Forward declarations
long long perft(Position* pos, int depth);

void perft_divide(Position* pos, int depth);

// Utility to get current time in milliseconds
long long get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000LL) + (tv.tv_usec / 1000);
}

int main() {
    printf("--- Booting Engine ---\n");
    printf("Initializing Leaper Tables...\n");
    init_leapers();
    printf("Initializing Magic Bitboards...\n");
    init_sliders();
    printf("Initialization Complete.\n");

    Position board; 
    parse_fen(&board, START_POSITION);
    print_board(&board);

    int max_depth = 7; 
    
    printf("--- Starting Perft Test ---\n");
    for (int depth = 1; depth <= max_depth; depth++) {
        long long start_time = get_time_ms();
        
        // reset diagnostics
        extern long long diag_castles, diag_ep, diag_promotions;
        diag_castles = diag_ep = diag_promotions = 0;

        long long nodes = perft(&board, depth);
        
        long long end_time = get_time_ms();
        long long time_taken = end_time - start_time;
        
        // Prevent division by zero if it finishes in <1 ms
        long long nps = (time_taken > 0) ? (nodes * 1000) / time_taken : 0; 

         printf("Depth %d | Nodes: %-10lld | Time: %-6lld ms | NPS: %lld | castles=%lld ep=%lld promos=%lld\n", 
             depth, nodes, time_taken, nps, diag_castles, diag_ep, diag_promotions);
    }
    //perft_divide(&board, 3);
    
    return 0;
}