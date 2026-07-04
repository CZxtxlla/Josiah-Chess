#ifndef INFERENCE_H
#define INFERENCE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include "../include/position.h"

#define NNUE_MAGIC_NUMBER 0x4E4E5545
#define MAX_ACTIVE 32

// NNUE structure defn

typedef struct {
    int in_features;
    int out_features;
    float* weight; // flattened 1D array of size (in_features * out_features)
    float* bias; // size out_features
} LinearLayer;

typedef struct {
    int num_hidden_layers;
    LinearLayer* feature_transformer;
    LinearLayer** hidden_layers;
} NNUE;

// loading

LinearLayer* load_layer(FILE* file);
NNUE* load_nnue(const char* filepath);

// forward
float nnue_forward(NNUE* model, int* w_idx, int* b_idx, int num_active, int stm);

// cleanup 
void free_layer(LinearLayer* layer);
void free_nnue(NNUE* model);

// accumulator stuff

void init_accumulator(Position* pos, NNUE* model);

void update_accumulator(Position* pos, NNUE* model, int piece, int sq, int is_adding);

float evaluate_nnue(NNUE* model, int ply, int stm);




#endif