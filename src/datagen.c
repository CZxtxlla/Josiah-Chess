#include "../include/datagen.h"
#include "../include/uci.h"
#include "../include/search.h"
#include "../include/zobrist.h"


#define _POSIX_C_SOURCE 200809L

char** load_openings_from_file(const char* filename, int* num_openings) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("CRITICAL ERROR: Failed to open %s\n", filename);
        *num_openings = 0;
        return NULL;
    }

    int capacity = 10000;
    char** fens = malloc(capacity * sizeof(char*));
    *num_openings = 0;
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // Strip the newline character from the end
        line[strcspn(line, "\r\n")] = 0; 
        
        if (strlen(line) > 5) { // Basic check to ignore empty lines
            if (*num_openings >= capacity) {
                capacity *= 2;
                fens = realloc(fens, capacity * sizeof(char*));
            }
            fens[*num_openings] = strdup(line);
            (*num_openings)++;
        }
    }
    
    fclose(file);
    return fens;
}

void extract_features(Position* pos, TrainingData* data) {
    data->num_features = 0;
    
    // iterate through all 12 piece types 
    for (int pc = P; pc <= k; pc++) {
        
        // Grab the bitboard for this specific piece type
        U64 bitboard = pos->pieces[pc];
        
        while (bitboard) {
            // Get the square index of the lowest bit
            int sq = __builtin_ctzll(bitboard);
        
            int feature_idx = (pc * 64) + sq;
            data->features[data->num_features] = feature_idx;
            data->num_features++;

            bitboard &= bitboard - 1; // clear lowest bit
        }
    }
    
    // pad the remaining slots with 65535
    for (int i = data->num_features; i < 32; i++) {
        data->features[i] = 65535;
    }

    data->stm = pos->side;
}


void run_datagen(char* command) {
    int games_to_play = 1000;
    char filename[256] = "training_data.bin";
    
    // Parse command: "datagen 5000 output.bin"
    if (sscanf(command, "datagen %d %255s", &games_to_play, filename) < 1) {
        printf("Usage: datagen <num_games> [output_file]\n");
        return;
    }
    
    // load openings into memory
    int num_openings;
    char openings_path[PATH_MAX];
    get_resource_path("UHO_4060_v3.epd", openings_path, sizeof(openings_path));
    char** opening_fens = load_openings_from_file(openings_path, &num_openings);

    if (opening_fens == NULL || num_openings <= 0) {
        opening_fens = load_openings_from_file("UHO_4060_v3.epd", &num_openings);
        snprintf(openings_path, sizeof(openings_path), "%s", "UHO_4060_v3.epd");
    }

    if (num_openings <= 0) {
        printf("Error: No opening positions were loaded from %s\n", openings_path);
        if (opening_fens) free(opening_fens);
        return;
    }
    
    FILE* output_file = fopen(filename, "wb");
    if (!output_file) {
        printf("Error: Failed to open %s for writing\n", filename);
        return;
    }

    unsigned int seed = time(NULL); 
    
    printf("Starting Datagen: Generating %d games to %s...\n", games_to_play, filename);
    
    // game loop
    for (int i = 0; i < games_to_play; i++) {
        // pick random opening fen
        int random_index = rand_r(&seed) % num_openings;
        char* starting_fen = opening_fens[random_index];

        clear_tt();

        // meat and potatoes
        play_datagen_game(starting_fen, output_file); 
        
        // Progress tracker
        if ((i + 1) % 50 == 0) {
            printf("Progress: %d / %d\n", i + 1, games_to_play);
        }
    }
    
    fclose(output_file);
    printf("Datagen finished successfully!\n");
    
    // Cleanup
    for (int i = 0; i < num_openings; i++) {
        free(opening_fens[i]);
    }
    free(opening_fens);
}