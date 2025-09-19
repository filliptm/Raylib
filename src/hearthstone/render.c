#include "render.h"
#include "game_state.h"
#include "card.h"
#include "input.h"
#include "combat.h"
#include <string.h>
#include <stdio.h>

// Main game rendering function
void DrawGame(GameState* game) {
    // Update camera with shake
    UpdateGameCamera(game, GetFrameTime());
    
    BeginMode3D(game->camera);
        // Draw 3D elements
        DrawGameBoard();
        
        // Draw all cards (3D only)
        for (int p = 0; p < 2; p++) {
            DrawPlayerHand(&game->players[p], game->camera);
            DrawPlayerBoard(&game->players[p], game->camera);
        }
        
        // Draw player portraits/heroes
        DrawPlayerPortrait(&game->players[0], game->camera);
        DrawPlayerPortrait(&game->players[1], game->camera);
        
        // Draw drag feedback
        DrawDragFeedback(game);
        
        // Draw grid for reference
        DrawGrid(20, 1.0f);
        
    EndMode3D();
    
    // Draw 2D UI elements
    DrawGameUI(game);
    DrawVisualEffects(game);
    
    // Draw card stats (2D overlay)
    DrawAllCardStats(game);
    
    if (game->gameEnded) {
        DrawGameEndScreen(game);
    }
    
    DrawFPS(GetScreenWidth() - 80, 10);
}

// Draw the game board
void DrawGameBoard(void) {
    // Main board surface
    DrawPlane((Vector3){0, -0.5f, 0}, (Vector2){20, 16}, DARKBROWN);
    
    // Player 1 (bottom) hand area
    DrawPlane((Vector3){0, -0.45f, 8}, (Vector2){18, 3}, Fade(BLUE, 0.4f));
    DrawCubeWires((Vector3){0, -0.4f, 8}, 18.2f, 0.1f, 3.2f, BLUE);
    
    // Player 1 (bottom) board area  
    DrawPlane((Vector3){0, -0.45f, 2}, (Vector2){16, 3}, Fade(SKYBLUE, 0.3f));
    DrawCubeWires((Vector3){0, -0.4f, 2}, 16.2f, 0.1f, 3.2f, LIGHTGRAY);
    
    // Center neutral zone
    DrawPlane((Vector3){0, -0.45f, 0}, (Vector2){16, 1}, Fade(GRAY, 0.2f));
    
    // Player 2 (top) board area
    DrawPlane((Vector3){0, -0.45f, -2}, (Vector2){16, 3}, Fade(PINK, 0.3f));
    DrawCubeWires((Vector3){0, -0.4f, -2}, 16.2f, 0.1f, 3.2f, LIGHTGRAY);
    
    // Player 2 (top) hand area
    DrawPlane((Vector3){0, -0.45f, -8}, (Vector2){18, 3}, Fade(RED, 0.4f));
    DrawCubeWires((Vector3){0, -0.4f, -8}, 18.2f, 0.1f, 3.2f, RED);
    
    // Card placement guides for board areas
    for (int i = 0; i < 7; i++) {
        float x = (i - 3) * 2.5f;
        // Player 1 board slots
        DrawCubeWires((Vector3){x, -0.35f, 2}, 1.8f, 0.05f, 2.6f, Fade(BLUE, 0.5f));
        // Player 2 board slots  
        DrawCubeWires((Vector3){x, -0.35f, -2}, 1.8f, 0.05f, 2.6f, Fade(RED, 0.5f));
    }
    
    // Hand placement guides
    for (int i = 0; i < 10; i++) {
        float x = (i - 4.5f) * 1.8f;
        // Player 1 hand slots
        DrawCubeWires((Vector3){x, -0.35f, 8}, 1.6f, 0.05f, 2.4f, Fade(BLUE, 0.3f));
        // Player 2 hand slots
        DrawCubeWires((Vector3){x, -0.35f, -8}, 1.6f, 0.05f, 2.4f, Fade(RED, 0.3f));
    }
}

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

// Draw game end screen
void DrawGameEndScreen(GameState* game) {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.7f));
    
    const char* winText = TextFormat("Player %d Wins!", game->winner + 1);
    int textWidth = MeasureText(winText, 48);
    DrawText(winText, GetScreenWidth()/2 - textWidth/2, GetScreenHeight()/2 - 24, 48, GOLD);
    
    DrawText("Press R to restart", GetScreenWidth()/2 - 80, GetScreenHeight()/2 + 40, 20, WHITE);
}

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
    
    // Note: Card stats are drawn in 2D mode in DrawAllCardStats
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

// Draw card statistics (legacy function - keeping for compatibility)
void DrawCardStats(Card* card, Vector2 screenPos) {
    // This function is now unused but kept for compatibility
    // Stats are drawn directly on cards via DrawCardStatsOnCard
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

// Draw player's hand
void DrawPlayerHand(Player* player, Camera3D camera) {
    for (int i = 0; i < player->handCount; i++) {
        DrawCard3D(&player->hand[i], camera);
    }
}

// Draw player's board
void DrawPlayerBoard(Player* player, Camera3D camera) {
    for (int i = 0; i < player->boardCount; i++) {
        DrawCard3D(&player->board[i], camera);
    }
}

// Draw player portrait/hero
void DrawPlayerPortrait(Player* player, Camera3D camera) {
    Vector3 portraitPos = (Vector3){
        7.0f, 
        0.2f, 
        player->playerId == 0 ? 6.0f : -6.0f
    };
    
    // Portrait base (larger than cards)
    Vector3 portraitSize = (Vector3){2.0f, 0.2f, 2.0f};
    
    // Health color coding
    Color portraitColor = GREEN;
    if (player->health <= 10) portraitColor = YELLOW;
    if (player->health <= 5) portraitColor = RED;
    
    // Draw portrait base
    DrawCube(portraitPos, portraitSize.x, portraitSize.y, portraitSize.z, portraitColor);
    DrawCubeWires(portraitPos, portraitSize.x, portraitSize.y, portraitSize.z, BLACK);
    
    // Draw portrait frame
    DrawCube(portraitPos, portraitSize.x + 0.1f, portraitSize.y + 0.05f, portraitSize.z + 0.1f, 
             Fade(GOLD, 0.6f));
    
    // Draw health and mana on portrait
    Vector2 portraitScreen = GetWorldToScreen((Vector3){portraitPos.x, portraitPos.y + 0.2f, portraitPos.z}, camera);
    
    if (portraitScreen.x >= 0 && portraitScreen.x <= GetScreenWidth() && 
        portraitScreen.y >= 0 && portraitScreen.y <= GetScreenHeight()) {
        
        // Player name
        int nameWidth = MeasureText(player->name, 16);
        DrawText(player->name, portraitScreen.x - nameWidth/2, portraitScreen.y - 40, 16, BLACK);
        DrawText(player->name, portraitScreen.x - nameWidth/2 + 1, portraitScreen.y - 39, 16, WHITE);
        
        // Health (large, centered)
        char healthText[8];
        sprintf(healthText, "%d", player->health);
        int healthWidth = MeasureText(healthText, 24);
        DrawText(healthText, portraitScreen.x - healthWidth/2, portraitScreen.y - 12, 24, BLACK);
        DrawText(healthText, portraitScreen.x - healthWidth/2 + 1, portraitScreen.y - 11, 24, WHITE);
        
        // Mana (top right of portrait)
        DrawCircle(portraitScreen.x + 40, portraitScreen.y - 20, 15, BLUE);
        DrawCircle(portraitScreen.x + 40, portraitScreen.y - 20, 12, DARKBLUE);
        char manaText[8];
        sprintf(manaText, "%d", player->mana);
        int manaWidth = MeasureText(manaText, 18);
        DrawText(manaText, portraitScreen.x + 40 - manaWidth/2, portraitScreen.y - 28, 18, WHITE);
    }
}

// Draw visual effects
void DrawVisualEffects(GameState* game) {
    for (int i = 0; i < game->activeEffectsCount; i++) {
        if (game->effects[i].active) {
            DrawEffect(&game->effects[i], game->camera);
        }
    }
}

// Draw a single effect
void DrawEffect(VisualEffect* effect, Camera3D camera) {
    Vector2 screenPos = GetWorldToScreen(effect->position, camera);
    float alpha = 1.0f - (effect->timer / effect->duration);
    Color effectColor = Fade(effect->color, alpha);
    
    int textWidth = MeasureText(effect->text, 16);
    DrawText(effect->text, screenPos.x - textWidth/2, screenPos.y, 16, effectColor);
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

// Draw turn indicator
void DrawTurnIndicator(GameState* game) {
    Player* activePlayer = &game->players[game->activePlayer];
    DrawText(TextFormat("Turn %d - %s's Turn", game->turnNumber, activePlayer->name), 
             10, 40, 20, YELLOW);
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

// Get card screen position
Vector2 GetCardScreenPosition(Card* card, Camera3D camera) {
    Vector3 textPos = Vector3Add(card->position, (Vector3){0, 1.0f, 0});
    return GetWorldToScreen(textPos, camera);
}

// Draw card highlight
void DrawCardHighlight(Card* card, Color highlightColor) {
    DrawCube(card->position, card->size.x + 0.4f, card->size.y + 0.2f, 
            card->size.z + 0.4f, Fade(highlightColor, 0.3f));
}

// Draw targeting line
void DrawTargetingLine(Vector3 from, Vector3 to, Color color) {
    DrawLine3D(from, to, color);
    DrawSphere(to, 0.2f, color);
}

// Draw drag feedback
void DrawDragFeedback(GameState* game) {
    if (!game->selectedCard || !game->selectedCard->isDragging) return;
    
    Card* draggedCard = game->selectedCard;
    
    // Draw valid drop zones
    DrawValidDropZones(game, draggedCard);
    
    // Draw line from original position to current position
    Vector3 originalPos = draggedCard->targetPosition;
    Vector3 currentPos = draggedCard->position;
    
    Color lineColor = GREEN;
    if (draggedCard->inHand && draggedCard->type == CARD_TYPE_MINION) {
        // Check if over valid board area
        if (currentPos.z < -1 || currentPos.z > 1) {
            lineColor = RED;
        }
    }
    
    DrawLine3D(originalPos, currentPos, lineColor);
    
    // Draw arrow at current position
    DrawSphere(currentPos, 0.1f, lineColor);
    
    // Show what's under the cursor
    Ray mouseRay = GetMouseRay(GetMousePosition(), game->camera);
    void* target = GetTargetUnderMouse(game, mouseRay);
    
    if (target) {
        // Highlight valid targets
        Card* targetCard = NULL;
        Player* targetPlayer = NULL;
        
        // Check if it's a card
        for (int p = 0; p < 2; p++) {
            for (int i = 0; i < game->players[p].boardCount; i++) {
                if (&game->players[p].board[i] == target) {
                    targetCard = (Card*)target;
                    break;
                }
            }
            if (&game->players[p] == target) {
                targetPlayer = (Player*)target;
                break;
            }
        }
        
        if (targetCard) {
            DrawCardHighlight(targetCard, YELLOW);
        } else if (targetPlayer) {
            // Highlight player portrait
            Vector3 portraitPos = (Vector3){7.0f, 0.2f, targetPlayer->playerId == 0 ? 6.0f : -6.0f};
            DrawCube(portraitPos, 2.3f, 0.3f, 2.3f, Fade(YELLOW, 0.3f));
        }
    }
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

// Draw valid drop zones for the dragged card
void DrawValidDropZones(GameState* game, Card* draggedCard) {
    if (draggedCard->inHand) {
        if (draggedCard->type == CARD_TYPE_MINION) {
            // Highlight board area for minions
            DrawPlane((Vector3){0, -0.35f, 2}, (Vector2){16, 3}, Fade(GREEN, 0.3f));
        } else if (draggedCard->type == CARD_TYPE_SPELL || draggedCard->hasBattlecry) {
            // Highlight all valid targets for spells/battlecries
            
            // Highlight enemy minions
            Player* enemy = &game->players[1 - draggedCard->ownerPlayer];
            for (int i = 0; i < enemy->boardCount; i++) {
                DrawCardHighlight(&enemy->board[i], Fade(GREEN, 0.5f));
            }
            
            // Highlight enemy player
            Vector3 enemyPortraitPos = (Vector3){7.0f, 0.2f, enemy->playerId == 0 ? 6.0f : -6.0f};
            DrawCube(enemyPortraitPos, 2.3f, 0.3f, 2.3f, Fade(GREEN, 0.3f));
            
            // For healing spells, also highlight friendly targets
            if (draggedCard->healing > 0) {
                Player* ally = &game->players[draggedCard->ownerPlayer];
                for (int i = 0; i < ally->boardCount; i++) {
                    if (ally->board[i].health < ally->board[i].maxHealth) {
                        DrawCardHighlight(&ally->board[i], Fade(BLUE, 0.5f));
                    }
                }
                
                if (ally->health < ally->maxHealth) {
                    Vector3 allyPortraitPos = (Vector3){7.0f, 0.2f, ally->playerId == 0 ? 6.0f : -6.0f};
                    DrawCube(allyPortraitPos, 2.3f, 0.3f, 2.3f, Fade(BLUE, 0.3f));
                }
            }
        }
    } else if (draggedCard->onBoard && CanAttack(draggedCard)) {
        // Highlight valid attack targets
        Player* enemy = &game->players[1 - draggedCard->ownerPlayer];
        
        // Check for taunt minions
        bool hasTaunt = HasTauntMinions(enemy);
        
        if (hasTaunt) {
            // Can only attack taunt minions
            for (int i = 0; i < enemy->boardCount; i++) {
                if (enemy->board[i].taunt) {
                    DrawCardHighlight(&enemy->board[i], Fade(RED, 0.5f));
                }
            }
        } else {
            // Can attack any enemy minion or the enemy player
            for (int i = 0; i < enemy->boardCount; i++) {
                DrawCardHighlight(&enemy->board[i], Fade(RED, 0.5f));
            }
            
            // Highlight enemy player
            Vector3 enemyPortraitPos = (Vector3){7.0f, 0.2f, enemy->playerId == 0 ? 6.0f : -6.0f};
            DrawCube(enemyPortraitPos, 2.3f, 0.3f, 2.3f, Fade(RED, 0.3f));
        }
    }
}