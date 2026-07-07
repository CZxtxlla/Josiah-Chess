#include "inference.h"

NNUE* model = NULL;

LinearLayer* load_layer(FILE* file) {
    LinearLayer* layer = (LinearLayer*)malloc(sizeof(LinearLayer));
    if (fread(&layer->in_features, sizeof(int), 1, file) != 1) return NULL;
    if (fread(&layer->out_features, sizeof(int), 1, file) != 1) return NULL;

    int w_size = layer->in_features * layer->out_features;
    int b_size = layer->out_features;

    layer->weight = (int16_t*)malloc(w_size * sizeof(float));
    layer->bias = (int16_t*)malloc(b_size * sizeof(float));

    fread(layer->weight, sizeof(int16_t), w_size, file);
    fread(layer->bias, sizeof(int16_t), b_size, file);

    return layer;
}

NNUE* load_nnue(const char* filepath) {
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        printf("Error: Could not open %s\n", filepath);
        return NULL;
    }

    uint32_t magic;
    if (fread(&magic, sizeof(uint32_t), 1, file) != 1 || magic != NNUE_MAGIC_NUMBER) {
        printf("Error: Invalid NNUE file format or magic number.\n");
        fclose(file);
        return NULL;
    }

    NNUE* model = (NNUE*)malloc(sizeof(NNUE));
    fread(&model->num_hidden_layers, sizeof(int), 1, file);
    
    model->feature_transformer = load_layer(file);

    model->hidden_layers = (LinearLayer**)malloc(model->num_hidden_layers * sizeof(LinearLayer*));
    for (int i = 0; i < model->num_hidden_layers; i++) {
        model->hidden_layers[i] = load_layer(file);
    }

    fclose(file);
    return model;
}

void free_layer(LinearLayer* layer) {
    if (layer) {
        free(layer->weight);
        free(layer->bias);
        free(layer);
    }
}

void free_nnue(NNUE* model) {
    if (model) {
        free_layer(model->feature_transformer);
        for (int i = 0; i < model->num_hidden_layers; i++) {
            free_layer(model->hidden_layers[i]);
        }

        free(model->hidden_layers);
        free(model);
    }
}

// inference


static int char_to_piece(char c) {
    switch (c) {
        case 'P': return 0; case 'N': return 1; case 'B': return 2; case 'R': return 3; case 'Q': return 4; case 'K': return 5;
        case 'p': return 6; case 'n': return 7; case 'b': return 8; case 'r': return 9; case 'q': return 10; case 'k': return 11;
        default: return -1;
    }
}
int flip_sq(int sq) { return sq ^ 56; }
int flip_piece(int p_type) { return (p_type + 6) % 12; }

static inline int clipped_relu_int(int x) {
    if (x < 0) return 0;//return x / 100;
    if (x > 256) return 256;
    return x;
}


// ----- accumulator stuff ------


void init_accumulator(Position* pos, NNUE* model) {
    // biases
    for (int i = 0; i < ACC_SIZE; i++) {
        pos->nnue_acc.white[i] = model->feature_transformer->bias[i];
        pos->nnue_acc.black[i] = model->feature_transformer->bias[i];
    }

    // features for pieces on the board
    for (int p = 0; p < 12; p++) {
        U64 bb = pos->pieces[p];
        while (bb) {
            int sq = __builtin_ctzll(bb);
            bb &= (bb - 1); // clear LSB

            int w_idx = (p * 64) + sq;
            int b_idx = (flip_piece(p) * 64) + flip_sq(sq);

            for (int i = 0; i < ACC_SIZE; i++) {
                pos->nnue_acc.white[i] += model->feature_transformer->weight[w_idx * ACC_SIZE + i];
                pos->nnue_acc.black[i] += model->feature_transformer->weight[b_idx * ACC_SIZE + i];
            }
        }
    }
}

void update_accumulator(Position* pos, NNUE* model, int piece, int sq, int is_adding) {
    int w_idx = (piece * 64) + sq;
    int b_idx = (flip_piece(piece) * 64) + flip_sq(sq);
    
    int sign = is_adding ? 1 : -1;

    for (int i = 0; i < ACC_SIZE; i++) {
        pos->nnue_acc.white[i] += sign * model->feature_transformer->weight[w_idx * ACC_SIZE + i];
        pos->nnue_acc.black[i] += sign * model->feature_transformer->weight[b_idx * ACC_SIZE + i];
    }
}

// evaluation given accumulators

// for quantized network
int evaluate_nnue_quantized(const Position* pos, NNUE* model) {
    int current_input[256];
    int next_input[256];

    int stm = pos->side;

    // get accumulators from pos struct
    const int16_t* stm_acc = (stm == 0) ? pos->nnue_acc.white : pos->nnue_acc.black;
    const int16_t* nstm_acc = (stm == 0) ? pos->nnue_acc.black : pos->nnue_acc.white;

    // perspective concat
    for (int i = 0; i < ACC_SIZE; i++) {
        current_input[i] = clipped_relu_int(stm_acc[i]);
        current_input[ACC_SIZE + i] = clipped_relu_int(nstm_acc[i]);
    }

    // hidden layers
    int current_dim = ACC_SIZE * 2;

    for (int l = 0; l < model->num_hidden_layers; l++) {
        LinearLayer* hl = model->hidden_layers[l];

        for (int i = 0; i < hl->out_features; i++) {
            int32_t sum = hl->bias[i] * 256; // scale up the bias

            // matmul
            for (int j = 0; j < hl->in_features; j++) {
                sum += current_input[j] * hl->weight[j * hl->out_features + i];
            }

            sum = sum >> 8; // divide by 256

            // apply relu except on final node
            if (l < model->num_hidden_layers - 1) {
                next_input[i] = clipped_relu_int(sum);
            } else {
                next_input[i] = sum;
            }
        }

        // copy results into current input for next layer
        current_dim = hl->out_features;
        for (int i = 0; i < current_dim; i++) {
            current_input[i] = next_input[i];
        }
    }

    // convert prob to centipawns
    float stm_win_prob = (float) current_input[0] / 256.0f;
    
    if (stm_win_prob < 0.001f) stm_win_prob = 0.001f;
    if (stm_win_prob > 0.999f) stm_win_prob = 0.999f;

    float centipawns = -400.0f * logf((1.0f / stm_win_prob) - 1.0f);

    return (int)roundf(centipawns);
}