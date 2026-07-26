#ifndef RULES_ENGINE_H
#define RULES_ENGINE_H

#include "../common.h"
#include "../errors.h"
#include "../types.h"

// Forward declaration
struct DataManager;

typedef enum {
    RULE_PLAY_CARD,
    RULE_ATTACK,
    RULE_END_TURN,
    RULE_BATTLECRY,
    RULE_DEATHRATTLE,
    RULE_DAMAGE,
    RULE_HEAL,
    RULE_DRAW_CARD,
    RULE_SUMMON_MINION
} RuleType;

typedef struct {
    RuleType type;
    void* source;
    void* target;
    int value;
    bool valid;
    char reason[128];
} RuleValidation;

typedef struct {
    struct DataManager* data_manager;
    GameState* game_state;
    
    // Rule callbacks
    bool (*validate_play_card)(Card* card, Player* player, GameState* game);
    bool (*validate_attack)(Card* attacker, void* target, GameState* game);
    bool (*validate_target)(Card* card, void* target, GameState* game);
    
    // Effect processors
    GameError (*process_battlecry)(Card* card, void* target, GameState* game);
    GameError (*process_deathrattle)(Card* card, GameState* game);
    GameError (*process_damage)(void* target, int damage, GameState* game);
    GameError (*process_heal)(void* target, int healing, GameState* game);
} RulesEngine;

// Rules engine initialization
GameError InitRulesEngine(RulesEngine* engine, struct DataManager* dm, GameState* game);
void CleanupRulesEngine(RulesEngine* engine);

// Core rule validation
RuleValidation ValidateAction(RulesEngine* engine, RuleType type, void* source, void* target, int value);
GameError ProcessAction(RulesEngine* engine, RuleType type, void* source, void* target, int value);

// Specific rule validators (renamed to avoid conflicts)
bool RulesCanPlayCard(RulesEngine* engine, Card* card, Player* player);
bool RulesCanAttack(RulesEngine* engine, Card* attacker, void* target);
bool RulesCanTargetWithSpell(RulesEngine* engine, Card* spell, void* target);
bool RulesCanEndTurn(RulesEngine* engine, Player* player);

// Action processors (renamed to avoid conflicts)
GameError RulesPlayCard(RulesEngine* engine, Card* card, Player* player, void* target);
GameError RulesAttackTarget(RulesEngine* engine, Card* attacker, void* target);
GameError RulesCastSpell(RulesEngine* engine, Card* spell, Player* caster, void* target);
GameError RulesEndPlayerTurn(RulesEngine* engine, Player* player);

// Effect processors (renamed to avoid conflicts)
GameError RulesProcessBattlecry(RulesEngine* engine, Card* card, void* target);
GameError RulesProcessDeathrattle(RulesEngine* engine, Card* card);
GameError RulesDealDamage(RulesEngine* engine, void* target, int damage, Card* source);
GameError RulesRestoreHealth(RulesEngine* engine, void* target, int healing, Card* source);
GameError RulesDrawCardForPlayer(RulesEngine* engine, Player* player);
GameError RulesSummonMinion(RulesEngine* engine, Card* minion, Player* owner, int position);

// Game state checkers (renamed to avoid conflicts)
bool RulesIsGameOver(RulesEngine* engine);
int RulesGetWinner(RulesEngine* engine);
bool RulesHasValidTargets(RulesEngine* engine, Card* card, Player* player);

// Turn management (renamed to avoid conflicts)
GameError RulesStartTurn(RulesEngine* engine, Player* player);
GameError RulesEndTurn(RulesEngine* engine, Player* player);

// Utility functions (renamed to avoid conflicts)
bool RulesIsValidTarget(void* target, Card* spell);
bool RulesHasTauntMinions(Player* player);
Player* RulesGetOpponent(GameState* game, Player* player);

#endif