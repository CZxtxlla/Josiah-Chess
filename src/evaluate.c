#include "../include/evaluate.h"
#include "../include/position.h"
#include "../include/bitboard.h"

// Piece values in centipawns
const int piece_values[12] = {
    100, 300, 300, 500, 900, 10000, // White: P, N, B, R, Q, K
    100, 300, 300, 500, 900, 10000  // Black: p, n, b, r, q, k
};

int evaluate(Position* pos) {
    // get score value based on total piece value difference between black and white
    int score = 0;;
    score += __builtin_popcountll(pos->pieces[P]) * piece_values[P];
    score += __builtin_popcountll(pos->pieces[N]) * piece_values[N];
    score += __builtin_popcountll(pos->pieces[B]) * piece_values[B];
    score += __builtin_popcountll(pos->pieces[R]) * piece_values[R];
    score += __builtin_popcountll(pos->pieces[Q]) * piece_values[Q];

    score -= __builtin_popcountll(pos->pieces[p]) * piece_values[p];
    score -= __builtin_popcountll(pos->pieces[n]) * piece_values[n];
    score -= __builtin_popcountll(pos->pieces[b]) * piece_values[b];
    score -= __builtin_popcountll(pos->pieces[r]) * piece_values[r];
    score -= __builtin_popcountll(pos->pieces[q]) * piece_values[q];

    return (pos->side == WHITE) ? score : -score; // flip the score if from the perspective of black
}