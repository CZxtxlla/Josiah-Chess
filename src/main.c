#include "../include/bitboard.h"
#include "../include/magic.h"
#include "../include/position.h"
#include "../include/uci.h"

int main() {
    init_leapers();
    init_sliders();

    Position board;
    
    // Hand control over to the UCI listener
    uci_loop(&board);

    return 0;
}