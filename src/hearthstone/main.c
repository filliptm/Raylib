#include "raylib.h"
#include "game_state.h"
#include "render.h"
#include "input.h"
#include "network.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    // Initialization
    const int screenWidth = 1400;
    const int screenHeight = 900;

    InitWindow(screenWidth, screenHeight, "Hearthstone Clone - Modular Version");
    SetTargetFPS(60);

    // Initialize game state
    GameState game = {0};

    // Parse command line arguments for game mode
    if (argc > 1) {
        if (strcmp(argv[1], "server") == 0) {
            int port = (argc > 2) ? atoi(argv[2]) : DEFAULT_PORT;
            printf("Starting game as server on port %d\n", port);
            InitializeGameAsServer(&game, port);
        } else if (strcmp(argv[1], "client") == 0) {
            const char* address = (argc > 2) ? argv[2] : "127.0.0.1";
            int port = (argc > 3) ? atoi(argv[3]) : DEFAULT_PORT;
            printf("Connecting to server at %s:%d\n", address, port);
            InitializeGameAsClient(&game, address, port);
        } else if (strcmp(argv[1], "ai") == 0) {
            int difficulty = (argc > 2) ? atoi(argv[2]) : 1;
            printf("Starting game with AI difficulty %d\n", difficulty);
            InitializeGameWithAI(&game, difficulty);
        } else {
            printf("Usage: %s [server [port] | client [address] [port] | ai [difficulty]]\n", argv[0]);
            printf("  server: Start as multiplayer server\n");
            printf("  client: Connect to multiplayer server\n");
            printf("  ai: Play against AI (0=easy, 1=medium, 2=hard)\n");
            InitializeGame(&game); // Default local game
        }
    } else {
        // Default: AI game with medium difficulty
        printf("Starting default AI game (medium difficulty)\n");
        InitializeGameWithAI(&game, 1);
    }
    
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