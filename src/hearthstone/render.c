#include "render.h"
#include "game_state.h"
#include "input.h"
#include <stdlib.h>

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

// Update game camera
void UpdateGameCamera(GameState* game, float deltaTime) {
    // Apply camera shake
    if (game->cameraShake > 0) {
        game->camera.target.x = game->cameraShake * ((float)rand()/RAND_MAX - 0.5f);
        game->camera.target.y = game->cameraShake * ((float)rand()/RAND_MAX - 0.5f);
        game->cameraShake *= 0.9f; // Decay
        
        if (game->cameraShake < 0.01f) {
            game->cameraShake = 0;
            game->camera.target = (Vector3){0.0f, 0.0f, 0.0f};
        }
    }
}