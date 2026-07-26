#include "data_manager.h"
#include "../card.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Simple JSON parsing helpers (basic implementation)
static char* find_json_value(const char* json, const char* key) {
    char search_key[256];
    snprintf(search_key, sizeof(search_key), "\"%s\":", key);
    
    char* start = strstr(json, search_key);
    if (!start) return NULL;
    
    start += strlen(search_key);
    while (*start == ' ' || *start == '\t') start++; // Skip whitespace
    
    if (*start == '"') {
        // String value
        start++;
        char* end = strchr(start, '"');
        if (!end) return NULL;
        
        int len = end - start;
        char* result = malloc(len + 1);
        strncpy(result, start, len);
        result[len] = '\0';
        return result;
    } else {
        // Number or boolean value
        char* end = start;
        while (*end && *end != ',' && *end != '}' && *end != ']' && *end != '\n') end++;
        
        int len = end - start;
        char* result = malloc(len + 1);
        strncpy(result, start, len);
        result[len] = '\0';
        
        // Trim whitespace
        while (len > 0 && (result[len-1] == ' ' || result[len-1] == '\t')) {
            result[--len] = '\0';
        }
        
        return result;
    }
}

static bool parse_keywords(const char* json, CardData* card) {
    // Reset all keywords
    card->charge = false;
    card->taunt = false;
    card->divine_shield = false;
    card->poisonous = false;
    card->windfury = false;
    
    // Simple keyword parsing - look for keyword strings
    if (strstr(json, "\"charge\"")) card->charge = true;
    if (strstr(json, "\"taunt\"")) card->taunt = true;
    if (strstr(json, "\"divine_shield\"")) card->divine_shield = true;
    if (strstr(json, "\"poisonous\"")) card->poisonous = true;
    if (strstr(json, "\"windfury\"")) card->windfury = true;
    
    return true;
}

GameError InitDataManager(DataManager* dm) {
    if (!dm) return GAME_ERROR_INVALID_PARAMETER;
    
    memset(dm, 0, sizeof(DataManager));
    
    // Set default game settings
    dm->game_settings.max_hand_size = MAX_HAND_SIZE;
    dm->game_settings.max_board_size = MAX_BOARD_SIZE;
    dm->game_settings.max_deck_size = MAX_DECK_SIZE;
    dm->game_settings.starting_health = 30;
    dm->game_settings.max_mana = 10;
    dm->game_settings.turn_time_limit = 75.0f;
    
    // Set default difficulty settings
    dm->difficulty[0] = (DifficultySettings){2.0f, 0.3f, 0.5f}; // easy
    dm->difficulty[1] = (DifficultySettings){1.5f, 0.15f, 0.75f}; // medium
    dm->difficulty[2] = (DifficultySettings){1.0f, 0.05f, 0.95f}; // hard
    
    return GAME_OK;
}

void CleanupDataManager(DataManager* dm) {
    if (!dm) return;
    
    // No dynamic allocations to clean up in this simple implementation
    memset(dm, 0, sizeof(DataManager));
}

GameError LoadCardDatabase(DataManager* dm, const char* json_path) {
    if (!dm || !json_path) return GAME_ERROR_INVALID_PARAMETER;
    
    FILE* file = fopen(json_path, "r");
    if (!file) {
        LogError(GAME_ERROR_FILE_NOT_FOUND, "LoadCardDatabase");
        return GAME_ERROR_FILE_NOT_FOUND;
    }
    
    // Read entire file
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* json_content = malloc(file_size + 1);
    if (!json_content) {
        fclose(file);
        return GAME_ERROR_OUT_OF_MEMORY;
    }
    
    fread(json_content, 1, file_size, file);
    json_content[file_size] = '\0';
    fclose(file);
    
    // Simple JSON parsing for cards
    dm->card_count = 0;
    char* card_start = strstr(json_content, "\"id\":");
    
    while (card_start && dm->card_count < MAX_CARD_DATABASE) {
        CardData* card = &dm->cards[dm->card_count];
        memset(card, 0, sizeof(CardData));
        
        // Parse basic fields
        char* value = find_json_value(card_start, "id");
        if (value) {
            card->id = atoi(value);
            free(value);
        }
        
        value = find_json_value(card_start, "name");
        if (value) {
            strncpy(card->name, value, MAX_CARD_NAME - 1);
            free(value);
        }
        
        value = find_json_value(card_start, "type");
        if (value) {
            if (strcmp(value, "minion") == 0) card->type = CARD_TYPE_MINION;
            else if (strcmp(value, "spell") == 0) card->type = CARD_TYPE_SPELL;
            else if (strcmp(value, "weapon") == 0) card->type = CARD_TYPE_WEAPON;
            free(value);
        }
        
        value = find_json_value(card_start, "cost");
        if (value) {
            card->cost = atoi(value);
            free(value);
        }
        
        value = find_json_value(card_start, "attack");
        if (value) {
            card->attack = atoi(value);
            free(value);
        }
        
        value = find_json_value(card_start, "health");
        if (value) {
            card->health = atoi(value);
            free(value);
        }
        
        value = find_json_value(card_start, "spell_damage");
        if (value) {
            card->spell_damage = atoi(value);
            free(value);
        }
        
        value = find_json_value(card_start, "healing");
        if (value) {
            card->healing = atoi(value);
            free(value);
        }
        
        value = find_json_value(card_start, "description");
        if (value) {
            strncpy(card->description, value, MAX_CARD_DESC - 1);
            free(value);
        }
        
        value = find_json_value(card_start, "art");
        if (value) {
            strncpy(card->art_path, value, 127);
            free(value);
        }
        
        // Parse rarity
        value = find_json_value(card_start, "rarity");
        if (value) {
            if (strcmp(value, "common") == 0) card->rarity = RARITY_COMMON;
            else if (strcmp(value, "rare") == 0) card->rarity = RARITY_RARE;
            else if (strcmp(value, "epic") == 0) card->rarity = RARITY_EPIC;
            else if (strcmp(value, "legendary") == 0) card->rarity = RARITY_LEGENDARY;
            free(value);
        }
        
        // Parse class
        value = find_json_value(card_start, "class");
        if (value) {
            if (strcmp(value, "neutral") == 0) card->class = CLASS_NEUTRAL;
            else if (strcmp(value, "mage") == 0) card->class = CLASS_MAGE;
            else if (strcmp(value, "priest") == 0) card->class = CLASS_PRIEST;
            else if (strcmp(value, "rogue") == 0) card->class = CLASS_ROGUE;
            // Add more classes as needed
            free(value);
        }
        
        // Parse keywords
        parse_keywords(card_start, card);
        
        // Check for battlecry
        if (strstr(card_start, "battlecry")) {
            card->has_battlecry = true;
            // Simple battlecry parsing - could be expanded
            char* battlecry_heal = find_json_value(card_start, "amount");
            if (battlecry_heal) {
                card->battlecry_heal_amount = atoi(battlecry_heal);
                free(battlecry_heal);
            }
        }
        
        dm->card_count++;
        
        // Find next card
        card_start = strstr(card_start + 1, "\"id\":");
    }
    
    free(json_content);
    return GAME_OK;
}

GameError LoadDeckTemplates(DataManager* dm, const char* json_path) {
    if (!dm || !json_path) return GAME_ERROR_INVALID_PARAMETER;
    
    FILE* file = fopen(json_path, "r");
    if (!file) {
        LogError(GAME_ERROR_FILE_NOT_FOUND, "LoadDeckTemplates");
        return GAME_ERROR_FILE_NOT_FOUND;
    }
    
    // Read entire file
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* json_content = malloc(file_size + 1);
    if (!json_content) {
        fclose(file);
        return GAME_ERROR_OUT_OF_MEMORY;
    }
    
    fread(json_content, 1, file_size, file);
    json_content[file_size] = '\0';
    fclose(file);
    
    // Simple parsing for deck templates
    dm->deck_template_count = 0;
    char* deck_start = strstr(json_content, "\"id\":");
    
    while (deck_start && dm->deck_template_count < MAX_DECK_TEMPLATES) {
        DeckTemplate* deck = &dm->deck_templates[dm->deck_template_count];
        memset(deck, 0, sizeof(DeckTemplate));
        
        char* value = find_json_value(deck_start, "id");
        if (value) {
            deck->id = atoi(value);
            free(value);
        }
        
        value = find_json_value(deck_start, "name");
        if (value) {
            strncpy(deck->name, value, 63);
            free(value);
        }
        
        value = find_json_value(deck_start, "description");
        if (value) {
            strncpy(deck->description, value, 255);
            free(value);
        }
        
        // Parse deck cards (simplified)
        char* cards_start = strstr(deck_start, "\"cards\":");
        if (cards_start) {
            // This is a very simplified parser - in a real implementation
            // you'd want a proper JSON parser library
            deck->card_count = 0; // Will be set properly in a full implementation
        }
        
        dm->deck_template_count++;
        
        // Find next deck
        deck_start = strstr(deck_start + 1, "\"id\":");
    }
    
    free(json_content);
    return GAME_OK;
}

GameError LoadGameBalance(DataManager* dm, const char* json_path) {
    if (!dm || !json_path) return GAME_ERROR_INVALID_PARAMETER;
    
    // For now, keep default settings
    // In a full implementation, this would parse the balance.json file
    
    return GAME_OK;
}

CardData* FindCardById(DataManager* dm, int card_id) {
    if (!dm) return NULL;
    
    for (int i = 0; i < dm->card_count; i++) {
        if (dm->cards[i].id == card_id) {
            return &dm->cards[i];
        }
    }
    
    return NULL;
}

CardData* FindCardByName(DataManager* dm, const char* name) {
    if (!dm || !name) return NULL;
    
    for (int i = 0; i < dm->card_count; i++) {
        if (strcmp(dm->cards[i].name, name) == 0) {
            return &dm->cards[i];
        }
    }
    
    return NULL;
}

DeckTemplate* FindDeckTemplate(DataManager* dm, int template_id) {
    if (!dm) return NULL;
    
    for (int i = 0; i < dm->deck_template_count; i++) {
        if (dm->deck_templates[i].id == template_id) {
            return &dm->deck_templates[i];
        }
    }
    
    return NULL;
}

DeckTemplate* FindDeckTemplateByName(DataManager* dm, const char* name) {
    if (!dm || !name) return NULL;
    
    for (int i = 0; i < dm->deck_template_count; i++) {
        if (strcmp(dm->deck_templates[i].name, name) == 0) {
            return &dm->deck_templates[i];
        }
    }
    
    return NULL;
}

GameError CreateCardFromData(const CardData* data, Card* card) {
    if (!data || !card) return GAME_ERROR_INVALID_PARAMETER;
    
    // Initialize card from data
    memset(card, 0, sizeof(Card));
    
    strncpy(card->name, data->name, MAX_CARD_NAME - 1);
    strncpy(card->description, data->description, MAX_CARD_DESC - 1);
    
    card->type = data->type;
    card->cost = data->cost;
    card->attack = data->attack;
    card->health = data->health;
    card->maxHealth = data->health;
    card->spellDamage = data->spell_damage;
    card->healing = data->healing;
    card->rarity = data->rarity;
    card->heroClass = data->class;
    
    // Set keywords
    card->charge = data->charge;
    card->taunt = data->taunt;
    card->divineShield = data->divine_shield;
    card->poisonous = data->poisonous;
    card->windfury = data->windfury;
    
    // Set battlecry
    card->hasBattlecry = data->has_battlecry;
    
    // Set default visual properties
    card->size = (Vector3){1.5f, 0.1f, 2.0f};
    card->color = (data->type == CARD_TYPE_MINION) ? LIGHTGRAY : 
                  (data->type == CARD_TYPE_SPELL) ? PURPLE : BROWN;
    
    return GAME_OK;
}

GameError CreateDeckFromTemplate(DataManager* dm, const DeckTemplate* template, Card* deck, int* deck_size) {
    if (!dm || !template || !deck || !deck_size) return GAME_ERROR_INVALID_PARAMETER;
    
    *deck_size = 0;
    
    for (int i = 0; i < template->card_count && *deck_size < MAX_DECK_SIZE; i++) {
        CardData* card_data = FindCardById(dm, template->cards[i].card_id);
        if (!card_data) continue;
        
        // Add multiple copies if specified
        for (int j = 0; j < template->cards[i].count && *deck_size < MAX_DECK_SIZE; j++) {
            GameError result = CreateCardFromData(card_data, &deck[*deck_size]);
            if (result != GAME_OK) return result;
            
            (*deck_size)++;
        }
    }
    
    return GAME_OK;
}

bool ValidateCardData(const CardData* data) {
    if (!data) return false;
    
    // Basic validation
    if (data->cost < 0 || data->cost > 20) return false;
    if (data->type == CARD_TYPE_MINION) {
        if (data->attack < 0 || data->health < 1) return false;
    }
    if (strlen(data->name) == 0) return false;
    
    return true;
}

bool ValidateDeckTemplate(DataManager* dm, const DeckTemplate* template) {
    if (!dm || !template) return false;
    
    // Check that all cards exist
    for (int i = 0; i < template->card_count; i++) {
        if (!FindCardById(dm, template->cards[i].card_id)) {
            return false;
        }
    }
    
    return true;
}