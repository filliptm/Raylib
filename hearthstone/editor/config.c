#include "config.h"
#include "editor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void GetDefaultConfig(EditorConfig* config) {
    if (!config) return;

    // Camera defaults
    config->cameraPosition = (Vector3){0.0f, 10.0f, 8.0f};
    config->cameraTarget = (Vector3){0.0f, 0.0f, 0.0f};
    config->cameraDistance = 12.0f;
    config->cameraAngleX = 0.0f;
    config->cameraAngleY = -30.0f;

    // Grid defaults
    config->showGrid = true;
    config->gridSize = 1.0f;
    config->gridColor = GRAY;

    // Gizmo defaults
    config->gizmoSize = 1.0f;
    config->snapToGrid = false;
    config->snapSize = 0.5f;

    // UI defaults
    config->showPropertyPanel = false;
    config->showObjectBrowser = false;
    config->showTimeline = false;
    config->showPerformanceOverlay = false;

    // Auto-save defaults
    config->autoSave = true;
    config->autoSaveInterval = 30.0f;
}

void SaveEditorConfig(const char* filename, EditorConfig* config) {
    if (!filename || !config) return;

    FILE* file = fopen(filename, "w");
    if (!file) {
        printf("Failed to save config to %s\n", filename);
        return;
    }

    // Write a simple text-based config format
    fprintf(file, "# Editor Configuration\n");
    fprintf(file, "camera_position %.2f %.2f %.2f\n", config->cameraPosition.x, config->cameraPosition.y, config->cameraPosition.z);
    fprintf(file, "camera_target %.2f %.2f %.2f\n", config->cameraTarget.x, config->cameraTarget.y, config->cameraTarget.z);
    fprintf(file, "camera_distance %.2f\n", config->cameraDistance);
    fprintf(file, "camera_angle_x %.2f\n", config->cameraAngleX);
    fprintf(file, "camera_angle_y %.2f\n", config->cameraAngleY);
    fprintf(file, "show_grid %d\n", config->showGrid ? 1 : 0);
    fprintf(file, "grid_size %.2f\n", config->gridSize);
    fprintf(file, "grid_color %d %d %d %d\n", config->gridColor.r, config->gridColor.g, config->gridColor.b, config->gridColor.a);
    fprintf(file, "gizmo_size %.2f\n", config->gizmoSize);
    fprintf(file, "snap_to_grid %d\n", config->snapToGrid ? 1 : 0);
    fprintf(file, "snap_size %.2f\n", config->snapSize);
    fprintf(file, "show_property_panel %d\n", config->showPropertyPanel ? 1 : 0);
    fprintf(file, "show_object_browser %d\n", config->showObjectBrowser ? 1 : 0);
    fprintf(file, "show_timeline %d\n", config->showTimeline ? 1 : 0);
    fprintf(file, "show_performance_overlay %d\n", config->showPerformanceOverlay ? 1 : 0);
    fprintf(file, "auto_save %d\n", config->autoSave ? 1 : 0);
    fprintf(file, "auto_save_interval %.2f\n", config->autoSaveInterval);

    fclose(file);
    printf("Saved editor config to %s\n", filename);
}

bool LoadEditorConfig(const char* filename, EditorConfig* config) {
    if (!filename || !config) return false;

    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Config file %s not found, using defaults\n", filename);
        GetDefaultConfig(config);
        return false;
    }

    char line[256];
    char key[64];

    while (fgets(line, sizeof(line), file)) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n') continue;

        if (sscanf(line, "%s", key) == 1) {
            if (strcmp(key, "camera_position") == 0) {
                sscanf(line, "%s %f %f %f", key, &config->cameraPosition.x, &config->cameraPosition.y, &config->cameraPosition.z);
            } else if (strcmp(key, "camera_target") == 0) {
                sscanf(line, "%s %f %f %f", key, &config->cameraTarget.x, &config->cameraTarget.y, &config->cameraTarget.z);
            } else if (strcmp(key, "camera_distance") == 0) {
                sscanf(line, "%s %f", key, &config->cameraDistance);
            } else if (strcmp(key, "camera_angle_x") == 0) {
                sscanf(line, "%s %f", key, &config->cameraAngleX);
            } else if (strcmp(key, "camera_angle_y") == 0) {
                sscanf(line, "%s %f", key, &config->cameraAngleY);
            } else if (strcmp(key, "show_grid") == 0) {
                int val;
                sscanf(line, "%s %d", key, &val);
                config->showGrid = val != 0;
            } else if (strcmp(key, "grid_size") == 0) {
                sscanf(line, "%s %f", key, &config->gridSize);
            } else if (strcmp(key, "grid_color") == 0) {
                int r, g, b, a;
                sscanf(line, "%s %d %d %d %d", key, &r, &g, &b, &a);
                config->gridColor = (Color){r, g, b, a};
            } else if (strcmp(key, "gizmo_size") == 0) {
                sscanf(line, "%s %f", key, &config->gizmoSize);
            } else if (strcmp(key, "snap_to_grid") == 0) {
                int val;
                sscanf(line, "%s %d", key, &val);
                config->snapToGrid = val != 0;
            } else if (strcmp(key, "snap_size") == 0) {
                sscanf(line, "%s %f", key, &config->snapSize);
            }
            // Add other settings as needed...
        }
    }

    fclose(file);
    printf("Loaded editor config from %s\n", filename);
    return true;
}

void ApplyConfigToEditor(EditorConfig* config) {
    if (!config || !g_editor) return;

    // Apply camera settings
    g_editor->camera.camera.position = config->cameraPosition;
    g_editor->camera.camera.target = config->cameraTarget;
    g_editor->camera.target = config->cameraTarget;
    g_editor->camera.distance = config->cameraDistance;
    g_editor->camera.angleX = config->cameraAngleX;
    g_editor->camera.angleY = config->cameraAngleY;

    // Apply grid settings
    g_editor->debug.showGrid = config->showGrid;
    g_editor->debug.gridSize = config->gridSize;
    g_editor->debug.gridColor = config->gridColor;

    // Apply gizmo settings
    g_editor->gizmo.gizmoSize = config->gizmoSize;
    g_editor->snapToGrid = config->snapToGrid;
    g_editor->snapSize = config->snapSize;

    // Apply UI settings
    g_editor->showPropertyPanel = config->showPropertyPanel;
    g_editor->showObjectBrowser = config->showObjectBrowser;
    g_editor->showTimeline = config->showTimeline;
    g_editor->showPerformanceOverlay = config->showPerformanceOverlay;

    // Apply auto-save settings
    g_editor->config.autoSave = config->autoSave;
    g_editor->config.autoSaveInterval = config->autoSaveInterval;

    printf("Applied configuration to editor\n");
}

void GetConfigFromEditor(EditorConfig* config) {
    if (!config || !g_editor) return;

    // Get camera settings
    config->cameraPosition = g_editor->camera.camera.position;
    config->cameraTarget = g_editor->camera.camera.target;
    config->cameraDistance = g_editor->camera.distance;
    config->cameraAngleX = g_editor->camera.angleX;
    config->cameraAngleY = g_editor->camera.angleY;

    // Get grid settings
    config->showGrid = g_editor->debug.showGrid;
    config->gridSize = g_editor->debug.gridSize;
    config->gridColor = g_editor->debug.gridColor;

    // Get gizmo settings
    config->gizmoSize = g_editor->gizmo.gizmoSize;
    config->snapToGrid = g_editor->snapToGrid;
    config->snapSize = g_editor->snapSize;

    // Get UI settings
    config->showPropertyPanel = g_editor->showPropertyPanel;
    config->showObjectBrowser = g_editor->showObjectBrowser;
    config->showTimeline = g_editor->showTimeline;
    config->showPerformanceOverlay = g_editor->showPerformanceOverlay;

    // Get auto-save settings
    config->autoSave = g_editor->config.autoSave;
    config->autoSaveInterval = g_editor->config.autoSaveInterval;
}