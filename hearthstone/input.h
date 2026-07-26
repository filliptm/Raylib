#ifndef INPUT_H
#define INPUT_H

#include "types.h"
#include "common.h"
#include "raylib.h"

// Input handling functions
void HandleInput(GameState* game);
void HandleMouseInput(GameState* game);
void HandleKeyboardInput(GameState* game);

// Card interaction
void HandleCardSelection(GameState* game, Card* card);
void HandleCardDrag(GameState* game);
void HandleCardDrop(GameState* game);
void HandleCardHover(GameState* game, Ray mouseRay);

// Action handling
void HandlePlayCard(GameState* game, Card* card, void* target);
void HandleAttack(GameState* game, Card* attacker, void* target);
void HandleEndTurn(GameState* game);
void HandleConcede(GameState* game);

// Targeting system
void StartTargeting(GameState* game, Card* card);
void UpdateTargeting(GameState* game, Ray mouseRay);
void EndTargeting(GameState* game);
bool IsValidPlayTarget(Card* card, void* target);

// Helper functions
Card* GetCardUnderMouse(GameState* game, Ray mouseRay);
Player* GetPlayerUnderMouse(GameState* game, Ray mouseRay);
void* GetTargetUnderMouse(GameState* game, Ray mouseRay);
bool IsPlayerTurn(GameState* game, int playerId);
bool CheckPlayerPortraitHit(Player* player, Ray ray);

#endif // INPUT_H