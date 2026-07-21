#ifndef UCI_H
#define UCI_H

#include "position.h"
#include <stddef.h>
#define PATH_MAX 4096


extern char current_game_history[2048];

void get_resource_path(const char* filename, char* out_path, size_t out_size);

int parse_move(char* move_string, Position* pos); // Helper function to turn a string like "e2e4" or "e7e8q" into an integer move

void parse_position(char* command, Position* pos); // perform all the moves that have happened

void uci_loop(Position* pos); // used for interaction with UCI compatible GUI


#endif