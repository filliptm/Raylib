# Hearthstone 3D Editor Roadmap

This document outlines the development plan for building a custom 3D sandbox editor specifically for the Hearthstone clone game. The editor will allow real-time manipulation of game elements in 3D space without recompilation.

## 🎯 Project Goals

- **Eliminate recompilation cycles** for positioning adjustments
- **Visual debugging tools** for collision detection and game logic
- **Real-time property editing** for rapid iteration
- **Save/load configurations** for different layouts
- **Designer-friendly interface** for non-programmers

## 📋 Development Phases

### Phase 1: Foundation & Core Infrastructure (Week 1-2)

#### 1.1 Editor Mode Framework
- [ ] Add editor mode toggle (`F12` key)
- [ ] Implement `EditorState` structure
- [ ] Create editor/game mode switching system
- [ ] Add editor overlay rendering
- [ ] Implement basic editor camera controls

**Files to create:**
- `src/hearthstone/editor/editor.h`
- `src/hearthstone/editor/editor.c`
- `src/hearthstone/editor/editor_camera.c`

**Files to modify:**
- `main.c` - Add editor mode integration
- `input.c` - Add editor input handling
- `Makefile` - Include editor sources

#### 1.2 Object Selection System
- [ ] Implement 3D object picking with mouse ray
- [ ] Create selection highlighting system
- [ ] Add multi-selection capability
- [ ] Implement selection persistence across frames

#### 1.3 Basic Transform System
- [ ] Create transform gizmo rendering (move arrows)
- [ ] Implement drag-to-move functionality
- [ ] Add snap-to-grid system
- [ ] Basic undo/redo for transforms

### Phase 2: Game Object Integration (Week 3-4)

#### 2.1 Card Editor System
- [ ] Make cards selectable in editor mode
- [ ] Real-time card position adjustment
- [ ] Card rotation and scaling controls
- [ ] Hand position layout tools
- [ ] Board position layout tools
- [ ] Card animation path editor

**Implementation details:**
```c
typedef struct {
    bool isSelected;
    Vector3 editorOffset;
    bool isBeingDragged;
    Vector3 originalPosition;
} CardEditorData;
```

#### 2.2 Player System Editor
- [ ] Player portrait position adjustment
- [ ] Hit box visualization and editing
- [ ] Player UI element positioning
- [ ] Camera angle adjustment per player
- [ ] Health/mana display positioning

#### 2.3 Real-time Property Editing
- [ ] In-game property panel (using Raylib's built-in GUI)
- [ ] Slider controls for numeric values
- [ ] Color pickers for visual elements
- [ ] Boolean toggles for game flags
- [ ] Dropdown menus for enums

### Phase 3: Configuration System (Week 5)

#### 3.1 Save/Load Infrastructure
- [ ] Design JSON configuration schema
- [ ] Implement config serialization
- [ ] Create config loading system
- [ ] Add preset management (multiple layouts)
- [ ] Hot-reload configuration during gameplay

**Configuration Schema Example:**
```json
{
  "version": "1.0",
  "player_portraits": {
    "player_0": {"position": [7.0, 0.2, 6.0], "size": [2.0, 0.2, 2.0]},
    "player_1": {"position": [7.0, 0.2, -6.0], "size": [2.0, 0.2, 2.0]}
  },
  "card_layouts": {
    "hand_spacing": 1.5,
    "board_spacing": 2.0,
    "hover_height": 0.5
  },
  "camera_settings": {
    "default_position": [0, 10, 8],
    "default_target": [0, 0, 0]
  }
}
```

#### 3.2 Preset System
- [ ] Default layout presets (desktop, mobile, ultrawide)
- [ ] User-created preset saving
- [ ] Quick preset switching
- [ ] Preset import/export functionality

### Phase 4: Advanced Editor Features (Week 6-7)

#### 4.1 Debug Visualization
- [ ] Collision box wireframe rendering
- [ ] Hit area visualization overlays
- [ ] AI decision highlighting
- [ ] Performance profiling display
- [ ] Game state inspector

#### 4.2 Animation Editor
- [ ] Visual timeline for card animations
- [ ] Keyframe editing system
- [ ] Animation preview controls
- [ ] Path visualization for card movements
- [ ] Timing adjustment tools

#### 4.3 Advanced Transform Tools
- [ ] Rotate gizmos (XYZ rings)
- [ ] Scale gizmos (corner handles)
- [ ] Precision numeric input
- [ ] Coordinate system switching (world/local)
- [ ] Transform constraints (axis locking)

### Phase 5: UI Polish & User Experience (Week 8)

#### 5.1 Editor UI Enhancement
- [ ] Hierarchical object browser
- [ ] Searchable property inspector
- [ ] Tabbed interface for different tools
- [ ] Keyboard shortcuts for common actions
- [ ] Context menus for right-click operations

#### 5.2 Quality of Life Features
- [ ] Auto-save functionality
- [ ] Recent files menu
- [ ] Editor preferences system
- [ ] Help tooltips and documentation
- [ ] Error handling and validation

### Phase 6: Integration & Testing (Week 9-10)

#### 6.1 Game Integration
- [ ] Seamless editor/game mode transitions
- [ ] Live gameplay testing while editing
- [ ] Configuration validation
- [ ] Backward compatibility with old configs
- [ ] Performance optimization

#### 6.2 Documentation & Examples
- [ ] User manual for editor features
- [ ] Video tutorials for common workflows
- [ ] Example configuration files
- [ ] Troubleshooting guide
- [ ] Developer documentation for extensions

## 🛠️ Technical Architecture

### Core Components

```c
// Main editor state
typedef struct {
    EditorMode mode;                    // GAME, EDITOR, HYBRID
    EditorCamera camera;                // 3D editor camera
    SelectionManager selection;         // Object selection system
    TransformGizmo gizmos;             // Move/rotate/scale tools
    PropertyPanel properties;          // Real-time editing panel
    ConfigManager config;              // Save/load system
    DebugRenderer debug;               // Visualization tools

    // Integration
    GameState* gameState;              // Live game reference
    bool livePreview;                  // Real-time updates
    float deltaTime;                   // Editor timing
} EditorState;

// Global editor access
extern EditorState* g_editor;
```

### File Structure
```
src/hearthstone/editor/
├── editor.h                 # Main editor interface
├── editor.c                 # Core editor logic
├── editor_camera.h/.c       # 3D camera controls
├── selection.h/.c           # Object selection system
├── transform.h/.c           # Gizmo and transform tools
├── properties.h/.c          # Property editing panel
├── config.h/.c              # Configuration system
├── debug_render.h/.c        # Debug visualization
└── ui/
    ├── panels.h/.c          # UI panel management
    ├── gizmos.h/.c          # 3D gizmos rendering
    └── widgets.h/.c         # Custom UI widgets
```

## 🎮 Usage Workflow

### Basic Editing Session
1. **Launch game** normally
2. **Press F12** to enter editor mode
3. **Click objects** to select them
4. **Drag gizmos** to move objects
5. **Adjust properties** in the panel
6. **Press F5** to save configuration
7. **Press F12** to return to game
8. **Test changes** immediately

### Advanced Features
- **Multi-select**: Hold Ctrl and click multiple objects
- **Precision mode**: Hold Shift for fine adjustments
- **Snap to grid**: Press G to toggle grid snapping
- **Reset transform**: Press R to reset selected object
- **Copy/paste**: Ctrl+C/Ctrl+V for position copying

## ✅ Success Metrics

- [ ] **Development Speed**: 10x faster iteration on positioning
- [ ] **Visual Quality**: Perfect alignment and spacing
- [ ] **Accessibility**: Non-programmers can adjust layouts
- [ ] **Debugging**: Visual tools eliminate guesswork
- [ ] **Flexibility**: Support for multiple screen sizes/layouts

## 🔧 Technical Requirements

### Dependencies
- **Raylib**: Already included (3D rendering, input, GUI)
- **cJSON**: For configuration file parsing
- **Math utilities**: Vector operations and transformations

### Performance Considerations
- Editor should maintain 60+ FPS
- Minimal impact on game performance when disabled
- Efficient object picking algorithms
- Optional LOD system for complex scenes

## 📝 Implementation Notes

### Phase 1 Priority
Start with the **Foundation** phase as it establishes the core architecture. Focus on getting basic object selection and movement working before adding advanced features.

### Integration Strategy
Use a **hybrid approach** where editor mode overlays on the live game, allowing real-time testing of changes without switching contexts.

### Testing Strategy
- **Unit tests** for configuration system
- **Integration tests** for editor/game mode switching
- **User acceptance testing** with actual layout creation workflows

---

**Estimated Total Development Time**: 8-10 weeks
**Minimum Viable Product**: Phase 1-3 (5 weeks)
**Full Feature Set**: All phases (10 weeks)

This roadmap transforms your Hearthstone clone from a traditional code-edit-compile workflow into a modern, visual development environment optimized for your specific game's needs.