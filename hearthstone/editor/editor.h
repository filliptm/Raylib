#ifndef EDITOR_H
#define EDITOR_H

#include "raylib.h"
#include "raymath.h"
#include "../common.h"
#include "../types.h"

// Forward declarations - only declare if not already defined
#ifndef GAMESTATE_TYPEDEF
#define GAMESTATE_TYPEDEF
typedef struct GameState GameState;
#endif

#ifndef CARD_TYPEDEF
#define CARD_TYPEDEF
typedef struct Card Card;
#endif

#ifndef PLAYER_TYPEDEF
#define PLAYER_TYPEDEF
typedef struct Player Player;
#endif

// Editor modes
typedef enum {
    EDITOR_MODE_GAME,       // Normal gameplay
    EDITOR_MODE_EDITOR,     // Full editor mode
    EDITOR_MODE_HYBRID      // Editor overlay on game
} EditorMode;

// Transform gizmo types
typedef enum {
    GIZMO_MOVE,
    GIZMO_ROTATE,
    GIZMO_SCALE
} GizmoType;

// Coordinate systems
typedef enum {
    COORD_WORLD,
    COORD_LOCAL
} CoordinateSystem;

// Selection types
typedef enum {
    SELECT_NONE,
    SELECT_CARD,
    SELECT_PLAYER,
    SELECT_UI_ELEMENT,
    SELECT_CAMERA
} SelectionType;

// Editor camera structure
typedef struct {
    Camera3D camera;
    Vector3 target;
    float distance;
    float angleX;
    float angleY;
    bool isControlled;
    float speed;
    float sensitivity;
} EditorCamera;

// Transform data for undo/redo (renamed to avoid conflict with raylib)
typedef struct {
    Vector3 position;
    Vector3 rotation;
    Vector3 scale;
    int timestamp;
} EditorTransform;

// Selection data
typedef struct {
    SelectionType type;
    void* object;
    int id;
    bool isSelected;
    EditorTransform originalTransform;
    EditorTransform currentTransform;
    BoundingBox bounds;
} Selection;

// Property editing data
typedef struct {
    char name[64];
    float value;
    float min;
    float max;
    bool isFloat;
    bool isBool;
    bool isColor;
    Color colorValue;
    bool boolValue;
} Property;

// Property panel
typedef struct {
    Property properties[32];
    int propertyCount;
    Rectangle bounds;
    bool isVisible;
    int selectedProperty;
} PropertyPanel;

// Gizmo state
typedef struct {
    GizmoType type;
    Vector3 position;
    bool isActive;
    bool isDragging;
    int activeAxis; // 0=X, 1=Y, 2=Z, -1=none
    Vector2 dragStart;
    Vector3 objectStart;
    float gizmoSize;
    CoordinateSystem coordSystem;
} TransformGizmo;

// Debug visualization
typedef struct {
    bool showCollisionBoxes;
    bool showHitAreas;
    bool showAIDecisions;
    bool showPerformanceStats;
    bool showGameState;
    bool showGrid;
    bool showWireframes;
    float gridSize;
    Color gridColor;
} DebugRenderer;

// Configuration preset
typedef struct {
    char name[64];
    char filename[256];
    bool isBuiltIn;
    bool isActive;
} ConfigPreset;

// Configuration manager
typedef struct {
    ConfigPreset presets[16];
    int presetCount;
    int activePreset;
    bool autoSave;
    float autoSaveInterval;
    float lastSaveTime;
    char currentConfigFile[256];
} ConfigManager;

// Animation timeline
typedef struct {
    float currentTime;
    float totalTime;
    bool isPlaying;
    bool isLooping;
    int keyframeCount;
    Vector3 keyframes[64];
    float keyframeTimes[64];
    bool showTimeline;
    Rectangle timelineRect;
} AnimationTimeline;

// Object browser
typedef struct {
    char objectNames[64][64];
    SelectionType objectTypes[64];
    void* objects[64];
    int objectCount;
    int selectedObject;
    bool isVisible;
    Rectangle bounds;
    char searchFilter[64];
} ObjectBrowser;

// Main editor state
typedef struct {
    EditorMode mode;
    bool isInitialized;

    // Core systems
    EditorCamera camera;
    Selection selections[16];
    int selectionCount;
    TransformGizmo gizmo;
    PropertyPanel properties;
    DebugRenderer debug;
    ConfigManager config;
    AnimationTimeline timeline;
    ObjectBrowser browser;

    // Integration
    GameState* gameState;
    bool livePreview;
    float deltaTime;

    // UI state
    bool showPropertyPanel;
    bool showObjectBrowser;
    bool showTimeline;
    bool showDebugPanel;
    Rectangle panelBounds[4]; // Property, Browser, Timeline, Debug

    // Input state
    Vector2 lastMousePos;
    bool isMouseOverUI;
    bool snapToGrid;
    float snapSize;

    // Keyboard shortcuts
    bool ctrlPressed;
    bool shiftPressed;
    bool altPressed;

    // Performance
    int frameCount;
    float avgFrameTime;
    bool showPerformanceOverlay;

    // Help and documentation
    bool showHelp;
    bool showTooltips;
    char lastTooltip[256];
} EditorState;

// Global editor instance
extern EditorState* g_editor;

// Core editor functions
void InitEditor(GameState* gameState);
void UpdateEditor(GameState* gameState);
void DrawEditor(GameState* gameState);
void CleanupEditor(void);

// Mode management
void SetEditorMode(EditorMode mode);
EditorMode GetEditorMode(void);
bool IsEditorMode(void);
void ToggleEditorMode(void);

// Camera functions
void InitEditorCamera(void);
void UpdateEditorCamera(void);
void ResetEditorCamera(void);
void SetEditorCameraTarget(Vector3 target);

// Selection functions
void ClearSelection(void);
void AddSelection(SelectionType type, void* object, int id);
void RemoveSelection(int index);
bool IsSelected(void* object);
Selection* GetSelection(int index);
int GetSelectionCount(void);

// Transform functions
void UpdateGizmo(void);
void DrawGizmo(void);
void SetGizmoType(GizmoType type);
void ApplyTransform(Selection* selection, Vector3 delta);
void StartTransform(void);
void EndTransform(void);

// Property editing
void UpdatePropertyPanel(void);
void DrawPropertyPanel(void);
void SetProperty(const char* name, float value);
void SetPropertyBool(const char* name, bool value);
void SetPropertyColor(const char* name, Color value);

// Configuration
void SaveConfiguration(const char* filename);
void LoadConfiguration(const char* filename);
void CreatePreset(const char* name);
void LoadPreset(int index);
void ExportPreset(const char* filename);
bool ImportPreset(const char* filename);

// Debug visualization
void UpdateDebugRenderer(void);
void DrawDebugRenderer(void);
void ToggleDebugOption(const char* option);

// UI helpers
void DrawEditorUI(void);
void HandleEditorInput(void);
bool IsMouseOverEditor(void);
void ShowTooltip(const char* text);

// Object picking
Ray GetEditorMouseRay(void);
bool CheckRayCollision(Ray ray, void* object, SelectionType type);
void* PickObject(Ray ray, SelectionType* outType);

// Utility functions
Vector3 SnapToGrid(Vector3 position);
void DrawEditorGrid(void);
void DrawBoundingBox(BoundingBox box, Color color);
void DrawWireframeCube(Vector3 position, Vector3 size, Color color);

#endif // EDITOR_H