#include "effect_renderer.h"
#include "../effects.h"
#include "../game_state.h"

// Draw visual effects
void DrawVisualEffects(GameState* game) {
    for (int i = 0; i < game->activeEffectsCount; i++) {
        if (game->effects[i].active) {
            DrawEffect(&game->effects[i], game->camera);
        }
    }
}

// Draw a single effect
void DrawEffect(VisualEffect* effect, Camera3D camera) {
    Vector2 screenPos = GetWorldToScreen(effect->position, camera);
    float alpha = 1.0f - (effect->timer / effect->duration);
    Color effectColor = Fade(effect->color, alpha);
    
    int textWidth = MeasureText(effect->text, 16);
    DrawText(effect->text, screenPos.x - textWidth/2, screenPos.y, 16, effectColor);
}