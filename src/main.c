#include "../include/bitboard.h"
#include "../include/magic.h"
#include "../include/position.h"
#include "../include/uci.h"
#include "../include/zobrist.h"
#include "../include/tbprobe.h"

int main() {
    init_leapers();
    init_sliders();
    
    init_zobrist();
    init_tt(128); // 128 megabytes

    tb_init("tables");

    Position board;
    
    // Hand control over to the UCI listener
    uci_loop(&board);

    return 0;
}