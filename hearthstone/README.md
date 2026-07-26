# Hearthstone Clone - Modular Implementation

A complete Hearthstone-inspired card game built with Raylib in C, featuring a clean modular architecture for easy expansion and maintenance.

## Features

🎮 **Complete Game Mechanics**
- Turn-based gameplay with proper phases
- Mana system and resource management  
- Multiple card types (minions, spells, weapons)
- Combat system with damage resolution
- Card keywords (Taunt, Charge, Divine Shield, Battlecry, Deathrattle)
- Visual effects and animations
- Win/loss conditions

🏗️ **Modular Architecture** 
- Clean separation of concerns across 8 modules
- Easy to test, maintain, and expand
- Well-documented APIs between modules
- Platform-independent build system

🎨 **3D Graphics**
- Full 3D scene with Hearthstone-style camera
- Interactive 3D cards with hover effects
- Real-time visual feedback system
- Professional game board rendering

## Quick Start

```bash
cd hearthstone
make run
```

## Prerequisites

You need Raylib installed on your system:

### macOS (Homebrew)
```bash
brew install raylib
```

### Linux (Ubuntu/Debian)
```bash
sudo apt install libraylib-dev
```

## Building

Run all commands from the `hearthstone/` directory. The binary is written to
`build/hearthstone`.

```bash
# Build and run (default)
make run

# Just build
make

# Debug build
make debug

# Run the unit tests
make test

# Clean build artifacts
make clean

# Show all options
make help
```

## Project Structure

```
hearthstone/
├── main.c                  # Main game loop
├── types.h                 # Core types and constants
├── common.h                # Forward declarations
├── card.h/card.c           # Card system
├── player.h/player.c       # Player management
├── game_state.h/.c         # Core game logic
├── combat.h/combat.c       # Combat system
├── effects.h/effects.c     # Visual effects
├── render.h/render.c       # Rendering entry point
├── input.h/input.c         # Input handling
├── ai.c, game_ai.c         # Opponent AI
├── network.c, game_network.c  # Networking layer
├── polish.c, game_polish.c, animation.c, audio.c
├── core/                   # data_manager, rules_engine, save_system, event_system
├── render/                 # board / card / ui / effect renderers
├── editor/                 # in-game board editor
├── utils/                  # logging
├── tests/                  # unit tests (make test)
├── data/                   # cards.json, decks.json, balance.json
├── docs/                   # design + architecture + editor docs
├── settings.cfg            # runtime game settings
├── editor_config.txt       # saved editor camera/layout state
├── Makefile                # Build system
└── README.md               # This file
```

## How to Play

### Basic Controls
- **Left Click**: Select a card
- **Right Click**: Play card or attack
- **Space**: End turn
- **Escape**: Cancel selection

### Game Flow
1. Both players start with 3 cards and 1 mana
2. Each turn: draw a card, gain 1 mana (max 10)
3. Play cards by selecting them and right-clicking
4. Attack with minions by selecting and right-clicking targets
5. Reduce opponent's health to 0 to win

### Card Types
- **Minions**: Creatures that can attack
- **Spells**: Instant effects (damage, healing)
- **Keywords**: 
  - **Taunt**: Must be attacked first
  - **Charge**: Can attack immediately
  - **Divine Shield**: Prevents first damage
  - **Battlecry**: Effect when played
  - **Deathrattle**: Effect when destroyed

## Architecture Benefits

### For Developers
- **Modular Design**: Each system is independent
- **Easy Testing**: Mock individual modules
- **Parallel Development**: Work on different features simultaneously
- **Clean APIs**: Well-defined interfaces between modules

### For Players
- **Expandable**: Easy to add new cards, mechanics, features
- **Stable**: Modular design reduces bugs
- **Performance**: Optimized rendering and game logic

## Expansion Ideas

The modular architecture makes it easy to add:

- **AI Opponents**: Add `ai.h/ai.c` module
- **Multiplayer**: Add `network.h/network.c` module  
- **Sound Effects**: Add `audio.h/audio.c` module
- **Custom Cards**: Extend the card database
- **Deck Building**: Add deck editor interface
- **Save/Load**: Add game state persistence
- **Card Editor**: Visual card creation tools

## Development

### Adding New Cards
1. Add card definition to `card.c` in `GetCardById()`
2. Define new keywords in `types.h` if needed
3. Implement effects in `effects.c`

### Adding New Mechanics
1. Extend structures in appropriate headers
2. Implement logic in corresponding `.c` files  
3. Add visual feedback in `effects.c`
4. Update input handling if needed

### Testing Individual Modules
Each module can be tested independently:
```c
// Example: Test card creation
Card card = CreateCard(1, "Test", 2, CARD_TYPE_MINION, 2, 3);
assert(card.cost == 2);
```

## Troubleshooting

**Build Errors**
- Ensure Raylib is properly installed
- Check compiler and library paths
- Run `make clean` before rebuilding

**Runtime Issues**
- Verify graphics drivers are up to date
- Check that window manager supports OpenGL
- Ensure proper file permissions on executable

## Contributing

1. Choose a module to work on
2. Follow existing code style and patterns
3. Add tests for new functionality
4. Update documentation as needed
5. Submit pull request with clear description

## Technical Details

- **Language**: C99
- **Graphics**: Raylib with OpenGL
- **Architecture**: Modular with clear separation
- **Platforms**: macOS, Linux, Windows
- **Build System**: Make with cross-platform support

For detailed architecture information, see [docs/MODULAR_ARCHITECTURE.md](docs/MODULAR_ARCHITECTURE.md).

For game design details, see [docs/HEARTHSTONE_DESIGN.md](docs/HEARTHSTONE_DESIGN.md).