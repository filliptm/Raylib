#include "rules_engine.h"
#include "data_manager.h"
#include "../game_state.h"
#include "../player.h"
#include "../card.h"
#include "../combat.h"
#include "../effects.h"
#include <string.h>
#include <stdio.h>

GameError InitRulesEngine(RulesEngine* engine, struct DataManager* dm, GameState* game) {
    if (!engine || !dm || !game) return GAME_ERROR_INVALID_PARAMETER;
    
    memset(engine, 0, sizeof(RulesEngine));
    engine->data_manager = dm;
    engine->game_state = game;
    
    return GAME_OK;
}

void CleanupRulesEngine(RulesEngine* engine) {
    if (!engine) return;
    memset(engine, 0, sizeof(RulesEngine));
}

RuleValidation ValidateAction(RulesEngine* engine, RuleType type, void* source, void* target, int value) {
    RuleValidation result = {0};
    result.type = type;
    result.source = source;
    result.target = target;
    result.value = value;
    result.valid = false;
    strcpy(result.reason, "Unknown error");
    
    if (!engine) {
        strcpy(result.reason, "Invalid engine");
        return result;
    }
    
    switch (type) {
        case RULE_PLAY_CARD: {
            Card* card = (Card*)source;
            Player* player = &engine->game_state->players[engine->game_state->activePlayer];
            
            if (!card || !card->inHand) {
                strcpy(result.reason, "Card not in hand");
                return result;
            }
            
            if (card->cost > player->mana) {
                strcpy(result.reason, "Not enough mana");
                return result;
            }
            
            if (card->type == CARD_TYPE_MINION && player->boardCount >= MAX_BOARD_SIZE) {
                strcpy(result.reason, "Board is full");
                return result;
            }
            
            result.valid = true;
            strcpy(result.reason, "Valid play");
            break;
        }
        
        case RULE_ATTACK: {
            Card* attacker = (Card*)source;
            
            if (!attacker || !attacker->onBoard) {
                strcpy(result.reason, "Attacker not on board");
                return result;
            }
            
            if (!attacker->canAttack || attacker->attackedThisTurn) {
                strcpy(result.reason, "Cannot attack this turn");
                return result;
            }
            
            if (attacker->attack <= 0) {
                strcpy(result.reason, "No attack power");
                return result;
            }
            
            // Check for taunt using existing function
            Player* opponent = RulesGetOpponent(engine->game_state, &engine->game_state->players[attacker->ownerPlayer]);
            if (RulesHasTauntMinions(opponent)) {
                Card* targetCard = (Card*)target;
                if (!targetCard || !targetCard->taunt) {
                    strcpy(result.reason, "Must attack taunt minion");
                    return result;
                }
            }
            
            result.valid = true;
            strcpy(result.reason, "Valid attack");
            break;
        }
        
        case RULE_END_TURN: {
            result.valid = true;
            strcpy(result.reason, "Can always end turn");
            break;
        }
        
        default:
            strcpy(result.reason, "Unhandled rule type");
            break;
    }
    
    return result;
}

GameError ProcessAction(RulesEngine* engine, RuleType type, void* source, void* target, int value) {
    if (!engine) return GAME_ERROR_INVALID_PARAMETER;
    
    RuleValidation validation = ValidateAction(engine, type, source, target, value);
    if (!validation.valid) {
        return GAME_ERROR_INVALID_STATE;
    }
    
    switch (type) {
        case RULE_PLAY_CARD:
            return RulesPlayCard(engine, (Card*)source, &engine->game_state->players[engine->game_state->activePlayer], target);
            
        case RULE_ATTACK:
            return RulesAttackTarget(engine, (Card*)source, target);
            
        case RULE_END_TURN:
            return RulesEndPlayerTurn(engine, &engine->game_state->players[engine->game_state->activePlayer]);
            
        default:
            return GAME_ERROR_INVALID_PARAMETER;
    }
}

bool RulesCanPlayCard(RulesEngine* engine, Card* card, Player* player) {
    if (!engine || !card || !player) return false;
    
    RuleValidation validation = ValidateAction(engine, RULE_PLAY_CARD, card, NULL, 0);
    return validation.valid;
}

bool RulesCanAttack(RulesEngine* engine, Card* attacker, void* target) {
    if (!engine || !attacker) return false;
    
    RuleValidation validation = ValidateAction(engine, RULE_ATTACK, attacker, target, 0);
    return validation.valid;
}

bool RulesCanTargetWithSpell(RulesEngine* engine, Card* spell, void* target) {
    if (!engine || !spell || spell->type != CARD_TYPE_SPELL) return false;
    
    // Basic targeting rules for spells
    if (spell->spellDamage > 0) {
        // Damage spells can target any character
        return true;
    }
    
    if (spell->healing > 0) {
        // Healing spells can target any character
        return true;
    }
    
    return false;
}

bool RulesCanEndTurn(RulesEngine* engine, Player* player) {
    if (!engine || !player) return false;
    return true; // Can always end turn
}

GameError RulesPlayCard(RulesEngine* engine, Card* card, Player* player, void* target) {
    if (!engine || !card || !player) return GAME_ERROR_INVALID_PARAMETER;
    
    // Validate the play
    if (!RulesCanPlayCard(engine, card, player)) {
        return GAME_ERROR_INVALID_STATE;
    }
    
    // Pay mana cost
    player->mana -= card->cost;
    
    // Remove from hand using existing function (find index first)
    int handIndex = -1;
    for (int i = 0; i < player->handCount; i++) {
        if (&player->hand[i] == card) {
            handIndex = i;
            break;
        }
    }
    if (handIndex >= 0) {
        RemoveCardFromHand(player, handIndex);
    }
    
    if (card->type == CARD_TYPE_MINION) {
        // Summon to board using existing function
        if (AddCardToBoard(player, *card)) {
            // Process battlecry if any
            if (card->hasBattlecry) {
                RulesProcessBattlecry(engine, card, target);
            }
        } else {
            return GAME_ERROR_BOARD_FULL;
        }
    } else if (card->type == CARD_TYPE_SPELL) {
        // Cast spell
        return RulesCastSpell(engine, card, player, target);
    }
    
    return GAME_OK;
}

GameError RulesAttackTarget(RulesEngine* engine, Card* attacker, void* target) {
    if (!engine || !attacker) return GAME_ERROR_INVALID_PARAMETER;
    
    if (!RulesCanAttack(engine, attacker, target)) {
        return GAME_ERROR_INVALID_STATE;
    }
    
    // Mark as attacked
    attacker->attackedThisTurn = true;
    
    // Use existing combat system
    Card* targetCard = (Card*)target;
    Player* targetPlayer = (Player*)target;
    
    if (targetCard && targetCard->type == CARD_TYPE_MINION) {
        // Minion vs minion combat - use existing function
        AttackWithCard(engine->game_state, attacker, targetCard);
    } else if (targetPlayer) {
        // Attack player directly - use existing function
        DealDamage(targetPlayer, attacker->attack, attacker, engine->game_state);
    }
    
    return GAME_OK;
}

GameError RulesCastSpell(RulesEngine* engine, Card* spell, Player* caster, void* target) {
    if (!engine || !spell || !caster) return GAME_ERROR_INVALID_PARAMETER;
    
    // Use existing spell casting system
    CastSpell(engine->game_state, spell, target);
    
    return GAME_OK;
}

GameError RulesEndPlayerTurn(RulesEngine* engine, Player* player) {
    if (!engine || !player) return GAME_ERROR_INVALID_PARAMETER;
    
    // Use existing turn management
    EndTurn(engine->game_state);
    
    return GAME_OK;
}

GameError RulesProcessBattlecry(RulesEngine* engine, Card* card, void* target) {
    if (!engine || !card) return GAME_ERROR_INVALID_PARAMETER;
    
    if (!card->hasBattlecry) return GAME_OK;
    
    // Get card data to determine battlecry effect
    CardData* data = FindCardById(engine->data_manager, card->id);
    if (!data) return GAME_ERROR_INVALID_CARD;
    
    if (data->battlecry_heal_amount > 0 && target) {
        return RulesRestoreHealth(engine, target, data->battlecry_heal_amount, card);
    }
    
    if (data->battlecry_damage_amount > 0 && target) {
        return RulesDealDamage(engine, target, data->battlecry_damage_amount, card);
    }
    
    return GAME_OK;
}

GameError RulesProcessDeathrattle(RulesEngine* engine, Card* card) {
    if (!engine || !card) return GAME_ERROR_INVALID_PARAMETER;
    
    // Implement deathrattle effects here
    // For now, just return OK
    
    return GAME_OK;
}

GameError RulesDealDamage(RulesEngine* engine, void* target, int damage, Card* source) {
    if (!engine || !target || damage <= 0) return GAME_ERROR_INVALID_PARAMETER;
    
    // Use existing damage system
    DealDamage(target, damage, source, engine->game_state);
    
    return GAME_OK;
}

GameError RulesRestoreHealth(RulesEngine* engine, void* target, int healing, Card* source) {
    if (!engine || !target || healing <= 0) return GAME_ERROR_INVALID_PARAMETER;
    
    Card* targetCard = NULL;
    Player* targetPlayer = NULL;
    
    // Identify target type
    for (int p = 0; p < 2; p++) {
        if (&engine->game_state->players[p] == target) {
            targetPlayer = (Player*)target;
            break;
        }
        for (int i = 0; i < engine->game_state->players[p].boardCount; i++) {
            if (&engine->game_state->players[p].board[i] == target) {
                targetCard = (Card*)target;
                break;
            }
        }
    }
    
    if (targetCard) {
        int oldHealth = targetCard->health;
        targetCard->health = (targetCard->health + healing > targetCard->maxHealth) ? 
                            targetCard->maxHealth : targetCard->health + healing;
        
        if (targetCard->health > oldHealth) {
            CreateHealEffect(engine->game_state, targetCard->position, healing);
        }
    } else if (targetPlayer) {
        int oldHealth = targetPlayer->health;
        targetPlayer->health = (targetPlayer->health + healing > targetPlayer->maxHealth) ? 
                              targetPlayer->maxHealth : targetPlayer->health + healing;
        
        if (targetPlayer->health > oldHealth) {
            // Create heal effect at player position
            Vector3 playerPos = {7.0f, 0.2f, targetPlayer->playerId == 0 ? 6.0f : -6.0f};
            CreateHealEffect(engine->game_state, playerPos, healing);
        }
    }
    
    return GAME_OK;
}

GameError RulesDrawCardForPlayer(RulesEngine* engine, Player* player) {
    if (!engine || !player) return GAME_ERROR_INVALID_PARAMETER;
    
    DrawCardFromDeck(player);
    return GAME_OK;
}

GameError RulesSummonMinion(RulesEngine* engine, Card* minion, Player* owner, int position) {
    if (!engine || !minion || !owner) return GAME_ERROR_INVALID_PARAMETER;
    
    return AddCardToBoard(owner, *minion) ? GAME_OK : GAME_ERROR_BOARD_FULL;
}

bool RulesIsGameOver(RulesEngine* engine) {
    if (!engine) return false;
    return IsGameOver(engine->game_state);
}

int RulesGetWinner(RulesEngine* engine) {
    if (!engine || !RulesIsGameOver(engine)) return -1;
    return engine->game_state->winner;
}

bool RulesHasValidTargets(RulesEngine* engine, Card* card, Player* player) {
    if (!engine || !card || !player) return false;
    
    if (card->type == CARD_TYPE_MINION) {
        return player->boardCount < MAX_BOARD_SIZE;
    }
    
    if (card->type == CARD_TYPE_SPELL) {
        // For now, assume spells always have valid targets
        return true;
    }
    
    return false;
}

GameError RulesStartTurn(RulesEngine* engine, Player* player) {
    if (!engine || !player) return GAME_ERROR_INVALID_PARAMETER;
    
    // Use existing start turn logic
    StartTurn(engine->game_state);
    
    return GAME_OK;
}

GameError RulesEndTurn(RulesEngine* engine, Player* player) {
    return RulesEndPlayerTurn(engine, player);
}

bool RulesIsValidTarget(void* target, Card* spell) {
    if (!target || !spell) return false;
    
    // Use existing validation if available
    return true; // Basic implementation
}

bool RulesHasTauntMinions(Player* player) {
    if (!player) return false;
    
    for (int i = 0; i < player->boardCount; i++) {
        if (player->board[i].taunt) {
            return true;
        }
    }
    
    return false;
}

Player* RulesGetOpponent(GameState* game, Player* player) {
    if (!game || !player) return NULL;
    
    return &game->players[1 - player->playerId];
}