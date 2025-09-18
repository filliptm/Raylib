#include "raylib.h"

int main(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "Raylib - Hello World!");

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        // TODO: Update your variables here
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawText("Hello, Raylib World!", 250, 200, 20, LIGHTGRAY);
            
            // Draw some shapes to make it more interesting
            DrawCircle(screenWidth/2, screenHeight/2 + 60, 40, RED);
            DrawRectangle(screenWidth/2 - 50, screenHeight/2 + 120, 100, 30, BLUE);
            DrawTriangle(
                (Vector2){screenWidth/2, screenHeight/2 + 180},
                (Vector2){screenWidth/2 - 30, screenHeight/2 + 220},
                (Vector2){screenWidth/2 + 30, screenHeight/2 + 220},
                GREEN
            );
            
            // Instructions
            DrawText("Press ESC to exit", 10, 10, 20, DARKGRAY);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}