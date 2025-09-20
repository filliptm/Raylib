#ifndef POLISH_H
#define POLISH_H

#include "types.h"
#include "game_state.h"
#include "performance.h"

// Quality of life features
typedef struct {
    bool showFPS;
    bool showDebugInfo;
    bool autoEndTurn;
    bool highlightTargets;
    bool showCardTooltips;
    bool enableParticles;
    bool enableScreenShake;
    bool enableSoundEffects;
    float masterVolume;
    float sfxVolume;
    float musicVolume;
} GameSettings;

// Debug information
typedef struct {
    bool showCollisionBoxes;
    bool showCardStats;
    bool showAIThinking;
    bool showNetworkStats;
    bool showMemoryUsage;
    bool logGameEvents;
} DebugSettings;

// Polish systems
typedef struct {
    GameSettings gameSettings;
    DebugSettings debugSettings;
    PerformanceMetrics* performanceMetrics;

    // Visual polish
    float screenShakeIntensity;
    float screenShakeTime;
    Vector3 screenShakeOffset;

    // Particle system
    bool particlesEnabled;
    int activeParticles;

    // Camera effects
    bool cameraAnimating;
    Vector3 cameraTargetPos;
    Vector3 cameraStartPos;
    float cameraAnimTime;
    float cameraAnimDuration;
} PolishSystem;

// Initialization
void InitializePolish(PolishSystem* polish);
void CleanupPolish(PolishSystem* polish);

// Settings management
void LoadGameSettings(GameSettings* settings, const char* filename);
void SaveGameSettings(const GameSettings* settings, const char* filename);
void ApplyGameSettings(const GameSettings* settings);

// Debug functions
void ToggleDebugInfo(PolishSystem* polish);
void DrawDebugInfo(const PolishSystem* polish, const GameState* game);
void DrawPerformanceOverlay(const PolishSystem* polish);

// Visual effects
void TriggerScreenShake(PolishSystem* polish, float intensity, float duration);
void UpdateScreenShake(PolishSystem* polish, float deltaTime);
Vector3 GetScreenShakeOffset(const PolishSystem* polish);

// Camera effects
void AnimateCameraTo(PolishSystem* polish, Vector3 targetPosition, float duration);
void UpdateCameraAnimation(PolishSystem* polish, GameState* game, float deltaTime);

// Game polish
void UpdateGamePolish(PolishSystem* polish, GameState* game, float deltaTime);
void DrawGamePolish(const PolishSystem* polish, const GameState* game);

// Input helpers
void HandleDebugInput(PolishSystem* polish);
void HandleSettingsInput(PolishSystem* polish);

// Quality of life
void AutoHighlightValidTargets(GameState* game, Card* selectedCard);
void ShowCardPreview(const Card* card, Vector2 mousePos);
void DisplayGameTips(const GameState* game);

#endif // POLISH_H