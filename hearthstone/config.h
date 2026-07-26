#ifndef CONFIG_H
#define CONFIG_H

#include "errors.h"
#include <stdbool.h>

typedef struct {
    // Display settings
    int screen_width;
    int screen_height;
    int target_fps;
    bool fullscreen;
    bool vsync;
    
    // Graphics settings
    float card_scale;
    float board_scale;
    float animation_speed;
    int shadow_quality;  // 0: off, 1: low, 2: high
    bool enable_particles;
    
    // Audio settings
    float master_volume;
    float sfx_volume;
    float music_volume;
    bool enable_audio;
    
    // Gameplay settings
    float turn_timer;
    float action_delay;
    bool auto_end_turn;
    bool show_tooltips;
    
    // Debug settings
    bool debug_mode;
    bool show_fps;
    bool show_collision_boxes;
    
    // Network settings
    int network_port;
    int connection_timeout;
    
    // Paths
    char resource_path[256];
    char save_path[256];
    char config_path[256];
} GameConfig;

// Configuration management
GameError InitConfig(GameConfig* config);
GameError LoadConfig(GameConfig* config, const char* filepath);
GameError SaveConfig(const GameConfig* config, const char* filepath);
void SetDefaultConfig(GameConfig* config);

// Config value getters with validation
float GetConfigCardScale(const GameConfig* config);
float GetConfigAnimationSpeed(const GameConfig* config);
int GetConfigScreenWidth(const GameConfig* config);
int GetConfigScreenHeight(const GameConfig* config);

// Runtime configuration updates
GameError UpdateScreenSize(GameConfig* config, int width, int height);
GameError UpdateVolume(GameConfig* config, float master, float sfx, float music);
GameError UpdateGraphicsQuality(GameConfig* config, int shadow_quality, bool particles);

#endif