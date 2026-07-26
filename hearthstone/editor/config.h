#ifndef EDITOR_CONFIG_H
#define EDITOR_CONFIG_H

#include "raylib.h"

// Simple configuration structure
typedef struct {
    // Camera settings
    Vector3 cameraPosition;
    Vector3 cameraTarget;
    float cameraDistance;
    float cameraAngleX;
    float cameraAngleY;

    // Grid settings
    bool showGrid;
    float gridSize;
    Color gridColor;

    // Gizmo settings
    float gizmoSize;
    bool snapToGrid;
    float snapSize;

    // UI settings
    bool showPropertyPanel;
    bool showObjectBrowser;
    bool showTimeline;
    bool showPerformanceOverlay;

    // Auto-save settings
    bool autoSave;
    float autoSaveInterval;
} EditorConfig;

// Configuration functions
void SaveEditorConfig(const char* filename, EditorConfig* config);
bool LoadEditorConfig(const char* filename, EditorConfig* config);
void GetDefaultConfig(EditorConfig* config);
void ApplyConfigToEditor(EditorConfig* config);
void GetConfigFromEditor(EditorConfig* config);

#endif // EDITOR_CONFIG_H