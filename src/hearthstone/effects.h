#ifndef EFFECTS_H
#define EFFECTS_H

#include "types.h"
#include "common.h"
#include "raylib.h"
#include "raymath.h"

// Visual effect structure
struct VisualEffect {
    int type;
    Vector3 position;
    float duration;
    float timer;
    Color color;
    bool active;
    char text[64];
};

// Effect management
void InitializeEffects(GameState* game);
void AddVisualEffect(GameState* game, int type, Vector3 position, const char* text);
void UpdateEffects(GameState* game, float deltaTime);
void ClearEffects(GameState* game);

// Specific effect creators
void CreateDamageEffect(GameState* game, Vector3 position, int damage);
void CreateHealEffect(GameState* game, Vector3 position, int healing);
void CreateDeathEffect(GameState* game, Vector3 position);
void CreateSummonEffect(GameState* game, Vector3 position);
void CreateSpellEffect(GameState* game, Vector3 position, const char* spellName);
void CreateBattlecryEffect(GameState* game, Vector3 position);
void CreateDeathrattleEffect(GameState* game, Vector3 position);

// Card ability effects
void ExecuteBattlecry(GameState* game, Card* card, void* target);
void ExecuteDeathrattle(GameState* game, Card* card);

// Effect helpers
Color GetEffectColor(int effectType);
float GetEffectDuration(int effectType);

#endif // EFFECTS_H