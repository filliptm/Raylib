#include "card.h"
#include <string.h>
#include <math.h>

// Create a basic card with default values
Card CreateCard(int id, const char* name, int cost, CardType type, int attack, int health) {
    Card card = {0};
    card.id = id;
    strncpy(card.name, name, sizeof(card.name) - 1);
    card.cost = cost;
    card.type = type;
    card.attack = attack;
    card.health = health;
    card.maxHealth = health;
    card.canAttack = false;
    card.attackedThisTurn = false;
    card.size = (Vector3){1.6f, 0.1f, 2.4f};
    card.color = WHITE;
    card.isHovered = false;
    card.isSelected = false;
    card.isDragging = false;
    card.inHand = false;
    card.onBoard = false;
    card.boardPosition = -1;
    card.ownerPlayer = -1;
    return card;
}

// Card database - returns a card by its ID
Card GetCardById(int id) {
    switch (id) {
        case 1: {
            Card card = CreateCard(1, "Elven Archer", 1, CARD_TYPE_MINION, 1, 1);
            card.hasBattlecry = true;
            card.battlecryValue = 1; // Deal 1 damage
            card.color = GREEN;
            strncpy(card.description, "Battlecry: Deal 1 damage.", sizeof(card.description) - 1);
            return card;
        }
        case 2: {
            Card card = CreateCard(2, "Boulderfist Ogre", 3, CARD_TYPE_MINION, 6, 7);
            card.color = BROWN;
            strncpy(card.description, "A big, vanilla minion.", sizeof(card.description) - 1);
            return card;
        }
        case 3: {
            Card card = CreateCard(3, "Chillwind Yeti", 2, CARD_TYPE_MINION, 4, 5);
            card.color = SKYBLUE;
            strncpy(card.description, "The best 4-drop in the game.", sizeof(card.description) - 1);
            return card;
        }
        case 4: {
            Card card = CreateCard(4, "War Golem", 3, CARD_TYPE_MINION, 7, 7);
            card.color = GRAY;
            strncpy(card.description, "A powerful late-game threat.", sizeof(card.description) - 1);
            return card;
        }
        case 5: {
            Card card = CreateCard(5, "Stormpike Commando", 2, CARD_TYPE_MINION, 4, 2);
            card.hasBattlecry = true;
            card.battlecryValue = 2; // Deal 2 damage
            card.color = ORANGE;
            strncpy(card.description, "Battlecry: Deal 2 damage.", sizeof(card.description) - 1);
            return card;
        }
        case 6: {
            Card card = CreateCard(6, "Ironforge Rifleman", 2, CARD_TYPE_MINION, 2, 2);
            card.hasBattlecry = true;
            card.battlecryValue = 1; // Deal 1 damage
            card.color = PURPLE;
            strncpy(card.description, "Battlecry: Deal 1 damage.", sizeof(card.description) - 1);
            return card;
        }
        case 7: {
            Card card = CreateCard(7, "Lord of the Arena", 3, CARD_TYPE_MINION, 6, 5);
            card.taunt = true;
            card.color = GOLD;
            strncpy(card.description, "Taunt", sizeof(card.description) - 1);
            return card;
        }
        case 8: {
            Card card = CreateCard(8, "Wolfrider", 2, CARD_TYPE_MINION, 3, 1);
            card.charge = true;
            card.color = RED;
            strncpy(card.description, "Charge", sizeof(card.description) - 1);
            return card;
        }
        case 9: {
            Card card = CreateCard(9, "Fireball", 2, CARD_TYPE_SPELL, 0, 0);
            card.spellDamage = 6;
            card.color = ORANGE;
            strncpy(card.description, "Deal 6 damage.", sizeof(card.description) - 1);
            return card;
        }
        case 10: {
            Card card = CreateCard(10, "Healing Potion", 1, CARD_TYPE_SPELL, 0, 0);
            card.healing = 3;
            card.color = PINK;
            strncpy(card.description, "Restore 3 Health.", sizeof(card.description) - 1);
            return card;
        }
        case 11: {
            Card card = CreateCard(11, "Divine Shield Knight", 2, CARD_TYPE_MINION, 2, 3);
            card.divineShield = true;
            card.color = YELLOW;
            strncpy(card.description, "Divine Shield", sizeof(card.description) - 1);
            return card;
        }
        case 12: {
            Card card = CreateCard(12, "Windfury Harpy", 3, CARD_TYPE_MINION, 4, 5);
            card.windfury = true;
            card.color = SKYBLUE;
            strncpy(card.description, "Windfury", sizeof(card.description) - 1);
            return card;
        }
        case 13: {
            Card card = CreateCard(13, "Poisonous Spider", 1, CARD_TYPE_MINION, 1, 1);
            card.poisonous = true;
            card.color = GREEN;
            strncpy(card.description, "Poisonous", sizeof(card.description) - 1);
            return card;
        }
        case 14: {
            Card card = CreateCard(14, "Lifesteal Vampire", 3, CARD_TYPE_MINION, 3, 4);
            card.lifesteal = true;
            card.color = DARKPURPLE;
            strncpy(card.description, "Lifesteal", sizeof(card.description) - 1);
            return card;
        }
        case 15: {
            Card card = CreateCard(15, "Loot Hoarder", 1, CARD_TYPE_MINION, 2, 1);
            card.hasDeathrattle = true;
            card.deathrattleValue = 1; // Draw a card
            card.color = BROWN;
            strncpy(card.description, "Deathrattle: Draw a card.", sizeof(card.description) - 1);
            return card;
        }
        case 100: {
            // Special card for Paladin hero power
            Card card = CreateCard(100, "Silver Hand Recruit", 0, CARD_TYPE_MINION, 1, 1);
            card.color = LIGHTGRAY;
            strncpy(card.description, "A loyal soldier.", sizeof(card.description) - 1);
            return card;
        }
        default: {
            Card card = CreateCard(0, "Unknown Card", 1, CARD_TYPE_MINION, 1, 1);
            card.color = LIGHTGRAY;
            return card;
        }
    }
}

// Update card position and animations
void UpdateCard(Card* card, float deltaTime) {
    // Smooth movement to target position
    card->position = Vector3Lerp(card->position, card->targetPosition, deltaTime * 8.0f);
    
    // Hover effect - lift card up
    if (card->isHovered && !card->isDragging) {
        Vector3 hoverPos = card->targetPosition;
        hoverPos.y = 0.5f;
        card->position = Vector3Lerp(card->position, hoverPos, deltaTime * 12.0f);
    }
}

// Check if a ray hits the card
bool CheckCardHit(Card* card, Ray ray) {
    if (!card->inHand && !card->onBoard) return false;
    
    BoundingBox cardBox = {
        Vector3Subtract(card->position, Vector3Scale(card->size, 0.5f)),
        Vector3Add(card->position, Vector3Scale(card->size, 0.5f))
    };
    RayCollision collision = GetRayCollisionBox(ray, cardBox);
    return collision.hit;
}

// Reset card combat state at turn start
void ResetCardCombatState(Card* card) {
    if (card->onBoard) {
        card->canAttack = true;
        card->attackedThisTurn = false;
    }
}

// Position a card in the player's hand
void PositionCardInHand(Card* card, int handIndex, int totalCards, int playerId) {
    float handSpacing = 2.0f;
    float handStartX = -((totalCards - 1) * handSpacing) / 2.0f;
    
    card->targetPosition = (Vector3){
        handStartX + handIndex * handSpacing,
        0.0f,
        playerId == 0 ? 8.0f : -8.0f
    };
    
    // Fan out cards in hand
    float fanAngle = (handIndex - (totalCards - 1) / 2.0f) * 0.1f;
    card->targetPosition.x += sinf(fanAngle) * 0.5f;
    card->targetPosition.y += cosf(fanAngle) * 0.2f - 0.2f;
}

// Position a card on the board
void PositionCardOnBoard(Card* card, int boardIndex, int totalCards, int playerId) {
    float boardSpacing = 2.5f;
    float boardStartX = -((totalCards - 1) * boardSpacing) / 2.0f;
    
    card->targetPosition = (Vector3){
        boardStartX + boardIndex * boardSpacing,
        0.0f,
        playerId == 0 ? 2.0f : -2.0f
    };
}