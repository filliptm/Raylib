#ifndef GAME_POLISH_H
#define GAME_POLISH_H

#include "game_state.h"
#include "polish.h"

// Polish integration functions
void InitializeGamePolish(GameState* game);
void UpdateGamePolishSystems(GameState* game, float deltaTime);
void DrawGamePolishSystems(const GameState* game);
void CleanupGamePolish(GameState* game);

// Polish event handlers
void OnCardPlayed(GameState* game, Card* card);
void OnAttack(GameState* game, Card* attacker, void* target);
void OnDamage(GameState* game, void* target, int damage);
void OnTurnStart(GameState* game);

#endif // GAME_POLISH_H