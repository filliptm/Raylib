#ifndef RENDER_H
#define RENDER_H

#include "types.h"
#include "common.h"
#include "raylib.h"

// Main rendering functions
void DrawGame(GameState* game);
void DrawGameBoard(void);
void DrawGameUI(GameState* game);
void DrawGameEndScreen(GameState* game);

// Card rendering
void DrawCard3D(Card* card, Camera3D camera);
void DrawCardStats(Card* card, Vector2 screenPos);
void DrawCardStatsOnCard(Card* card, Camera3D camera);
void DrawAllCardStats(GameState* game);
void DrawCardKeywords(Card* card, Vector2 screenPos);
void DrawCardEffects(Card* card);

// Player rendering
void DrawPlayerInfo(Player* player, int yPosition, bool isActive);
void DrawPlayerHand(Player* player, Camera3D camera);
void DrawPlayerBoard(Player* player, Camera3D camera);
void DrawPlayerPortrait(Player* player, Camera3D camera);

// Effect rendering
void DrawVisualEffects(GameState* game);
void DrawEffect(VisualEffect* effect, Camera3D camera);

// UI elements
void DrawManaBar(int current, int max, int x, int y);
void DrawHealthBar(int current, int max, int x, int y);
void DrawTurnIndicator(GameState* game);
void DrawControls(void);
void DrawSelectedCardInfo(Card* card);

// Helper functions
Vector2 GetCardScreenPosition(Card* card, Camera3D camera);
void DrawCardHighlight(Card* card, Color highlightColor);
void DrawTargetingLine(Vector3 from, Vector3 to, Color color);
void DrawDragFeedback(GameState* game);
void DrawValidDropZones(GameState* game, Card* draggedCard);

#endif // RENDER_H