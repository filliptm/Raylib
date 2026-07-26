#ifndef GAME_AI_H
#define GAME_AI_H

#include "game_state.h"
#include "ai.h"

// Helper functions to avoid circular dependency
void InitializeGameAI(GameState* game, int aiDifficulty);
void UpdateGameAI(GameState* game, float deltaTime);
void CleanupGameAI(GameState* game);
void ResetGameAITurn(GameState* game);

#endif // GAME_AI_H