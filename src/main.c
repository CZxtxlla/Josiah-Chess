#include <stdio.h>
#include "../include/position.h"
#include "../include/movegen.h"

int main() {
    Position board;
    MoveList list;
    
    parse_fen(&board, START_POSITION);
    generate_moves(&board, &list);
    
    printf("Generated White Pawn Moves (%d total):\n", list.count);
    for (int i = 0; i < list.count; i++) {
        print_move(list.moves[i]);
    }
    printf("\n");

    return 0;
}