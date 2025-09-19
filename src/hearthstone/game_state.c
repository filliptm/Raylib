#include "game_state.h"
#include "combat.h"
#include <string.h>
#include <time.h>
#include <stdlib.h>

// Initialize the game state
void InitializeGame(GameState* game) {
    memset(game, 0, sizeof(GameState));
    
    // Set random seed
    srand(time(NULL));
    game->randomSeed = rand();
    
    // Initialize game state
    game->gamePhase = GAME_STATE_PLAYING;
    game->turnPhase = TURN_PHASE_START;
    game->activePlayer = 0;
    game->turnNumber = 1;
    game->selectedCard = NULL;
    game->targetCard = NULL;
    game->targetingMode = false;
    game->combatInProgress = false;
    game->activeEffectsCount = 0;
    game->queueCount = 0;
    game->turnTimer = 0.0f;
    game->winner = -1;
    game->gameEnded = false;
    
    // Initialize camera
    InitializeCamera(game);
    
    // Initialize players
    InitializePlayer(&game->players[0], 0, "Player 1");
    InitializePlayer(&game->players[1], 1, "Player 2");
    
    // Initialize decks
    InitializePlayerDeck(&game->players[0]);
    InitializePlayerDeck(&game->players[1]);
    
    // Draw starting hands (3 cards each)
    for (int p = 0; p < 2; p++) {
        for (int i = 0; i < 3; i++) {
            DrawCardFromDeck(&game->players[p]);
        }
    }
    
    // Set first player as active
    game->players[0].isActivePlayer = true;
    
    // Start first turn
    StartTurn(game);
}

// Update the game state
void UpdateGame(GameState* game) {
    float deltaTime = GetFrameTime();
    
    if (game->gameEnded) return;
    
    // Check win conditions
    CheckWinConditions(game);
    
    // Update camera shake
    if (game->cameraShake > 0) {
        game->cameraShake -= deltaTime * 3.0f;
        if (game->cameraShake < 0) game->cameraShake = 0;
    }
    
    // Update turn timer
    game->turnTimer += deltaTime;
    
    // Update all cards
    for (int p = 0; p < 2; p++) {
        Player* player = &game->players[p];
        
        // Update hand cards
        for (int i = 0; i < player->handCount; i++) {
            UpdateCard(&player->hand[i], deltaTime);
        }
        
        // Update board cards
        for (int i = 0; i < player->boardCount; i++) {
            UpdateCard(&player->board[i], deltaTime);
        }
    }
    
    // Update visual effects
    UpdateEffects(game, deltaTime);
    
    // Process action queue if any
    ProcessActionQueue(game);
}

// Cleanup game resources
void CleanupGame(GameState* game) {
    // Currently nothing to cleanup as we're not using dynamic memory
    // This function is here for future use if needed
}

// Start a new turn
void StartTurn(GameState* game) {
    Player* player = &game->players[game->activePlayer];
    
    game->turnPhase = TURN_PHASE_START;
    
    // Draw card
    DrawCardFromDeck(player);
    
    // Refresh mana
    RefreshMana(player);
    
    // Reset hero power
    player->heroPowerUsed = false;
    
    // Reset minion attack availability
    for (int i = 0; i < player->boardCount; i++) {
        ResetCardCombatState(&player->board[i]);
    }
    
    // Process turn start effects
    ProcessTurnStart(game, player);
    
    game->turnPhase = TURN_PHASE_MAIN;
    game->turnTimer = 0.0f;
    
    // Add turn start visual effect
    AddVisualEffect(game, EFFECT_TURN_START, (Vector3){0, 2, 0}, 
                   TextFormat("%s's Turn", player->name));
}

// End the current turn
void EndTurn(GameState* game) {
    Player* player = &game->players[game->activePlayer];
    
    game->turnPhase = TURN_PHASE_END;
    
    // Process turn end effects
    ProcessTurnEnd(game, player);
    
    // Switch active player
    game->players[game->activePlayer].isActivePlayer = false;
    game->activePlayer = 1 - game->activePlayer;
    game->players[game->activePlayer].isActivePlayer = true;
    
    game->turnNumber++;
    
    // Start next player's turn
    StartTurn(game);
}

// Process effects at turn start
void ProcessTurnStart(GameState* game, Player* player) {
    // This is where we would process turn start effects
    // Currently empty but ready for expansion
}

// Process effects at turn end
void ProcessTurnEnd(GameState* game, Player* player) {
    // This is where we would process turn end effects
    // Currently empty but ready for expansion
}

// Check if either player has won
void CheckWinConditions(GameState* game) {
    for (int i = 0; i < 2; i++) {
        if (!game->players[i].isAlive || game->players[i].health <= 0) {
            game->gameEnded = true;
            game->winner = 1 - i;
            return;
        }
    }
}

// Set the winner of the game
void SetWinner(GameState* game, int playerId) {
    game->gameEnded = true;
    game->winner = playerId;
}

// Check if the game is over
bool IsGameOver(GameState* game) {
    return game->gameEnded;
}

// Queue an action to be processed
void QueueAction(GameState* game, ActionType type, int playerId, int cardId, int targetId, int value) {
    if (game->queueCount >= 50) return;
    
    GameAction* action = &game->actionQueue[game->queueCount];
    action->type = type;
    action->playerId = playerId;
    action->cardId = cardId;
    action->targetId = targetId;
    action->value = value;
    
    game->queueCount++;
}

// Process all queued actions
void ProcessActionQueue(GameState* game) {
    while (game->queueCount > 0) {
        GameAction action = game->actionQueue[0];
        
        // Shift remaining actions
        for (int i = 0; i < game->queueCount - 1; i++) {
            game->actionQueue[i] = game->actionQueue[i + 1];
        }
        game->queueCount--;
        
        // Process the action based on type
        switch (action.type) {
            case ACTION_PLAY_CARD:
                // Handle in input module
                break;
            case ACTION_ATTACK:
                // Handle in combat module
                break;
            case ACTION_END_TURN:
                EndTurn(game);
                break;
            case ACTION_CONCEDE:
                SetWinner(game, 1 - action.playerId);
                break;
            default:
                break;
        }
    }
}

// Initialize the camera
void InitializeCamera(GameState* game) {
    game->camera.position = (Vector3){0.0f, 20.0f, 8.0f};
    game->camera.target = (Vector3){0.0f, 0.0f, 0.0f};
    game->camera.up = (Vector3){0.0f, 0.0f, -1.0f};
    game->camera.fovy = 45.0f;
    game->camera.projection = CAMERA_PERSPECTIVE;
    game->boardCenter = (Vector3){0.0f, 0.0f, 0.0f};
    game->cameraShake = 0.0f;
}

// Update camera (with shake effect)
void UpdateGameCamera(GameState* game, float deltaTime) {
    if (game->cameraShake > 0) {
        float shakeX = (rand() % 100 - 50) / 100.0f * game->cameraShake;
        float shakeZ = (rand() % 100 - 50) / 100.0f * game->cameraShake;
        
        game->camera.position.x = shakeX;
        game->camera.position.z = 8.0f + shakeZ;
    } else {
        game->camera.position.x = 0.0f;
        game->camera.position.z = 8.0f;
    }
}

// Apply camera shake effect
void ShakeCamera(GameState* game, float intensity) {
    game->cameraShake = intensity;
}