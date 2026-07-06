#include "../include/bitboard.h"
#include "../include/magic.h"
#include "../include/position.h"
#include "../nnue/inference.h"
#include "../include/uci.h"
#include "../include/zobrist.h"
#include "../syzygy/tbprobe.h"
#include <limits.h>
#include <mach-o/dyld.h>
#include <string.h>

static void get_nnue_path(char* out_path, size_t out_size) {
    char exe_path[PATH_MAX];
    uint32_t exe_size = sizeof(exe_path);

    if (_NSGetExecutablePath(exe_path, &exe_size) == 0) {
        char resolved_path[PATH_MAX];
        if (realpath(exe_path, resolved_path) != NULL) {
            char* last_slash = strrchr(resolved_path, '/');
            if (last_slash != NULL) {
                *last_slash = '\0';
                snprintf(out_path, out_size, "%s/nnue/768_model_quant_9_18.nnue", resolved_path);
                return;
            }
        }
    }

    snprintf(out_path, out_size, "nnue/768_model_quant_9_18.nnue");
}

int main(int argc, char** argv) {

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stdin, NULL, _IONBF, 0);
    
    init_leapers();
    init_sliders();
    
    init_zobrist();
    init_tt(128); // 128 megabytes

    //tb_init("tables");

    Position board = {0};

    char nnue_path[PATH_MAX];
    get_nnue_path(nnue_path, sizeof(nnue_path));

    model = load_nnue(nnue_path);
    if (model == NULL) {
        printf("info string ERROR: Could not find 768_model_quant_9_18.nnue at %s!\n", nnue_path);
        exit(1); // Force crash cleanly
    }

    parse_fen(&board, START_POSITION);
    
    // Hand control over to the UCI listener
    uci_loop(&board);

    return 0;
}