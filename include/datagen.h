#ifndef DATAGEN_H
#define DATAGEN_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <string.h>
#include "position.h"

// Struct to pass data to each thread
typedef struct {
    int thread_id;
    int games_to_play;
    char** opening_fens; // array of starting positions
    int num_openings;
} DatagenArgs;


#pragma pack(push, 1)
typedef struct {
    int16_t eval; 
    uint16_t win;
    uint16_t draw;
    uint16_t loss;
    uint8_t stm; 
    uint8_t num_features;
    uint16_t features[32]; 
} TrainingData;
#pragma pack(pop)

#define MAX_GAME_PLYS 1000

// run datagen command, "datagen <threads> <games per thread>"
// store generated data in sample format.
void run_datagen(char* command); 

void extract_features(Position* pos, TrainingData* data);

#endif