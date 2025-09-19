#include "ui_renderer.h"
#include "../card.h"
#include "../game_state.h"
#include <string.h>

// Draw UI elements
void DrawGameUI(GameState* game) {
    DrawText("Hearthstone Clone", 10, 10, 24, WHITE);
    
    // Draw turn indicator
    DrawTurnIndicator(game);
    
    // Draw player info
    DrawPlayerInfo(&game->players[0], 70, game->activePlayer == 0);
    DrawPlayerInfo(&game->players[1], 90, game->activePlayer == 1);
    
    // Draw controls
    DrawControls();
    
    // Draw selected card info
    if (game->selectedCard) {
        DrawSelectedCardInfo(game->selectedCard);
    }
}

// Draw turn indicator
void DrawTurnIndicator(GameState* game) {
    Player* activePlayer = &game->players[game->activePlayer];
    DrawText(TextFormat("Turn %d - %s's Turn", game->turnNumber, activePlayer->name), 
             10, 40, 20, YELLOW);
}

// Draw player information
void DrawPlayerInfo(Player* player, int yPosition, bool isActive) {
    Color textColor = isActive ? GREEN : WHITE;
    
    DrawText(TextFormat("%s: %d/%d HP, %d/%d Mana, %d Cards", 
             player->name,
             player->health, player->maxHealth,
             player->mana, player->maxMana,
             player->handCount), 
             10, yPosition, 16, textColor);
    
    // Draw mana bar
    DrawManaBar(player->mana, player->maxMana, 300, yPosition);
    
    // Draw health bar
    DrawHealthBar(player->health, player->maxHealth, 450, yPosition);
}

// Draw control instructions
void DrawControls(void) {
    int y = 120;
    DrawText("Drag Cards: Play/Attack", 10, y, 14, LIGHTGRAY);
    DrawText("Green: Valid drops", 10, y + 20, 14, GREEN);
    DrawText("Space: End Turn", 10, y + 40, 14, LIGHTGRAY);
}

// Draw selected card information
void DrawSelectedCardInfo(Card* card) {
    int y = 180;
    DrawText(TextFormat("Selected: %s", card->name), 10, y, 16, YELLOW);
    
    if (strlen(card->description) > 0) {
        DrawText(card->description, 10, y + 20, 12, LIGHTGRAY);
    }
    
    // Show drag instruction and debug info
    if (card->inHand) {
        if (card->type == CARD_TYPE_MINION) {
            DrawText("Drag to board to play", 10, y + 40, 12, GREEN);
        } else if (card->type == CARD_TYPE_SPELL || card->hasBattlecry) {
            DrawText("Drag to target", 10, y + 40, 12, GREEN);
        }
    } else if (card->onBoard) {
        // Debug info for board cards
        DrawText(TextFormat("On Board - CanAttack: %s, Attacked: %s", 
                 card->canAttack ? "YES" : "NO",
                 card->attackedThisTurn ? "YES" : "NO"), 10, y + 40, 12, WHITE);
        
        if (card->canAttack && !card->attackedThisTurn) {
            DrawText("Drag to attack target", 10, y + 60, 12, RED);
        } else if (card->attackedThisTurn) {
            DrawText("Already attacked this turn", 10, y + 60, 12, GRAY);
        } else {
            DrawText("Cannot attack (summoning sickness)", 10, y + 60, 12, GRAY);
        }
    }
}

// Draw game end screen
void DrawGameEndScreen(GameState* game) {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.7f));
    
    const char* winText = TextFormat("Player %d Wins!", game->winner + 1);
    int textWidth = MeasureText(winText, 48);
    DrawText(winText, GetScreenWidth()/2 - textWidth/2, GetScreenHeight()/2 - 24, 48, GOLD);
    
    DrawText("Press R to restart", GetScreenWidth()/2 - 80, GetScreenHeight()/2 + 40, 20, WHITE);
}

// Draw mana bar
void DrawManaBar(int current, int max, int x, int y) {
    DrawRectangle(x, y, 100, 10, DARKBLUE);
    if (max > 0) {
        int fillWidth = (current * 100) / max;
        DrawRectangle(x, y, fillWidth, 10, BLUE);
    }
    DrawRectangleLines(x, y, 100, 10, WHITE);
}

// Draw health bar
void DrawHealthBar(int current, int max, int x, int y) {
    DrawRectangle(x, y, 100, 10, MAROON);
    if (max > 0) {
        int fillWidth = (current * 100) / max;
        DrawRectangle(x, y, fillWidth, 10, RED);
    }
    DrawRectangleLines(x, y, 100, 10, WHITE);
}