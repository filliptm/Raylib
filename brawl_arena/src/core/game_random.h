#ifndef BRAWL_GAME_RANDOM_H
#define BRAWL_GAME_RANDOM_H

#include "game_types.h"

void GameRandomSeed(GameRandom *random, unsigned int seed);
unsigned int GameRandomNext(GameRandom *random);
int GameRandomInt(GameRandom *random, int minimum, int maximum);

#endif
