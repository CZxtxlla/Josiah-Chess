#include "../include/bitboard.h"
#include "../include/magic.h"
#include "../include/position.h"
#include "../nnue/inference.h"
#include "../include/uci.h"
#include "../include/zobrist.h"
#include "../syzygy/tbprobe.h"
#include <limits.h>
#include <string.h>
#include <unistd.h>


void get_resource_path(const char* filename, char* out_path, size_t out_size) {
    char exe_path[4096];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    
    if (len != -1) {
        exe_path[len] = '\0';
        
        // find the last '/' to isolate the directory containing the executable
        char* last_slash = strrchr(exe_path, '/');
        if (last_slash != NULL) {
            *last_slash = '\0'; // Truncate the string at the last slash
            
            // construct the absolute path: <exe_directory>/<filename>
            snprintf(out_path, out_size, "%s/%s", exe_path, filename);
            return; // success
        }
    }
    
    // Fallback, if readlink fails or no slash is found, use the relative path
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
    get_resource_path("nnue/768_quant_9_18_50_1024_v2.nnue", nnue_path, sizeof(nnue_path));

    model = load_nnue(nnue_path);
    if (model == NULL) {
        printf("info string ERROR: Could not find 768_quant_9_18_50_1024_v2.nnue at %s!\n", nnue_path);
        exit(1); // Force crash cleanly
    }

    parse_fen(&board, START_POSITION);
    
    // Hand control over to the UCI listener
    uci_loop(&board);

    return 0;
}