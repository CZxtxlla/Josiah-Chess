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
    int16_t* weight; // flattened 1D array of size (in_features * out_features)
    int16_t* bias; // size out_features
} LinearLayer;

typedef struct {
    int num_hidden_layers;
    LinearLayer* feature_transformer;
    LinearLayer** hidden_layers;
} NNUE;

extern NNUE* model;

// loading

LinearLayer* load_layer(FILE* file);
NNUE* load_nnue(const char* filepath);

// cleanup 
void free_layer(LinearLayer* layer);
void free_nnue(NNUE* model);

// accumulator stuff

void init_accumulator(Position* pos, NNUE* model);

void update_accumulator(Position* pos, NNUE* model, int piece, int sq, int is_adding);

// integer evaluation for quantized model
int evaluate_nnue_quantized(const Position* pos, NNUE* model);


#endif