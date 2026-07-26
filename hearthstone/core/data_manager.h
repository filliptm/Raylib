#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include "../common.h"
#include "../errors.h"
#include "../types.h"

#define MAX_CARD_DATABASE 200
#define MAX_DECK_TEMPLATES 50

typedef struct {
    int id;
    char name[MAX_CARD_NAME];
    CardType type;
    int cost;
    int attack;
    int health;
    int spell_damage;
    int healing;
    CardRarity rarity;
    HeroClass class;
    
    // Keywords as flags
    bool charge;
    bool taunt;
    bool divine_shield;
    bool poisonous;
    bool windfury;
    
    // Battlecry data
    bool has_battlecry;
    int battlecry_heal_amount;
    int battlecry_damage_amount;
    
    char description[MAX_CARD_DESC];
    char art_path[128];
} CardData;

typedef struct {
    int card_id;
    int count;
} DeckCardEntry;

typedef struct {
    int id;
    char name[64];
    char class_name[32];
    char description[256];
    DeckCardEntry cards[MAX_DECK_SIZE];
    int card_count;
} DeckTemplate;

typedef struct {
    int max_hand_size;
    int max_board_size;
    int max_deck_size;
    int starting_health;
    int max_mana;
    float turn_time_limit;
} GameSettings;

typedef struct {
    float ai_think_time;
    float mistake_chance;
    float optimal_play_chance;
} DifficultySettings;

typedef struct {
    CardData cards[MAX_CARD_DATABASE];
    int card_count;
    
    DeckTemplate deck_templates[MAX_DECK_TEMPLATES];
    int deck_template_count;
    
    GameSettings game_settings;
    DifficultySettings difficulty[3]; // easy, medium, hard
} DataManager;

// Data manager functions
GameError InitDataManager(DataManager* dm);
void CleanupDataManager(DataManager* dm);

// Loading functions
GameError LoadCardDatabase(DataManager* dm, const char* json_path);
GameError LoadDeckTemplates(DataManager* dm, const char* json_path);
GameError LoadGameBalance(DataManager* dm, const char* json_path);

// Lookup functions
CardData* FindCardById(DataManager* dm, int card_id);
CardData* FindCardByName(DataManager* dm, const char* name);
DeckTemplate* FindDeckTemplate(DataManager* dm, int template_id);
DeckTemplate* FindDeckTemplateByName(DataManager* dm, const char* name);

// Card creation helpers
GameError CreateCardFromData(const CardData* data, Card* card);
GameError CreateDeckFromTemplate(DataManager* dm, const DeckTemplate* template, Card* deck, int* deck_size);

// Validation functions
bool ValidateCardData(const CardData* data);
bool ValidateDeckTemplate(DataManager* dm, const DeckTemplate* template);

#endif