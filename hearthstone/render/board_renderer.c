#include "board_renderer.h"
#include "../game_state.h"
#include "../card.h"
#include "../input.h"
#include "../combat.h"
#include "card_renderer.h"
#include <stdio.h>

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

// Draw targeting line
void DrawTargetingLine(Vector3 from, Vector3 to, Color color) {
    DrawLine3D(from, to, color);
    DrawSphere(to, 0.2f, color);
}