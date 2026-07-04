#include "inference.h"

LinearLayer* load_layer(FILE* file) {
    LinearLayer* layer = (LinearLayer*)malloc(sizeof(LinearLayer));
    if (fread(&layer->in_features, sizeof(int), 1, file) != 1) return NULL;
    if (fread(&layer->out_features, sizeof(int), 1, file) != 1) return NULL;

    int w_size = layer->in_features * layer->out_features;
    int b_size = layer->out_features;

    layer->weight = (float*)malloc(w_size * sizeof(float));
    layer->bias = (float*)malloc(b_size * sizeof(float));

    fread(layer->weight, sizeof(float), w_size, file);
    fread(layer->bias, sizeof(float), b_size, file);

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

static inline float clipped_relu(float x) {
    if (x < 0.0f) return 0.01f * x;
    if (x > 1.0f) return 1.0f;
    return x;
}

float nnue_forward(NNUE* model, int* w_idx, int* b_idx, int num_active, int stm) {
    int acc_size = model->feature_transformer->out_features;

    float* w_acc = (float*)malloc(acc_size * sizeof(float));
    float* b_acc = (float*)malloc(acc_size * sizeof(float));

    memcpy(w_acc, model->feature_transformer->bias, acc_size * sizeof(float));
    memcpy(b_acc, model->feature_transformer->bias, acc_size * sizeof(float));

    for (int f = 0; f < num_active; f++) {
        int w_f = w_idx[f];
        int b_f = b_idx[f];
        for (int i = 0; i < acc_size; i++) {
            w_acc[i] += model->feature_transformer->weight[w_f * acc_size + i];
            b_acc[i] += model->feature_transformer->weight[b_f * acc_size + i];
        }
    }

    int current_dim = acc_size * 2;
    float* current_input = (float*)malloc(current_dim * sizeof(float));

    float* stm_acc = (stm == 0) ? w_acc : b_acc;
    float* nstm_acc = (stm == 0) ? b_acc : w_acc;

    //accumulator
    for (int i = 0; i < acc_size; i++) {
        current_input[i]= clipped_relu(stm_acc[i]);
        current_input[acc_size + i] = clipped_relu(nstm_acc[i]);
    }

    // hidden layers
    for (int l = 0; l < model->num_hidden_layers; l++) {
        LinearLayer* hl = model->hidden_layers[l];
        float* next_input = (float*)malloc(hl->out_features * sizeof(float));

        for (int i = 0; i < hl->out_features; i++) {
            float sum = hl->bias[i];

            for (int j = 0; j < hl->in_features; j++) {
                sum += current_input[j] * hl->weight[j * hl->out_features + i];
            }

            if (l < model->num_hidden_layers - 1) {
                next_input[i] = clipped_relu(sum);
            } else {
                next_input[i] = sum;
            }
        }
        free(current_input);
        current_input = next_input;
    }

    float final_logit = current_input[0];

    free(current_input);
    free(w_acc);
    free(b_acc);

    return final_logit;
}

int char_to_piece(char c) {
    switch (c) {
        case 'P': return 0; case 'N': return 1; case 'B': return 2; case 'R': return 3; case 'Q': return 4; case 'K': return 5;
        case 'p': return 6; case 'n': return 7; case 'b': return 8; case 'r': return 9; case 'q': return 10; case 'k': return 11;
        default: return -1;
    }
}
int flip_sq(int sq) { return sq ^ 56; }
int flip_piece(int p_type) { return (p_type + 6) % 12; }

void evaluate_fen(NNUE* model, const char* fen) {
    int w_idx[MAX_ACTIVE], b_idx[MAX_ACTIVE];
    int stm, num_pieces = 0, sq = 56;
    
    char fen_copy[256]; strncpy(fen_copy, fen, 256);
    char* token = strtok(fen_copy, " ");
    
    for (int i = 0; token[i] != '\0'; i++) {
        char c = token[i];
        if (c == '/') sq -= 16;
        else if (isdigit(c)) sq += (c - '0');
        else {
            int p = char_to_piece(c);
            if (p != -1) {
                w_idx[num_pieces] = (p * 64) + sq;
                b_idx[num_pieces] = (flip_piece(p) * 64) + flip_sq(sq);
                num_pieces++; sq++;
            }
        }
    }
    token = strtok(NULL, " ");
    stm = (token && token[0] == 'b') ? 1 : 0;

    // Run the standalone CPU inference
    float raw_logit = nnue_forward(model, w_idx, b_idx, num_pieces, stm);

    // Convert back to CP
    float stm_win_prob = raw_logit;
    if (stm_win_prob < 0.001f) stm_win_prob = 0.001f;
    if (stm_win_prob > 0.999f) stm_win_prob = 0.999f;

    float centipawns = -400.0f * logf((1.0f / stm_win_prob) - 1.0f);
    if (stm == 1) centipawns = -centipawns;

    printf("\nFEN: %s\n", fen);
    printf("Side to move: %s\n", stm == 1 ? "Black" : "White");
    printf("Win Prob:     %.2f%%\n", stm_win_prob * 100.0f);
    printf("Eval:         %.2f (%.2f Pawns)\n", centipawns, centipawns / 100.0f);
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
    
    float sign = is_adding ? 1.0f : -1.0f;

    for (int i = 0; i < ACC_SIZE; i++) {
        pos->nnue_acc.white[i] += sign * model->feature_transformer->weight[w_idx * ACC_SIZE + i];
        pos->nnue_acc.black[i] += sign * model->feature_transformer->weight[b_idx * ACC_SIZE + i];
    }
}

void evaluate_nnue(const Position* pos, NNUE* model) {
    float current_input[256];
    float next_input[256];

    int stm = pos->side;

    // get accumulators from pos struct
    const float* stm_acc = (stm == 0) ? pos->nnue_acc.white : pos->nnue_acc.black;
    const float* nstm_acc = (stm == 0) ? pos->nnue_acc.black : pos->nnue_acc.white;

    // perspective concat
    for (int i = 0; i < ACC_SIZE; i++) {
        current_input[i] = clipped_relu(stm_acc[i]);
        current_input[ACC_SIZE + i] = clipped_relu(nstm_acc[i]);
    }

    // hidden layers
    int current_dim = ACC_SIZE * 2;

    for (int l = 0; l < model->num_hidden_layers; l++) {
        LinearLayer* hl = model->hidden_layers[l];

        for (int i = 0; i < hl->out_features; i++) {
            float sum = hl->bias[i];

            // matmul
            for (int j = 0; j < hl->in_features; j++) {
                sum += current_input[j] * hl->weight[j * hl->out_features + i];
            }

            // apply relu except on final node
            if (l < model->num_hidden_layers - 1) {
                next_input[i] = clipped_relu(sum);
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
    float stm_win_prob = current_input[0];
    
    if (stm_win_prob < 0.001f) stm_win_prob = 0.001f;
    if (stm_win_prob > 0.999f) stm_win_prob = 0.999f;

    float centipawns = -400.0f * logf((1.0f / stm_win_prob) - 1.0f);

    // convert absolute centipawns to relative centipawns
    if (stm == 1) {
        centipawns = -centipawns;
    }

    return (int)roundf(centipawns);
}