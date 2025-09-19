#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN_SCREEN_WIDTH 800
#define MIN_SCREEN_HEIGHT 600
#define MAX_SCREEN_WIDTH 3840
#define MAX_SCREEN_HEIGHT 2160

GameError InitConfig(GameConfig* config) {
    if (!config) return GAME_ERROR_INVALID_PARAMETER;
    
    SetDefaultConfig(config);
    return GAME_OK;
}

void SetDefaultConfig(GameConfig* config) {
    if (!config) return;
    
    // Display settings
    config->screen_width = 1920;
    config->screen_height = 1080;
    config->target_fps = 60;
    config->fullscreen = false;
    config->vsync = true;
    
    // Graphics settings
    config->card_scale = 1.0f;
    config->board_scale = 1.0f;
    config->animation_speed = 1.0f;
    config->shadow_quality = 1;
    config->enable_particles = true;
    
    // Audio settings
    config->master_volume = 1.0f;
    config->sfx_volume = 0.8f;
    config->music_volume = 0.6f;
    config->enable_audio = true;
    
    // Gameplay settings
    config->turn_timer = 75.0f;
    config->action_delay = 0.5f;
    config->auto_end_turn = false;
    config->show_tooltips = true;
    
    // Debug settings
    config->debug_mode = false;
    config->show_fps = false;
    config->show_collision_boxes = false;
    
    // Network settings
    config->network_port = 7777;
    config->connection_timeout = 30;
    
    // Paths
    strcpy(config->resource_path, "./resources/");
    strcpy(config->save_path, "./saves/");
    strcpy(config->config_path, "./config/");
}

GameError LoadConfig(GameConfig* config, const char* filepath) {
    if (!config || !filepath) return GAME_ERROR_INVALID_PARAMETER;
    
    FILE* file = fopen(filepath, "r");
    if (!file) {
        LogError(GAME_ERROR_FILE_NOT_FOUND, "LoadConfig");
        return GAME_ERROR_FILE_NOT_FOUND;
    }
    
    char line[512];
    while (fgets(line, sizeof(line), file)) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        
        // Skip empty lines and comments
        if (line[0] == '\0' || line[0] == '#') continue;
        
        char key[256];
        char value[256];
        if (sscanf(line, "%s = %s", key, value) != 2) continue;
        
        // Parse configuration values
        if (strcmp(key, "screen_width") == 0) {
            config->screen_width = atoi(value);
        } else if (strcmp(key, "screen_height") == 0) {
            config->screen_height = atoi(value);
        } else if (strcmp(key, "target_fps") == 0) {
            config->target_fps = atoi(value);
        } else if (strcmp(key, "fullscreen") == 0) {
            config->fullscreen = (strcmp(value, "true") == 0);
        } else if (strcmp(key, "vsync") == 0) {
            config->vsync = (strcmp(value, "true") == 0);
        } else if (strcmp(key, "card_scale") == 0) {
            config->card_scale = atof(value);
        } else if (strcmp(key, "animation_speed") == 0) {
            config->animation_speed = atof(value);
        } else if (strcmp(key, "master_volume") == 0) {
            config->master_volume = atof(value);
        } else if (strcmp(key, "sfx_volume") == 0) {
            config->sfx_volume = atof(value);
        } else if (strcmp(key, "music_volume") == 0) {
            config->music_volume = atof(value);
        } else if (strcmp(key, "resource_path") == 0) {
            strcpy(config->resource_path, value);
        } else if (strcmp(key, "save_path") == 0) {
            strcpy(config->save_path, value);
        }
    }
    
    fclose(file);
    return GAME_OK;
}

GameError SaveConfig(const GameConfig* config, const char* filepath) {
    if (!config || !filepath) return GAME_ERROR_INVALID_PARAMETER;
    
    FILE* file = fopen(filepath, "w");
    if (!file) {
        LogError(GAME_ERROR_SAVE_FAILED, "SaveConfig");
        return GAME_ERROR_SAVE_FAILED;
    }
    
    fprintf(file, "# Hearthstone Clone Configuration\n\n");
    
    fprintf(file, "# Display Settings\n");
    fprintf(file, "screen_width = %d\n", config->screen_width);
    fprintf(file, "screen_height = %d\n", config->screen_height);
    fprintf(file, "target_fps = %d\n", config->target_fps);
    fprintf(file, "fullscreen = %s\n", config->fullscreen ? "true" : "false");
    fprintf(file, "vsync = %s\n", config->vsync ? "true" : "false");
    fprintf(file, "\n");
    
    fprintf(file, "# Graphics Settings\n");
    fprintf(file, "card_scale = %.2f\n", config->card_scale);
    fprintf(file, "animation_speed = %.2f\n", config->animation_speed);
    fprintf(file, "shadow_quality = %d\n", config->shadow_quality);
    fprintf(file, "enable_particles = %s\n", config->enable_particles ? "true" : "false");
    fprintf(file, "\n");
    
    fprintf(file, "# Audio Settings\n");
    fprintf(file, "master_volume = %.2f\n", config->master_volume);
    fprintf(file, "sfx_volume = %.2f\n", config->sfx_volume);
    fprintf(file, "music_volume = %.2f\n", config->music_volume);
    fprintf(file, "enable_audio = %s\n", config->enable_audio ? "true" : "false");
    fprintf(file, "\n");
    
    fprintf(file, "# Paths\n");
    fprintf(file, "resource_path = %s\n", config->resource_path);
    fprintf(file, "save_path = %s\n", config->save_path);
    
    fclose(file);
    return GAME_OK;
}

// Config value getters with validation
float GetConfigCardScale(const GameConfig* config) {
    if (!config) return 1.0f;
    return (config->card_scale > 0.1f && config->card_scale < 5.0f) ? config->card_scale : 1.0f;
}

float GetConfigAnimationSpeed(const GameConfig* config) {
    if (!config) return 1.0f;
    return (config->animation_speed > 0.1f && config->animation_speed < 10.0f) ? config->animation_speed : 1.0f;
}

int GetConfigScreenWidth(const GameConfig* config) {
    if (!config) return 1920;
    if (config->screen_width < MIN_SCREEN_WIDTH) return MIN_SCREEN_WIDTH;
    if (config->screen_width > MAX_SCREEN_WIDTH) return MAX_SCREEN_WIDTH;
    return config->screen_width;
}

int GetConfigScreenHeight(const GameConfig* config) {
    if (!config) return 1080;
    if (config->screen_height < MIN_SCREEN_HEIGHT) return MIN_SCREEN_HEIGHT;
    if (config->screen_height > MAX_SCREEN_HEIGHT) return MAX_SCREEN_HEIGHT;
    return config->screen_height;
}

// Runtime configuration updates
GameError UpdateScreenSize(GameConfig* config, int width, int height) {
    if (!config) return GAME_ERROR_INVALID_PARAMETER;
    
    if (width < MIN_SCREEN_WIDTH || width > MAX_SCREEN_WIDTH ||
        height < MIN_SCREEN_HEIGHT || height > MAX_SCREEN_HEIGHT) {
        return GAME_ERROR_INVALID_PARAMETER;
    }
    
    config->screen_width = width;
    config->screen_height = height;
    return GAME_OK;
}

GameError UpdateVolume(GameConfig* config, float master, float sfx, float music) {
    if (!config) return GAME_ERROR_INVALID_PARAMETER;
    
    if (master < 0.0f || master > 1.0f ||
        sfx < 0.0f || sfx > 1.0f ||
        music < 0.0f || music > 1.0f) {
        return GAME_ERROR_INVALID_PARAMETER;
    }
    
    config->master_volume = master;
    config->sfx_volume = sfx;
    config->music_volume = music;
    return GAME_OK;
}

GameError UpdateGraphicsQuality(GameConfig* config, int shadow_quality, bool particles) {
    if (!config) return GAME_ERROR_INVALID_PARAMETER;
    
    if (shadow_quality < 0 || shadow_quality > 2) {
        return GAME_ERROR_INVALID_PARAMETER;
    }
    
    config->shadow_quality = shadow_quality;
    config->enable_particles = particles;
    return GAME_OK;
}