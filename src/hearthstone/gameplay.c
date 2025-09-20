#include "gameplay.h"
#include "combat.h"
#include "effects.h"
#include <string.h>
#include <stdio.h>

// Play a card from hand
bool PlayCard(GameState* game, Player* player, int handIndex, void* target) {
    if (!CanPlayCardAtIndex(player, handIndex)) {
        return false;
    }

    Card* card = &player->hand[handIndex];

    // Check if player has enough mana
    if (player->mana < card->cost) {
        return false;
    }

    bool success = false;

    switch (card->type) {
        case CARD_TYPE_MINION:
            success = PlayMinionCard(game, player, card);
            break;
        case CARD_TYPE_SPELL:
            success = PlaySpellCard(game, player, card, target);
            break;
        case CARD_TYPE_WEAPON:
            success = PlayWeaponCard(game, player, card);
            break;
        default:
            return false;
    }

    if (success) {
        // Spend mana
        SpendMana(player, card->cost);

        // Remove card from hand
        RemoveCardFromHand(player, handIndex);

        // Add visual effect
        CreateSpellEffect(game, card->position, "Card Played");
    }

    return success;
}

// Attack with a minion
bool AttackWithMinion(GameState* game, Player* player, int boardIndex, void* target) {
    if (boardIndex < 0 || boardIndex >= player->boardCount) {
        return false;
    }

    Card* attacker = &player->board[boardIndex];

    if (!CanAttack(attacker)) {
        return false;
    }

    if (!IsValidTarget(attacker, target, game)) {
        return false;
    }

    // Determine if attacking a card or player
    Card* targetCard = NULL;
    Player* targetPlayer = NULL;

    // Check if target is a card on enemy board
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
        AttackWithCard(game, attacker, targetCard);
    } else if (targetPlayer) {
        AttackPlayer(game, attacker, targetPlayer);
    } else {
        return false;
    }

    return true;
}

// Use hero power
bool UseHeroPower(GameState* game, Player* player, void* target) {
    if (player->heroPowerUsed) {
        return false;
    }

    Card* heroPower = &player->heroPower;

    if (player->mana < heroPower->cost) {
        return false;
    }

    // Execute hero power effect based on class
    switch (player->heroClass) {
        case CLASS_MAGE:
            // Fireblast: Deal 1 damage to any target
            if (target) {
                DealDamage(target, 1, heroPower, game);
            }
            break;
        case CLASS_PALADIN:
            // Reinforce: Summon a 1/1 Silver Hand Recruit
            if (player->boardCount < MAX_BOARD_SIZE) {
                Card recruit = GetCardById(100); // Special recruit card
                AddCardToBoard(player, recruit);
            }
            break;
        case CLASS_PRIEST:
            // Lesser Heal: Restore 2 health to any target
            if (target) {
                ApplyHealing(game, 2, target);
            }
            break;
        default:
            // Generic hero power: gain 2 armor
            player->armor += 2;
            break;
    }

    // Spend mana and mark hero power as used
    SpendMana(player, heroPower->cost);
    player->heroPowerUsed = true;

    // Visual effect
    CreateSpellEffect(game, (Vector3){0, 2, 0}, "Hero Power");

    return true;
}

// End the current player's turn
void EndPlayerTurn(GameState* game) {
    EndTurn(game);
}

// Check if a card at hand index can be played
bool CanPlayCardAtIndex(Player* player, int handIndex) {
    if (handIndex < 0 || handIndex >= player->handCount) {
        return false;
    }

    return CanPlayCard(player, &player->hand[handIndex]);
}

// Play a minion card
bool PlayMinionCard(GameState* game, Player* player, Card* card) {
    if (player->boardCount >= MAX_BOARD_SIZE) {
        return false; // Board is full
    }

    // Add to board
    bool success = AddCardToBoard(player, *card);

    if (success) {
        // Trigger battlecry if the card has one
        if (card->hasBattlecry) {
            Card* boardCard = &player->board[player->boardCount - 1];
            TriggerBattlecry(game, boardCard, NULL);
        }

        // Visual effect
        CreateSummonEffect(game, player->board[player->boardCount - 1].position);
    }

    return success;
}

// Play a spell card
bool PlaySpellCard(GameState* game, Player* player, Card* card, void* target) {
    // Cast the spell
    CastSpell(game, card, target);

    // Trigger battlecry if the spell has one (some spells have battlecry-like effects)
    if (card->hasBattlecry) {
        TriggerBattlecry(game, card, target);
    }

    return true;
}

// Play a weapon card
bool PlayWeaponCard(GameState* game, Player* player, Card* card) {
    // Destroy existing weapon if any
    if (player->hasWeapon && player->weapon) {
        // Trigger weapon deathrattle if any
        if (player->weapon->hasDeathrattle) {
            ExecuteDeathrattle(game, player->weapon);
        }
    }

    // Equip new weapon
    player->weapon = card;
    player->hasWeapon = true;

    // Visual effect
    CreateSpellEffect(game, (Vector3){0, 2, 0}, "Weapon Equipped");

    return true;
}

// Start a player's turn
void StartPlayerTurn(GameState* game, Player* player) {
    // This is called from game_state.c StartTurn function
    // Handle any additional turn start logic here

    // Enable all minions to attack (remove summoning sickness)
    for (int i = 0; i < player->boardCount; i++) {
        if (!player->board[i].charge) {
            player->board[i].canAttack = true;
        }
        player->board[i].attackedThisTurn = false;
    }
}

// Process turn events
void ProcessTurnEvents(GameState* game) {
    // Process any ongoing effects, triggers, etc.
    ValidateGameState(game);
}

// Trigger battlecry effect
void TriggerBattlecry(GameState* game, Card* card, void* target) {
    if (!card->hasBattlecry) return;

    ExecuteBattlecry(game, card, target);
    CreateBattlecryEffect(game, card->position);
}

// Check if a game action is valid
bool IsValidGameAction(GameState* game, Player* player, ActionType action) {
    if (game->gameEnded) return false;
    if (!player->isActivePlayer) return false;
    if (game->turnPhase != TURN_PHASE_MAIN) return false;

    return true;
}

// Validate and fix any inconsistent game state
void ValidateGameState(GameState* game) {
    for (int p = 0; p < 2; p++) {
        Player* player = &game->players[p];

        // Check for dead minions
        for (int i = player->boardCount - 1; i >= 0; i--) {
            if (player->board[i].health <= 0) {
                ProcessCardDeath(&player->board[i], player, game);
            }
        }

        // Check player health
        if (player->health <= 0) {
            player->isAlive = false;
            if (!game->gameEnded) {
                SetWinner(game, 1 - p);
            }
        }

        // Validate mana
        if (player->mana < 0) player->mana = 0;
        if (player->mana > player->maxMana) player->mana = player->maxMana;

        // Validate hand size
        if (player->handCount > MAX_HAND_SIZE) {
            player->handCount = MAX_HAND_SIZE;
        }

        // Validate board size
        if (player->boardCount > MAX_BOARD_SIZE) {
            player->boardCount = MAX_BOARD_SIZE;
        }
    }
}