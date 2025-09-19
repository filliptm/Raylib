# Hearthstone Clone - Modular Architecture

## Overview

The original monolithic `hearthstone_clone.c` (1090 lines) has been successfully broken down into a modular architecture with clear separation of concerns. This makes the codebase more maintainable, testable, and expandable.

## Module Structure

### Core Files (`src/hearthstone/`)

1. **types.h** - Core enums and constants
   - Card types, rarities, hero classes
   - Game phases, turn phases, action types
   - Effect types and constants

2. **common.h** - Forward declarations
   - Prevents circular dependencies
   - Defines shared type aliases

3. **card.h/card.c** - Card System (183 lines)
   - Card structure and properties
   - Card database and factory functions
   - Card positioning and animation
   - Card hit detection

4. **player.h/player.c** - Player Management (154 lines)
   - Player structure and stats
   - Deck and hand management
   - Mana and resource management
   - Card drawing and positioning

5. **game_state.h/game_state.c** - Core Game Logic (260 lines)
   - Main game state structure
   - Game initialization and cleanup
   - Turn management system
   - Win condition checking
   - Action queue processing
   - Camera management

6. **combat.h/combat.c** - Combat System (283 lines)
   - Attack resolution
   - Damage calculation
   - Keyword mechanics (taunt, divine shield, etc.)
   - Spell casting
   - Card death handling

7. **effects.h/effects.c** - Visual Effects (103 lines)
   - Visual effect management
   - Battlecry and deathrattle execution
   - Effect timing and rendering
   - Effect color and duration handling

8. **render.h/render.c** - Rendering System (326 lines)
   - 3D card rendering
   - UI and HUD rendering
   - Effect visualization
   - Game board rendering

9. **input.h/input.c** - Input Handling (368 lines)
   - Mouse and keyboard input
   - Card selection and interaction
   - Drag and drop functionality
   - Targeting system

10. **main.c** - Main Entry Point (30 lines)
    - Window initialization
    - Game loop coordination
    - Module integration

## Benefits of Modular Architecture

### 1. **Maintainability**
- Each module has a single responsibility
- Changes to one system don't affect others
- Easier to locate and fix bugs

### 2. **Testability**
- Individual modules can be unit tested
- Mock implementations can be created
- Isolated testing of specific features

### 3. **Expandability**
- New card types can be added to card.c
- New effects can be added to effects.c
- New input methods can be added to input.c
- AI can be added as a separate module

### 4. **Parallel Development**
- Multiple developers can work on different modules
- Clear interfaces prevent conflicts
- Reduced merge conflicts

### 5. **Code Reusability**
- Modules can be reused in other projects
- Common patterns are abstracted
- Clean API boundaries

## Build System

### Updated Makefile
- Supports both monolithic and modular builds
- Automatic dependency tracking
- Object file management
- Platform-specific linking

### Build Commands
```bash
# Build modular version (default)
make run

# Build specific version
make hearthstone-mod
make run-hearthstone-mod

# Build original monolithic version
make hearthstone
make run-hearthstone

# Clean all builds
make clean
```

## Module Dependencies

```
main.c
├── game_state.h
│   ├── player.h
│   │   └── card.h
│   └── effects.h
├── render.h
└── input.h
    ├── combat.h
    └── effects.h
```

## API Design Principles

1. **Clear Interfaces** - Each module exposes only necessary functions
2. **Minimal Dependencies** - Modules depend on interfaces, not implementations
3. **Forward Declarations** - Prevent circular dependencies
4. **Consistent Naming** - Function names clearly indicate their module
5. **Error Handling** - Functions validate input and handle edge cases

## Future Expansion Ideas

With this modular architecture, you can easily add:

1. **AI Module** (`ai.h/ai.c`)
   - Computer opponent logic
   - Difficulty levels
   - Decision trees

2. **Network Module** (`network.h/network.c`)
   - Multiplayer support
   - Client-server architecture
   - Synchronization

3. **Audio Module** (`audio.h/audio.c`)
   - Sound effects
   - Background music
   - Voice lines

4. **Config Module** (`config.h/config.c`)
   - Settings management
   - Key bindings
   - Graphics options

5. **Save/Load Module** (`save.h/save.c`)
   - Game state persistence
   - Deck saving
   - Progress tracking

6. **Card Editor Module** (`editor.h/editor.c`)
   - Custom card creation
   - Deck building tools
   - Asset management

## Testing Strategy

Each module can now be tested independently:

```c
// Example: Test card creation
void test_card_creation() {
    Card card = CreateCard(1, "Test Card", 2, CARD_TYPE_MINION, 2, 3);
    assert(card.cost == 2);
    assert(card.attack == 2);
    assert(card.health == 3);
}

// Example: Test combat
void test_combat_damage() {
    // Create mock game state
    // Test damage calculation
    // Verify health changes
}
```

## Performance Considerations

- **Compile Time**: Slightly increased due to multiple files
- **Runtime**: No performance impact - same compiled code
- **Memory**: Identical memory usage
- **Loading**: Faster incremental builds during development

The modular architecture provides a solid foundation for expanding the Hearthstone clone into a full-featured card game while maintaining clean, maintainable code.