#ifndef TYPES_H
#define TYPES_H

#include "raylib.h"
#include <stdbool.h>

// Core constants
#define MAX_HAND_SIZE 10
#define MAX_BOARD_SIZE 7
#define MAX_DECK_SIZE 30
#define MAX_EFFECTS 50
#define MAX_HISTORY 1000
#define MAX_CARD_NAME 64
#define MAX_CARD_DESC 256
#define MAX_PLAYER_NAME 32

// Card types
typedef enum {
    CARD_TYPE_MINION,
    CARD_TYPE_SPELL,
    CARD_TYPE_WEAPON,
    CARD_TYPE_HERO,
    CARD_TYPE_HERO_POWER
} CardType;

// Card rarity
typedef enum {
    RARITY_COMMON,
    RARITY_RARE,
    RARITY_EPIC,
    RARITY_LEGENDARY
} CardRarity;

// Hero classes
typedef enum {
    CLASS_NEUTRAL,
    CLASS_MAGE,
    CLASS_PALADIN,
    CLASS_WARRIOR,
    CLASS_ROGUE,
    CLASS_HUNTER,
    CLASS_DRUID,
    CLASS_WARLOCK,
    CLASS_SHAMAN,
    CLASS_PRIEST,
    CLASS_COUNT
} HeroClass;

// Game phases
typedef enum {
    GAME_STATE_MULLIGAN,
    GAME_STATE_PLAYING,
    GAME_STATE_ENDED
} GamePhase;

// Turn phases
typedef enum {
    TURN_PHASE_START,
    TURN_PHASE_MAIN,
    TURN_PHASE_END
} TurnPhase;

// Action types
typedef enum {
    ACTION_PLAY_CARD,
    ACTION_ATTACK,
    ACTION_USE_HERO_POWER,
    ACTION_END_TURN,
    ACTION_CONCEDE
} ActionType;

// Visual effect types
typedef enum {
    EFFECT_DAMAGE = 0,
    EFFECT_DIVINE_SHIELD,
    EFFECT_DEATH,
    EFFECT_DEATHRATTLE,
    EFFECT_ATTACK,
    EFFECT_BATTLECRY,
    EFFECT_SUMMON,
    EFFECT_SPELL,
    EFFECT_HEAL,
    EFFECT_TURN_START
} EffectType;

#endif // TYPES_H