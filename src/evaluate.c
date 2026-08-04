#include "../include/evaluate.h"
#include "../include/position.h"
#include "../include/bitboard.h"
#include "../nnue/inference.h"

// ------ Piece Square Tables ------
// (I copied these from my previous chess bot program, also can be found here: 
// https://www.chessprogramming.org/Simplified_Evaluation_Function)

const int pawn_table[64] = {
    0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
    5,  5, 10, 25, 25, 10,  5,  5,
    0,  0,  0, 20, 20,  0,  0,  0,
    5, -5,-10,  0,  0,-10, -5,  5,
    5, 10, 10,-20,-20, 10, 10,  5,
    0,  0,  0,  0,  0,  0,  0,  0

};

const int knight_table[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50
};

const int bishop_table[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20
};

const int rook_table[64] = {
    0,  0,  0,  0,  0,  0,  0,  0,
    5, 10, 10, 10, 10, 10, 10,  5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    0,  0,  0,  5,  5,  0,  0,  0
};

const int queen_table[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
    -5,  0,  5,  5,  5,  5,  0, -5,
    0,  0,  5,  5,  5,  5,  0, -5,
    -10,  5,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20
};

const int king_table[64] = {
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -10,-20,-20,-20,-20,-20,-20,-10,
    20, 20,  0,  0,  0,  0, 20, 20,
    20, 30, 10,  0,  0, 10, 30, 20
};

// endgame tables
const int pawn_eg_table[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
     80,  80,  80,  80,  80,  80,  80,  80,
     50,  50,  50,  50,  50,  50,  50,  50,
     30,  30,  30,  30,  30,  30,  30,  30,
     20,  20,  20,  20,  20,  20,  20,  20,
     10,  10,  10,  10,  10,  10,  10,  10,
     10,  10,  10,  10,  10,  10,  10,  10,
      0,   0,   0,   0,   0,   0,   0,   0
};

const int king_eg_table[64] = {
    -50,-40,-30,-20,-20,-30,-40,-50,
    -30,-20,-10,  0,  0,-10,-20,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-30,  0,  0,  0,  0,-30,-30,
    -50,-30,-30,-30,-30,-30,-30,-50
};

const int* mg_table_pointers[6] = {pawn_table, knight_table, bishop_table, rook_table, queen_table, king_table};
const int* eg_table_pointers[6] = {pawn_eg_table, knight_table, bishop_table, rook_table, queen_table, king_eg_table};

// Calculates the current phase of the game (24 = Midgame, 0 = Endgame)
int get_game_phase(Position* pos) {
    int phase = 0;

    // knights bishop have no multiplier, rook has times 2 queen has times 4
    phase += __builtin_popcountll(pos->pieces[N] | pos->pieces[n]) * 1;
    phase += __builtin_popcountll(pos->pieces[B] | pos->pieces[b]) * 1;
    phase += __builtin_popcountll(pos->pieces[R] | pos->pieces[r]) * 2;
    phase += __builtin_popcountll(pos->pieces[Q] | pos->pieces[q]) * 4;

    if (phase > 24) {
        phase = 24;
    }

    return phase;
}


// Piece values in centipawns
const int piece_values[12] = {
    100, 300, 300, 500, 900, 10000, // White: P, N, B, R, Q, K
    100, 300, 300, 500, 900, 10000  // Black: p, n, b, r, q, k
};

int evaluate(Position* pos) {
    
    // get white-absolute score value for a given position
    
    int phase = get_game_phase(pos);

    int score = 0;
    for (int piece_type = P; piece_type <= k; piece_type++) {
        U64 temp = pos->pieces[piece_type];

        while (temp) {
            int square = __builtin_ctzll(temp);

            pop_bit(temp, square);

            int is_white = (piece_type <= K);
            int type = piece_type % 6; // get white variant of the type

            int material = piece_values[piece_type];

            int flipped_index = is_white ? (square ^ 56) : square; // ^ 56 flips the index's rank but keeps the same file

            int mg_position_score = mg_table_pointers[type][flipped_index];
            int eg_position_score = eg_table_pointers[type][flipped_index];

            int position = (mg_position_score * phase + (24 - phase) * eg_position_score) / 24;

            if (is_white) {
                score += (material + position);
            } else {
                score -= (material + position);
            }
        } 
    }
    // positional bonuses
    int positional_bonus = 0;

    // bishop pair bonus
    if (__builtin_popcountll(pos->pieces[B]) >= 2) positional_bonus += 40;
    if (__builtin_popcountll(pos->pieces[b]) >= 2) positional_bonus -= 40;

    // rook on 7th rank
    positional_bonus += __builtin_popcountll(pos->pieces[R] & 0x00FF000000000000ULL) * 20;
    positional_bonus -= __builtin_popcountll(pos->pieces[r] & 0x000000000000FF00ULL) * 20;

    U64 file_A = 0x0101010101010101ULL; 
    
    for (int file = 0; file < 8; file++) {
        U64 file_mask = file_A << file;
        
        // check for pawns
        int white_pawns = (pos->pieces[P] & file_mask) != 0;
        int black_pawns = (pos->pieces[p] & file_mask) != 0;
        
        // white rooks
        if ((pos->pieces[R] & file_mask) != 0) {
            if (!white_pawns && !black_pawns) {
                positional_bonus += 25; // fully open file (no pawns)
            } else if (!white_pawns) {
                positional_bonus += 15; // half open file (only enemy pawns)
            }
        }
        
        // black rooks
        if ((pos->pieces[r] & file_mask) != 0) {
            if (!white_pawns && !black_pawns) {
                positional_bonus -= 25; //full open
            } else if (!black_pawns) {
                positional_bonus -= 15; // half open
            }
        }
    }

    score += positional_bonus;
    return (pos->side == WHITE) ? score : -score;

    /*
    int nnue_score = evaluate_nnue_quantized(pos, model);
    return nnue_score;

    int hc_score = (pos->side == WHITE) ? score : -score; // flip the score if from the perspective of black

    if (abs(hc_score) < 500) {
        return (nnue_score + hc_score) >> 1; 
    } 
    
    // If a piece is hung use more hc evaluation to ensure it its taken
    else {
        return (hc_score * 3 + nnue_score) >> 2;
    }
    */
    
}