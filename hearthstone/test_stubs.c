#include "game_state.h"
#include "effects.h"
#include "raylib_compat.h"
#include <stdio.h>

// Test stub implementations for raylib-dependent functions

void AddVisualEffect(GameState* game, int type, Vector3 position, const char* text) {
    // Test stub - just increment counter
    if (game->activeEffectsCount < MAX_EFFECTS) {
        game->activeEffectsCount++;
    }
}

void CreateDamageEffect(GameState* game, Vector3 position, int damage) {
    // Test stub
    char text[32];
    snprintf(text, sizeof(text), "-%d", damage);
    AddVisualEffect(game, EFFECT_DAMAGE, position, text);
}

void CreateHealEffect(GameState* game, Vector3 position, int healing) {
    // Test stub
    char text[32];
    snprintf(text, sizeof(text), "+%d", healing);
    AddVisualEffect(game, EFFECT_HEAL, position, text);
}

void CreateDeathEffect(GameState* game, Vector3 position) {
    AddVisualEffect(game, EFFECT_DEATH, position, "Dies!");
}

void CreateSpellEffect(GameState* game, Vector3 position, const char* spellName) {
    AddVisualEffect(game, EFFECT_SPELL, position, spellName);
}

void ExecuteDeathrattle(GameState* game, Card* card) {
    // Test stub - basic deathrattle processing
    if (!card || !card->hasDeathrattle) return;

    CreateDeathEffect(game, card->position);

    // Process specific deathrattle effects
    switch (card->id) {
        case 5: // Loot Hoarder - draw card
            // In test, just simulate card draw
            break;
        default:
            break;
    }
}

// Stub for GetFrameTime
float GetFrameTime(void) {
    return 0.016f; // 60 FPS
}

RayCollision GetRayCollisionBox(Ray ray, BoundingBox box) {
    RayCollision collision = {0};
    collision.hit = false;
    return collision;
}