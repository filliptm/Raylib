#include "game_random.h"

void GameRandomSeed(GameRandom *random, unsigned int seed)
{
    random->state = seed ? seed : 0x9E3779B9u;
}

unsigned int GameRandomNext(GameRandom *random)
{
    unsigned int value = random->state;
    if (!value) value = 0x9E3779B9u;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    random->state = value;
    return value;
}

int GameRandomInt(GameRandom *random, int minimum, int maximum)
{
    if (maximum < minimum)
    {
        int swap = minimum;
        minimum = maximum;
        maximum = swap;
    }
    unsigned int span = (unsigned int)(maximum - minimum) + 1u;
    return minimum + (int)(GameRandomNext(random)%span);
}
