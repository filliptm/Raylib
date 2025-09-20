#include "game_polish.h"
#include <stdlib.h>

// Initialize polish system for game
void InitializeGamePolish(GameState* game) {
    game->polishSystem = malloc(sizeof(PolishSystem));
    if (game->polishSystem) {
        InitializePolish((PolishSystem*)game->polishSystem);

        // Load settings if available
        LoadGameSettings(&((PolishSystem*)game->polishSystem)->gameSettings, "settings.cfg");
        ApplyGameSettings(&((PolishSystem*)game->polishSystem)->gameSettings);
    }
}

// Update polish systems
void UpdateGamePolishSystems(GameState* game, float deltaTime) {
    if (game->polishSystem) {
        UpdateGamePolish((PolishSystem*)game->polishSystem, game, deltaTime);
    }
}

// Draw polish systems
void DrawGamePolishSystems(const GameState* game) {
    if (game->polishSystem) {
        DrawGamePolish((const PolishSystem*)game->polishSystem, game);
    }
}

// Cleanup polish system
void CleanupGamePolish(GameState* game) {
    if (game->polishSystem) {
        // Save settings before cleanup
        SaveGameSettings(&((PolishSystem*)game->polishSystem)->gameSettings, "settings.cfg");

        CleanupPolish((PolishSystem*)game->polishSystem);
        free(game->polishSystem);
        game->polishSystem = NULL;
    }
}

// Polish event handlers
void OnCardPlayed(GameState* game, Card* card) {
    if (game->polishSystem) {
        PolishSystem* polish = (PolishSystem*)game->polishSystem;

        // Trigger screen shake for big spells
        if (card->type == CARD_TYPE_SPELL && card->spellDamage >= 4) {
            TriggerScreenShake(polish, 0.5f, 0.3f);
        }

        // Animate camera to played card
        if (card->cost >= 5) {
            AnimateCameraTo(polish, card->position, 0.5f);
        }
    }
}

void OnAttack(GameState* game, Card* attacker, void* target) {
    if (game->polishSystem) {
        PolishSystem* polish = (PolishSystem*)game->polishSystem;

        // Screen shake based on attack power
        float intensity = (float)attacker->attack / 10.0f;
        TriggerScreenShake(polish, intensity, 0.2f);
    }

    (void)target; // Unused for now
}

void OnDamage(GameState* game, void* target, int damage) {
    if (game->polishSystem) {
        PolishSystem* polish = (PolishSystem*)game->polishSystem;

        // Stronger shake for high damage
        if (damage >= 5) {
            TriggerScreenShake(polish, 0.8f, 0.4f);
        }
    }

    (void)target; // Unused for now
}

void OnTurnStart(GameState* game) {
    if (game->polishSystem) {
        PolishSystem* polish = (PolishSystem*)game->polishSystem;

        // Subtle camera adjustment
        Vector3 newCameraPos = game->camera.position;
        newCameraPos.y += (game->activePlayer == 0) ? -1.0f : 1.0f;
        AnimateCameraTo(polish, newCameraPos, 1.0f);
    }
}