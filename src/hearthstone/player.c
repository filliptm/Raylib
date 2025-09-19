#include "player.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Initialize a player with default values
void InitializePlayer(Player* player, int playerId, const char* name) {
    memset(player, 0, sizeof(Player));
    
    player->playerId = playerId;
    strncpy(player->name, name, sizeof(player->name) - 1);
    player->health = 30;
    player->maxHealth = 30;
    player->armor = 0;
    player->mana = 0;
    player->maxMana = 0;
    player->handCount = 0;
    player->boardCount = 0;
    player->deckCount = 0;
    player->isActivePlayer = false;
    player->turnCount = 0;
    player->fatigueDamage = 0;
    player->isAlive = true;
    player->heroPowerUsed = false;
    player->hasWeapon = false;
}

// Draw a card from the deck to hand
void DrawCardFromDeck(Player* player) {
    if (player->deckCount <= 0) {
        // Fatigue damage
        player->fatigueDamage++;
        player->health -= player->fatigueDamage;
        if (player->health <= 0) {
            player->isAlive = false;
        }
        return;
    }
    
    if (player->handCount >= MAX_HAND_SIZE) {
        // Hand full, burn the card
        player->deckCount--;
        return;
    }
    
    // Draw card
    Card drawnCard = player->deck[player->deckCount - 1];
    player->deckCount--;
    
    drawnCard.inHand = true;
    drawnCard.onBoard = false;
    drawnCard.ownerPlayer = player->playerId;
    
    // Add to hand
    player->hand[player->handCount] = drawnCard;
    player->handCount++;
    
    // Update hand positions
    UpdateHandPositions(player);
}

// Check if player can play a specific card
bool CanPlayCard(Player* player, Card* card) {
    if (player->mana < card->cost) return false;
    if (card->type == CARD_TYPE_MINION && player->boardCount >= MAX_BOARD_SIZE) return false;
    return true;
}

// Refresh mana at turn start
void RefreshMana(Player* player) {
    // Increase maximum mana (cap at 10)
    if (player->maxMana < 10) {
        player->maxMana++;
    }
    // Refresh current mana to maximum
    player->mana = player->maxMana;
}

// Spend mana when playing a card
void SpendMana(Player* player, int amount) {
    player->mana -= amount;
    if (player->mana < 0) {
        player->mana = 0;
    }
}

// Initialize a player's deck with random cards
void InitializePlayerDeck(Player* player) {
    // Create basic deck for testing
    int deckCards[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
                       1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
                       1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    
    player->deckCount = 30;
    
    for (int i = 0; i < 30; i++) {
        player->deck[i] = GetCardById(deckCards[i]);
        player->deck[i].ownerPlayer = player->playerId;
    }
    
    // Shuffle the deck
    ShuffleDeck(player);
}

// Shuffle the player's deck
void ShuffleDeck(Player* player) {
    for (int i = player->deckCount - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        Card temp = player->deck[i];
        player->deck[i] = player->deck[j];
        player->deck[j] = temp;
    }
}

// Add a card to the player's hand
bool AddCardToHand(Player* player, Card card) {
    if (player->handCount >= MAX_HAND_SIZE) {
        return false; // Hand is full
    }
    
    card.inHand = true;
    card.onBoard = false;
    card.ownerPlayer = player->playerId;
    
    player->hand[player->handCount] = card;
    player->handCount++;
    
    UpdateHandPositions(player);
    return true;
}

// Remove a card from the player's hand
void RemoveCardFromHand(Player* player, int handIndex) {
    if (handIndex < 0 || handIndex >= player->handCount) return;
    
    // Shift remaining cards
    for (int i = handIndex; i < player->handCount - 1; i++) {
        player->hand[i] = player->hand[i + 1];
    }
    player->handCount--;
    
    UpdateHandPositions(player);
}

// Update positions of all cards in hand
void UpdateHandPositions(Player* player) {
    for (int i = 0; i < player->handCount; i++) {
        PositionCardInHand(&player->hand[i], i, player->handCount, player->playerId);
    }
}

// Add a card to the player's board
bool AddCardToBoard(Player* player, Card card) {
    if (player->boardCount >= MAX_BOARD_SIZE) {
        return false; // Board is full
    }
    
    card.onBoard = true;
    card.inHand = false;
    card.boardPosition = player->boardCount;
    card.ownerPlayer = player->playerId;
    
    // Cards with charge can attack immediately, others have summoning sickness
    if (card.charge) {
        card.canAttack = true;
        card.attackedThisTurn = false;
    } else {
        card.canAttack = false;  // Summoning sickness - can't attack this turn
        card.attackedThisTurn = false;
    }
    
    player->board[player->boardCount] = card;
    player->boardCount++;
    
    UpdateBoardPositions(player);
    return true;
}

// Remove a card from the player's board
void RemoveCardFromBoard(Player* player, int boardIndex) {
    if (boardIndex < 0 || boardIndex >= player->boardCount) return;
    
    // Shift remaining cards
    for (int i = boardIndex; i < player->boardCount - 1; i++) {
        player->board[i] = player->board[i + 1];
        player->board[i].boardPosition = i;
    }
    player->boardCount--;
    
    UpdateBoardPositions(player);
}

// Update positions of all cards on board
void UpdateBoardPositions(Player* player) {
    for (int i = 0; i < player->boardCount; i++) {
        PositionCardOnBoard(&player->board[i], i, player->boardCount, player->playerId);
    }
}

// Check if player is still alive
bool IsPlayerAlive(Player* player) {
    return player->health > 0;
}

// Check and apply fatigue damage
void CheckFatigue(Player* player) {
    if (player->deckCount <= 0) {
        player->fatigueDamage++;
        player->health -= player->fatigueDamage;
        
        if (player->health <= 0) {
            player->health = 0;
            player->isAlive = false;
        }
    }
}