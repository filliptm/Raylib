# Hearthstone Clone - 3D Editor User Manual

## 🎯 Overview

The Hearthstone Clone 3D Editor is a comprehensive visual development environment that allows real-time manipulation of game elements without recompilation. This editor transforms your development workflow from traditional code-edit-compile cycles into a modern, visual development experience.

## 🚀 Quick Start Guide

### Starting the Editor

1. **Build and run the game:**
   ```bash
   cd /path/to/Raylib
   make run
   ```

2. **Enter Editor Mode:**
   - Press `F12` to toggle between Game Mode and Editor Mode
   - The game starts in Game Mode by default

3. **You'll see:**
   - A 3D grid on the ground
   - Placeholder objects: Blue card at center, Green/Red player areas
   - "Mode: EDITOR" indicator in top-right corner
   - Performance overlay showing FPS

## 🎮 Basic Controls

### Camera Controls (Editor Mode Only)
- **Right Mouse + Drag**: Rotate camera around the scene
- **Mouse Wheel**: Zoom in/out (distance: 1-50 units)
- **WASD Keys**: Move camera target
  - `W`: Move forward
  - `S`: Move backward
  - `A`: Move left
  - `D`: Move right
  - `Q`: Move down
  - `E`: Move up

### Object Interaction
- **Left Click**: Select objects (blue card, green/red player areas)
- **Ctrl + Left Click**: Add to selection (multi-select)
- **ESC**: Clear all selections

### Transform Controls
- **Drag Red Arrow**: Move object along X-axis
- **Drag Green Arrow**: Move object along Y-axis
- **Drag Blue Arrow**: Move object along Z-axis
- **White Center Sphere**: Indicates gizmo center

## 🔧 Editor Features

### 1. Mode Switching
- **F12**: Toggle between Game/Editor modes
- **Game Mode**: Normal gameplay, editor hidden
- **Editor Mode**: Full editor interface with tools

### 2. Object Selection System
- **Visual Highlighting**: Selected objects show yellow wireframe
- **Multi-Selection**: Hold Ctrl while clicking
- **Selection Info**: Displayed in property panel

### 3. Transform Gizmos
- **Interactive 3D Arrows**: Red (X), Green (Y), Blue (Z)
- **Visual Feedback**: Active axis highlights in orange
- **Real-time Updates**: Object bounds update as you drag
- **Status Display**: Shows which axis is being dragged

### 4. Property Panel (Tab to toggle)
When objects are selected, shows:
- **Object Type**: Card, Player, etc.
- **Transform Values**: Real-time X/Y/Z position
- **Visual Sliders**: Position indicators
- **Editor Settings**: Grid size, gizmo size, snap status
- **Camera Info**: Position and distance

### 5. Grid System
- **Visual Grid**: 20x20 unit grid on ground plane
- **Grid Snapping**: Press `G` to toggle snap-to-grid
- **Customizable**: Grid size adjustable via settings

### 6. Configuration System
- **Auto-Save**: Automatically saves every 30 seconds
- **Manual Save**: Press `F5` to save immediately
- **Config File**: `editor_config.txt` in project root
- **Persistent Settings**: Camera position, grid settings, UI state

## ⌨️ Complete Keyboard Shortcuts

| Key | Action |
|-----|--------|
| `F12` | Toggle Editor Mode |
| `F1` | Show/Hide Help |
| `F5` | Save Configuration |
| `Tab` | Toggle Property Panel |
| `G` | Toggle Grid Snapping |
| `ESC` | Clear Selection |
| `WASD` | Camera Movement |
| `QE` | Camera Up/Down |
| `Ctrl` | Multi-select modifier |

## 🖱️ Mouse Controls

| Input | Action |
|-------|--------|
| Left Click | Select Object |
| Ctrl + Left Click | Add to Selection |
| Right Mouse + Drag | Rotate Camera |
| Mouse Wheel | Zoom Camera |
| Drag Gizmo Arrows | Move Objects |

## 📊 Testing the Editor

### Test Scenario 1: Basic Object Selection
1. Press `F12` to enter Editor Mode
2. Click the blue card at center - should see yellow wireframe
3. Property panel should show "Selected: Card"
4. Click a player area (green/red) - selection should change
5. Press `ESC` - selection should clear

### Test Scenario 2: Transform Gizmos
1. Select the blue card
2. Look for red/green/blue arrows at the card center
3. Drag the red arrow left/right - card should move along X-axis
4. Drag the green arrow up/down - card should move along Y-axis
5. Drag the blue arrow forward/back - card should move along Z-axis
6. Check property panel for updated position values

### Test Scenario 3: Multi-Selection
1. Click the blue card to select it
2. Hold `Ctrl` and click a player area
3. Both objects should show yellow wireframes
4. Property panel should show info for first selected object

### Test Scenario 4: Camera Controls
1. Right-click and drag - camera should orbit around scene
2. Use mouse wheel - should zoom in/out smoothly
3. Press `WASD` - camera target should move
4. Property panel should show updated camera position

### Test Scenario 5: Configuration System
1. Move some objects around
2. Change camera position
3. Press `F5` to save
4. Exit and restart the game
5. Press `F12` - objects and camera should be in saved positions

### Test Scenario 6: Grid and Snapping
1. Press `G` to enable grid snapping
2. Property panel should show "Snap: ON"
3. Move objects - they should snap to grid positions
4. Press `G` again to disable - should show "Snap: OFF"

## 🔍 Visual Indicators

### Selection States
- **Unselected**: Normal object colors
- **Selected**: Yellow wireframe overlay
- **Multi-selected**: All selected objects show yellow wireframes

### Gizmo States
- **Inactive**: Red/Green/Blue arrows
- **Active**: Orange highlighting on dragged axis
- **Dragging**: Status text shows "Dragging X/Y/Z axis"

### UI Elements
- **Mode Indicator**: Top-right shows current mode
- **Performance Info**: FPS and frame time display
- **Help Text**: Instructions when no objects selected

## 📋 Configuration File Format

The editor saves settings in `editor_config.txt`:

```
# Editor Configuration
camera_position 0.00 10.00 8.00
camera_target 0.00 0.00 0.00
camera_distance 12.00
show_grid 1
grid_size 1.00
grid_color 130 130 130 255
gizmo_size 1.00
snap_to_grid 0
snap_size 0.50
auto_save 1
auto_save_interval 30.00
```

## 🛠️ Troubleshooting

### Editor Won't Start
- Ensure game compiled successfully: `make clean && make build`
- Check console for error messages
- Verify Raylib is properly installed

### Objects Won't Select
- Make sure you're in Editor Mode (press `F12`)
- Click directly on object geometry (blue card, colored player areas)
- Try different camera angles if objects are obscured

### Gizmos Not Appearing
- Select an object first (click on blue card or player areas)
- Check that you're in Editor Mode
- Try moving camera closer to selected object

### Transform Not Working
- Click and drag on the colored arrows (not the center)
- Make sure you're dragging from the arrow endpoints
- Check console for "Started dragging X/Y/Z axis" messages

### Config Not Saving
- Check file permissions in project directory
- Verify auto-save is enabled (should see periodic save messages)
- Try manual save with `F5`

### Performance Issues
- Disable property panel with `Tab` if not needed
- Check FPS display in top-left corner
- Consider reducing grid size in config file

## 🎯 Next Steps

This editor provides a solid foundation for visual development. The implemented features include:

✅ **Core Editor Framework**
- Mode switching (F12)
- 3D camera controls
- Object selection system
- Transform gizmos with drag functionality
- Property panel with real-time feedback
- Configuration save/load system
- Grid visualization and snapping

✅ **Professional Features**
- Multi-selection capability
- Visual feedback systems
- Auto-save functionality
- Performance monitoring
- Comprehensive keyboard shortcuts

The editor is ready for use and can be extended with additional features as needed for your specific game development requirements.

## 📞 Support

For issues or feature requests:
- Check console output for error messages
- Verify all build dependencies are installed
- Review this manual for proper usage instructions
- Test with the provided scenarios to isolate issues

---

🎮 **Happy Editing!** You now have a professional-grade 3D editor for your Hearthstone clone that will dramatically speed up your development workflow.