#include "combat.h"
#include "game_state.h"
#include "effects.h"
#include <stddef.h>

// Attack with a minion
void AttackWithCard(GameState* game, Card* attacker, Card* target) {
    if (!CanAttack(attacker)) return;
    
    AddVisualEffect(game, EFFECT_ATTACK, attacker->position, "Attack!");
    
    // Deal damage to each other
    if (target) {
        DealDamageToCard(target, attacker->attack, attacker, game);
        DealDamageToCard(attacker, target->attack, target, game);
    }
    
    // Mark as attacked
    attacker->attackedThisTurn = true;
    attacker->canAttack = false;
    
    // Process windfury
    ProcessWindfury(attacker);
}

// Attack a player directly
void AttackPlayer(GameState* game, Card* attacker, Player* target) {
    if (!CanAttack(attacker)) return;
    
    // Check for taunt minions
    if (HasTauntMinions(target)) {
        return; // Can't attack player directly if taunt minions exist
    }
    
    AddVisualEffect(game, EFFECT_ATTACK, attacker->position, "Attack!");
    
    DealDamageToPlayer(target, attacker->attack, attacker, game);
    
    // Mark as attacked
    attacker->attackedThisTurn = true;
    attacker->canAttack = false;
    
    // Process windfury
    ProcessWindfury(attacker);
}

// General damage dealing function
void DealDamage(void* target, int damage, Card* source, GameState* game) {
    if (damage <= 0) return;
    
    // Try to identify target type
    Card* targetCard = NULL;
    Player* targetPlayer = NULL;
    
    // Check if target is a card on any player's board
    for (int p = 0; p < 2; p++) {
        for (int i = 0; i < game->players[p].boardCount; i++) {
            if (&game->players[p].board[i] == target) {
                targetCard = (Card*)target;
                break;
            }
        }
        if (&game->players[p] == target) {
            targetPlayer = (Player*)target;
            break;
        }
    }
    
    if (targetCard) {
        DealDamageToCard(targetCard, damage, source, game);
    } else if (targetPlayer) {
        DealDamageToPlayer(targetPlayer, damage, source, game);
    }
}

// Deal damage to a card
void DealDamageToCard(Card* target, int damage, Card* source, GameState* game) {
    if (damage <= 0) return;
    
    // Process divine shield
    ProcessDivineShield(target, &damage);
    if (damage == 0) return;
    
    target->health -= damage;
    CreateDamageEffect(game, target->position, damage);
    
    // Process poisonous
    if (source && source->poisonous && damage > 0) {
        ProcessPoisonous(source, target);
    }
    
    // Check for death
    if (target->health <= 0) {
        Player* owner = &game->players[target->ownerPlayer];
        ProcessCardDeath(target, owner, game);
    }
}

// Deal damage to a player
void DealDamageToPlayer(Player* target, int damage, Card* source, GameState* game) {
    if (damage <= 0) return;
    
    target->health -= damage;
    Vector3 playerPos = {0, 2, target->playerId == 0 ? 6 : -6};
    CreateDamageEffect(game, playerPos, damage);
    
    // Process lifesteal
    if (source && source->lifesteal) {
        ProcessLifesteal(source, damage, game);
    }
    
    // Check for death
    if (target->health <= 0) {
        target->health = 0;
        target->isAlive = false;
        SetWinner(game, 1 - target->playerId);
    }
}

// Check if a card can attack
bool CanAttack(Card* attacker) {
    return attacker && attacker->onBoard && attacker->canAttack && 
           !attacker->attackedThisTurn && attacker->attack > 0;
}

// Check if a target is valid
bool IsValidTarget(Card* attacker, void* target, GameState* game) {
    if (!attacker || !target) return false;
    
    // Try to identify what type of target this is
    Card* targetCard = NULL;
    Player* targetPlayer = NULL;
    
    // Check if target is a card on any player's board
    for (int p = 0; p < 2; p++) {
        for (int i = 0; i < game->players[p].boardCount; i++) {
            if (&game->players[p].board[i] == target) {
                targetCard = (Card*)target;
                break;
            }
        }
        if (&game->players[p] == target) {
            targetPlayer = (Player*)target;
            break;
        }
    }
    
    // If attacking a card
    if (targetCard) {
        // Can't attack your own minions
        if (targetCard->ownerPlayer == attacker->ownerPlayer) return false;
        
        // Check for taunt restrictions
        Player* enemyPlayer = &game->players[1 - attacker->ownerPlayer];
        if (HasTauntMinions(enemyPlayer)) {
            // Must attack taunt minions only
            return targetCard->taunt;
        }
        
        return true;
    }
    
    // If attacking a player
    if (targetPlayer) {
        // Can't attack your own player
        if (targetPlayer->playerId == attacker->ownerPlayer) return false;
        
        // Can't attack player directly if there are taunt minions
        if (HasTauntMinions(targetPlayer)) return false;
        
        return true;
    }
    
    return false;
}

// Check if player has taunt minions
bool HasTauntMinions(Player* player) {
    for (int i = 0; i < player->boardCount; i++) {
        if (player->board[i].taunt) {
            return true;
        }
    }
    return false;
}

// Get first taunt minion (for AI)
Card* GetTauntMinion(Player* player) {
    for (int i = 0; i < player->boardCount; i++) {
        if (player->board[i].taunt) {
            return &player->board[i];
        }
    }
    return NULL;
}

// Process card death
void ProcessCardDeath(Card* card, Player* owner, GameState* game) {
    CreateDeathEffect(game, card->position);
    
    // Trigger deathrattle
    TriggerDeathrattle(card, game);
    
    // Find and remove from board
    for (int i = 0; i < owner->boardCount; i++) {
        if (&owner->board[i] == card) {
            RemoveCardFromBoard(owner, i);
            break;
        }
    }
}

// Trigger deathrattle effect
void TriggerDeathrattle(Card* card, GameState* game) {
    if (card->hasDeathrattle) {
        ExecuteDeathrattle(game, card);
    }
}

// Process poisonous keyword
void ProcessPoisonous(Card* attacker, Card* target) {
    if (attacker->poisonous && target->health > 0) {
        target->health = 0; // Instant kill
    }
}

// Process lifesteal keyword
void ProcessLifesteal(Card* attacker, int damage, GameState* game) {
    if (!attacker->lifesteal) return;
    
    Player* owner = &game->players[attacker->ownerPlayer];
    int healAmount = damage;
    
    if (owner->health + healAmount > owner->maxHealth) {
        healAmount = owner->maxHealth - owner->health;
    }
    
    if (healAmount > 0) {
        owner->health += healAmount;
        Vector3 playerPos = {0, 2, owner->playerId == 0 ? 6 : -6};
        CreateHealEffect(game, playerPos, healAmount);
    }
}

// Process divine shield keyword
void ProcessDivineShield(Card* target, int* damage) {
    if (target->divineShield && *damage > 0) {
        target->divineShield = false;
        *damage = 0;
        // Visual effect is handled in effects module
    }
}

// Process windfury keyword
void ProcessWindfury(Card* card) {
    if (card->windfury && !card->attackedThisTurn) {
        card->canAttack = true; // Can attack again
    }
}

// Cast a spell
void CastSpell(GameState* game, Card* spell, void* target) {
    CreateSpellEffect(game, spell->position, spell->name);
    
    if (spell->spellDamage > 0) {
        ApplySpellDamage(game, spell, target);
    }
    
    if (spell->healing > 0) {
        ApplyHealing(game, spell->healing, target);
    }
}

// Apply spell damage
void ApplySpellDamage(GameState* game, Card* spell, void* target) {
    if (!target || spell->spellDamage <= 0) return;
    
    DealDamage(target, spell->spellDamage, spell, game);
}

// Apply healing
void ApplyHealing(GameState* game, int amount, void* target) {
    if (!target || amount <= 0) return;
    
    // Check if target is a card
    Card* targetCard = NULL;
    Player* targetPlayer = NULL;
    
    // Try to find the target
    for (int p = 0; p < 2; p++) {
        for (int i = 0; i < game->players[p].boardCount; i++) {
            if (&game->players[p].board[i] == target) {
                targetCard = (Card*)target;
                break;
            }
        }
        if (&game->players[p] == target) {
            targetPlayer = (Player*)target;
            break;
        }
    }
    
    if (targetCard && targetCard->health < targetCard->maxHealth) {
        int healAmount = amount;
        if (targetCard->health + healAmount > targetCard->maxHealth) {
            healAmount = targetCard->maxHealth - targetCard->health;
        }
        targetCard->health += healAmount;
        CreateHealEffect(game, targetCard->position, healAmount);
    } else if (targetPlayer && targetPlayer->health < targetPlayer->maxHealth) {
        int healAmount = amount;
        if (targetPlayer->health + healAmount > targetPlayer->maxHealth) {
            healAmount = targetPlayer->maxHealth - targetPlayer->health;
        }
        targetPlayer->health += healAmount;
        Vector3 playerPos = {0, 2, targetPlayer->playerId == 0 ? 6 : -6};
        CreateHealEffect(game, playerPos, healAmount);
    }
}