#include "editor.h"
#include "config.h"
#include "../game_state.h"
#include "../card.h"
#include "../player.h"
#include "../input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Global editor instance
EditorState* g_editor = NULL;

// Initialize the editor
void InitEditor(GameState* gameState) {
    if (g_editor != NULL) {
        CleanupEditor();
    }

    g_editor = (EditorState*)malloc(sizeof(EditorState));
    memset(g_editor, 0, sizeof(EditorState));

    g_editor->mode = EDITOR_MODE_GAME;
    g_editor->gameState = gameState;
    g_editor->isInitialized = true;
    g_editor->livePreview = true;

    // Initialize camera
    InitEditorCamera();

    // Initialize property panel
    g_editor->properties.bounds = (Rectangle){10, 100, 300, 400};
    g_editor->properties.isVisible = false;
    g_editor->showPropertyPanel = false;

    // Initialize object browser
    g_editor->browser.bounds = (Rectangle){GetScreenWidth() - 310, 100, 300, 400};
    g_editor->browser.isVisible = false;
    g_editor->showObjectBrowser = false;

    // Initialize timeline
    g_editor->timeline.timelineRect = (Rectangle){10, GetScreenHeight() - 110, GetScreenWidth() - 20, 100};
    g_editor->timeline.showTimeline = false;
    g_editor->showTimeline = false;

    // Initialize debug renderer
    g_editor->debug.gridSize = 1.0f;
    g_editor->debug.gridColor = GRAY;
    g_editor->debug.showGrid = true;

    // Initialize gizmo
    g_editor->gizmo.type = GIZMO_MOVE;
    g_editor->gizmo.gizmoSize = 1.0f;
    g_editor->gizmo.coordSystem = COORD_WORLD;
    g_editor->gizmo.activeAxis = -1;

    // Initialize config manager
    g_editor->config.autoSave = true;
    g_editor->config.autoSaveInterval = 30.0f; // 30 seconds
    strcpy(g_editor->config.currentConfigFile, "editor_config.txt");

    // Initialize snap settings
    g_editor->snapToGrid = false;
    g_editor->snapSize = 0.5f;

    // Try to load existing config
    EditorConfig config;
    if (LoadEditorConfig(g_editor->config.currentConfigFile, &config)) {
        ApplyConfigToEditor(&config);
    }

    // Initialize UI panels
    g_editor->panelBounds[0] = g_editor->properties.bounds;      // Property panel
    g_editor->panelBounds[1] = g_editor->browser.bounds;        // Object browser
    g_editor->panelBounds[2] = g_editor->timeline.timelineRect; // Timeline
    g_editor->panelBounds[3] = (Rectangle){10, 10, 250, 80};    // Debug panel

    printf("Editor initialized successfully\n");
}

// Update the editor
void UpdateEditor(GameState* gameState) {
    if (!g_editor || !g_editor->isInitialized) return;

    g_editor->deltaTime = GetFrameTime();
    g_editor->frameCount++;

    // Update performance metrics
    if (g_editor->frameCount % 60 == 0) {
        g_editor->avgFrameTime = g_editor->deltaTime;
    }

    // Handle input
    HandleEditorInput();

    // Update camera if in editor mode
    if (g_editor->mode != EDITOR_MODE_GAME) {
        UpdateEditorCamera();
    }

    // Update gizmo
    if (g_editor->selectionCount > 0) {
        UpdateGizmo();
    }

    // Update property panel
    if (g_editor->showPropertyPanel) {
        UpdatePropertyPanel();
    }

    // Auto-save
    if (g_editor->config.autoSave) {
        g_editor->config.lastSaveTime += g_editor->deltaTime;
        if (g_editor->config.lastSaveTime >= g_editor->config.autoSaveInterval) {
            SaveConfiguration(g_editor->config.currentConfigFile);
            g_editor->config.lastSaveTime = 0.0f;
        }
    }

    // Update debug renderer
    UpdateDebugRenderer();
}

// Draw the editor (3D elements only - this is called within BeginMode3D)
void DrawEditor(GameState* gameState) {
    if (!g_editor || !g_editor->isInitialized) return;

    // Only draw editor elements if not in pure game mode
    if (g_editor->mode == EDITOR_MODE_GAME) return;

    // Draw editor grid (separate from game grid)
    if (g_editor->debug.showGrid) {
        DrawEditorGrid();
    }

    // Draw selection highlights
    for (int i = 0; i < g_editor->selectionCount; i++) {
        Selection* selection = &g_editor->selections[i];
        if (selection->isSelected) {
            DrawBoundingBox(selection->bounds, YELLOW);
        }
    }

    // Draw gizmo if objects are selected
    if (g_editor->selectionCount > 0) {
        DrawGizmo();
    }
}

// Cleanup the editor
void CleanupEditor(void) {
    if (g_editor) {
        free(g_editor);
        g_editor = NULL;
        printf("Editor cleaned up\n");
    }
}

// Mode management functions
void SetEditorMode(EditorMode mode) {
    if (!g_editor) return;

    EditorMode oldMode = g_editor->mode;
    g_editor->mode = mode;

    if (oldMode != mode) {
        printf("Editor mode changed from %d to %d\n", oldMode, mode);

        // Handle mode-specific initialization
        switch (mode) {
            case EDITOR_MODE_GAME:
                g_editor->showPropertyPanel = false;
                g_editor->showObjectBrowser = false;
                g_editor->showTimeline = false;
                break;
            case EDITOR_MODE_EDITOR:
                g_editor->showPropertyPanel = true;
                g_editor->showObjectBrowser = false;  // Start with object browser off for cleaner view
                break;
            case EDITOR_MODE_HYBRID:
                g_editor->showPropertyPanel = true;
                break;
        }
    }
}

EditorMode GetEditorMode(void) {
    return g_editor ? g_editor->mode : EDITOR_MODE_GAME;
}

bool IsEditorMode(void) {
    return g_editor && g_editor->mode != EDITOR_MODE_GAME;
}

void ToggleEditorMode(void) {
    if (!g_editor) return;

    switch (g_editor->mode) {
        case EDITOR_MODE_GAME:
            SetEditorMode(EDITOR_MODE_EDITOR);
            break;
        case EDITOR_MODE_EDITOR:
            SetEditorMode(EDITOR_MODE_GAME);
            break;
        case EDITOR_MODE_HYBRID:
            SetEditorMode(EDITOR_MODE_GAME);
            break;
    }
}

// Camera functions
void InitEditorCamera(void) {
    if (!g_editor) return;

    g_editor->camera.camera.position = (Vector3){0.0f, 10.0f, 8.0f};
    g_editor->camera.camera.target = (Vector3){0.0f, 0.0f, 0.0f};
    g_editor->camera.camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    g_editor->camera.camera.fovy = 45.0f;
    g_editor->camera.camera.projection = CAMERA_PERSPECTIVE;

    g_editor->camera.target = g_editor->camera.camera.target;
    g_editor->camera.distance = Vector3Distance(g_editor->camera.camera.position, g_editor->camera.camera.target);
    g_editor->camera.angleX = 0.0f;
    g_editor->camera.angleY = -30.0f * DEG2RAD;
    g_editor->camera.speed = 5.0f;
    g_editor->camera.sensitivity = 0.003f;
    g_editor->camera.isControlled = false;
}

void UpdateEditorCamera(void) {
    if (!g_editor || !IsEditorMode() || !g_editor->gameState) return;

    // In editor mode, we'll modify the game camera directly
    Camera3D* gameCamera = &g_editor->gameState->camera;

    // Check if mouse is over UI
    g_editor->isMouseOverUI = IsMouseOverEditor();

    // Only update camera if not over UI
    if (!g_editor->isMouseOverUI) {
        // Right mouse button for camera control
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            g_editor->camera.isControlled = true;

            Vector2 mousePos = GetMousePosition();
            Vector2 mouseDelta = Vector2Subtract(mousePos, g_editor->lastMousePos);

            // Create a simple orbit camera around the target
            static float cameraAngleX = 0.0f;
            static float cameraAngleY = -30.0f * DEG2RAD;
            static Vector3 cameraTarget = {0, 0, 0};
            static float cameraDistance = 15.0f;

            // Rotate camera
            cameraAngleX += mouseDelta.x * 0.003f;
            cameraAngleY += mouseDelta.y * 0.003f;

            // Clamp vertical angle
            if (cameraAngleY > 89.0f * DEG2RAD) cameraAngleY = 89.0f * DEG2RAD;
            if (cameraAngleY < -89.0f * DEG2RAD) cameraAngleY = -89.0f * DEG2RAD;

            // Update camera position
            float x = cosf(cameraAngleY) * sinf(cameraAngleX) * cameraDistance;
            float y = sinf(cameraAngleY) * cameraDistance;
            float z = cosf(cameraAngleY) * cosf(cameraAngleX) * cameraDistance;

            gameCamera->position = Vector3Add(cameraTarget, (Vector3){x, y, z});
            gameCamera->target = cameraTarget;
        } else {
            g_editor->camera.isControlled = false;
        }

        // Mouse wheel for zoom
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            Vector3 forward = Vector3Normalize(Vector3Subtract(gameCamera->target, gameCamera->position));
            gameCamera->position = Vector3Add(gameCamera->position, Vector3Scale(forward, wheel * 0.5f));
        }

        // WASD for movement
        Vector3 movement = {0};
        if (IsKeyDown(KEY_W)) movement.z -= 1.0f;
        if (IsKeyDown(KEY_S)) movement.z += 1.0f;
        if (IsKeyDown(KEY_A)) movement.x -= 1.0f;
        if (IsKeyDown(KEY_D)) movement.x += 1.0f;
        if (IsKeyDown(KEY_Q)) movement.y -= 1.0f;
        if (IsKeyDown(KEY_E)) movement.y += 1.0f;

        if (Vector3Length(movement) > 0.0f) {
            movement = Vector3Normalize(movement);
            movement = Vector3Scale(movement, 5.0f * g_editor->deltaTime);
            gameCamera->target = Vector3Add(gameCamera->target, movement);
            gameCamera->position = Vector3Add(gameCamera->position, movement);
        }
    }

    g_editor->lastMousePos = GetMousePosition();
}

void ResetEditorCamera(void) {
    InitEditorCamera();
}

void SetEditorCameraTarget(Vector3 target) {
    if (!g_editor) return;
    g_editor->camera.target = target;
    g_editor->camera.camera.target = target;
}

// Selection functions
void ClearSelection(void) {
    if (!g_editor) return;
    g_editor->selectionCount = 0;
    memset(g_editor->selections, 0, sizeof(g_editor->selections));
}

void AddSelection(SelectionType type, void* object, int id) {
    if (!g_editor || g_editor->selectionCount >= 16) return;

    Selection* selection = &g_editor->selections[g_editor->selectionCount];
    selection->type = type;
    selection->object = object;
    selection->id = id;
    selection->isSelected = true;

    // Set bounds based on actual object position
    switch (type) {
        case SELECT_CARD: {
            Card* card = (Card*)object;
            selection->bounds = (BoundingBox){
                {card->position.x - 0.5f, card->position.y, card->position.z - 0.7f},
                {card->position.x + 0.5f, card->position.y + 0.1f, card->position.z + 0.7f}
            };
            break;
        }
        case SELECT_PLAYER: {
            Player* player = (Player*)object;
            // Determine player index for positioning
            int playerIndex = 0;
            if (g_editor->gameState) {
                if (player == &g_editor->gameState->players[1]) {
                    playerIndex = 1;
                }
            }
            float zPos = (playerIndex == 0) ? 5.0f : -5.0f;
            selection->bounds = (BoundingBox){
                {-2.0f, 0.0f, zPos - 1.0f},
                {2.0f, 0.2f, zPos + 1.0f}
            };
            break;
        }
        default:
            selection->bounds = (BoundingBox){{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}};
            break;
    }

    g_editor->selectionCount++;
    printf("Selected %s object\n",
           type == SELECT_CARD ? "card" :
           type == SELECT_PLAYER ? "player" : "unknown");
}

void RemoveSelection(int index) {
    if (!g_editor || index < 0 || index >= g_editor->selectionCount) return;

    // Shift selections down
    for (int i = index; i < g_editor->selectionCount - 1; i++) {
        g_editor->selections[i] = g_editor->selections[i + 1];
    }
    g_editor->selectionCount--;
}

bool IsSelected(void* object) {
    if (!g_editor) return false;

    for (int i = 0; i < g_editor->selectionCount; i++) {
        if (g_editor->selections[i].object == object) {
            return true;
        }
    }
    return false;
}

Selection* GetSelection(int index) {
    if (!g_editor || index < 0 || index >= g_editor->selectionCount) return NULL;
    return &g_editor->selections[index];
}

int GetSelectionCount(void) {
    return g_editor ? g_editor->selectionCount : 0;
}

// Transform functions (improved implementation)
void UpdateGizmo(void) {
    if (!g_editor || g_editor->selectionCount == 0) return;

    // Position gizmo at center of first selected object
    Selection* selection = &g_editor->selections[0];
    if (selection->object) {
        Vector3 center = Vector3Scale(Vector3Add(selection->bounds.min, selection->bounds.max), 0.5f);
        g_editor->gizmo.position = center;
        g_editor->gizmo.isActive = true;

        // Handle gizmo interaction
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !g_editor->isMouseOverUI) {
            Ray mouseRay = GetEditorMouseRay();

            // Check collision with gizmo axes
            float axisLength = g_editor->gizmo.gizmoSize;

            // X axis (red)
            BoundingBox xAxisBox = {
                {center.x, center.y - 0.1f, center.z - 0.1f},
                {center.x + axisLength, center.y + 0.1f, center.z + 0.1f}
            };
            RayCollision xCollision = GetRayCollisionBox(mouseRay, xAxisBox);

            // Y axis (green)
            BoundingBox yAxisBox = {
                {center.x - 0.1f, center.y, center.z - 0.1f},
                {center.x + 0.1f, center.y + axisLength, center.z + 0.1f}
            };
            RayCollision yCollision = GetRayCollisionBox(mouseRay, yAxisBox);

            // Z axis (blue)
            BoundingBox zAxisBox = {
                {center.x - 0.1f, center.y - 0.1f, center.z},
                {center.x + 0.1f, center.y + 0.1f, center.z + axisLength}
            };
            RayCollision zCollision = GetRayCollisionBox(mouseRay, zAxisBox);

            if (xCollision.hit) {
                g_editor->gizmo.activeAxis = 0; // X
                g_editor->gizmo.isDragging = true;
                g_editor->gizmo.dragStart = GetMousePosition();
                printf("Started dragging X axis\n");
            } else if (yCollision.hit) {
                g_editor->gizmo.activeAxis = 1; // Y
                g_editor->gizmo.isDragging = true;
                g_editor->gizmo.dragStart = GetMousePosition();
                printf("Started dragging Y axis\n");
            } else if (zCollision.hit) {
                g_editor->gizmo.activeAxis = 2; // Z
                g_editor->gizmo.isDragging = true;
                g_editor->gizmo.dragStart = GetMousePosition();
                printf("Started dragging Z axis\n");
            }
        }

        // Handle drag movement
        if (g_editor->gizmo.isDragging && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 currentMouse = GetMousePosition();
            Vector2 mouseDelta = Vector2Subtract(currentMouse, g_editor->gizmo.dragStart);

            float moveSpeed = 0.01f;
            Vector3 movement = {0};

            switch (g_editor->gizmo.activeAxis) {
                case 0: // X axis
                    movement.x = mouseDelta.x * moveSpeed;
                    break;
                case 1: // Y axis
                    movement.y = -mouseDelta.y * moveSpeed;
                    break;
                case 2: // Z axis
                    movement.z = mouseDelta.y * moveSpeed;
                    break;
            }

            // Apply movement to selection bounds (simplified)
            selection->bounds.min = Vector3Add(selection->bounds.min, movement);
            selection->bounds.max = Vector3Add(selection->bounds.max, movement);

            g_editor->gizmo.dragStart = currentMouse;
        }

        // End dragging
        if (g_editor->gizmo.isDragging && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            g_editor->gizmo.isDragging = false;
            g_editor->gizmo.activeAxis = -1;
            printf("Finished dragging\n");
        }
    }
}

void DrawGizmo(void) {
    if (!g_editor || !g_editor->gizmo.isActive) return;

    Vector3 pos = g_editor->gizmo.position;
    float size = g_editor->gizmo.gizmoSize;

    // Draw gizmo axes with thicker lines for active axis
    Color xColor = (g_editor->gizmo.activeAxis == 0) ? ORANGE : RED;
    Color yColor = (g_editor->gizmo.activeAxis == 1) ? ORANGE : GREEN;
    Color zColor = (g_editor->gizmo.activeAxis == 2) ? ORANGE : BLUE;

    // Draw move gizmo arrows
    DrawLine3D(pos, Vector3Add(pos, (Vector3){size, 0, 0}), xColor);     // X axis
    DrawLine3D(pos, Vector3Add(pos, (Vector3){0, size, 0}), yColor);     // Y axis
    DrawLine3D(pos, Vector3Add(pos, (Vector3){0, 0, size}), zColor);     // Z axis

    // Draw arrow heads
    Vector3 xEnd = Vector3Add(pos, (Vector3){size, 0, 0});
    Vector3 yEnd = Vector3Add(pos, (Vector3){0, size, 0});
    Vector3 zEnd = Vector3Add(pos, (Vector3){0, 0, size});

    DrawSphere(xEnd, 0.05f, xColor);  // X arrow head
    DrawSphere(yEnd, 0.05f, yColor);  // Y arrow head
    DrawSphere(zEnd, 0.05f, zColor);  // Z arrow head

    // Draw center sphere
    DrawSphere(pos, 0.08f, WHITE);

    // Draw axis labels
    Vector2 screenPos = GetWorldToScreen(Vector3Add(pos, (Vector3){size + 0.2f, 0, 0}), g_editor->camera.camera);
    DrawText("X", (int)screenPos.x, (int)screenPos.y, 14, xColor);

    screenPos = GetWorldToScreen(Vector3Add(pos, (Vector3){0, size + 0.2f, 0}), g_editor->camera.camera);
    DrawText("Y", (int)screenPos.x, (int)screenPos.y, 14, yColor);

    screenPos = GetWorldToScreen(Vector3Add(pos, (Vector3){0, 0, size + 0.2f}), g_editor->camera.camera);
    DrawText("Z", (int)screenPos.x, (int)screenPos.y, 14, zColor);
}

void SetGizmoType(GizmoType type) {
    if (!g_editor) return;
    g_editor->gizmo.type = type;
}

// Basic property panel implementation
void UpdatePropertyPanel(void) {
    if (!g_editor || !g_editor->showPropertyPanel) return;
    // Basic implementation - will be expanded in later phases
}

void DrawPropertyPanel(void) {
    if (!g_editor || !g_editor->showPropertyPanel) return;

    Rectangle panel = g_editor->properties.bounds;
    DrawRectangleRec(panel, Fade(DARKGRAY, 0.9f));
    DrawRectangleLinesEx(panel, 1, WHITE);
    DrawText("Properties", (int)panel.x + 10, (int)panel.y + 10, 16, WHITE);

    int yOffset = 40;

    if (g_editor->selectionCount > 0) {
        Selection* selection = &g_editor->selections[0];
        Vector3 center = Vector3Scale(Vector3Add(selection->bounds.min, selection->bounds.max), 0.5f);

        DrawText(TextFormat("Selected: %s",
                 selection->type == SELECT_CARD ? "Card" :
                 selection->type == SELECT_PLAYER ? "Player" : "Object"),
                 (int)panel.x + 10, (int)panel.y + yOffset, 12, WHITE);
        yOffset += 25;

        DrawText("Transform:", (int)panel.x + 10, (int)panel.y + yOffset, 12, YELLOW);
        yOffset += 20;

        // Position sliders (simplified - real sliders would be more complex)
        DrawText(TextFormat("X: %.2f", center.x), (int)panel.x + 20, (int)panel.y + yOffset, 10, WHITE);
        DrawRectangle((int)panel.x + 80, (int)panel.y + yOffset + 2, 150, 6, DARKGRAY);
        DrawRectangle((int)panel.x + 80 + (int)((center.x + 10) * 7.5f), (int)panel.y + yOffset + 2, 6, 6, RED);
        yOffset += 20;

        DrawText(TextFormat("Y: %.2f", center.y), (int)panel.x + 20, (int)panel.y + yOffset, 10, WHITE);
        DrawRectangle((int)panel.x + 80, (int)panel.y + yOffset + 2, 150, 6, DARKGRAY);
        DrawRectangle((int)panel.x + 80 + (int)((center.y + 10) * 7.5f), (int)panel.y + yOffset + 2, 6, 6, GREEN);
        yOffset += 20;

        DrawText(TextFormat("Z: %.2f", center.z), (int)panel.x + 20, (int)panel.y + yOffset, 10, WHITE);
        DrawRectangle((int)panel.x + 80, (int)panel.y + yOffset + 2, 150, 6, DARKGRAY);
        DrawRectangle((int)panel.x + 80 + (int)((center.z + 10) * 7.5f), (int)panel.y + yOffset + 2, 6, 6, BLUE);
        yOffset += 30;

        // Editor settings
        DrawText("Editor Settings:", (int)panel.x + 10, (int)panel.y + yOffset, 12, YELLOW);
        yOffset += 20;

        DrawText(TextFormat("Grid Size: %.1f", g_editor->debug.gridSize), (int)panel.x + 20, (int)panel.y + yOffset, 10, WHITE);
        yOffset += 15;

        DrawText(TextFormat("Gizmo Size: %.1f", g_editor->gizmo.gizmoSize), (int)panel.x + 20, (int)panel.y + yOffset, 10, WHITE);
        yOffset += 15;

        DrawText(TextFormat("Snap: %s", g_editor->snapToGrid ? "ON" : "OFF"), (int)panel.x + 20, (int)panel.y + yOffset, 10, WHITE);
        yOffset += 20;

        // Camera info
        DrawText("Camera:", (int)panel.x + 10, (int)panel.y + yOffset, 12, YELLOW);
        yOffset += 20;

        Vector3 camPos = g_editor->camera.camera.position;
        DrawText(TextFormat("Pos: %.1f,%.1f,%.1f", camPos.x, camPos.y, camPos.z), (int)panel.x + 20, (int)panel.y + yOffset, 9, WHITE);
        yOffset += 15;

        DrawText(TextFormat("Dist: %.1f", g_editor->camera.distance), (int)panel.x + 20, (int)panel.y + yOffset, 9, WHITE);

    } else {
        DrawText("No selection", (int)panel.x + 10, (int)panel.y + yOffset, 12, GRAY);
        yOffset += 25;

        DrawText("Press F12 to toggle editor", (int)panel.x + 10, (int)panel.y + yOffset, 10, WHITE);
        yOffset += 15;
        DrawText("Click objects to select", (int)panel.x + 10, (int)panel.y + yOffset, 10, WHITE);
        yOffset += 15;
        DrawText("Drag gizmo axes to move", (int)panel.x + 10, (int)panel.y + yOffset, 10, WHITE);
    }
}

// Configuration functions
void SaveConfiguration(const char* filename) {
    if (!g_editor) return;

    EditorConfig config;
    GetConfigFromEditor(&config);
    SaveEditorConfig(filename, &config);
}

void LoadConfiguration(const char* filename) {
    if (!g_editor) return;

    EditorConfig config;
    if (LoadEditorConfig(filename, &config)) {
        ApplyConfigToEditor(&config);
    }
}

// Debug renderer
void UpdateDebugRenderer(void) {
    if (!g_editor) return;
    // Basic implementation
}

void DrawDebugRenderer(void) {
    if (!g_editor) return;

    // Draw performance overlay if enabled
    if (g_editor->showPerformanceOverlay) {
        DrawText(TextFormat("FPS: %d", GetFPS()), 10, 10, 20, WHITE);
        DrawText(TextFormat("Frame Time: %.2f ms", g_editor->avgFrameTime * 1000), 10, 35, 16, WHITE);
        DrawText(TextFormat("Selections: %d", g_editor->selectionCount), 10, 55, 16, WHITE);
    }
}

void DrawEditorGrid(void) {
    if (!g_editor) return;

    float size = g_editor->debug.gridSize;
    int lines = 20;
    float extent = lines * size;

    for (int i = -lines; i <= lines; i++) {
        float pos = i * size;
        DrawLine3D((Vector3){pos, 0, -extent}, (Vector3){pos, 0, extent}, g_editor->debug.gridColor);
        DrawLine3D((Vector3){-extent, 0, pos}, (Vector3){extent, 0, pos}, g_editor->debug.gridColor);
    }
}

// UI and input handling
void DrawEditorUI(void) {
    if (!g_editor) return;

    // Only draw UI if in editor mode
    if (g_editor->mode == EDITOR_MODE_GAME) return;

    // Draw debug visualizations (2D overlays)
    DrawDebugRenderer();

    // Draw property panel
    if (g_editor->showPropertyPanel) {
        DrawPropertyPanel();
    }

    // Draw object browser
    if (g_editor->showObjectBrowser) {
        Rectangle panel = g_editor->browser.bounds;
        DrawRectangleRec(panel, Fade(DARKGRAY, 0.9f));
        DrawRectangleLinesEx(panel, 1, WHITE);
        DrawText("Object Browser", (int)panel.x + 10, (int)panel.y + 10, 16, WHITE);
        DrawText("Cards: 1", (int)panel.x + 10, (int)panel.y + 40, 12, WHITE);
        DrawText("Players: 2", (int)panel.x + 10, (int)panel.y + 60, 12, WHITE);
        DrawText("Click objects to select", (int)panel.x + 10, (int)panel.y + 80, 10, GRAY);
    }

    // Draw mode indicator
    const char* modeText = "GAME";
    if (g_editor->mode == EDITOR_MODE_EDITOR) modeText = "EDITOR";
    else if (g_editor->mode == EDITOR_MODE_HYBRID) modeText = "HYBRID";

    DrawText(TextFormat("Mode: %s", modeText), GetScreenWidth() - 150, 10, 16, YELLOW);

    // Draw help text
    if (g_editor->mode != EDITOR_MODE_GAME) {
        DrawText("F12: Toggle Editor", GetScreenWidth() - 150, 30, 12, WHITE);
        DrawText("Tab: Property Panel", GetScreenWidth() - 150, 50, 12, WHITE);
        DrawText("G: Toggle Snap", GetScreenWidth() - 150, 70, 12, WHITE);
        DrawText("WASD: Move Camera", GetScreenWidth() - 150, 90, 12, WHITE);
        DrawText("Right-click: Rotate", GetScreenWidth() - 150, 110, 12, WHITE);
    }

    // Show gizmo drag status (2D overlay)
    if (g_editor->gizmo.isDragging) {
        const char* axisName = (g_editor->gizmo.activeAxis == 0) ? "X" :
                              (g_editor->gizmo.activeAxis == 1) ? "Y" : "Z";
        DrawText(TextFormat("Dragging %s axis", axisName), 10, GetScreenHeight() - 30, 16, WHITE);
    }
}

void HandleEditorInput(void) {
    if (!g_editor) return;

    // Update modifier keys
    g_editor->ctrlPressed = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    g_editor->shiftPressed = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    g_editor->altPressed = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);

    // F12 to toggle editor mode
    if (IsKeyPressed(KEY_F12)) {
        ToggleEditorMode();
    }

    // Only handle editor-specific input if in editor mode
    if (!IsEditorMode()) return;

    // F1 for help
    if (IsKeyPressed(KEY_F1)) {
        g_editor->showHelp = !g_editor->showHelp;
    }

    // F5 to save
    if (IsKeyPressed(KEY_F5)) {
        SaveConfiguration(g_editor->config.currentConfigFile);
    }

    // Tab to toggle property panel
    if (IsKeyPressed(KEY_TAB)) {
        g_editor->showPropertyPanel = !g_editor->showPropertyPanel;
    }

    // G to toggle grid snap
    if (IsKeyPressed(KEY_G)) {
        g_editor->snapToGrid = !g_editor->snapToGrid;
        printf("Grid snap: %s\n", g_editor->snapToGrid ? "ON" : "OFF");
    }

    // ESC to clear selection
    if (IsKeyPressed(KEY_ESCAPE)) {
        ClearSelection();
    }

    // Object selection with mouse
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !g_editor->isMouseOverUI) {
        Ray mouseRay = GetEditorMouseRay();
        SelectionType outType;
        void* pickedObject = PickObject(mouseRay, &outType);

        if (pickedObject) {
            if (!g_editor->ctrlPressed) {
                ClearSelection();
            }
            AddSelection(outType, pickedObject, 0);
        } else if (!g_editor->ctrlPressed) {
            ClearSelection();
        }
    }
}

bool IsMouseOverEditor(void) {
    if (!g_editor) return false;

    Vector2 mousePos = GetMousePosition();

    // Check if mouse is over any UI panel
    for (int i = 0; i < 4; i++) {
        if (g_editor->showPropertyPanel && i == 0 && CheckCollisionPointRec(mousePos, g_editor->panelBounds[0])) return true;
        if (g_editor->showObjectBrowser && i == 1 && CheckCollisionPointRec(mousePos, g_editor->panelBounds[1])) return true;
        if (g_editor->showTimeline && i == 2 && CheckCollisionPointRec(mousePos, g_editor->panelBounds[2])) return true;
        if (g_editor->showDebugPanel && i == 3 && CheckCollisionPointRec(mousePos, g_editor->panelBounds[3])) return true;
    }

    return false;
}

// Utility functions
Vector3 SnapToGrid(Vector3 position) {
    if (!g_editor || !g_editor->snapToGrid) return position;

    float snap = g_editor->snapSize;
    return (Vector3){
        roundf(position.x / snap) * snap,
        roundf(position.y / snap) * snap,
        roundf(position.z / snap) * snap
    };
}

void DrawBoundingBox(BoundingBox box, Color color) {
    Vector3 size = Vector3Subtract(box.max, box.min);
    Vector3 center = Vector3Scale(Vector3Add(box.min, box.max), 0.5f);
    DrawCubeWires(center, size.x, size.y, size.z, color);
}

void DrawWireframeCube(Vector3 position, Vector3 size, Color color) {
    DrawCubeWires(position, size.x, size.y, size.z, color);
}

// Object picking implementation
Ray GetEditorMouseRay(void) {
    if (!g_editor || !g_editor->gameState) return (Ray){0};

    Vector2 mousePos = GetMousePosition();
    return GetScreenToWorldRay(mousePos, g_editor->gameState->camera);
}

bool CheckRayCollision(Ray ray, void* object, SelectionType type) {
    if (!object) return false;

    switch (type) {
        case SELECT_CARD: {
            // For now, use a simple bounding box at origin
            // This will be improved when we integrate with actual card objects
            BoundingBox cardBox = {{-0.5f, 0.0f, -0.7f}, {0.5f, 0.1f, 0.7f}};
            RayCollision collision = GetRayCollisionBox(ray, cardBox);
            return collision.hit;
        }
        case SELECT_PLAYER: {
            // Simple player area detection
            BoundingBox playerBox = {{-1.0f, 0.0f, -1.0f}, {1.0f, 0.2f, 1.0f}};
            RayCollision collision = GetRayCollisionBox(ray, playerBox);
            return collision.hit;
        }
        default:
            return false;
    }
}

void* PickObject(Ray ray, SelectionType* outType) {
    if (!g_editor || !outType || !g_editor->gameState) return NULL;

    GameState* game = g_editor->gameState;

    // Test picking against actual game objects

    // Test cards in both players' hands and boards
    for (int p = 0; p < 2; p++) {
        Player* player = &game->players[p];

        // Test hand cards
        for (int i = 0; i < player->handCount; i++) {
            Card* card = &player->hand[i];
            // Create approximate bounding box for card based on its position
            BoundingBox cardBox = {
                {card->position.x - 0.5f, card->position.y, card->position.z - 0.7f},
                {card->position.x + 0.5f, card->position.y + 0.1f, card->position.z + 0.7f}
            };

            RayCollision collision = GetRayCollisionBox(ray, cardBox);
            if (collision.hit) {
                *outType = SELECT_CARD;
                return card;
            }
        }

        // Test board cards
        for (int i = 0; i < player->boardCount; i++) {
            Card* card = &player->board[i];
            BoundingBox cardBox = {
                {card->position.x - 0.5f, card->position.y, card->position.z - 0.7f},
                {card->position.x + 0.5f, card->position.y + 0.1f, card->position.z + 0.7f}
            };

            RayCollision collision = GetRayCollisionBox(ray, cardBox);
            if (collision.hit) {
                *outType = SELECT_CARD;
                return card;
            }
        }
    }

    // Test player areas (simplified - use approximate positions)
    for (int p = 0; p < 2; p++) {
        Player* player = &game->players[p];
        // Create an approximate player area based on typical positioning
        float zPos = (p == 0) ? 5.0f : -5.0f;  // Player 0 at +Z, Player 1 at -Z

        BoundingBox playerBox = {
            {-2.0f, 0.0f, zPos - 1.0f},
            {2.0f, 0.2f, zPos + 1.0f}
        };

        RayCollision collision = GetRayCollisionBox(ray, playerBox);
        if (collision.hit) {
            *outType = SELECT_PLAYER;
            return player;
        }
    }

    *outType = SELECT_NONE;
    return NULL;
}