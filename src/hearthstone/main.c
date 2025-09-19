#include "raylib.h"
#include "game_state.h"
#include "render.h"
#include "input.h"

int main(void) {
    // Initialization
    const int screenWidth = 1400;
    const int screenHeight = 900;
    
    InitWindow(screenWidth, screenHeight, "Hearthstone Clone - Modular Version");
    SetTargetFPS(60);
    
    // Initialize game state
    GameState game = {0};
    InitializeGame(&game);
    
    // Main game loop
    while (!WindowShouldClose()) {
        // Input
        HandleInput(&game);
        
        // Update
        UpdateGame(&game);
        
        // Draw
        BeginDrawing();
            ClearBackground(DARKGREEN);
            DrawGame(&game);
        EndDrawing();
    }
    
    // Cleanup
    CleanupGame(&game);
    CloseWindow();
    
    return 0;
}