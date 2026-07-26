#ifndef SAVE_SYSTEM_H
#define SAVE_SYSTEM_H

#include "../common.h"
#include "../errors.h"
#include "../types.h"
#include <time.h>

#define MAX_SAVE_NAME 64
#define MAX_SAVE_SLOTS 10
#define SAVE_FILE_EXTENSION ".hsv"
#define SAVE_DIRECTORY "saves/"

typedef struct {
    char name[MAX_SAVE_NAME];
    char filename[MAX_SAVE_NAME + 16];
    time_t timestamp;
    int turn_number;
    int active_player;
    bool is_valid;
} SaveSlot;

typedef struct {
    SaveSlot slots[MAX_SAVE_SLOTS];
    int slot_count;
    char save_directory[256];
} SaveManager;

// Save system initialization
GameError InitSaveSystem(SaveManager* manager, const char* save_dir);
void CleanupSaveSystem(SaveManager* manager);

// Save/Load operations
GameError SaveGameState(SaveManager* manager, GameState* game, const char* save_name);
GameError LoadGameState(SaveManager* manager, GameState* game, const char* save_name);
GameError DeleteSave(SaveManager* manager, const char* save_name);

// Save slot management
GameError RefreshSaveSlots(SaveManager* manager);
SaveSlot* GetSaveSlot(SaveManager* manager, const char* save_name);
SaveSlot* GetSaveSlotByIndex(SaveManager* manager, int index);
bool IsValidSaveFile(const char* filepath);

// Quick save/load
GameError QuickSave(SaveManager* manager, GameState* game);
GameError QuickLoad(SaveManager* manager, GameState* game);

// Auto save
GameError AutoSave(SaveManager* manager, GameState* game);
GameError LoadAutoSave(SaveManager* manager, GameState* game);

// Utility functions
GameError CreateSaveDirectory(const char* directory);
char* GenerateSaveFilename(const char* save_name);
bool ValidateSaveName(const char* save_name);

#endif // SAVE_SYSTEM_H