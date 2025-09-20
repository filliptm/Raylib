#ifndef AI_H
#define AI_H

#include "types.h"
#include "common.h"
#include "game_state.h"
#include "player.h"
#include "card.h"

// AI difficulty levels
typedef enum {
    AI_DIFFICULTY_EASY,
    AI_DIFFICULTY_MEDIUM,
    AI_DIFFICULTY_HARD
} AIDifficulty;

// AI action types
typedef enum {
    AI_ACTION_NONE,
    AI_ACTION_PLAY_CARD,
    AI_ACTION_ATTACK_MINION,
    AI_ACTION_ATTACK_PLAYER,
    AI_ACTION_USE_HERO_POWER,
    AI_ACTION_END_TURN
} AIActionType;

// AI action structure
typedef struct {
    AIActionType type;
    int cardIndex;      // Index in hand or board
    int targetIndex;    // Target index (for attacks/spells)
    bool targetIsPlayer; // True if targeting a player
    float score;        // Action value score
    Card* sourceCard;   // Source card for action
    void* target;       // Target for action
} AIAction;

// AI player structure
typedef struct {
    AIDifficulty difficulty;
    int playerId;

    // AI personality weights
    float aggressionWeight;     // How much AI values face damage
    float boardControlWeight;   // How much AI values board presence
    float valueWeight;          // How much AI values card efficiency
    float survivalWeight;       // How much AI values staying alive

    // Decision parameters
    float thinkTime;           // Time AI takes to make decisions
    float mistakeChance;       // Chance of making suboptimal plays
    int maxDepth;              // Lookahead depth for evaluation

    // Current turn state
    AIAction plannedActions[20]; // Queue of actions to execute
    int actionCount;
    float turnTimer;
} AIPlayer;

// AI initialization and management
void InitializeAI(AIPlayer* ai, int playerId, AIDifficulty difficulty);
void UpdateAI(GameState* game, AIPlayer* ai, float deltaTime);
void ResetAITurn(AIPlayer* ai);

// AI decision making
AIAction CalculateBestAction(GameState* game, AIPlayer* ai);
void ExecuteAIAction(GameState* game, AIPlayer* ai, AIAction action);

// Action evaluation
float EvaluatePlayCard(GameState* game, AIPlayer* ai, int handIndex, void* target);
float EvaluateAttack(GameState* game, AIPlayer* ai, Card* attacker, void* target);
float EvaluateHeroPower(GameState* game, AIPlayer* ai, void* target);
float EvaluateEndTurn(GameState* game, AIPlayer* ai);

// Board state evaluation
float EvaluateBoardState(GameState* game, int playerId);
float EvaluateCard(Card* card, GameState* game);
float EvaluateTrade(Card* attacker, Card* defender);

// Targeting
void* FindBestTarget(GameState* game, AIPlayer* ai, Card* card);
void* FindBestAttackTarget(GameState* game, AIPlayer* ai, Card* attacker);
Player* GetEnemyPlayer(GameState* game, int playerId);

// AI utility functions
bool ShouldPlayCard(GameState* game, AIPlayer* ai, Card* card);
bool ShouldAttack(GameState* game, AIPlayer* ai, Card* card);
bool ShouldUseHeroPower(GameState* game, AIPlayer* ai);
bool ShouldEndTurn(GameState* game, AIPlayer* ai);

// Difficulty scaling
void ApplyDifficultyModifiers(AIPlayer* ai);
bool ShouldMakeMistake(AIPlayer* ai);

#endif // AI_H