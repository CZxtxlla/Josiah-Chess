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


void* datagen_worker(void* arg) {
    DatagenArgs* args = (DatagenArgs*)arg;
    
    // local setup
    char filename[256];
    sprintf(filename, "training_data_thread_%d.bin", args->thread_id);
    FILE* output_file = fopen(filename, "wb");
    if (!output_file) {
        printf("CRITICAL ERROR: Failed to open %s for writing\n", filename);
        free(args);
        return NULL;
    }
    
    unsigned int seed = time(NULL) ^ args->thread_id; 
    
    printf("Thread %d starting: generating %d games...\n", args->thread_id, args->games_to_play);
    
    // game loop
    for (int i = 0; i < args->games_to_play; i++) {
        // pick a random opening fen 
        int random_index = rand_r(&seed) % args->num_openings;
        char* starting_fen = args->opening_fens[random_index];

        // meat and potatoes
        play_datagen_game(starting_fen, output_file); 
        
        // Progress tracker
        if ((i + 1) % 20 == 0) {
            printf("Thread %d progress: %d / %d\n", args->thread_id, i + 1, args->games_to_play);
        }
    }
    
    fclose(output_file);
    printf("Thread %d finished!\n", args->thread_id);
    
    free(args);
    return NULL;
}


void run_datagen(char* command) {
    int num_threads = 1;
    int games_per_thread = 1000;
    
    // parse command: "datagen 8 5000" (8 threads, 5000 games each)
    sscanf(command, "datagen %d %d", &num_threads, &games_per_thread);
    
    // load openings into mem
    int num_openings;
    char openings_path[PATH_MAX];
    get_resource_path("UHO_4060_v3.epd", openings_path, sizeof(openings_path));
    char** opening_fens = load_openings_from_file(openings_path, &num_openings);

    if (opening_fens == NULL || num_openings <= 0) {
        opening_fens = load_openings_from_file("UHO_4060_v3.epd", &num_openings);
        snprintf(openings_path, sizeof(openings_path), "%s", "UHO_4060_v3.epd");
    }

    if (num_openings <= 0) {
        printf("CRITICAL ERROR: No opening positions were loaded from %s\n", openings_path);
        free(opening_fens);
        return;
    }
    
    printf("Starting Datagen: %d Threads, %d Games/Thread...\n", num_threads, games_per_thread);
    
    // Array to hold the thread handles
    pthread_t* threads = malloc(num_threads * sizeof(pthread_t));
    
    // spwan threads
    for (int i = 0; i < num_threads; i++) {
        DatagenArgs* args = malloc(sizeof(DatagenArgs));
        args->thread_id = i;
        args->games_to_play = games_per_thread;
        args->opening_fens = opening_fens;
        args->num_openings = num_openings;
        
        // create the thread and tell it to run 'datagen_worker'
        if (pthread_create(&threads[i], NULL, datagen_worker, args) != 0) {
            printf("Failed to create thread %d\n", i);
        }
    }
    
    // wait until all threads done
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("All threads finished generating data\n");
    
    free(threads);
    free(opening_fens);
}