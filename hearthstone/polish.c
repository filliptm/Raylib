#include "polish.h"
#include "combat.h"
#include "ai.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Initialize polish system
void InitializePolish(PolishSystem* polish) {
    memset(polish, 0, sizeof(PolishSystem));

    // Default settings
    polish->gameSettings.showFPS = false;
    polish->gameSettings.showDebugInfo = false;
    polish->gameSettings.autoEndTurn = false;
    polish->gameSettings.highlightTargets = true;
    polish->gameSettings.showCardTooltips = true;
    polish->gameSettings.enableParticles = true;
    polish->gameSettings.enableScreenShake = true;
    polish->gameSettings.enableSoundEffects = true;
    polish->gameSettings.masterVolume = 1.0f;
    polish->gameSettings.sfxVolume = 0.8f;
    polish->gameSettings.musicVolume = 0.6f;

    // Debug settings off by default
    memset(&polish->debugSettings, 0, sizeof(DebugSettings));

    // Initialize performance metrics
    polish->performanceMetrics = malloc(sizeof(PerformanceMetrics));
    if (polish->performanceMetrics) {
        InitializePerformance(polish->performanceMetrics);
    }
}

// Cleanup polish system
void CleanupPolish(PolishSystem* polish) {
    if (polish->performanceMetrics) {
        CleanupPerformance(polish->performanceMetrics);
        free(polish->performanceMetrics);
        polish->performanceMetrics = NULL;
    }
}

// Load game settings from file
void LoadGameSettings(GameSettings* settings, const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        // Use defaults
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char key[128], value[128];
        if (sscanf(line, "%127[^=]=%127s", key, value) == 2) {
            if (strcmp(key, "showFPS") == 0) {
                settings->showFPS = (strcmp(value, "true") == 0);
            } else if (strcmp(key, "masterVolume") == 0) {
                settings->masterVolume = atof(value);
            } else if (strcmp(key, "sfxVolume") == 0) {
                settings->sfxVolume = atof(value);
            } else if (strcmp(key, "musicVolume") == 0) {
                settings->musicVolume = atof(value);
            }
            // Add more settings as needed
        }
    }

    fclose(file);
}

// Save game settings to file
void SaveGameSettings(const GameSettings* settings, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        return;
    }

    fprintf(file, "showFPS=%s\n", settings->showFPS ? "true" : "false");
    fprintf(file, "showDebugInfo=%s\n", settings->showDebugInfo ? "true" : "false");
    fprintf(file, "autoEndTurn=%s\n", settings->autoEndTurn ? "true" : "false");
    fprintf(file, "highlightTargets=%s\n", settings->highlightTargets ? "true" : "false");
    fprintf(file, "showCardTooltips=%s\n", settings->showCardTooltips ? "true" : "false");
    fprintf(file, "enableParticles=%s\n", settings->enableParticles ? "true" : "false");
    fprintf(file, "enableScreenShake=%s\n", settings->enableScreenShake ? "true" : "false");
    fprintf(file, "enableSoundEffects=%s\n", settings->enableSoundEffects ? "true" : "false");
    fprintf(file, "masterVolume=%.2f\n", settings->masterVolume);
    fprintf(file, "sfxVolume=%.2f\n", settings->sfxVolume);
    fprintf(file, "musicVolume=%.2f\n", settings->musicVolume);

    fclose(file);
}

// Apply game settings
void ApplyGameSettings(const GameSettings* settings) {
    // Apply volume settings
    SetMasterVolume(settings->masterVolume);
    // Other settings would be applied to their respective systems
}

// Toggle debug info
void ToggleDebugInfo(PolishSystem* polish) {
    polish->gameSettings.showDebugInfo = !polish->gameSettings.showDebugInfo;
}

// Draw debug information
void DrawDebugInfo(const PolishSystem* polish, const GameState* game) {
    if (!polish->gameSettings.showDebugInfo) return;

    int y = 10;
    const int lineHeight = 20;

    DrawText("=== DEBUG INFO ===", 10, y, 16, LIME);
    y += lineHeight * 2;

    // Game state info
    DrawText(TextFormat("Turn: %d", game->turnNumber), 10, y, 14, WHITE);
    y += lineHeight;
    DrawText(TextFormat("Active Player: %d", game->activePlayer), 10, y, 14, WHITE);
    y += lineHeight;
    DrawText(TextFormat("Phase: %d", (int)game->turnPhase), 10, y, 14, WHITE);
    y += lineHeight;

    // Player info
    for (int i = 0; i < 2; i++) {
        const Player* player = &game->players[i];
        DrawText(TextFormat("P%d: %d/%d HP, %d/%d mana", i,
                          player->health, player->maxHealth,
                          player->mana, player->maxMana), 10, y, 14, WHITE);
        y += lineHeight;
        DrawText(TextFormat("  Hand: %d, Board: %d", player->handCount, player->boardCount), 10, y, 14, WHITE);
        y += lineHeight;
    }

    // Effects
    DrawText(TextFormat("Active Effects: %d", game->activeEffectsCount), 10, y, 14, WHITE);
    y += lineHeight;

    // AI info
    if (game->vsAI) {
        DrawText("AI: Enabled", 10, y, 14, YELLOW);
        y += lineHeight;

        // Show AI difficulty if available
        if (game->aiPlayer) {
            const char* difficulty = "Unknown";
            AIPlayer* ai = (AIPlayer*)game->aiPlayer;
            switch (ai->difficulty) {
                case AI_DIFFICULTY_EASY: difficulty = "Easy"; break;
                case AI_DIFFICULTY_MEDIUM: difficulty = "Medium"; break;
                case AI_DIFFICULTY_HARD: difficulty = "Hard"; break;
            }
            char difficultyText[64];
            snprintf(difficultyText, sizeof(difficultyText), "AI Difficulty: %s", difficulty);
            DrawText(difficultyText, 10, y, 14, YELLOW);
            y += lineHeight;
        }
    }

    // Network info
    if (game->isNetworkGame) {
        DrawText("Network: Enabled", 10, y, 14, BLUE);
        y += lineHeight;
    }
}

// Draw performance overlay
void DrawPerformanceOverlay(const PolishSystem* polish) {
    if (!polish->gameSettings.showFPS && !polish->gameSettings.showDebugInfo) return;

    if (polish->performanceMetrics) {
        int x = GetScreenWidth() - 200;
        int y = 10;

        if (polish->gameSettings.showFPS) {
            DrawText(TextFormat("FPS: %.1f", polish->performanceMetrics->fps), x, y, 16, GREEN);
            y += 20;
            DrawText(TextFormat("Frame: %.2fms", polish->performanceMetrics->frameTime * 1000), x, y, 14, GREEN);
            y += 18;
        }

        if (polish->gameSettings.showDebugInfo) {
            DrawText(TextFormat("Update: %.2fms", polish->performanceMetrics->updateTime * 1000), x, y, 12, WHITE);
            y += 16;
            DrawText(TextFormat("Render: %.2fms", polish->performanceMetrics->renderTime * 1000), x, y, 12, WHITE);
            y += 16;
            DrawText(TextFormat("AI: %.2fms", polish->performanceMetrics->aiTime * 1000), x, y, 12, WHITE);
            y += 16;
            DrawText(TextFormat("Net: %.2fms", polish->performanceMetrics->networkTime * 1000), x, y, 12, WHITE);
        }
    }
}

// Trigger screen shake
void TriggerScreenShake(PolishSystem* polish, float intensity, float duration) {
    if (!polish->gameSettings.enableScreenShake) return;

    polish->screenShakeIntensity = intensity;
    polish->screenShakeTime = duration;
}

// Update screen shake
void UpdateScreenShake(PolishSystem* polish, float deltaTime) {
    if (polish->screenShakeTime <= 0.0f) {
        polish->screenShakeOffset = (Vector3){0, 0, 0};
        return;
    }

    polish->screenShakeTime -= deltaTime;

    float intensity = polish->screenShakeIntensity * (polish->screenShakeTime / 1.0f);
    polish->screenShakeOffset.x = (float)(rand() % 200 - 100) / 100.0f * intensity;
    polish->screenShakeOffset.y = (float)(rand() % 200 - 100) / 100.0f * intensity;
    polish->screenShakeOffset.z = (float)(rand() % 200 - 100) / 100.0f * intensity;
}

// Get screen shake offset
Vector3 GetScreenShakeOffset(const PolishSystem* polish) {
    return polish->screenShakeOffset;
}

// Animate camera to position
void AnimateCameraTo(PolishSystem* polish, Vector3 targetPosition, float duration) {
    polish->cameraTargetPos = targetPosition;
    polish->cameraAnimDuration = duration;
    polish->cameraAnimTime = 0.0f;
    polish->cameraAnimating = true;
}

// Update camera animation
void UpdateCameraAnimation(PolishSystem* polish, GameState* game, float deltaTime) {
    if (!polish->cameraAnimating) return;

    polish->cameraAnimTime += deltaTime;

    if (polish->cameraAnimTime >= polish->cameraAnimDuration) {
        game->camera.position = polish->cameraTargetPos;
        polish->cameraAnimating = false;
    } else {
        float t = polish->cameraAnimTime / polish->cameraAnimDuration;
        // Smooth interpolation
        t = t * t * (3.0f - 2.0f * t);

        game->camera.position = Vector3Lerp(polish->cameraStartPos, polish->cameraTargetPos, t);
    }
}

// Update game polish
void UpdateGamePolish(PolishSystem* polish, GameState* game, float deltaTime) {
    // Update performance metrics
    if (polish->performanceMetrics) {
        UpdatePerformanceMetrics(polish->performanceMetrics, deltaTime);
    }

    // Update screen shake
    UpdateScreenShake(polish, deltaTime);

    // Update camera animation
    UpdateCameraAnimation(polish, game, deltaTime);

    // Apply camera shake to camera
    Vector3 shakeOffset = GetScreenShakeOffset(polish);
    game->camera.position = Vector3Add(game->camera.position, shakeOffset);

    // Handle input
    HandleDebugInput(polish);

    // Periodic optimization
    static float optimizeTimer = 0.0f;
    optimizeTimer += deltaTime;
    if (optimizeTimer >= 1.0f) {
        OptimizeGameState(game);
        OptimizeRendering(game);
        OptimizeMemoryUsage(game);
        optimizeTimer = 0.0f;
    }
}

// Draw game polish
void DrawGamePolish(const PolishSystem* polish, const GameState* game) {
    DrawPerformanceOverlay(polish);
    DrawDebugInfo(polish, game);

    // Always show AI indicator when playing vs AI
    if (game->vsAI) {
        int x = GetScreenWidth() - 150;
        int y = GetScreenHeight() - 60;

        // AI indicator background
        DrawRectangle(x - 5, y - 5, 140, 50, Fade(BLACK, 0.7f));
        DrawRectangleLines(x - 5, y - 5, 140, 50, YELLOW);

        // AI text
        DrawText("VS AI", x, y, 16, YELLOW);

        // Show difficulty if available
        if (game->aiPlayer) {
            const char* difficulty = "Unknown";
            AIPlayer* ai = (AIPlayer*)game->aiPlayer;
            switch (ai->difficulty) {
                case AI_DIFFICULTY_EASY: difficulty = "Easy"; break;
                case AI_DIFFICULTY_MEDIUM: difficulty = "Medium"; break;
                case AI_DIFFICULTY_HARD: difficulty = "Hard"; break;
            }
            DrawText(difficulty, x, y + 20, 14, WHITE);
        }

        // Show if it's AI's turn
        if (game->activePlayer == 1 && game->players[1].isActivePlayer) {
            DrawText("AI Turn", x, y + 35, 12, SKYBLUE);
        }
    }
}

// Handle debug input
void HandleDebugInput(PolishSystem* polish) {
    // F1 - Toggle FPS
    if (IsKeyPressed(KEY_F1)) {
        polish->gameSettings.showFPS = !polish->gameSettings.showFPS;
    }

    // F2 - Toggle debug info
    if (IsKeyPressed(KEY_F2)) {
        ToggleDebugInfo(polish);
    }

    // F3 - Log performance stats
    if (IsKeyPressed(KEY_F3) && polish->performanceMetrics) {
        LogPerformanceStats(polish->performanceMetrics);
    }

    // F4 - Toggle collision boxes
    if (IsKeyPressed(KEY_F4)) {
        polish->debugSettings.showCollisionBoxes = !polish->debugSettings.showCollisionBoxes;
    }
}

// Handle settings input
void HandleSettingsInput(PolishSystem* polish) {
    // Volume controls
    if (IsKeyDown(KEY_LEFT_CONTROL)) {
        if (IsKeyPressed(KEY_EQUAL)) {
            polish->gameSettings.masterVolume = fminf(1.0f, polish->gameSettings.masterVolume + 0.1f);
            ApplyGameSettings(&polish->gameSettings);
        }
        if (IsKeyPressed(KEY_MINUS)) {
            polish->gameSettings.masterVolume = fmaxf(0.0f, polish->gameSettings.masterVolume - 0.1f);
            ApplyGameSettings(&polish->gameSettings);
        }
    }
}

// Auto highlight valid targets
void AutoHighlightValidTargets(GameState* game, Card* selectedCard) {
    if (!selectedCard) return;

    // Clear previous highlights
    for (int p = 0; p < 2; p++) {
        for (int i = 0; i < game->players[p].boardCount; i++) {
            game->players[p].board[i].isTargeted = false;
        }
        game->players[p].isTargeted = false;
    }

    // Highlight valid targets based on card type
    if (selectedCard->type == CARD_TYPE_SPELL && selectedCard->spellDamage > 0) {
        // Damage spells can target any character
        for (int p = 0; p < 2; p++) {
            for (int i = 0; i < game->players[p].boardCount; i++) {
                game->players[p].board[i].isTargeted = true;
            }
            game->players[p].isTargeted = true;
        }
    } else if (selectedCard->onBoard && CanAttack(selectedCard)) {
        // Attacking minion - highlight valid attack targets
        Player* enemyPlayer = &game->players[1 - selectedCard->ownerPlayer];

        // Check for taunt minions
        bool hasTaunt = false;
        for (int i = 0; i < enemyPlayer->boardCount; i++) {
            if (enemyPlayer->board[i].taunt) {
                enemyPlayer->board[i].isTargeted = true;
                hasTaunt = true;
            }
        }

        // If no taunt, can target any enemy character
        if (!hasTaunt) {
            for (int i = 0; i < enemyPlayer->boardCount; i++) {
                enemyPlayer->board[i].isTargeted = true;
            }
            enemyPlayer->isTargeted = true;
        }
    }
}

// Show card preview
void ShowCardPreview(const Card* card, Vector2 mousePos) {
    if (!card) return;

    int panelWidth = 300;
    int panelHeight = 200;
    int x = (int)mousePos.x + 20;
    int y = (int)mousePos.y - panelHeight - 10;

    // Keep panel on screen
    if (x + panelWidth > GetScreenWidth()) {
        x = (int)mousePos.x - panelWidth - 20;
    }
    if (y < 0) {
        y = (int)mousePos.y + 20;
    }

    // Draw panel background
    DrawRectangle(x, y, panelWidth, panelHeight, Fade(BLACK, 0.8f));
    DrawRectangleLines(x, y, panelWidth, panelHeight, WHITE);

    // Draw card info
    int textY = y + 10;
    DrawText(card->name, x + 10, textY, 16, WHITE);
    textY += 20;

    DrawText(TextFormat("Cost: %d", card->cost), x + 10, textY, 14, YELLOW);
    textY += 18;

    if (card->type == CARD_TYPE_MINION) {
        DrawText(TextFormat("Attack: %d  Health: %d", card->attack, card->health), x + 10, textY, 14, WHITE);
        textY += 18;
    }

    // Draw description
    DrawText("Description:", x + 10, textY, 12, GRAY);
    textY += 16;
    DrawText(card->description, x + 10, textY, 12, LIGHTGRAY);
}

// Display game tips
void DisplayGameTips(const GameState* game) {
    if (game->turnNumber < 3) {
        const char* tips[] = {
            "Click and drag cards to play them",
            "Click on your minions to attack with them",
            "Click 'End Turn' when you're done",
            "Use your hero power by clicking the hero portrait"
        };

        int tip = game->turnNumber % 4;
        DrawText(tips[tip], 10, GetScreenHeight() - 30, 14, YELLOW);
    }
}