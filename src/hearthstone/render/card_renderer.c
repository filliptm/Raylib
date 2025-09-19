#include "card_renderer.h"
#include "../card.h"
#include "../game_state.h"
#include <stdio.h>
#include <string.h>

// Draw a 3D card
void DrawCard3D(Card* card, Camera3D camera) {
    if (!card->inHand && !card->onBoard) return;
    
    Vector3 pos = card->position;
    Vector3 size = card->size;
    
    // Card base
    Color cardColor = card->color;
    if (card->isSelected) {
        cardColor = ColorBrightness(cardColor, 0.3f);
    }
    
    // Draw card frame for better visibility (draw BEHIND the card)
    DrawCube(pos, size.x + 0.05f, size.y + 0.02f, size.z + 0.05f, Fade(LIGHTGRAY, 0.8f));
    
    DrawCube(pos, size.x, size.y, size.z, cardColor);
    DrawCubeWires(pos, size.x, size.y, size.z, BLACK);
    
    // Draw card effects
    DrawCardEffects(card);
}

// Draw card statistics directly on the card surface
void DrawCardStatsOnCard(Card* card, Camera3D camera) {
    Vector3 pos = card->position;
    
    // Get card center for positioning - use card position directly
    Vector2 center = GetWorldToScreen(pos, camera);
    
    // Check if card is visible on screen (with more generous bounds)
    if (center.x < -100 || center.x > GetScreenWidth() + 100 || 
        center.y < -100 || center.y > GetScreenHeight() + 100) return;
    
    // Card name (center top)
    int nameWidth = MeasureText(card->name, 14);
    DrawText(card->name, center.x - nameWidth/2, center.y - 35, 14, BLACK);
    DrawText(card->name, center.x - nameWidth/2 + 1, center.y - 34, 14, WHITE);
    
    // Mana cost (top left corner)
    DrawCircle(center.x - 40, center.y - 25, 12, BLUE);
    DrawCircle(center.x - 40, center.y - 25, 10, DARKBLUE);
    char costText[4];
    sprintf(costText, "%d", card->cost);
    int costWidth = MeasureText(costText, 16);
    DrawText(costText, center.x - 40 - costWidth/2, center.y - 32, 16, WHITE);
    
    if (card->type == CARD_TYPE_MINION) {
        // Attack (bottom left)
        DrawCircle(center.x - 40, center.y + 25, 12, ORANGE);
        DrawCircle(center.x - 40, center.y + 25, 10, (Color){255, 140, 0, 255});
        char attackText[4];
        sprintf(attackText, "%d", card->attack);
        int attackWidth = MeasureText(attackText, 16);
        DrawText(attackText, center.x - 40 - attackWidth/2, center.y + 18, 16, WHITE);
        
        // Health (bottom right)
        DrawCircle(center.x + 40, center.y + 25, 12, RED);
        DrawCircle(center.x + 40, center.y + 25, 10, (Color){139, 0, 0, 255});
        char healthText[4];
        sprintf(healthText, "%d", card->health);
        int healthWidth = MeasureText(healthText, 16);
        DrawText(healthText, center.x + 40 - healthWidth/2, center.y + 18, 16, WHITE);
    }
    
    if (card->type == CARD_TYPE_SPELL) {
        // Spell effects (center)
        if (card->spellDamage > 0) {
            char spellText[16];
            sprintf(spellText, "DMG: %d", card->spellDamage);
            int spellWidth = MeasureText(spellText, 12);
            DrawText(spellText, center.x - spellWidth/2, center.y - 6, 12, BLACK);
            DrawText(spellText, center.x - spellWidth/2 + 1, center.y - 5, 12, RED);
        }
        if (card->healing > 0) {
            char healText[16];
            sprintf(healText, "HEAL: %d", card->healing);
            int healWidth = MeasureText(healText, 12);
            DrawText(healText, center.x - healWidth/2, center.y - 6, 12, BLACK);
            DrawText(healText, center.x - healWidth/2 + 1, center.y - 5, 12, GREEN);
        }
    }
    
    // Keywords (below center)
    int keywordY = center.y + 10;
    if (card->charge) {
        int chargeWidth = MeasureText("CHARGE", 10);
        DrawText("CHARGE", center.x - chargeWidth/2, keywordY, 10, BLACK);
        DrawText("CHARGE", center.x - chargeWidth/2 + 1, keywordY + 1, 10, ORANGE);
        keywordY += 12;
    }
    if (card->taunt) {
        int tauntWidth = MeasureText("TAUNT", 10);
        DrawText("TAUNT", center.x - tauntWidth/2, keywordY, 10, BLACK);
        DrawText("TAUNT", center.x - tauntWidth/2 + 1, keywordY + 1, 10, GOLD);
        keywordY += 12;
    }
    if (card->divineShield) {
        int shieldWidth = MeasureText("DIVINE SHIELD", 10);
        DrawText("DIVINE SHIELD", center.x - shieldWidth/2, keywordY, 10, BLACK);
        DrawText("DIVINE SHIELD", center.x - shieldWidth/2 + 1, keywordY + 1, 10, YELLOW);
        keywordY += 12;
    }
    if (card->poisonous) {
        int poisonWidth = MeasureText("POISONOUS", 10);
        DrawText("POISONOUS", center.x - poisonWidth/2, keywordY, 10, BLACK);
        DrawText("POISONOUS", center.x - poisonWidth/2 + 1, keywordY + 1, 10, PURPLE);
    }
}

// Draw card visual effects
void DrawCardEffects(Card* card) {
    // Taunt indicator
    if (card->taunt) {
        DrawCube(card->position, card->size.x + 0.2f, card->size.y + 0.1f, 
                card->size.z + 0.2f, Fade(GOLD, 0.3f));
    }
    
    // Divine Shield indicator
    if (card->divineShield) {
        DrawCube(card->position, card->size.x + 0.1f, card->size.y + 0.05f, 
                card->size.z + 0.1f, Fade(YELLOW, 0.5f));
    }
    
    // Selection glow
    if (card->isSelected) {
        DrawCube(card->position, card->size.x + 0.3f, card->size.y + 0.1f, 
                card->size.z + 0.3f, Fade(YELLOW, 0.2f));
    }
}

// Draw card highlight
void DrawCardHighlight(Card* card, Color highlightColor) {
    DrawCube(card->position, card->size.x + 0.4f, card->size.y + 0.2f, 
            card->size.z + 0.4f, Fade(highlightColor, 0.3f));
}

// Draw card keywords
void DrawCardKeywords(Card* card, Vector2 screenPos) {
    int keywordY = screenPos.y + 5;
    
    if (card->charge) {
        DrawText("CHARGE", screenPos.x - 20, keywordY, 8, ORANGE);
        keywordY += 10;
    }
    if (card->taunt) {
        DrawText("TAUNT", screenPos.x - 15, keywordY, 8, GOLD);
        keywordY += 10;
    }
    if (card->poisonous) {
        DrawText("POISON", screenPos.x - 20, keywordY, 8, GREEN);
        keywordY += 10;
    }
    if (card->divineShield) {
        DrawText("DIVINE", screenPos.x - 20, keywordY, 8, YELLOW);
        keywordY += 10;
    }
    if (card->windfury) {
        DrawText("WINDFURY", screenPos.x - 25, keywordY, 8, SKYBLUE);
    }
}

// Get card screen position
Vector2 GetCardScreenPosition(Card* card, Camera3D camera) {
    Vector3 textPos = Vector3Add(card->position, (Vector3){0, 1.0f, 0});
    return GetWorldToScreen(textPos, camera);
}

// Draw all card stats in 2D mode
void DrawAllCardStats(GameState* game) {
    // Draw stats for all visible cards
    for (int p = 0; p < 2; p++) {
        // Draw hand card stats
        for (int i = 0; i < game->players[p].handCount; i++) {
            DrawCardStatsOnCard(&game->players[p].hand[i], game->camera);
        }
        
        // Draw board card stats
        for (int i = 0; i < game->players[p].boardCount; i++) {
            DrawCardStatsOnCard(&game->players[p].board[i], game->camera);
        }
    }
}