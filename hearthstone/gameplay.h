#ifndef GAMEPLAY_H
#define GAMEPLAY_H

#include "types.h"
#include "common.h"
#include "game_state.h"
#include "player.h"
#include "card.h"

// High-level gameplay functions
bool PlayCard(GameState* game, Player* player, int handIndex, void* target);
bool AttackWithMinion(GameState* game, Player* player, int boardIndex, void* target);
bool UseHeroPower(GameState* game, Player* player, void* target);
void EndPlayerTurn(GameState* game);

// Card playing validation and execution
bool CanPlayCardAtIndex(Player* player, int handIndex);
bool PlayMinionCard(GameState* game, Player* player, Card* card);
bool PlaySpellCard(GameState* game, Player* player, Card* card, void* target);
bool PlayWeaponCard(GameState* game, Player* player, Card* card);

// Turn mechanics
void StartPlayerTurn(GameState* game, Player* player);
void ProcessTurnEvents(GameState* game);

// Battlecry system
void TriggerBattlecry(GameState* game, Card* card, void* target);

// Game state validation
bool IsValidGameAction(GameState* game, Player* player, ActionType action);
void ValidateGameState(GameState* game);

#endif // GAMEPLAY_H