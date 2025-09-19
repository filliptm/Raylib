#include "effects.h"
#include "game_state.h"
#include "combat.h"
#include <string.h>

// Initialize the effects system
void InitializeEffects(GameState* game) {
    game->activeEffectsCount = 0;
    memset(game->effects, 0, sizeof(game->effects));
}

// Add a visual effect to the game
void AddVisualEffect(GameState* game, int type, Vector3 position, const char* text) {
    if (game->activeEffectsCount >= MAX_EFFECTS) return;
    
    VisualEffect* effect = &game->effects[game->activeEffectsCount];
    effect->type = type;
    effect->position = position;
    effect->position.y += 1.0f; // Lift above board
    effect->duration = GetEffectDuration(type);
    effect->timer = 0.0f;
    effect->active = true;
    effect->color = GetEffectColor(type);
    strncpy(effect->text, text, sizeof(effect->text) - 1);
    
    game->activeEffectsCount++;
}

// Update all active effects
void UpdateEffects(GameState* game, float deltaTime) {
    for (int i = 0; i < game->activeEffectsCount; i++) {
        VisualEffect* effect = &game->effects[i];
        if (effect->active) {
            effect->timer += deltaTime;
            effect->position.y += deltaTime * 2.0f; // Float upward
            
            if (effect->timer >= effect->duration) {
                effect->active = false;
            }
        }
    }
    
    // Remove inactive effects
    int writeIndex = 0;
    for (int i = 0; i < game->activeEffectsCount; i++) {
        if (game->effects[i].active) {
            if (writeIndex != i) {
                game->effects[writeIndex] = game->effects[i];
            }
            writeIndex++;
        }
    }
    game->activeEffectsCount = writeIndex;
}

// Clear all effects
void ClearEffects(GameState* game) {
    game->activeEffectsCount = 0;
}

// Create specific effect types
void CreateDamageEffect(GameState* game, Vector3 position, int damage) {
    AddVisualEffect(game, EFFECT_DAMAGE, position, TextFormat("-%d", damage));
}

void CreateHealEffect(GameState* game, Vector3 position, int healing) {
    AddVisualEffect(game, EFFECT_HEAL, position, TextFormat("+%d", healing));
}

void CreateDeathEffect(GameState* game, Vector3 position) {
    AddVisualEffect(game, EFFECT_DEATH, position, "Dies!");
}

void CreateSummonEffect(GameState* game, Vector3 position) {
    AddVisualEffect(game, EFFECT_SUMMON, position, "Summoned!");
}

void CreateSpellEffect(GameState* game, Vector3 position, const char* spellName) {
    AddVisualEffect(game, EFFECT_SPELL, position, spellName);
}

void CreateBattlecryEffect(GameState* game, Vector3 position) {
    AddVisualEffect(game, EFFECT_BATTLECRY, position, "Battlecry!");
}

void CreateDeathrattleEffect(GameState* game, Vector3 position) {
    AddVisualEffect(game, EFFECT_DEATHRATTLE, position, "Deathrattle!");
}

// Execute battlecry effect
void ExecuteBattlecry(GameState* game, Card* card, void* target) {
    if (!card->hasBattlecry) return;
    
    CreateBattlecryEffect(game, card->position);
    
    // Simple battlecry implementation - deal damage
    if (card->battlecryValue > 0 && target) {
        DealDamage(target, card->battlecryValue, card, game);
    }
}

// Execute deathrattle effect
void ExecuteDeathrattle(GameState* game, Card* card) {
    if (!card->hasDeathrattle) return;
    
    CreateDeathrattleEffect(game, card->position);
    
    // Simple deathrattle implementation
    // This would be expanded with specific deathrattle effects
}

// Get the color for an effect type
Color GetEffectColor(int effectType) {
    switch (effectType) {
        case EFFECT_DAMAGE:       return RED;
        case EFFECT_DIVINE_SHIELD: return YELLOW;
        case EFFECT_DEATH:        return PURPLE;
        case EFFECT_DEATHRATTLE:  return ORANGE;
        case EFFECT_ATTACK:       return GREEN;
        case EFFECT_BATTLECRY:    return BLUE;
        case EFFECT_SUMMON:       return WHITE;
        case EFFECT_SPELL:        return PINK;
        case EFFECT_HEAL:         return LIME;
        case EFFECT_TURN_START:   return GOLD;
        default:                  return WHITE;
    }
}

// Get the duration for an effect type
float GetEffectDuration(int effectType) {
    switch (effectType) {
        case EFFECT_TURN_START:   return 3.0f;
        case EFFECT_DEATH:        return 2.5f;
        default:                  return 2.0f;
    }
}