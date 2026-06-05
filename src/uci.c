#include "../include/uci.h"
#include "../include/search.h"
#include "../include/movegen.h"
#include <string.h>
#include <stdio.h>


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

    // 1. Set the initial board state (Either startpos or a custom FEN)
    if (strncmp(command, "startpos", 8) == 0) {
        parse_fen(pos, START_POSITION);
        current_char += 8;
    } else if (strncmp(command, "fen", 3) == 0) {
        current_char += 4;
        parse_fen(pos, current_char);
    }

    // 2. Look ahead in the string to see if the word "moves" exists
    current_char = strstr(command, "moves");

    if (current_char != NULL) {
        current_char += 6; // Skip the word "moves "
        
        // 3. Loop through all the moves and physically apply them to the board
        while (*current_char) {
            int move = parse_move(current_char, pos);
            if (move == 0) break; // Safety catch for bad string parsing
            
            make_move(pos, move);
            
            // Advance the text pointer to the next word
            while (*current_char && *current_char != ' ') current_char++;
            if (*current_char == ' ') current_char++;
        }
    }
}


void uci_loop(Position* pos) {
    // unbuffer the output so GUI gets text instantly
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);

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
            printf("uciok\n");
        } 
        else if (strcmp(line, "isready") == 0) {
            printf("readyok\n");
        } 
        else if (strcmp(line, "ucinewgame") == 0) {
            parse_fen(pos, START_POSITION);
        }
        else if (strncmp(line, "position", 8) == 0) {
            // Hand the string off to our massive parser
            parse_position(line, pos);
        } 
        else if (strncmp(line, "go", 2) == 0) {
            search_position(pos, 5); 
        } 
        else if (strcmp(line, "quit") == 0) {
            break;
        }
    }
}