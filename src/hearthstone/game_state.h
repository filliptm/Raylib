#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "types.h"
#include "common.h"
#include "player.h"
#include "card.h"
#include "effects.h"

// Forward declarations to avoid circular dependency
struct AIPlayer;
struct NetworkSystem;
struct PolishSystem;

// Game action structure
typedef struct {
    ActionType type;
    int playerId;
    int cardId;
    int targetId;
    int value;
} GameAction;

// Main game state structure
struct GameState {
    GamePhase gamePhase;
    TurnPhase turnPhase;

    Player players[2];
    int activePlayer;
    int turnNumber;

    // Game state
    Card* selectedCard;
    Card* targetCard;
    bool targetingMode;
    bool combatInProgress;

    // Visual effects
    VisualEffect effects[MAX_EFFECTS];
    int activeEffectsCount;

    // Action queue
    GameAction actionQueue[50];
    int queueCount;

    // Random seed
    unsigned int randomSeed;

    // Camera and rendering
    Camera3D camera;
    Vector3 boardCenter;
    float cameraShake;
    float turnTimer;

    // Game result
    int winner;
    bool gameEnded;

    // AI system
    struct AIPlayer* aiPlayer;
    bool vsAI;

    // Network system
    struct NetworkSystem* networkSystem;
    bool isNetworkGame;

    // Polish system
    struct PolishSystem* polishSystem;
};

// Game state management
void InitializeGame(GameState* game);
void InitializeGameWithAI(GameState* game, int aiDifficulty);
void InitializeGameAsServer(GameState* game, int port);
void InitializeGameAsClient(GameState* game, const char* address, int port);
void UpdateGame(GameState* game);
void CleanupGame(GameState* game);

// Turn management
void StartTurn(GameState* game);
void EndTurn(GameState* game);
void ProcessTurnStart(GameState* game, Player* player);
void ProcessTurnEnd(GameState* game, Player* player);

// Game flow
void CheckWinConditions(GameState* game);
void SetWinner(GameState* game, int playerId);
bool IsGameOver(GameState* game);

// Action queue
void QueueAction(GameState* game, ActionType type, int playerId, int cardId, int targetId, int value);
void ProcessActionQueue(GameState* game);

// Camera management
void InitializeCamera(GameState* game);
void ShakeCamera(GameState* game, float intensity);

#endif // GAME_STATE_H