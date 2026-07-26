#ifndef RESOURCES_H
#define RESOURCES_H

#include "raylib.h"
#include "errors.h"

#define MAX_TEXTURES 100
#define MAX_MODELS 20
#define MAX_SOUNDS 50
#define MAX_FONTS 10

typedef struct {
    Texture2D* textures;
    int texture_count;
    int texture_capacity;
    
    Model* models;
    int model_count;
    int model_capacity;
    
    Sound* sounds;
    int sound_count;
    int sound_capacity;
    
    Font* fonts;
    int font_count;
    int font_capacity;
    
    // Hash maps for quick lookup
    char** texture_names;
    char** model_names;
    char** sound_names;
    char** font_names;
} Resources;

// Resource management functions
GameError InitResources(Resources* resources);
void CleanupResources(Resources* resources);

// Texture management
GameError LoadTextureResource(Resources* resources, const char* path, const char* name);
Texture2D* GetTexture(Resources* resources, const char* name);
void UnloadTextureResource(Resources* resources, const char* name);

// Model management
GameError LoadModelResource(Resources* resources, const char* path, const char* name);
Model* GetModel(Resources* resources, const char* name);
void UnloadModelResource(Resources* resources, const char* name);

// Sound management
GameError LoadSoundResource(Resources* resources, const char* path, const char* name);
Sound* GetSound(Resources* resources, const char* name);
void UnloadSoundResource(Resources* resources, const char* name);

// Font management
GameError LoadFontResource(Resources* resources, const char* path, const char* name);
Font* GetFont(Resources* resources, const char* name);
void UnloadFontResource(Resources* resources, const char* name);

// Batch loading from configuration
GameError LoadAllResources(Resources* resources, const char* resource_config_file);

#endif