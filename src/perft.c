#include <stdio.h>
#include "../include/position.h"
#include "../include/movegen.h"

long long diag_castles = 0;
long long diag_ep = 0;
long long diag_promotions = 0;

// Recursive function to count nodes
long long perft(Position* pos, int depth) {
    if (depth == 0) return 1ULL;

    MoveList list;
    generate_moves(pos, &list);
    
    long long nodes = 0;

    for (int i = 0; i < list.count; i++) {
        Position next_state = *pos;
        
        // Only count the move if it is legally valid
        if (make_move(&next_state, list.moves[i])) {
            // diagnostics: increment counters for special move types
            extern long long diag_castles, diag_ep, diag_promotions;
            if (get_move_castle(list.moves[i])) diag_castles++;
            if (get_move_ep(list.moves[i])) diag_ep++;
            if (get_move_promoted(list.moves[i])) diag_promotions++;

            nodes += perft(&next_state, depth - 1);
        }
    }
    
    return nodes;
}

void perft_divide(Position* pos, int depth) {
    MoveList list;
    generate_moves(pos, &list);

    long long total_nodes = 0;

    for (int i = 0; i < list.count; i++) {
        Position next_state = *pos;

        if (make_move(&next_state, list.moves[i])) {
            long long nodes = perft(&next_state, depth - 1);
            total_nodes += nodes;

            print_move(list.moves[i]);
            printf("%lld\n", nodes);
        }
    }

    printf("divide total: %lld\n", total_nodes);
}