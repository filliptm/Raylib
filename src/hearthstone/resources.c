#include "resources.h"
#include <stdlib.h>
#include <string.h>

static int FindResourceIndex(char** names, int count, const char* name) {
    for (int i = 0; i < count; i++) {
        if (names[i] && strcmp(names[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

GameError InitResources(Resources* resources) {
    if (!resources) return GAME_ERROR_INVALID_PARAMETER;
    
    memset(resources, 0, sizeof(Resources));
    
    // Initialize textures
    resources->texture_capacity = MAX_TEXTURES;
    resources->textures = calloc(resources->texture_capacity, sizeof(Texture2D));
    resources->texture_names = calloc(resources->texture_capacity, sizeof(char*));
    if (!resources->textures || !resources->texture_names) {
        CleanupResources(resources);
        return GAME_ERROR_OUT_OF_MEMORY;
    }
    
    // Initialize models
    resources->model_capacity = MAX_MODELS;
    resources->models = calloc(resources->model_capacity, sizeof(Model));
    resources->model_names = calloc(resources->model_capacity, sizeof(char*));
    if (!resources->models || !resources->model_names) {
        CleanupResources(resources);
        return GAME_ERROR_OUT_OF_MEMORY;
    }
    
    // Initialize sounds
    resources->sound_capacity = MAX_SOUNDS;
    resources->sounds = calloc(resources->sound_capacity, sizeof(Sound));
    resources->sound_names = calloc(resources->sound_capacity, sizeof(char*));
    if (!resources->sounds || !resources->sound_names) {
        CleanupResources(resources);
        return GAME_ERROR_OUT_OF_MEMORY;
    }
    
    // Initialize fonts
    resources->font_capacity = MAX_FONTS;
    resources->fonts = calloc(resources->font_capacity, sizeof(Font));
    resources->font_names = calloc(resources->font_capacity, sizeof(char*));
    if (!resources->fonts || !resources->font_names) {
        CleanupResources(resources);
        return GAME_ERROR_OUT_OF_MEMORY;
    }
    
    return GAME_OK;
}

void CleanupResources(Resources* resources) {
    if (!resources) return;
    
    // Cleanup textures
    if (resources->textures) {
        for (int i = 0; i < resources->texture_count; i++) {
            UnloadTexture(resources->textures[i]);
        }
        free(resources->textures);
    }
    
    if (resources->texture_names) {
        for (int i = 0; i < resources->texture_capacity; i++) {
            free(resources->texture_names[i]);
        }
        free(resources->texture_names);
    }
    
    // Cleanup models
    if (resources->models) {
        for (int i = 0; i < resources->model_count; i++) {
            UnloadModel(resources->models[i]);
        }
        free(resources->models);
    }
    
    if (resources->model_names) {
        for (int i = 0; i < resources->model_capacity; i++) {
            free(resources->model_names[i]);
        }
        free(resources->model_names);
    }
    
    // Cleanup sounds
    if (resources->sounds) {
        for (int i = 0; i < resources->sound_count; i++) {
            UnloadSound(resources->sounds[i]);
        }
        free(resources->sounds);
    }
    
    if (resources->sound_names) {
        for (int i = 0; i < resources->sound_capacity; i++) {
            free(resources->sound_names[i]);
        }
        free(resources->sound_names);
    }
    
    // Cleanup fonts
    if (resources->fonts) {
        for (int i = 0; i < resources->font_count; i++) {
            UnloadFont(resources->fonts[i]);
        }
        free(resources->fonts);
    }
    
    if (resources->font_names) {
        for (int i = 0; i < resources->font_capacity; i++) {
            free(resources->font_names[i]);
        }
        free(resources->font_names);
    }
    
    memset(resources, 0, sizeof(Resources));
}

// Texture management
GameError LoadTextureResource(Resources* resources, const char* path, const char* name) {
    if (!resources || !path || !name) return GAME_ERROR_INVALID_PARAMETER;
    
    // Check if already loaded
    if (FindResourceIndex(resources->texture_names, resources->texture_count, name) >= 0) {
        return GAME_ERROR_RESOURCE_ALREADY_EXISTS;
    }
    
    if (resources->texture_count >= resources->texture_capacity) {
        return GAME_ERROR_OUT_OF_MEMORY;
    }
    
    Texture2D texture = LoadTexture(path);
    // Note: In newer raylib versions, you'd check texture.id != 0
    if (texture.id == 0) {
        return GAME_ERROR_FILE_NOT_FOUND;
    }
    
    int index = resources->texture_count;
    resources->textures[index] = texture;
    resources->texture_names[index] = strdup(name);
    resources->texture_count++;
    
    return GAME_OK;
}

Texture2D* GetTexture(Resources* resources, const char* name) {
    if (!resources || !name) return NULL;
    
    int index = FindResourceIndex(resources->texture_names, resources->texture_count, name);
    if (index < 0) return NULL;
    
    return &resources->textures[index];
}

void UnloadTextureResource(Resources* resources, const char* name) {
    if (!resources || !name) return;
    
    int index = FindResourceIndex(resources->texture_names, resources->texture_count, name);
    if (index < 0) return;
    
    UnloadTexture(resources->textures[index]);
    free(resources->texture_names[index]);
    
    // Shift remaining textures
    for (int i = index; i < resources->texture_count - 1; i++) {
        resources->textures[i] = resources->textures[i + 1];
        resources->texture_names[i] = resources->texture_names[i + 1];
    }
    
    resources->texture_count--;
}

// Model management
GameError LoadModelResource(Resources* resources, const char* path, const char* name) {
    if (!resources || !path || !name) return GAME_ERROR_INVALID_PARAMETER;
    
    if (FindResourceIndex(resources->model_names, resources->model_count, name) >= 0) {
        return GAME_ERROR_RESOURCE_ALREADY_EXISTS;
    }
    
    if (resources->model_count >= resources->model_capacity) {
        return GAME_ERROR_OUT_OF_MEMORY;
    }
    
    Model model = LoadModel(path);
    // Note: Basic check for model validity
    if (model.meshCount == 0) {
        return GAME_ERROR_FILE_NOT_FOUND;
    }
    
    int index = resources->model_count;
    resources->models[index] = model;
    resources->model_names[index] = strdup(name);
    resources->model_count++;
    
    return GAME_OK;
}

Model* GetModel(Resources* resources, const char* name) {
    if (!resources || !name) return NULL;
    
    int index = FindResourceIndex(resources->model_names, resources->model_count, name);
    if (index < 0) return NULL;
    
    return &resources->models[index];
}

void UnloadModelResource(Resources* resources, const char* name) {
    if (!resources || !name) return;
    
    int index = FindResourceIndex(resources->model_names, resources->model_count, name);
    if (index < 0) return;
    
    UnloadModel(resources->models[index]);
    free(resources->model_names[index]);
    
    for (int i = index; i < resources->model_count - 1; i++) {
        resources->models[i] = resources->models[i + 1];
        resources->model_names[i] = resources->model_names[i + 1];
    }
    
    resources->model_count--;
}

// Sound management
GameError LoadSoundResource(Resources* resources, const char* path, const char* name) {
    if (!resources || !path || !name) return GAME_ERROR_INVALID_PARAMETER;
    
    if (FindResourceIndex(resources->sound_names, resources->sound_count, name) >= 0) {
        return GAME_ERROR_RESOURCE_ALREADY_EXISTS;
    }
    
    if (resources->sound_count >= resources->sound_capacity) {
        return GAME_ERROR_OUT_OF_MEMORY;
    }
    
    Sound sound = LoadSound(path);
    // Note: Basic check for sound validity
    if (sound.frameCount == 0) {
        return GAME_ERROR_FILE_NOT_FOUND;
    }
    
    int index = resources->sound_count;
    resources->sounds[index] = sound;
    resources->sound_names[index] = strdup(name);
    resources->sound_count++;
    
    return GAME_OK;
}

Sound* GetSound(Resources* resources, const char* name) {
    if (!resources || !name) return NULL;
    
    int index = FindResourceIndex(resources->sound_names, resources->sound_count, name);
    if (index < 0) return NULL;
    
    return &resources->sounds[index];
}

void UnloadSoundResource(Resources* resources, const char* name) {
    if (!resources || !name) return;
    
    int index = FindResourceIndex(resources->sound_names, resources->sound_count, name);
    if (index < 0) return;
    
    UnloadSound(resources->sounds[index]);
    free(resources->sound_names[index]);
    
    for (int i = index; i < resources->sound_count - 1; i++) {
        resources->sounds[i] = resources->sounds[i + 1];
        resources->sound_names[i] = resources->sound_names[i + 1];
    }
    
    resources->sound_count--;
}

// Font management
GameError LoadFontResource(Resources* resources, const char* path, const char* name) {
    if (!resources || !path || !name) return GAME_ERROR_INVALID_PARAMETER;
    
    if (FindResourceIndex(resources->font_names, resources->font_count, name) >= 0) {
        return GAME_ERROR_RESOURCE_ALREADY_EXISTS;
    }
    
    if (resources->font_count >= resources->font_capacity) {
        return GAME_ERROR_OUT_OF_MEMORY;
    }
    
    Font font = LoadFont(path);
    // Note: Basic check for font validity
    if (font.texture.id == 0) {
        return GAME_ERROR_FILE_NOT_FOUND;
    }
    
    int index = resources->font_count;
    resources->fonts[index] = font;
    resources->font_names[index] = strdup(name);
    resources->font_count++;
    
    return GAME_OK;
}

Font* GetFont(Resources* resources, const char* name) {
    if (!resources || !name) return NULL;
    
    int index = FindResourceIndex(resources->font_names, resources->font_count, name);
    if (index < 0) return NULL;
    
    return &resources->fonts[index];
}

void UnloadFontResource(Resources* resources, const char* name) {
    if (!resources || !name) return;
    
    int index = FindResourceIndex(resources->font_names, resources->font_count, name);
    if (index < 0) return;
    
    UnloadFont(resources->fonts[index]);
    free(resources->font_names[index]);
    
    for (int i = index; i < resources->font_count - 1; i++) {
        resources->fonts[i] = resources->fonts[i + 1];
        resources->font_names[i] = resources->font_names[i + 1];
    }
    
    resources->font_count--;
}

// TODO: Implement batch loading from configuration file
GameError LoadAllResources(Resources* resources, const char* resource_config_file) {
    // This would load a JSON or similar config file with all resource paths
    // For now, return OK
    return GAME_OK;
}