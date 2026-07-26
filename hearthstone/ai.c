#include "ai.h"
#include "gameplay.h"
#include "combat.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

// Initialize AI player
void InitializeAI(AIPlayer* ai, int playerId, AIDifficulty difficulty) {
    ai->difficulty = difficulty;
    ai->playerId = playerId;
    ai->actionCount = 0;
    ai->turnTimer = 0.0f;

    // Set AI personality based on difficulty
    switch (difficulty) {
        case AI_DIFFICULTY_EASY:
            ai->aggressionWeight = 0.3f;
            ai->boardControlWeight = 0.2f;
            ai->valueWeight = 0.2f;
            ai->survivalWeight = 0.3f;
            ai->thinkTime = 0.5f;
            ai->mistakeChance = 0.3f;
            ai->maxDepth = 1;
            break;
        case AI_DIFFICULTY_MEDIUM:
            ai->aggressionWeight = 0.4f;
            ai->boardControlWeight = 0.3f;
            ai->valueWeight = 0.2f;
            ai->survivalWeight = 0.1f;
            ai->thinkTime = 1.0f;
            ai->mistakeChance = 0.15f;
            ai->maxDepth = 2;
            break;
        case AI_DIFFICULTY_HARD:
            ai->aggressionWeight = 0.5f;
            ai->boardControlWeight = 0.3f;
            ai->valueWeight = 0.1f;
            ai->survivalWeight = 0.1f;
            ai->thinkTime = 1.5f;
            ai->mistakeChance = 0.05f;
            ai->maxDepth = 3;
            break;
    }
}

// Update AI and execute actions
void UpdateAI(GameState* game, AIPlayer* ai, float deltaTime) {
    Player* aiPlayer = &game->players[ai->playerId];

    // Only act if it's AI's turn and game is in main phase
    if (!aiPlayer->isActivePlayer || game->turnPhase != TURN_PHASE_MAIN || game->gameEnded) {
        return;
    }

    ai->turnTimer += deltaTime;

    // Give AI time to "think"
    if (ai->turnTimer < ai->thinkTime) {
        return;
    }

    // Calculate and execute best action
    AIAction bestAction = CalculateBestAction(game, ai);

    if (bestAction.type != AI_ACTION_NONE) {
        ExecuteAIAction(game, ai, bestAction);
        ai->turnTimer = 0.0f; // Reset timer for next action
    }
}

// Reset AI state for new turn
void ResetAITurn(AIPlayer* ai) {
    ai->actionCount = 0;
    ai->turnTimer = 0.0f;
}

// Calculate the best action for AI to take
AIAction CalculateBestAction(GameState* game, AIPlayer* ai) {
    Player* aiPlayer = &game->players[ai->playerId];
    AIAction bestAction = {AI_ACTION_NONE, -1, -1, false, -1000.0f, NULL, NULL};

    // 1. Evaluate playing cards from hand
    for (int i = 0; i < aiPlayer->handCount; i++) {
        Card* card = &aiPlayer->hand[i];

        if (!CanPlayCard(aiPlayer, card)) continue;

        void* target = FindBestTarget(game, ai, card);
        float score = EvaluatePlayCard(game, ai, i, target);

        if (score > bestAction.score) {
            bestAction.type = AI_ACTION_PLAY_CARD;
            bestAction.cardIndex = i;
            bestAction.score = score;
            bestAction.sourceCard = card;
            bestAction.target = target;
        }
    }

    // 2. Evaluate attacking with minions
    for (int i = 0; i < aiPlayer->boardCount; i++) {
        Card* attacker = &aiPlayer->board[i];

        if (!CanAttack(attacker)) continue;

        void* target = FindBestAttackTarget(game, ai, attacker);
        if (!target) continue;

        float score = EvaluateAttack(game, ai, attacker, target);

        if (score > bestAction.score) {
            bestAction.type = (target == &game->players[1 - ai->playerId]) ?
                            AI_ACTION_ATTACK_PLAYER : AI_ACTION_ATTACK_MINION;
            bestAction.cardIndex = i;
            bestAction.score = score;
            bestAction.sourceCard = attacker;
            bestAction.target = target;
        }
    }

    // 3. Evaluate using hero power
    if (!aiPlayer->heroPowerUsed && aiPlayer->mana >= aiPlayer->heroPower.cost) {
        void* target = FindBestTarget(game, ai, &aiPlayer->heroPower);
        float score = EvaluateHeroPower(game, ai, target);

        if (score > bestAction.score) {
            bestAction.type = AI_ACTION_USE_HERO_POWER;
            bestAction.score = score;
            bestAction.target = target;
        }
    }

    // 4. Consider ending turn
    float endTurnScore = EvaluateEndTurn(game, ai);
    if (endTurnScore > bestAction.score || bestAction.score < 0.0f) {
        bestAction.type = AI_ACTION_END_TURN;
        bestAction.score = endTurnScore;
    }

    // Apply difficulty-based mistakes
    if (ShouldMakeMistake(ai) && bestAction.type != AI_ACTION_END_TURN) {
        // Make a random suboptimal choice
        if (aiPlayer->handCount > 0 && CanPlayCard(aiPlayer, &aiPlayer->hand[0])) {
            bestAction.type = AI_ACTION_PLAY_CARD;
            bestAction.cardIndex = 0;
            bestAction.target = NULL;
            bestAction.score = 0.0f;
        }
    }

    return bestAction;
}

// Execute an AI action
void ExecuteAIAction(GameState* game, AIPlayer* ai, AIAction action) {
    Player* aiPlayer = &game->players[ai->playerId];

    switch (action.type) {
        case AI_ACTION_PLAY_CARD:
            if (action.cardIndex >= 0 && action.cardIndex < aiPlayer->handCount) {
                PlayCard(game, aiPlayer, action.cardIndex, action.target);
            }
            break;

        case AI_ACTION_ATTACK_MINION:
        case AI_ACTION_ATTACK_PLAYER:
            if (action.cardIndex >= 0 && action.cardIndex < aiPlayer->boardCount) {
                AttackWithMinion(game, aiPlayer, action.cardIndex, action.target);
            }
            break;

        case AI_ACTION_USE_HERO_POWER:
            UseHeroPower(game, aiPlayer, action.target);
            break;

        case AI_ACTION_END_TURN:
            EndPlayerTurn(game);
            break;

        default:
            break;
    }
}

// Evaluate playing a card
float EvaluatePlayCard(GameState* game, AIPlayer* ai, int handIndex, void* target) {
    Player* aiPlayer = &game->players[ai->playerId];
    Card* card = &aiPlayer->hand[handIndex];

    if (!CanPlayCard(aiPlayer, card)) return -1000.0f;

    float score = 0.0f;

    // Base value of playing the card
    score += EvaluateCard(card, game) * ai->valueWeight;

    // Mana efficiency
    float manaEfficiency = (float)card->attack + card->health + card->spellDamage + card->healing;
    if (card->cost > 0) {
        manaEfficiency /= card->cost;
    }
    score += manaEfficiency * ai->valueWeight * 10.0f;

    // Board control value
    if (card->type == CARD_TYPE_MINION) {
        score += (card->attack + card->health) * ai->boardControlWeight * 5.0f;

        // Taunt is valuable for survival
        if (card->taunt) {
            score += ai->survivalWeight * 15.0f;
        }

        // Charge is valuable for aggression
        if (card->charge) {
            score += ai->aggressionWeight * 10.0f;
        }
    }

    // Spell value
    if (card->type == CARD_TYPE_SPELL && target) {
        if (card->spellDamage > 0) {
            score += card->spellDamage * ai->aggressionWeight * 8.0f;
        }
        if (card->healing > 0) {
            score += card->healing * ai->survivalWeight * 6.0f;
        }
    }

    // Don't play if board is full
    if (card->type == CARD_TYPE_MINION && aiPlayer->boardCount >= MAX_BOARD_SIZE) {
        return -1000.0f;
    }

    return score;
}

// Evaluate an attack
float EvaluateAttack(GameState* game, AIPlayer* ai, Card* attacker, void* target) {
    if (!CanAttack(attacker) || !target) return -1000.0f;

    float score = 0.0f;
    Player* enemyPlayer = GetEnemyPlayer(game, ai->playerId);

    // Check if target is a card or player
    Card* targetCard = NULL;
    Player* targetPlayer = NULL;

    // Identify target type
    for (int i = 0; i < enemyPlayer->boardCount; i++) {
        if (&enemyPlayer->board[i] == target) {
            targetCard = (Card*)target;
            break;
        }
    }
    if (&game->players[1 - ai->playerId] == target) {
        targetPlayer = (Player*)target;
    }

    if (targetCard) {
        // Attacking a minion
        float tradeValue = EvaluateTrade(attacker, targetCard);
        score += tradeValue * ai->boardControlWeight * 10.0f;

        // Prioritize removing taunt minions
        if (targetCard->taunt) {
            score += ai->aggressionWeight * 20.0f;
        }

        // High-value targets
        if (targetCard->attack >= 4 || targetCard->health >= 5) {
            score += ai->boardControlWeight * 15.0f;
        }
    } else if (targetPlayer) {
        // Attacking the enemy hero
        score += attacker->attack * ai->aggressionWeight * 12.0f;

        // Bonus for lethal damage
        if (attacker->attack >= targetPlayer->health) {
            score += 1000.0f; // Always go for lethal
        }

        // Don't attack face if there are threatening enemy minions
        int enemyThreat = 0;
        for (int i = 0; i < enemyPlayer->boardCount; i++) {
            enemyThreat += enemyPlayer->board[i].attack;
        }

        if (enemyThreat > 6) {
            score -= ai->survivalWeight * 30.0f;
        }
    }

    return score;
}

// Evaluate using hero power
float EvaluateHeroPower(GameState* game, AIPlayer* ai, void* target) {
    Player* aiPlayer = &game->players[ai->playerId];

    if (aiPlayer->heroPowerUsed || aiPlayer->mana < aiPlayer->heroPower.cost) {
        return -1000.0f;
    }

    float score = 10.0f; // Base value for using hero power

    switch (aiPlayer->heroClass) {
        case CLASS_MAGE:
            // Fireblast - prioritize damaged minions or face damage
            if (target) {
                score += ai->aggressionWeight * 8.0f;
            }
            break;
        case CLASS_PALADIN:
            // Reinforce - board presence
            if (aiPlayer->boardCount < MAX_BOARD_SIZE) {
                score += ai->boardControlWeight * 15.0f;
            } else {
                return -1000.0f; // Can't summon if board is full
            }
            break;
        default:
            score += ai->survivalWeight * 5.0f;
            break;
    }

    return score;
}

// Evaluate ending turn
float EvaluateEndTurn(GameState* game, AIPlayer* ai) {
    Player* aiPlayer = &game->players[ai->playerId];

    // If we have no mana left, end turn
    if (aiPlayer->mana == 0) {
        return 50.0f;
    }

    // If we can't do anything useful, end turn
    bool canAct = false;

    // Check if we can play any cards
    for (int i = 0; i < aiPlayer->handCount; i++) {
        if (CanPlayCard(aiPlayer, &aiPlayer->hand[i])) {
            canAct = true;
            break;
        }
    }

    // Check if we can attack
    for (int i = 0; i < aiPlayer->boardCount; i++) {
        if (CanAttack(&aiPlayer->board[i])) {
            canAct = true;
            break;
        }
    }

    // Check if we can use hero power
    if (!aiPlayer->heroPowerUsed && aiPlayer->mana >= aiPlayer->heroPower.cost) {
        canAct = true;
    }

    if (!canAct) {
        return 100.0f; // End turn if we can't do anything
    }

    return -10.0f; // Small penalty for ending turn when we could still act
}

// Evaluate board state for a player
float EvaluateBoardState(GameState* game, int playerId) {
    Player* player = &game->players[playerId];
    Player* enemy = &game->players[1 - playerId];

    float score = 0.0f;

    // Health difference
    score += (player->health - enemy->health) * 2.0f;

    // Board presence
    float playerBoardValue = 0.0f;
    float enemyBoardValue = 0.0f;

    for (int i = 0; i < player->boardCount; i++) {
        playerBoardValue += player->board[i].attack + player->board[i].health;
    }

    for (int i = 0; i < enemy->boardCount; i++) {
        enemyBoardValue += enemy->board[i].attack + enemy->board[i].health;
    }

    score += (playerBoardValue - enemyBoardValue) * 1.5f;

    // Hand advantage
    score += (player->handCount - enemy->handCount) * 3.0f;

    // Mana advantage
    score += player->mana * 0.5f;

    return score;
}

// Evaluate card value
float EvaluateCard(Card* card, GameState* game) {
    float value = 0.0f;

    if (card->type == CARD_TYPE_MINION) {
        value += card->attack * 1.0f;
        value += card->health * 1.0f;

        if (card->taunt) value += 2.0f;
        if (card->charge) value += 2.0f;
        if (card->divineShield) value += 1.5f;
        if (card->windfury) value += 1.5f;
        if (card->poisonous) value += 1.0f;
        if (card->lifesteal) value += 1.0f;
        if (card->hasBattlecry) value += 1.0f;
        if (card->hasDeathrattle) value += 1.0f;
    } else if (card->type == CARD_TYPE_SPELL) {
        value += card->spellDamage * 1.2f;
        value += card->healing * 1.0f;
    }

    return value;
}

// Evaluate a trade between two cards
float EvaluateTrade(Card* attacker, Card* defender) {
    float attackerValue = EvaluateCard(attacker, NULL);
    float defenderValue = EvaluateCard(defender, NULL);

    // Simple trade evaluation
    bool attackerDies = (defender->attack >= attacker->health);
    bool defenderDies = (attacker->attack >= defender->health);

    if (defenderDies && !attackerDies) {
        return defenderValue; // Good trade - we kill their minion and keep ours
    } else if (defenderDies && attackerDies) {
        return defenderValue - attackerValue; // Mutual destruction
    } else if (!defenderDies && attackerDies) {
        return -attackerValue; // Bad trade - we lose our minion
    } else {
        return attacker->attack * 0.5f; // Damage without death
    }
}

// Find best target for a card
void* FindBestTarget(GameState* game, AIPlayer* ai, Card* card) {
    if (card->type == CARD_TYPE_SPELL && card->spellDamage > 0) {
        // For damage spells, find best target
        return FindBestAttackTarget(game, ai, card);
    } else if (card->type == CARD_TYPE_SPELL && card->healing > 0) {
        // For healing spells, target damaged friendly characters
        Player* aiPlayer = &game->players[ai->playerId];

        // Check if AI player needs healing
        if (aiPlayer->health < aiPlayer->maxHealth) {
            return aiPlayer;
        }

        // Check friendly minions
        for (int i = 0; i < aiPlayer->boardCount; i++) {
            if (aiPlayer->board[i].health < aiPlayer->board[i].maxHealth) {
                return &aiPlayer->board[i];
            }
        }
    }

    return NULL;
}

// Find best attack target
void* FindBestAttackTarget(GameState* game, AIPlayer* ai, Card* attacker) {
    Player* enemyPlayer = GetEnemyPlayer(game, ai->playerId);
    void* bestTarget = NULL;
    float bestScore = -1000.0f;

    // Check enemy minions
    for (int i = 0; i < enemyPlayer->boardCount; i++) {
        Card* target = &enemyPlayer->board[i];

        if (!IsValidTarget(attacker, target, game)) continue;

        float score = EvaluateTrade(attacker, target);

        // Prioritize taunt minions
        if (target->taunt) {
            score += 20.0f;
        }

        if (score > bestScore) {
            bestScore = score;
            bestTarget = target;
        }
    }

    // Consider attacking the enemy player
    if (IsValidTarget(attacker, enemyPlayer, game)) {
        float faceScore = attacker->attack * ai->aggressionWeight * 10.0f;

        // Bonus for lethal
        if (attacker->attack >= enemyPlayer->health) {
            faceScore += 1000.0f;
        }

        if (faceScore > bestScore) {
            bestTarget = enemyPlayer;
        }
    }

    return bestTarget;
}

// Get enemy player
Player* GetEnemyPlayer(GameState* game, int playerId) {
    return &game->players[1 - playerId];
}

// AI decision helpers
bool ShouldPlayCard(GameState* game, AIPlayer* ai, Card* card) {
    return EvaluatePlayCard(game, ai, 0, NULL) > 0.0f;
}

bool ShouldAttack(GameState* game, AIPlayer* ai, Card* card) {
    void* target = FindBestAttackTarget(game, ai, card);
    return target != NULL && EvaluateAttack(game, ai, card, target) > 0.0f;
}

bool ShouldUseHeroPower(GameState* game, AIPlayer* ai) {
    void* target = FindBestTarget(game, ai, &game->players[ai->playerId].heroPower);
    return EvaluateHeroPower(game, ai, target) > 0.0f;
}

bool ShouldEndTurn(GameState* game, AIPlayer* ai) {
    return EvaluateEndTurn(game, ai) > 50.0f;
}

// Apply difficulty modifiers
void ApplyDifficultyModifiers(AIPlayer* ai) {
    // This function can be called to adjust AI behavior dynamically
    // Currently modifiers are set in InitializeAI
}

// Check if AI should make a mistake
bool ShouldMakeMistake(AIPlayer* ai) {
    return (rand() / (float)RAND_MAX) < ai->mistakeChance;
}