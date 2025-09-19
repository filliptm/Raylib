#ifndef CARD_H
#define CARD_H

#include "types.h"
#include "common.h"
#include "raymath.h"

// Card structure
struct Card {
    int id;
    char name[MAX_CARD_NAME];
    char description[MAX_CARD_DESC];
    int cost;
    CardType type;
    CardRarity rarity;
    HeroClass heroClass;
    
    // Minion stats
    int attack;
    int health;
    int maxHealth;
    bool canAttack;
    bool attackedThisTurn;
    
    // Keywords
    bool taunt;
    bool charge;
    bool rush;
    bool divineShield;
    bool stealth;
    bool poisonous;
    bool windfury;
    bool lifesteal;
    
    // Spell properties
    int spellDamage;
    int healing;
    
    // Weapon properties
    int durability;
    int maxDurability;
    
    // Visual and game state
    Vector3 position;
    Vector3 targetPosition;
    Vector3 size;
    Color color;
    bool isHovered;
    bool isSelected;
    bool isDragging;
    bool inHand;
    bool onBoard;
    int boardPosition;
    int ownerPlayer;
    
    // Effects
    bool hasBattlecry;
    bool hasDeathrattle;
    int battlecryValue;
    int deathrattleValue;
};

// Card management functions
Card CreateCard(int id, const char* name, int cost, CardType type, int attack, int health);
Card GetCardById(int id);
void InitializeCardDatabase(void);

// Card state functions
void UpdateCard(Card* card, float deltaTime);
bool CheckCardHit(Card* card, Ray ray);
void ResetCardCombatState(Card* card);

// Card positioning
void PositionCardInHand(Card* card, int handIndex, int totalCards, int playerId);
void PositionCardOnBoard(Card* card, int boardIndex, int totalCards, int playerId);

#endif // CARD_H