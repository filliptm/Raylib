#include "raylib.h"
#include "raymath.h"
#include <stdio.h>
#include <stdlib.h>

// Card structure
typedef struct {
    Vector3 position;
    Vector3 targetPosition;
    Vector3 size;
    Color color;
    bool isHovered;
    bool isSelected;
    bool isDragging;
    char name[32];
    int cost;
    int attack;
    int health;
} Card;

// Game state
typedef struct {
    Camera3D camera;
    Card playerCards[7];    // Hand
    Card boardCards[14];    // 7 per side
    Card *selectedCard;
    int playerCardCount;
    int boardCardCount;
    Vector3 mouseWorldPos;
    Ray mouseRay;
} GameState;

// Initialize a card
Card CreateCard(Vector3 pos, Color color, const char* name, int cost, int attack, int health) {
    Card card = {0};
    card.position = pos;
    card.targetPosition = pos;
    card.size = (Vector3){1.6f, 0.1f, 2.4f};  // Credit card proportions
    card.color = color;
    card.isHovered = false;
    card.isSelected = false;
    card.isDragging = false;
    sprintf(card.name, "%.31s", name);
    card.cost = cost;
    card.attack = attack;
    card.health = health;
    return card;
}

// Initialize game state
void InitGame(GameState *game) {
    // Setup top-down camera (like Hearthstone)
    game->camera.position = (Vector3){0.0f, 20.0f, 8.0f};  // High up, slight angle
    game->camera.target = (Vector3){0.0f, 0.0f, 0.0f};
    game->camera.up = (Vector3){0.0f, 0.0f, -1.0f};        // Important for top-down
    game->camera.fovy = 45.0f;
    game->camera.projection = CAMERA_PERSPECTIVE;
    
    // Create player hand cards
    game->playerCardCount = 5;
    for (int i = 0; i < game->playerCardCount; i++) {
        Vector3 handPos = {-4.0f + i * 2.0f, 0.0f, 8.0f};
        game->playerCards[i] = CreateCard(handPos, BLUE, 
            TextFormat("Card %d", i + 1), i + 1, i + 2, i + 3);
    }
    
    // Create some board cards
    game->boardCardCount = 4;
    game->boardCards[0] = CreateCard((Vector3){-3.0f, 0.0f, 2.0f}, RED, "Enemy 1", 3, 4, 2);
    game->boardCards[1] = CreateCard((Vector3){0.0f, 0.0f, 2.0f}, RED, "Enemy 2", 5, 6, 4);
    game->boardCards[2] = CreateCard((Vector3){-1.5f, 0.0f, -2.0f}, GREEN, "Ally 1", 2, 3, 3);
    game->boardCards[3] = CreateCard((Vector3){1.5f, 0.0f, -2.0f}, GREEN, "Ally 2", 4, 5, 5);
    
    game->selectedCard = NULL;
}

// Update card animations
void UpdateCard(Card *card, float deltaTime) {
    // Smooth movement to target position
    card->position = Vector3Lerp(card->position, card->targetPosition, deltaTime * 8.0f);
    
    // Hover effect - lift card up
    if (card->isHovered && !card->isDragging) {
        card->targetPosition.y = 0.5f;
    } else if (!card->isDragging) {
        card->targetPosition.y = 0.0f;
    }
}

// Check if mouse ray hits a card
bool CheckCardHit(Card *card, Ray ray) {
    BoundingBox cardBox = {
        Vector3Subtract(card->position, Vector3Scale(card->size, 0.5f)),
        Vector3Add(card->position, Vector3Scale(card->size, 0.5f))
    };
    RayCollision collision = GetRayCollisionBox(ray, cardBox);
    return collision.hit;
}

// Draw a 3D card
void DrawCard(Card *card, Camera3D camera) {
    Vector3 pos = card->position;
    
    // Card base
    DrawCube(pos, card->size.x, card->size.y, card->size.z, card->color);
    DrawCubeWires(pos, card->size.x, card->size.y, card->size.z, BLACK);
    
    // Draw card info (floating above card)
    Vector3 textPos = Vector3Add(pos, (Vector3){0, 1.0f, 0});
    
    // Convert 3D position to screen position for text
    Vector2 screenPos = GetWorldToScreen(textPos, camera);
    
    if (screenPos.x >= 0 && screenPos.x <= GetScreenWidth() && 
        screenPos.y >= 0 && screenPos.y <= GetScreenHeight()) {
        
        // Card name
        DrawText(card->name, screenPos.x - MeasureText(card->name, 16)/2, 
                 screenPos.y - 40, 16, WHITE);
        
        // Cost/Attack/Health
        DrawText(TextFormat("%d", card->cost), screenPos.x - 30, screenPos.y - 20, 14, YELLOW);
        DrawText(TextFormat("%d/%d", card->attack, card->health), 
                 screenPos.x + 10, screenPos.y - 20, 14, WHITE);
    }
    
    // Selection glow
    if (card->isSelected) {
        DrawCube(pos, card->size.x + 0.2f, card->size.y + 0.1f, card->size.z + 0.2f, 
                 Fade(YELLOW, 0.3f));
    }
}

// Draw the game board
void DrawGameBoard() {
    // Main board surface
    DrawPlane((Vector3){0, -0.5f, 0}, (Vector2){20, 16}, BROWN);
    
    // Player area
    DrawPlane((Vector3){0, -0.4f, 6}, (Vector2){16, 4}, Fade(BLUE, 0.3f));
    
    // Enemy area  
    DrawPlane((Vector3){0, -0.4f, -6}, (Vector2){16, 4}, Fade(RED, 0.3f));
    
    // Board center
    DrawPlane((Vector3){0, -0.4f, 0}, (Vector2){16, 8}, Fade(GREEN, 0.2f));
    
    // Board lines
    for (int i = -7; i <= 7; i++) {
        DrawLine3D((Vector3){i * 2, 0, -8}, (Vector3){i * 2, 0, 8}, DARKGRAY);
    }
    for (int i = -4; i <= 4; i++) {
        DrawLine3D((Vector3){-8, 0, i * 2}, (Vector3){8, 0, i * 2}, DARKGRAY);
    }
}

// Update game logic
void UpdateGame(GameState *game) {
    float deltaTime = GetFrameTime();
    
    // Get mouse ray for 3D picking
    game->mouseRay = GetMouseRay(GetMousePosition(), game->camera);
    
    // Reset hover states
    for (int i = 0; i < game->playerCardCount; i++) {
        game->playerCards[i].isHovered = false;
    }
    for (int i = 0; i < game->boardCardCount; i++) {
        game->boardCards[i].isHovered = false;
    }
    
    // Check card hovering
    Card *hoveredCard = NULL;
    
    // Check player cards
    for (int i = 0; i < game->playerCardCount; i++) {
        if (CheckCardHit(&game->playerCards[i], game->mouseRay)) {
            game->playerCards[i].isHovered = true;
            hoveredCard = &game->playerCards[i];
        }
    }
    
    // Check board cards
    for (int i = 0; i < game->boardCardCount; i++) {
        if (CheckCardHit(&game->boardCards[i], game->mouseRay)) {
            game->boardCards[i].isHovered = true;
            hoveredCard = &game->boardCards[i];
        }
    }
    
    // Handle mouse input
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (hoveredCard) {
            // Select/deselect card
            if (game->selectedCard) {
                game->selectedCard->isSelected = false;
            }
            game->selectedCard = hoveredCard;
            hoveredCard->isSelected = true;
            hoveredCard->isDragging = true;
        } else {
            // Deselect if clicking empty space
            if (game->selectedCard) {
                game->selectedCard->isSelected = false;
                game->selectedCard = NULL;
            }
        }
    }
    
    // Handle card dragging
    if (game->selectedCard && game->selectedCard->isDragging) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            // Move card to mouse position (projected onto board plane)
            RayCollision boardHit = GetRayCollisionQuad(game->mouseRay,
                (Vector3){-10, 0, -10}, (Vector3){10, 0, -10},
                (Vector3){10, 0, 10}, (Vector3){-10, 0, 10});
            
            if (boardHit.hit) {
                game->selectedCard->targetPosition = boardHit.point;
                game->selectedCard->targetPosition.y = 0.8f; // Keep elevated while dragging
            }
        } else {
            // Drop card
            game->selectedCard->isDragging = false;
            game->selectedCard->targetPosition.y = 0.0f;
        }
    }
    
    // Update all cards
    for (int i = 0; i < game->playerCardCount; i++) {
        UpdateCard(&game->playerCards[i], deltaTime);
    }
    for (int i = 0; i < game->boardCardCount; i++) {
        UpdateCard(&game->boardCards[i], deltaTime);
    }
}

// Draw game
void DrawGame(GameState *game) {
    BeginMode3D(game->camera);
        // Draw environment
        DrawGameBoard();
        
        // Draw cards
        for (int i = 0; i < game->playerCardCount; i++) {
            DrawCard(&game->playerCards[i], game->camera);
        }
        for (int i = 0; i < game->boardCardCount; i++) {
            DrawCard(&game->boardCards[i], game->camera);
        }
        
        // Draw grid for reference
        DrawGrid(20, 1.0f);
        
    EndMode3D();
    
    // Draw UI
    DrawText("3D Card Game MVP", 10, 10, 20, WHITE);
    DrawText("Left click to select/move cards", 10, 40, 16, LIGHTGRAY);
    DrawText("Cards lift when hovered, glow when selected", 10, 60, 16, LIGHTGRAY);
    
    if (game->selectedCard) {
        DrawText(TextFormat("Selected: %s (%d mana, %d/%d)", 
            game->selectedCard->name, game->selectedCard->cost,
            game->selectedCard->attack, game->selectedCard->health), 
            10, 90, 16, YELLOW);
    }
    
    DrawFPS(GetScreenWidth() - 80, 10);
}

int main(void) {
    // Initialization
    const int screenWidth = 1200;
    const int screenHeight = 800;
    
    InitWindow(screenWidth, screenHeight, "3D Card Game - Hearthstone Style");
    SetTargetFPS(60);
    
    GameState game = {0};
    InitGame(&game);
    
    // Main game loop
    while (!WindowShouldClose()) {
        // Update
        UpdateGame(&game);
        
        // Draw
        BeginDrawing();
            ClearBackground(DARKGREEN);
            DrawGame(&game);
        EndDrawing();
    }
    
    // Cleanup
    CloseWindow();
    
    return 0;
}