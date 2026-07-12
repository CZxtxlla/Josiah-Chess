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

void get_resource_path(const char* filename, char* out_path, size_t out_size) {
    char exe_path[PATH_MAX];
    uint32_t exe_size = sizeof(exe_path);

    if (_NSGetExecutablePath(exe_path, &exe_size) == 0) {
        char resolved_path[PATH_MAX];
        if (realpath(exe_path, resolved_path) != NULL) {
            char* last_slash = strrchr(resolved_path, '/');
            if (last_slash != NULL) {
                *last_slash = '\0';
                snprintf(out_path, out_size, "%s/%s", resolved_path, filename);
                return;
            }
        }
    }
    // Fallback
    snprintf(out_path, out_size, "%s", filename);
}

int main(int argc, char** argv) {

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stdin, NULL, _IONBF, 0);
    
    init_leapers();
    init_sliders();
    
    init_zobrist();
    init_tt(64); // 64 megabytes

    Position board = {0};

    char nnue_path[PATH_MAX];
    get_resource_path("nnue/768_quant_9_18_50_1024.nnue", nnue_path, sizeof(nnue_path));

    model = load_nnue(nnue_path);
    if (model == NULL) {
        printf("info string ERROR: Could not find 768_quant_9_18_50_1024.nnue at %s!\n", nnue_path);
        exit(1); // Force crash cleanly
    }

    parse_fen(&board, START_POSITION);
    
    // Hand control over to the UCI listener
    uci_loop(&board);

    return 0;
}