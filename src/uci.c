#include "../include/uci.h"
#include "../include/search.h"
#include "../include/movegen.h"
#include "../include/zobrist.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

char current_game_history[2048] = "";
int book_enabled = 0;

// helper to get book move if the history is recognized
int get_book_move(char* history, Position* pos) {
    if (!book_enabled) return 0;

    FILE* file = fopen("book.txt", "r");
    if (!file) return 0;

    char chosen_move_str[6] = {0};
    int valid_lines_seen = 0;
    char line[2048];
    
    int history_len = strlen(history);

    while (fgets(line, sizeof(line), file)) {
        // check if matches the game history
        if (strncmp(line, history, history_len) == 0) {
            
            char* next_move_str = line + history_len;
            while (*next_move_str == ' ') next_move_str++;
            
            if (*next_move_str != '\n' && *next_move_str != '\0' && *next_move_str != '\r') {
                
                char single_move[6] = {0};
                sscanf(next_move_str, "%5s", single_move);
                
                // We found a valid line
                valid_lines_seen++;
                
                // Reservoir Sampling 
                // we keep the new move with a probability of 1 / valid_lines_seen
                if (rand() % valid_lines_seen == 0) {
                    strcpy(chosen_move_str, single_move);
                }
            }
        }
    }
    fclose(file);

    // if we didn't find any matching lines, exit
    if (valid_lines_seen == 0) return 0; 

    return parse_move(chosen_move_str, pos);
}


void parse_go(char* command, Position* pos) {
    int depth = 64; // massive depth so the clock breaks
    int wtime = -1, btime = -1, winc = 0, binc = 0, movestogo = 30;
    int movetime = -1;

    char* ptr;
    
    // Extract values
    if ((ptr = strstr(command, "wtime"))) wtime = atoi(ptr + 6);
    if ((ptr = strstr(command, "btime"))) btime = atoi(ptr + 6);
    if ((ptr = strstr(command, "winc"))) winc = atoi(ptr + 5);
    if ((ptr = strstr(command, "binc"))) binc = atoi(ptr + 5);
    if ((ptr = strstr(command, "movestogo"))) movestogo = atoi(ptr + 10);
    if ((ptr = strstr(command, "movetime"))) {
        movetime = atoi(ptr + 9); // Give it 50ms of safety padding
    }
    
    // If GUI specifically asks for fixed depth, do it
    if ((ptr = strstr(command, "depth"))) depth = atoi(ptr + 6); 

    // Determine whose clock we are looking at
    int time_left = (pos->side == WHITE) ? wtime : btime;
    int increment = (pos->side == WHITE) ? winc : binc;

    if (movetime != -1) {
        search_time_limit = movetime - 50; 
    } else if (time_left != -1) {
        // divide remaining time by the moves we think are left, plus half the increment
        search_time_limit = (time_left / movestogo) + (increment / 2);
        
        // Safety Buffer, 50 ms
        if (search_time_limit > time_left - 50) {
            search_time_limit = time_left - 50;
        }
        
        // If we are literally at 0, give the engine 10ms to find ANY move
        if (search_time_limit <= 0) search_time_limit = 10;
    } else {
        // If no time was sent, just think for 2 seconds
        search_time_limit = 2000; 
    }

    if (search_time_limit <= 0) search_time_limit = 10;

    int book_move = get_book_move(current_game_history, pos);
    if (book_move != 0) {
        // found book move, print it and skip search
        printf("bestmove ");
        print_move(book_move);
        printf("\n");
        return; 
    }


    // iterative deepening
    search_position(pos, depth);
}


int parse_move(char* move_string, Position* pos) {
    MoveList list;
    generate_moves(pos, &list);

    // Parse source and target squares from the text
    int source_file = move_string[0] - 'a';
    int source_rank = move_string[1] - '1';
    int target_file = move_string[2] - 'a';
    int target_rank = move_string[3] - '1';

    int source_square = source_rank * 8 + source_file;
    int target_square = target_rank * 8 + target_file;

    // Search our generated moves for a match
    for (int i = 0; i < list.count; i++) {
        int move = list.moves[i];
        
        if (get_move_from(move) == source_square && get_move_to(move) == target_square) {
            int promoted_piece = get_move_promoted(move);
            
            // If our generator says this is a promotion move...
            if (promoted_piece) {
                // Check if the GUI provided a 5th character
                if (strlen(move_string) >= 5) {
                    char promoted_char = move_string[4];
                    if ((promoted_char == 'q' || promoted_char == 'Q') && (promoted_piece == Q || promoted_piece == q)) return move;
                    if ((promoted_char == 'r' || promoted_char == 'R') && (promoted_piece == R || promoted_piece == r)) return move;
                    if ((promoted_char == 'b' || promoted_char == 'B') && (promoted_piece == B || promoted_piece == b)) return move;
                    if ((promoted_char == 'n' || promoted_char == 'N') && (promoted_piece == N || promoted_piece == n)) return move;
                    continue; // GUI gave a letter, but this move isn't it
                } else {
                    // Default to Queen promotion.
                    if (promoted_piece == Q || promoted_piece == q) {
                        return move;
                    }
                    continue; 
                }
            }
            // match
            return move; 
        }
    }
    return 0; // Illegal move /  not found
}

void parse_position(char* command, Position* pos) {
    command += 9; // Skip the word "position "
    char* current_char = command;

    game_ply = 0;
    current_game_history[0] = '\0';
    book_enabled = 0;

    // 1. Set the initial board state (Either startpos or a custom FEN)
    if (strncmp(command, "startpos", 8) == 0) {
        parse_fen(pos, START_POSITION);
        current_char += 8;
        book_enabled = 1;
    } else if (strncmp(command, "fen", 3) == 0) {
        current_char += 4;
        parse_fen(pos, current_char);
    }

    // 2. Look ahead in the string to see if the word "moves" exists
    current_char = strstr(command, "moves");

    if (current_char != NULL) {
        current_char += 6; // Skip the word "moves "

        strcpy(current_game_history, current_char);
        
        // 3. Loop through all the moves and physically apply them to the board
        while (*current_char) {
            int move = parse_move(current_char, pos);
            if (move == 0) break; // Safety catch for bad string parsing
            
            make_move(pos, move);

            // log history
            game_history[game_ply] = pos->hash_key;
            game_ply++;
            
            // Advance the text pointer to the next word
            while (*current_char && *current_char != ' ') current_char++;
            if (*current_char == ' ') current_char++;
        }
    }
}

void run_benchmark(char* command, Position* pos) {
    // Expected command format: bench [depth] [FEN]

    char* ptr = command + 5; // skip word bench

    while (*ptr == ' ') ptr++; // skip spaces

    int target_depth = 5; // Default depth if none is provided

    // extract the given depth
    if (*ptr >= '0' && *ptr <= '9') {
        target_depth = atoi(ptr);
        while (*ptr != ' ' && *ptr != '\0') ptr++;
        while (*ptr == ' ') ptr++;
    }

    char* fen = START_POSITION; // default fen if none is provided

    // extract the fen
    if (*ptr != '\0') {
        fen = ptr;
    }

    parse_fen(pos, fen); // set the board

    printf("\n=== BENCHMARK STARTED: DEPTH %d ===\n", target_depth);

    // disable the clock
    search_time_limit = 99999999; 
    time_over = 0;
    nodes_evaluated = 0;
    search_start_time = get_time_ms();

    int best_move_so_far = 0;
    long long total_nodes = 0;

    memset(killer_moves, 0, sizeof(killer_moves));
    memset(history_moves, 0, sizeof(history_moves));

    // run iterative deepening loop
    for (int current_depth = 1; current_depth <= target_depth; current_depth++) {
        long long nodes_before = nodes_evaluated;
        int final_score = negamax(pos, current_depth, 0, -50000, 50000);
        best_move_so_far = best_move;
        
        long long duration = get_time_ms() - search_start_time;
        long long depth_nodes = nodes_evaluated - nodes_before;
        total_nodes += depth_nodes;
        
        // print table row for each depth
        printf("Depth %2d | Score: %5d | Nodes: %10lld | Time: %5lld ms\n", current_depth, final_score, depth_nodes, duration);
    }

    printf("===================================\n");
    printf("Best Move: ");
    print_move(best_move_so_far);
    printf("\nTotal Nodes: %lld\n\n", total_nodes);
}


void uci_loop(Position* pos) {
    // unbuffer the output so GUI gets text instantly
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);

    srand(time(NULL));

    char line[2048];

    printf("CharlesEngine UCI Interface Started.\n");

    while (fgets(line, sizeof(line), stdin)) {
        // Strip newline and carriage returns
        line[strcspn(line, "\n")] = 0;
        line[strcspn(line, "\r")] = 0;

        // Use strcmp (exact match) instead of strncmp to prevent "ucinewgame" trap
        if (strcmp(line, "uci") == 0) {
            printf("id name CharlesEngine\n");
            printf("id author Charles\n");
            printf("option name Move Overhead type spin default 500 min 0 max 5000\n");
            printf("option name Threads type spin default 1 min 1 max 128\n");
            printf("option name Hash type spin default 16 min 1 max 32768\n");
            printf("uciok\n");
        } 
        else if (strcmp(line, "isready") == 0) {
            printf("readyok\n");
        } 
        else if (strcmp(line, "ucinewgame") == 0) {
            clear_tt();
            memset(killer_moves, 0, sizeof(killer_moves));
            memset(history_moves, 0, sizeof(history_moves));
            parse_fen(pos, START_POSITION);
        }
        else if (strncmp(line, "position", 8) == 0) {
            // Hand the string off to our massive parser
            parse_position(line, pos);
        } 
        else if (strncmp(line, "go", 2) == 0) {
            parse_go(line, pos);
        } else if (strncmp(line, "bench", 5) == 0) {
            run_benchmark(line, pos);
        } else if (strcmp(line, "quit") == 0) {
            break;
        }
    }
}