# Editor Troubleshooting Guide

## 🔧 Major Bug Fixes Applied

I've identified and fixed the critical bugs that were preventing the editor from working:

### 1. **Camera Issue (FIXED)**
- **Problem**: Editor camera was never used for 3D rendering
- **Fix**: Modified main.c to use editor camera with `BeginMode3D(g_editor->camera.camera)` when in editor mode

### 2. **Rendering Order (FIXED)**
- **Problem**: 3D and 2D elements were mixed incorrectly
- **Fix**: Separated 3D rendering (inside BeginMode3D) from 2D UI (outside)

### 3. **Input Conflicts (FIXED)**
- **Problem**: Game input was interfering with editor input
- **Fix**: Game input is now disabled when in editor mode

## 🎮 How to Test the Fixed Editor

### Step 1: Launch and Enter Editor Mode
```bash
cd /Users/fillipisgro/Desktop/Git\ Projects/Raylib
./hearthstone
```

1. **Press F12** to enter Editor Mode
2. **You should now see:**
   - A 3D grid on the ground
   - Blue card object at center (0,0,0)
   - Green player area at (0,0,5)
   - Red player area at (0,0,-5)
   - Property panel on the left side
   - "Mode: EDITOR" in top-right corner

### Step 2: Test Camera Controls
- **Right-click + drag**: Should rotate camera around the scene
- **Mouse wheel**: Should zoom in/out
- **WASD keys**: Should move camera target
- **Property panel** should show updated camera position

### Step 3: Test Object Selection
- **Left-click on blue card**: Should show yellow wireframe around it
- **Left-click on green area**: Should select it (wireframe changes)
- **Left-click on red area**: Should select it
- **Property panel** should show "Selected: Card/Player" and position info

### Step 4: Test Transform Gizmos
1. Select an object (click on blue card)
2. **Look for colored arrows**: Red (X), Green (Y), Blue (Z)
3. **Click and drag red arrow**: Should move object left/right
4. **Click and drag green arrow**: Should move object up/down
5. **Click and drag blue arrow**: Should move object forward/back
6. **Property panel** should show updated position values

### Step 5: Test Additional Features
- **Press G**: Toggle grid snapping (check property panel for "Snap: ON/OFF")
- **Press Tab**: Toggle property panel visibility
- **Press F5**: Save configuration
- **ESC**: Clear selection

## 📊 Visual Indicators to Look For

### ✅ Working Correctly:
- 3D grid visible on ground
- Colored objects (blue card, green/red areas) render properly
- Camera moves smoothly with right-click + drag
- Yellow wireframes appear when objects are selected
- Red/Green/Blue gizmo arrows appear on selected objects
- Property panel shows real-time position updates
- "Mode: EDITOR" shows in top-right

### ❌ Still Not Working? Check:
1. **Objects not visible**: Camera might be too far - use mouse wheel to zoom in
2. **Can't select objects**: Make sure you're clicking directly on the colored cubes
3. **Gizmos not appearing**: First select an object, then look for the arrows
4. **Camera not moving**: Make sure you're right-clicking (not left-clicking) and dragging

## 🐛 Debug Information

The editor now prints useful debug info to console:
- "Editor initialized successfully"
- "Editor mode changed from X to Y"
- "Selected card/player object (id X)"
- "Started dragging X/Y/Z axis"
- "Finished dragging"

## 🔄 If Still Having Issues

1. **Restart the application** completely
2. **Check console output** for any error messages
3. **Try different camera angles** - some objects might be behind you
4. **Delete editor_config.txt** to reset all settings to defaults

The major bugs have been fixed - the editor should now be fully functional!