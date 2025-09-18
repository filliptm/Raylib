# Raylib Projects

This repository contains Raylib projects demonstrating various capabilities from basic graphics to 3D game development.

## Projects

### 1. Hello World (`hello_world.c`)
A simple introduction to Raylib:
- Creates an 800x450 pixel window
- Displays "Hello, Raylib World!" text
- Draws colorful shapes (circle, rectangle, triangle)
- Runs at 60 FPS
- Exits when you press ESC or close the window

### 2. 3D Card Game MVP (`card_game_3d.c`)
A Hearthstone-inspired 3D card game prototype:
- **3D Environment**: Full 3D scene with top-down camera (like Hearthstone)
- **Interactive Cards**: Click to select, drag to move cards around the board
- **Visual Effects**: Cards lift when hovered, glow when selected
- **Game Areas**: Distinct zones for player hand, enemy area, and battle board
- **Card Information**: Display cost, attack, health stats
- **3D Depth**: Leverage 3D space while maintaining familiar card game perspective

### 3. Hearthstone Clone (`hearthstone_clone.c`) 🎮
A complete Hearthstone clone with full game mechanics:
- **Complete Game Loop**: Turn-based gameplay with proper phases
- **Card Types**: Minions, Spells, with proper stats and abilities
- **Combat System**: Full damage resolution, minion vs minion combat
- **Mana System**: Proper mana curves, fatigue damage
- **Visual Effects**: Real-time damage numbers, battle effects
- **Keywords**: Taunt, Charge, Divine Shield, Battlecry, Deathrattle
- **Game State**: Hand management, board control, win conditions
- **Interactive 3D**: Full 3D cards with Hearthstone-style camera

## Prerequisites

You need to have Raylib installed on your system.

### Install Raylib

**macOS (using Homebrew):**
```bash
make install-raylib-mac
```

**Linux (Ubuntu/Debian):**
```bash
make install-raylib-linux
```

**Or install manually:** See the [Getting Started guide](documentation/raylib/GETTING_STARTED.md) for detailed installation instructions.

## Building and Running

### Quick Start
```bash
# Build and run the Hearthstone clone (default)
make run

# Or run specific projects
make run-hello      # Hello World
make run-card       # 3D Card Game MVP
make run-hearthstone # Full Hearthstone Clone
```

### Individual Building
```bash
# Build everything
make all

# Build specific projects
make hello          # Hello World only
make card           # 3D Card Game MVP only
make hearthstone    # Hearthstone Clone only
```

### Running
```bash
# After building, run directly
./hello_world       # Simple graphics demo
./card_game_3d      # 3D card game MVP
./hearthstone_clone # Full Hearthstone clone
```

### Clean Up
```bash
# Remove all compiled executables
make clean
```

## File Structure

```
.
├── hello_world.c        # Hello World source code
├── hello_world          # Hello World executable
├── card_game_3d.c       # 3D Card Game MVP source code  
├── card_game_3d         # 3D Card Game MVP executable
├── hearthstone_clone.c  # Full Hearthstone Clone source code
├── hearthstone_clone    # Full Hearthstone Clone executable
├── Makefile            # Build configuration for all projects
├── README.md           # This file
├── HEARTHSTONE_DESIGN.md # Complete game design document
└── documentation/      # Comprehensive Raylib documentation
    └── raylib/
        ├── README.md
        ├── GETTING_STARTED.md
        ├── GRAPHICS_2D.md
        ├── GRAPHICS_3D.md
        ├── AUDIO.md
        └── INPUT_HANDLING.md
```

## Code Explanation

### Hello World (`hello_world.c`)
Demonstrates the basic Raylib structure:
1. **Initialization:** Create window and set FPS
2. **Game Loop:** 
   - Update game state
   - Draw everything to screen
   - Repeat until user exits
3. **Cleanup:** Close window and free resources

### 3D Card Game MVP (`card_game_3d.c`)
Shows advanced 3D game development concepts:
1. **3D Scene Setup:** Top-down camera, 3D coordinate system
2. **Interactive Objects:** Card selection with mouse ray casting
3. **3D-to-2D Projection:** World positions to screen coordinates for UI
4. **Animation System:** Smooth card movement and hover effects
5. **Game State Management:** Player cards, board state, selection tracking

### Hearthstone Clone (`hearthstone_clone.c`)
A complete card game implementation demonstrating:
1. **Game Engine Architecture:** Modular systems with clean separation
2. **Turn-Based Logic:** Proper game phases and state management
3. **Card Database:** Extensible card creation and management system
4. **Combat Resolution:** Complex damage calculations and effect triggers
5. **Visual Effects System:** Real-time feedback and animation
6. **Resource Management:** Mana, health, fatigue mechanics
7. **Interactive 3D UI:** Professional game interface in 3D space

## Next Steps

- Check out the [comprehensive documentation](documentation/raylib/) for advanced features
- Try modifying the shapes, colors, or text
- Add interactive elements using input handling
- Explore 2D/3D graphics, audio, and more!

## Troubleshooting

**"raylib.h: No such file or directory"**
- Make sure Raylib is installed properly
- Check the installation commands above

**Linking errors**
- Verify Raylib libraries are in the correct path
- Try updating your system's library paths

**Permission denied**
- Make sure the executable has run permissions: `chmod +x hello_world`

For more help, see the [Getting Started guide](documentation/raylib/GETTING_STARTED.md).