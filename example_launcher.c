/*******************************************************************************************
*
*   Raylib Examples Launcher - Enhanced Edition
*
*   A modern tabbed interface to browse and launch all 182 raylib examples
*   Features: Mouse support, tabs, smooth scrolling, click to select, double-click to run
*
********************************************************************************************/

#include "raylib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define MAX_EXAMPLES 200
#define MAX_NAME_LENGTH 128
#define TAB_COUNT 8

typedef struct {
    char name[MAX_NAME_LENGTH];
    char category[32];
    char filepath[256];
    char difficulty[16];
} ExampleInfo;

typedef struct {
    char name[32];
    Color color;
    int count;
} TabInfo;

// Global variables
ExampleInfo examples[MAX_EXAMPLES];
int exampleCount = 0;
int selectedExample = 0;
int scrollOffset = 0;
int maxVisibleItems = 20;
int activeTab = 0; // 0 = All, 1-7 = specific categories
double lastClickTime = 0;
int lastClickedIndex = -1;

// Tabs
TabInfo tabs[TAB_COUNT] = {
    {"All", DARKGRAY, 0},
    {"Core", SKYBLUE, 0},
    {"Shapes", RED, 0},
    {"Textures", GREEN, 0},
    {"Text", GOLD, 0},
    {"Models", PURPLE, 0},
    {"Shaders", ORANGE, 0},
    {"Audio", PINK, 0}
};

// Category colors
Color GetCategoryColor(const char* category) {
    if (strcmp(category, "core") == 0) return SKYBLUE;
    if (strcmp(category, "shapes") == 0) return RED;
    if (strcmp(category, "textures") == 0) return GREEN;
    if (strcmp(category, "text") == 0) return GOLD;
    if (strcmp(category, "models") == 0) return PURPLE;
    if (strcmp(category, "shaders") == 0) return ORANGE;
    if (strcmp(category, "audio") == 0) return PINK;
    return GRAY;
}

// Get tab index from category name
int GetTabIndexFromCategory(const char* category) {
    if (strcmp(category, "core") == 0) return 1;
    if (strcmp(category, "shapes") == 0) return 2;
    if (strcmp(category, "textures") == 0) return 3;
    if (strcmp(category, "text") == 0) return 4;
    if (strcmp(category, "models") == 0) return 5;
    if (strcmp(category, "shaders") == 0) return 6;
    if (strcmp(category, "audio") == 0) return 7;
    return 0;
}

// Load all examples from the examples list
void LoadExamplesList() {
    FILE* fp = fopen("raylib-examples/examples_list.txt", "r");
    if (!fp) {
        printf("Could not open examples_list.txt\n");
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), fp) && exampleCount < MAX_EXAMPLES) {
        // Skip empty lines and comments
        if (line[0] == '\n' || line[0] == '#') continue;

        // Parse the line format: "category;example_name;stars;..."
        char category[32] = {0};
        char filename[128] = {0};
        char stars[16] = {0};

        // Use strtok to parse semicolon-separated values
        char* token = strtok(line, ";");
        if (token) {
            strncpy(category, token, 31);

            token = strtok(NULL, ";");
            if (token) {
                strncpy(filename, token, 127);

                token = strtok(NULL, ";");
                if (token) {
                    strncpy(stars, token, 15);
                }
            }
        }

        // Skip if parsing failed or not a main category
        if (strlen(category) == 0 || strlen(filename) == 0) continue;

        if (strcmp(category, "core") != 0 &&
            strcmp(category, "shapes") != 0 &&
            strcmp(category, "textures") != 0 &&
            strcmp(category, "text") != 0 &&
            strcmp(category, "models") != 0 &&
            strcmp(category, "shaders") != 0 &&
            strcmp(category, "audio") != 0) continue;

        // Store example info
        snprintf(examples[exampleCount].name, MAX_NAME_LENGTH, "%s", filename);
        snprintf(examples[exampleCount].category, 32, "%s", category);
        snprintf(examples[exampleCount].filepath, 256, "raylib-examples/%s/%s.c", category, filename);

        // Convert star symbols
        if (strlen(stars) > 0) {
            strncpy(examples[exampleCount].difficulty, stars, 15);
        } else {
            strcpy(examples[exampleCount].difficulty, "★☆☆☆");
        }

        // Count examples per category
        int tabIndex = GetTabIndexFromCategory(category);
        tabs[tabIndex].count++;
        tabs[0].count++; // All

        exampleCount++;
    }

    fclose(fp);
    printf("Loaded %d examples\n", exampleCount);
}

// Get filtered example index (accounting for active tab filter)
int GetFilteredExampleCount() {
    if (activeTab == 0) return exampleCount;

    int count = 0;
    for (int i = 0; i < exampleCount; i++) {
        if (GetTabIndexFromCategory(examples[i].category) == activeTab) {
            count++;
        }
    }
    return count;
}

// Get real index from filtered index
int GetRealIndexFromFiltered(int filteredIndex) {
    if (activeTab == 0) return filteredIndex;

    int count = 0;
    for (int i = 0; i < exampleCount; i++) {
        if (GetTabIndexFromCategory(examples[i].category) == activeTab) {
            if (count == filteredIndex) return i;
            count++;
        }
    }
    return 0;
}

// Get filtered index from real index
int GetFilteredIndexFromReal(int realIndex) {
    if (activeTab == 0) return realIndex;

    int count = 0;
    for (int i = 0; i <= realIndex && i < exampleCount; i++) {
        if (GetTabIndexFromCategory(examples[i].category) == activeTab) {
            if (i == realIndex) return count;
            count++;
        }
    }
    return 0;
}

// Draw tabs at the top
void DrawTabs(int screenWidth) {
    int tabWidth = screenWidth / TAB_COUNT;
    int tabHeight = 50;

    for (int i = 0; i < TAB_COUNT; i++) {
        int x = i * tabWidth;

        // Tab background
        Color bgColor = (i == activeTab) ? tabs[i].color : Fade(tabs[i].color, 0.3f);
        DrawRectangle(x, 0, tabWidth - 2, tabHeight, bgColor);

        // Tab border
        if (i == activeTab) {
            DrawRectangleLinesEx((Rectangle){x, 0, tabWidth - 2, tabHeight}, 3, WHITE);
        }

        // Tab text
        const char* tabText = TextFormat("%s (%d)", tabs[i].name, tabs[i].count);
        int textWidth = MeasureText(tabText, 18);
        Color textColor = (i == activeTab) ? WHITE : LIGHTGRAY;
        DrawText(tabText, x + (tabWidth - textWidth) / 2, 16, 18, textColor);
    }
}

// Draw the example list
void DrawExampleList(int screenWidth, int screenHeight) {
    int tabHeight = 50;
    int listY = tabHeight + 10;
    int itemHeight = 40;
    int footerHeight = 120;
    int visibleHeight = screenHeight - listY - footerHeight;
    maxVisibleItems = visibleHeight / itemHeight;

    int filteredCount = GetFilteredExampleCount();

    // Adjust scroll offset
    if (selectedExample < scrollOffset) scrollOffset = selectedExample;
    if (selectedExample >= scrollOffset + maxVisibleItems) scrollOffset = selectedExample - maxVisibleItems + 1;

    // Draw example list
    int displayIndex = 0;
    for (int i = 0; i < exampleCount; i++) {
        // Skip if not in active category
        if (activeTab != 0 && GetTabIndexFromCategory(examples[i].category) != activeTab) {
            continue;
        }

        // Skip if outside visible range
        if (displayIndex < scrollOffset || displayIndex >= scrollOffset + maxVisibleItems) {
            displayIndex++;
            continue;
        }

        int yPos = listY + (displayIndex - scrollOffset) * itemHeight;

        // Check mouse hover
        Vector2 mousePos = GetMousePosition();
        bool isHovered = CheckCollisionPointRec(mousePos,
            (Rectangle){10, yPos, screenWidth - 30, itemHeight - 2});

        // Background
        Color bgColor;
        if (i == GetRealIndexFromFiltered(selectedExample)) {
            bgColor = Fade(GetCategoryColor(examples[i].category), 0.5f);
        } else if (isHovered) {
            bgColor = Fade(LIGHTGRAY, 0.8f);
        } else {
            bgColor = RAYWHITE;
        }
        DrawRectangle(10, yPos, screenWidth - 30, itemHeight - 2, bgColor);

        // Border on hover
        if (isHovered) {
            DrawRectangleLinesEx((Rectangle){10, yPos, screenWidth - 30, itemHeight - 2}, 2, GRAY);
        }

        // Category indicator circle
        Color catColor = GetCategoryColor(examples[i].category);
        DrawCircle(30, yPos + itemHeight/2, 8, catColor);

        // Difficulty stars
        DrawText(examples[i].difficulty, 50, yPos + 10, 16, GOLD);

        // Example name
        Color textColor = (i == GetRealIndexFromFiltered(selectedExample)) ? BLACK : DARKGRAY;
        DrawText(examples[i].name, 130, yPos + 10, 20, textColor);

        displayIndex++;
    }

    // Scrollbar
    if (filteredCount > maxVisibleItems) {
        float scrollbarHeight = ((float)maxVisibleItems / filteredCount) * visibleHeight;
        float scrollbarY = listY + ((float)scrollOffset / filteredCount) * visibleHeight;
        DrawRectangle(screenWidth - 18, scrollbarY, 14, scrollbarHeight, GRAY);
        DrawRectangleLines(screenWidth - 18, listY, 14, visibleHeight, LIGHTGRAY);
    }
}

// Compile and run an example
void CompileAndRunExample(int index) {
    if (index < 0 || index >= exampleCount) return;

    char command[1024];

    // Create a compilation command with pkg-config for proper raylib paths
    snprintf(command, sizeof(command),
             "cd raylib-examples/%s && gcc %s.c -o /tmp/%s $(pkg-config --cflags --libs raylib) -framework OpenGL -framework Cocoa -framework IOKit && /tmp/%s",
             examples[index].category,
             examples[index].name,
             examples[index].name,
             examples[index].name);

    printf("\n==============================================\n");
    printf("Compiling: %s\n", examples[index].name);
    printf("Category: %s\n", examples[index].category);
    printf("Difficulty: %s\n", examples[index].difficulty);
    printf("==============================================\n");

    // Execute the command
    int result = system(command);
    if (result != 0) {
        printf("\n❌ Compilation/execution failed!\n");
        printf("Make sure raylib is installed: brew install raylib\n");
    } else {
        printf("\n✅ Example completed successfully!\n");
    }
    printf("==============================================\n\n");
}

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(void)
{
    // Initialization
    const int screenWidth = 1200;
    const int screenHeight = 800;

    InitWindow(screenWidth, screenHeight, "Raylib Examples Launcher - Enhanced Edition");
    SetTargetFPS(60);

    // Load all examples
    LoadExamplesList();

    if (exampleCount == 0) {
        printf("No examples found! Make sure examples_list.txt exists.\n");
        CloseWindow();
        return 1;
    }

    // Main loop
    while (!WindowShouldClose())
    {
        // Update
        //----------------------------------------------------------------------------------

        int filteredCount = GetFilteredExampleCount();
        int realSelectedIndex = GetRealIndexFromFiltered(selectedExample);

        // Mouse wheel scrolling
        float wheelMove = GetMouseWheelMove();
        if (wheelMove != 0) {
            selectedExample -= (int)wheelMove * 3;
            if (selectedExample < 0) selectedExample = 0;
            if (selectedExample >= filteredCount) selectedExample = filteredCount - 1;
        }

        // Tab clicking
        Vector2 mousePos = GetMousePosition();
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            int tabWidth = screenWidth / TAB_COUNT;
            if (mousePos.y < 50) {
                int clickedTab = (int)(mousePos.x / tabWidth);
                if (clickedTab >= 0 && clickedTab < TAB_COUNT) {
                    activeTab = clickedTab;
                    selectedExample = 0;
                    scrollOffset = 0;
                }
            }
        }

        // Mouse clicking on examples
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            int tabHeight = 50;
            int listY = tabHeight + 10;
            int itemHeight = 40;

            int displayIndex = 0;
            for (int i = 0; i < exampleCount; i++) {
                if (activeTab != 0 && GetTabIndexFromCategory(examples[i].category) != activeTab) {
                    continue;
                }

                if (displayIndex < scrollOffset || displayIndex >= scrollOffset + maxVisibleItems) {
                    displayIndex++;
                    continue;
                }

                int yPos = listY + (displayIndex - scrollOffset) * itemHeight;

                if (CheckCollisionPointRec(mousePos, (Rectangle){10, yPos, screenWidth - 30, itemHeight - 2})) {
                    double currentTime = GetTime();

                    // Double-click detection
                    if (lastClickedIndex == displayIndex && (currentTime - lastClickTime) < 0.3) {
                        // Double-click: run example
                        CompileAndRunExample(i);
                        lastClickedIndex = -1;
                    } else {
                        // Single click: select
                        selectedExample = displayIndex;
                        lastClickedIndex = displayIndex;
                        lastClickTime = currentTime;
                    }
                    break;
                }

                displayIndex++;
            }
        }

        // Keyboard navigation
        if (IsKeyPressed(KEY_UP)) {
            selectedExample--;
            if (selectedExample < 0) selectedExample = 0;
        }
        if (IsKeyPressed(KEY_DOWN)) {
            selectedExample++;
            if (selectedExample >= filteredCount) selectedExample = filteredCount - 1;
        }

        // Page navigation
        if (IsKeyPressed(KEY_PAGE_UP)) {
            selectedExample -= maxVisibleItems;
            if (selectedExample < 0) selectedExample = 0;
        }
        if (IsKeyPressed(KEY_PAGE_DOWN)) {
            selectedExample += maxVisibleItems;
            if (selectedExample >= filteredCount) selectedExample = filteredCount - 1;
        }

        // Home/End
        if (IsKeyPressed(KEY_HOME)) selectedExample = 0;
        if (IsKeyPressed(KEY_END)) selectedExample = filteredCount - 1;

        // Run selected example
        if (IsKeyPressed(KEY_ENTER)) {
            CompileAndRunExample(realSelectedIndex);
        }

        // View source code
        if (IsKeyPressed(KEY_C)) {
            char command[512];
            snprintf(command, sizeof(command), "cat %s", examples[realSelectedIndex].filepath);
            system(command);
        }

        // Quick tab switching with number keys
        if (IsKeyPressed(KEY_ONE)) { activeTab = 0; selectedExample = 0; scrollOffset = 0; }
        if (IsKeyPressed(KEY_TWO)) { activeTab = 1; selectedExample = 0; scrollOffset = 0; }
        if (IsKeyPressed(KEY_THREE)) { activeTab = 2; selectedExample = 0; scrollOffset = 0; }
        if (IsKeyPressed(KEY_FOUR)) { activeTab = 3; selectedExample = 0; scrollOffset = 0; }
        if (IsKeyPressed(KEY_FIVE)) { activeTab = 4; selectedExample = 0; scrollOffset = 0; }
        if (IsKeyPressed(KEY_SIX)) { activeTab = 5; selectedExample = 0; scrollOffset = 0; }
        if (IsKeyPressed(KEY_SEVEN)) { activeTab = 6; selectedExample = 0; scrollOffset = 0; }
        if (IsKeyPressed(KEY_EIGHT)) { activeTab = 7; selectedExample = 0; scrollOffset = 0; }

        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();
            ClearBackground((Color){245, 245, 245, 255});

            // Draw tabs
            DrawTabs(screenWidth);

            // Draw example list
            DrawExampleList(screenWidth, screenHeight);

            // Footer with selected example info
            int footerY = screenHeight - 110;
            DrawRectangle(0, footerY, screenWidth, 110, DARKGRAY);

            if (realSelectedIndex >= 0 && realSelectedIndex < exampleCount) {
                // Title
                DrawText("SELECTED EXAMPLE:", 20, footerY + 10, 18, LIGHTGRAY);

                // Example name with category color
                Color catColor = GetCategoryColor(examples[realSelectedIndex].category);
                DrawRectangle(20, footerY + 35, 10, 35, catColor);
                DrawText(examples[realSelectedIndex].name, 40, footerY + 38, 24, WHITE);

                // Info line
                DrawText(TextFormat("Category: %s  |  Difficulty: %s  |  File: %s",
                    examples[realSelectedIndex].category,
                    examples[realSelectedIndex].difficulty,
                    examples[realSelectedIndex].filepath),
                    20, footerY + 70, 16, LIGHTGRAY);
            }

            // Instructions
            DrawText("🖱️ CLICK to select  |  DOUBLE-CLICK to run  |  SCROLL to navigate  |  ENTER to compile & run  |  ESC to exit",
                20, footerY + 90, 14, GRAY);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    CloseWindow();

    return 0;
}
