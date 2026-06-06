### Intro

This document will eventually contain an explanation of the code/how to use it, I will get around to it soon.


Mark_1 (quiescent search, basic move ordering, material only evaluation function)

...      CharlesEngine playing White: 18 - 4 - 3  [0.780] 25
...      CharlesEngine playing Black: 12 - 10 - 3  [0.540] 25
...      White vs Black: 28 - 16 - 6  [0.620] 50
Elo difference: 115.2 +/- 97.8, LOS: 99.2 %, DrawRatio: 12.0 %
SPRT: llr 0 (0.0%), lbound -inf, ubound inf

Player: CharlesEngine
   "Draw by 3-fold repetition": 4
   "Draw by insufficient mating material": 1
   "Draw by stalemate": 1
   "Loss: Black mates": 4
   "Loss: White mates": 10
   "Win: Black loses on time": 1
   "Win: Black mates": 11
   "Win: White loses on time": 1
   "Win: White mates": 17
Player: Stockfish
   "Draw by 3-fold repetition": 4
   "Draw by insufficient mating material": 1
   "Draw by stalemate": 1
   "Loss: Black loses on time": 1
   "Loss: Black mates": 11
   "Loss: White loses on time": 1
   "Loss: White mates": 17
   "Win: Black mates": 4
   "Win: White mates": 10
Finished match

Mark_2 (quiescent search, basic move ordering, material & piece square tables evaluation function)

The following is performance against stockfish skill level 4 and 10 seconds + 0.1 time control

...      Mark2 playing White: 8 - 15 - 2  [0.360] 25
...      Mark2 playing Black: 9 - 16 - 0  [0.360] 25
...      White vs Black: 24 - 24 - 2  [0.500] 50
Elo difference: -100.0 +/- 101.4, LOS: 2.2 %, DrawRatio: 4.0 %
SPRT: llr 0 (0.0%), lbound -inf, ubound inf

Player: Mark2
   "Draw by 3-fold repetition": 2
   "Loss: Black mates": 15
   "Loss: White mates": 16
   "Win: Black mates": 9
   "Win: White mates": 8
Player: Stockfish
   "Draw by 3-fold repetition": 2
   "Loss: Black mates": 9
   "Loss: White mates": 8
   "Win: Black mates": 15
   "Win: White mates": 16
Finished match

Mark_3 (quiescent search, basic move ordering, material & multiple piece square tables for opening/endgame evaluation function)

The following is performance against stockfish skill level 4 and 10 seconds + 0.1 time control

...      Mark3 playing White: 13 - 10 - 2  [0.560] 25
...      Mark3 playing Black: 10 - 12 - 3  [0.460] 25
...      White vs Black: 25 - 20 - 5  [0.550] 50
Elo difference: 6.9 +/- 93.4, LOS: 55.9 %, DrawRatio: 10.0 %
SPRT: llr 0 (0.0%), lbound -inf, ubound inf

Player: Mark3
   "Draw by 3-fold repetition": 4
   "Draw by insufficient mating material": 1
   "Loss: Black mates": 10
   "Loss: White mates": 12
   "Win: Black mates": 10
   "Win: White mates": 13
Player: Stockfish
   "Draw by 3-fold repetition": 4
   "Draw by insufficient mating material": 1
   "Loss: Black mates": 10
   "Loss: White mates": 13
   "Win: Black mates": 10
   "Win: White mates": 12
Finished match


Mark_4 (quiescent search, basic move ordering, material & multiple piece square tables for opening/endgame evaluation function, small opening book)