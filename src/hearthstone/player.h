#ifndef PLAYER_H
#define PLAYER_H

#include "types.h"
#include "common.h"
#include "card.h"

// Player structure
struct Player {
    int playerId;
    char name[MAX_PLAYER_NAME];
    HeroClass heroClass;
    
    // Resources
    int health;
    int maxHealth;
    int armor;
    int mana;
    int maxMana;
    
    // Hero power
    Card heroPower;
    bool heroPowerUsed;
    
    // Weapon
    Card* weapon;
    bool hasWeapon;
    
    // Collections
    Card deck[MAX_DECK_SIZE];
    Card hand[MAX_HAND_SIZE];
    Card board[MAX_BOARD_SIZE];
    
    // Counts
    int deckCount;
    int handCount;
    int boardCount;
    
    // Game state
    bool isActivePlayer;
    int turnCount;
    int fatigueDamage;
    bool isAlive;

    // Polish features
    bool isTargeted;
};

// Player management functions
void InitializePlayer(Player* player, int playerId, const char* name);
void DrawCardFromDeck(Player* player);
bool CanPlayCard(Player* player, Card* card);
void RefreshMana(Player* player);
void SpendMana(Player* player, int amount);

// Deck management
void InitializePlayerDeck(Player* player);
void ShuffleDeck(Player* player);

// Hand management
bool AddCardToHand(Player* player, Card card);
void RemoveCardFromHand(Player* player, int handIndex);
void UpdateHandPositions(Player* player);

// Board management
bool AddCardToBoard(Player* player, Card card);
void RemoveCardFromBoard(Player* player, int boardIndex);
void UpdateBoardPositions(Player* player);

// Player state checks
bool IsPlayerAlive(Player* player);
void CheckFatigue(Player* player);

#endif // PLAYER_H