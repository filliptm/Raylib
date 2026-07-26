#ifndef COMBAT_H
#define COMBAT_H

#include "types.h"
#include "common.h"
#include "card.h"
#include "player.h"

// Combat functions
void AttackWithCard(GameState* game, Card* attacker, Card* target);
void AttackPlayer(GameState* game, Card* attacker, Player* target);
void DealDamage(void* target, int damage, Card* source, GameState* game);
void DealDamageToCard(Card* target, int damage, Card* source, GameState* game);
void DealDamageToPlayer(Player* target, int damage, Card* source, GameState* game);

// Combat validation
bool CanAttack(Card* attacker);
bool IsValidTarget(Card* attacker, void* target, GameState* game);
bool HasTauntMinions(Player* player);
Card* GetTauntMinion(Player* player);

// Card death handling
void ProcessCardDeath(Card* card, Player* owner, GameState* game);
void TriggerDeathrattle(Card* card, GameState* game);

// Keyword mechanics
void ProcessPoisonous(Card* attacker, Card* target);
void ProcessLifesteal(Card* attacker, int damage, GameState* game);
void ProcessDivineShield(Card* target, int* damage);
void ProcessWindfury(Card* card);

// Spell mechanics
void CastSpell(GameState* game, Card* spell, void* target);
void ApplySpellDamage(GameState* game, Card* spell, void* target);
void ApplyHealing(GameState* game, int amount, void* target);

#endif // COMBAT_H