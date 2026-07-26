#include "game_ai.h"
#include "effects.h"
#include <stdlib.h>

// Initialize AI for the game
void InitializeGameAI(GameState* game, int aiDifficulty) {
    game->vsAI = true;
    game->aiPlayer = malloc(sizeof(AIPlayer));

    if (game->aiPlayer) {
        InitializeAI((AIPlayer*)game->aiPlayer, 1, (AIDifficulty)aiDifficulty);
    }
}

// Update AI during game loop
void UpdateGameAI(GameState* game, float deltaTime) {
    if (game->vsAI && game->aiPlayer) {
        UpdateAI(game, (AIPlayer*)game->aiPlayer, deltaTime);
    }
}

// Cleanup AI memory
void CleanupGameAI(GameState* game) {
    if (game->aiPlayer) {
        free(game->aiPlayer);
        game->aiPlayer = NULL;
    }
}

// Reset AI turn state
void ResetGameAITurn(GameState* game) {
    if (game->vsAI && game->aiPlayer) {
        AIPlayer* ai = (AIPlayer*)game->aiPlayer;
        if (game->activePlayer == ai->playerId) {
            ResetAITurn(ai);

            // Create AI turn effect to show it's the AI's turn
            Vector3 aiPosition = {0, 2, -3}; // Center of screen, elevated
            CreateAITurnEffect(game, aiPosition);
        }
    }
}