#include "save_system.h"
#include "data_manager.h"
#include "../game_state.h"
#include "../player.h"
#include "../card.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>

GameError InitSaveSystem(SaveManager* manager, const char* save_dir) {
    if (!manager) return GAME_ERROR_INVALID_PARAMETER;
    
    memset(manager, 0, sizeof(SaveManager));
    
    if (save_dir) {
        strncpy(manager->save_directory, save_dir, sizeof(manager->save_directory) - 1);
    } else {
        strcpy(manager->save_directory, SAVE_DIRECTORY);
    }
    
    // Create save directory if it doesn't exist
    GameError result = CreateSaveDirectory(manager->save_directory);
    if (result != GAME_OK) return result;
    
    // Refresh save slots
    return RefreshSaveSlots(manager);
}

void CleanupSaveSystem(SaveManager* manager) {
    if (!manager) return;
    memset(manager, 0, sizeof(SaveManager));
}

GameError SaveGameState(SaveManager* manager, GameState* game, const char* save_name) {
    if (!manager || !game || !save_name) return GAME_ERROR_INVALID_PARAMETER;
    
    if (!ValidateSaveName(save_name)) {
        return GAME_ERROR_INVALID_PARAMETER;
    }
    
    char* filename = GenerateSaveFilename(save_name);
    if (!filename) return GAME_ERROR_OUT_OF_MEMORY;
    
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s%s", manager->save_directory, filename);
    
    FILE* file = fopen(filepath, "w");
    if (!file) {
        free(filename);
        return GAME_ERROR_FILE_NOT_FOUND;
    }
    
    // Write JSON header
    fprintf(file, "{\n");
    fprintf(file, "  \"version\": \"1.0\",\n");
    fprintf(file, "  \"timestamp\": %ld,\n", time(NULL));
    fprintf(file, "  \"save_name\": \"%s\",\n", save_name);
    
    // Write game state
    fprintf(file, "  \"game_state\": {\n");
    fprintf(file, "    \"turn_number\": %d,\n", game->turnNumber);
    fprintf(file, "    \"active_player\": %d,\n", game->activePlayer);
    fprintf(file, "    \"is_game_over\": %s,\n", game->gameEnded ? "true" : "false");
    fprintf(file, "    \"winner\": %d,\n", game->winner);
    
    // Write players
    fprintf(file, "    \"players\": [\n");
    for (int p = 0; p < 2; p++) {
        Player* player = &game->players[p];
        fprintf(file, "      {\n");
        fprintf(file, "        \"player_id\": %d,\n", player->playerId);
        fprintf(file, "        \"name\": \"%s\",\n", player->name);
        fprintf(file, "        \"hero_class\": %d,\n", player->heroClass);
        fprintf(file, "        \"health\": %d,\n", player->health);
        fprintf(file, "        \"max_health\": %d,\n", player->maxHealth);
        fprintf(file, "        \"armor\": %d,\n", player->armor);
        fprintf(file, "        \"mana\": %d,\n", player->mana);
        fprintf(file, "        \"max_mana\": %d,\n", player->maxMana);
        fprintf(file, "        \"hero_power_used\": %s,\n", player->heroPowerUsed ? "true" : "false");
        fprintf(file, "        \"has_weapon\": %s,\n", player->hasWeapon ? "true" : "false");
        fprintf(file, "        \"deck_count\": %d,\n", player->deckCount);
        fprintf(file, "        \"hand_count\": %d,\n", player->handCount);
        fprintf(file, "        \"board_count\": %d,\n", player->boardCount);
        fprintf(file, "        \"turn_count\": %d,\n", player->turnCount);
        fprintf(file, "        \"fatigue_damage\": %d,\n", player->fatigueDamage);
        fprintf(file, "        \"is_alive\": %s,\n", player->isAlive ? "true" : "false");
        
        // Write deck
        fprintf(file, "        \"deck\": [\n");
        for (int i = 0; i < player->deckCount; i++) {
            Card* card = &player->deck[i];
            fprintf(file, "          {\n");
            fprintf(file, "            \"id\": %d,\n", card->id);
            fprintf(file, "            \"cost\": %d,\n", card->cost);
            fprintf(file, "            \"attack\": %d,\n", card->attack);
            fprintf(file, "            \"health\": %d,\n", card->health);
            fprintf(file, "            \"max_health\": %d,\n", card->maxHealth);
            fprintf(file, "            \"type\": %d\n", card->type);
            fprintf(file, "          }%s\n", i < player->deckCount - 1 ? "," : "");
        }
        fprintf(file, "        ],\n");
        
        // Write hand
        fprintf(file, "        \"hand\": [\n");
        for (int i = 0; i < player->handCount; i++) {
            Card* card = &player->hand[i];
            fprintf(file, "          {\n");
            fprintf(file, "            \"id\": %d,\n", card->id);
            fprintf(file, "            \"cost\": %d,\n", card->cost);
            fprintf(file, "            \"attack\": %d,\n", card->attack);
            fprintf(file, "            \"health\": %d,\n", card->health);
            fprintf(file, "            \"max_health\": %d,\n", card->maxHealth);
            fprintf(file, "            \"type\": %d,\n", card->type);
            fprintf(file, "            \"in_hand\": %s,\n", card->inHand ? "true" : "false");
            fprintf(file, "            \"is_selected\": %s\n", card->isSelected ? "true" : "false");
            fprintf(file, "          }%s\n", i < player->handCount - 1 ? "," : "");
        }
        fprintf(file, "        ],\n");
        
        // Write board
        fprintf(file, "        \"board\": [\n");
        for (int i = 0; i < player->boardCount; i++) {
            Card* card = &player->board[i];
            fprintf(file, "          {\n");
            fprintf(file, "            \"id\": %d,\n", card->id);
            fprintf(file, "            \"cost\": %d,\n", card->cost);
            fprintf(file, "            \"attack\": %d,\n", card->attack);
            fprintf(file, "            \"health\": %d,\n", card->health);
            fprintf(file, "            \"max_health\": %d,\n", card->maxHealth);
            fprintf(file, "            \"type\": %d,\n", card->type);
            fprintf(file, "            \"on_board\": %s,\n", card->onBoard ? "true" : "false");
            fprintf(file, "            \"can_attack\": %s,\n", card->canAttack ? "true" : "false");
            fprintf(file, "            \"attacked_this_turn\": %s,\n", card->attackedThisTurn ? "true" : "false");
            fprintf(file, "            \"taunt\": %s,\n", card->taunt ? "true" : "false");
            fprintf(file, "            \"charge\": %s,\n", card->charge ? "true" : "false");
            fprintf(file, "            \"divine_shield\": %s,\n", card->divineShield ? "true" : "false");
            fprintf(file, "            \"stealth\": %s,\n", card->stealth ? "true" : "false");
            fprintf(file, "            \"poisonous\": %s,\n", card->poisonous ? "true" : "false");
            fprintf(file, "            \"lifesteal\": %s,\n", card->lifesteal ? "true" : "false");
            fprintf(file, "            \"windfury\": %s,\n", card->windfury ? "true" : "false");
            fprintf(file, "            \"has_battlecry\": %s,\n", card->hasBattlecry ? "true" : "false");
            fprintf(file, "            \"has_deathrattle\": %s\n", card->hasDeathrattle ? "true" : "false");
            fprintf(file, "          }%s\n", i < player->boardCount - 1 ? "," : "");
        }
        fprintf(file, "        \"]\n");
        
        fprintf(file, "      }%s\n", p < 1 ? "," : "");
    }
    fprintf(file, "    ]\n");
    fprintf(file, "  }\n");
    fprintf(file, "}\n");
    
    fclose(file);
    free(filename);
    
    // Refresh save slots to include new save
    RefreshSaveSlots(manager);
    
    return GAME_OK;
}

GameError LoadGameState(SaveManager* manager, GameState* game, const char* save_name) {
    if (!manager || !game || !save_name) return GAME_ERROR_INVALID_PARAMETER;
    
    char filepath[512];
    char* filename = GenerateSaveFilename(save_name);
    if (!filename) return GAME_ERROR_OUT_OF_MEMORY;
    
    snprintf(filepath, sizeof(filepath), "%s%s", manager->save_directory, filename);
    free(filename);
    
    if (!IsValidSaveFile(filepath)) {
        return GAME_ERROR_FILE_NOT_FOUND;
    }
    
    FILE* file = fopen(filepath, "r");
    if (!file) {
        return GAME_ERROR_FILE_NOT_FOUND;
    }
    
    // For now, implement basic loading
    // In a full implementation, you would parse the JSON
    // Here we'll just verify the file exists and is readable
    
    char buffer[1024];
    bool found_version = false;
    
    while (fgets(buffer, sizeof(buffer), file)) {
        if (strstr(buffer, "\"version\"")) {
            found_version = true;
            break;
        }
    }
    
    fclose(file);
    
    if (!found_version) {
        return GAME_ERROR_INVALID_STATE;
    }
    
    // TODO: Implement full JSON parsing and game state restoration
    // For now, just return success to indicate file is valid
    
    return GAME_OK;
}

GameError DeleteSave(SaveManager* manager, const char* save_name) {
    if (!manager || !save_name) return GAME_ERROR_INVALID_PARAMETER;
    
    char filepath[512];
    char* filename = GenerateSaveFilename(save_name);
    if (!filename) return GAME_ERROR_OUT_OF_MEMORY;
    
    snprintf(filepath, sizeof(filepath), "%s%s", manager->save_directory, filename);
    free(filename);
    
    if (remove(filepath) != 0) {
        return GAME_ERROR_FILE_NOT_FOUND;
    }
    
    // Refresh save slots
    RefreshSaveSlots(manager);
    
    return GAME_OK;
}

GameError RefreshSaveSlots(SaveManager* manager) {
    if (!manager) return GAME_ERROR_INVALID_PARAMETER;
    
    manager->slot_count = 0;
    
    DIR* dir = opendir(manager->save_directory);
    if (!dir) return GAME_ERROR_FILE_NOT_FOUND;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL && manager->slot_count < MAX_SAVE_SLOTS) {
        if (strstr(entry->d_name, SAVE_FILE_EXTENSION)) {
            SaveSlot* slot = &manager->slots[manager->slot_count];
            
            // Extract save name from filename
            strncpy(slot->filename, entry->d_name, sizeof(slot->filename) - 1);
            
            char* ext_pos = strstr(slot->filename, SAVE_FILE_EXTENSION);
            if (ext_pos) {
                *ext_pos = '\0';
                strncpy(slot->name, slot->filename, sizeof(slot->name) - 1);
                *ext_pos = '.'; // Restore filename
            }
            
            // Get file stats
            char filepath[512];
            snprintf(filepath, sizeof(filepath), "%s%s", manager->save_directory, entry->d_name);
            
            struct stat file_stat;
            if (stat(filepath, &file_stat) == 0) {
                slot->timestamp = file_stat.st_mtime;
                slot->is_valid = IsValidSaveFile(filepath);
            } else {
                slot->is_valid = false;
            }
            
            // TODO: Parse save file to get turn_number and active_player
            slot->turn_number = 0;
            slot->active_player = 0;
            
            manager->slot_count++;
        }
    }
    
    closedir(dir);
    return GAME_OK;
}

SaveSlot* GetSaveSlot(SaveManager* manager, const char* save_name) {
    if (!manager || !save_name) return NULL;
    
    for (int i = 0; i < manager->slot_count; i++) {
        if (strcmp(manager->slots[i].name, save_name) == 0) {
            return &manager->slots[i];
        }
    }
    
    return NULL;
}

SaveSlot* GetSaveSlotByIndex(SaveManager* manager, int index) {
    if (!manager || index < 0 || index >= manager->slot_count) return NULL;
    return &manager->slots[index];
}

bool IsValidSaveFile(const char* filepath) {
    if (!filepath) return false;
    
    FILE* file = fopen(filepath, "r");
    if (!file) return false;
    
    char buffer[256];
    bool has_version = false;
    bool has_game_state = false;
    
    while (fgets(buffer, sizeof(buffer), file)) {
        if (strstr(buffer, "\"version\"")) has_version = true;
        if (strstr(buffer, "\"game_state\"")) has_game_state = true;
        
        if (has_version && has_game_state) break;
    }
    
    fclose(file);
    return has_version && has_game_state;
}

GameError QuickSave(SaveManager* manager, GameState* game) {
    return SaveGameState(manager, game, "quicksave");
}

GameError QuickLoad(SaveManager* manager, GameState* game) {
    return LoadGameState(manager, game, "quicksave");
}

GameError AutoSave(SaveManager* manager, GameState* game) {
    return SaveGameState(manager, game, "autosave");
}

GameError LoadAutoSave(SaveManager* manager, GameState* game) {
    return LoadGameState(manager, game, "autosave");
}

GameError CreateSaveDirectory(const char* directory) {
    if (!directory) return GAME_ERROR_INVALID_PARAMETER;
    
    struct stat st = {0};
    if (stat(directory, &st) == -1) {
        if (mkdir(directory, 0755) != 0) {
            return GAME_ERROR_FILE_NOT_FOUND;
        }
    }
    
    return GAME_OK;
}

char* GenerateSaveFilename(const char* save_name) {
    if (!save_name) return NULL;
    
    size_t len = strlen(save_name) + strlen(SAVE_FILE_EXTENSION) + 1;
    char* filename = malloc(len);
    if (!filename) return NULL;
    
    snprintf(filename, len, "%s%s", save_name, SAVE_FILE_EXTENSION);
    return filename;
}

bool ValidateSaveName(const char* save_name) {
    if (!save_name || strlen(save_name) == 0 || strlen(save_name) >= MAX_SAVE_NAME) {
        return false;
    }
    
    // Check for invalid characters
    const char* invalid_chars = "/\\:*?\"<>|";
    for (const char* p = save_name; *p; p++) {
        if (strchr(invalid_chars, *p)) {
            return false;
        }
    }
    
    return true;
}