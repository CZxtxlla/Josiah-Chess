#ifndef EVALUATE_H
#define EVALUATE_H

#include "position.h"

int evaluate(Position* pos);

// Shared piece values array (defined in evaluate.c)
extern const int piece_values[12];


#endif