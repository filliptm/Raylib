# Raylib Examples Launcher

A graphical launcher program for browsing and running the indexed raylib examples.

The collection contains 182 examples in the seven primary categories, but the current
index omits `shapes/shapes_lines_drawing.c`. The launcher therefore displays 181 entries.
See [PROJECT_OVERVIEW.md](PROJECT_OVERVIEW.md) for the code-verified inventory and known
limitations.

## Overview

The primary collection contains 182 raylib examples organized by category:

- **Core** (45 examples) - Window management, input, cameras, timing
- **Shapes** (32 examples) - 2D drawing, collisions, animations
- **Textures** (26 examples) - Image loading, sprites, particles
- **Text** (15 examples) - Fonts, text rendering, input
- **Models** (25 examples) - 3D models, meshes, animations
- **Shaders** (31 examples) - GLSL shaders, lighting, effects
- **Audio** (8 examples) - Sound, music streaming

## Quick Start

Run launcher commands from the repository root because its example paths are relative.

### 1. Run the Launcher

```bash
./example_launcher
```

The launcher will display a graphical menu with the 181 currently indexed primary
examples.

### 2. Navigate and Select

**Keyboard Controls:**
- `↑/↓` - Navigate through examples
- `PAGE UP/DOWN` - Jump multiple examples
- `HOME/END` - Go to first/last example
- `ENTER` - Compile and run selected example
- `C` - Print the selected example's source code to the terminal
- `ESC` - Exit launcher

### 3. Browse by Category

Each example is color-coded by category:
- 🔵 **Core** - Sky Blue
- 🔴 **Shapes** - Red
- 🟢 **Textures** - Green
- 🟡 **Text** - Yellow
- 🟣 **Models** - Purple
- 🟠 **Shaders** - Orange
- 🩷 **Audio** - Pink

## Compilation

### Compile the Launcher

```bash
gcc example_launcher.c -o example_launcher $(pkg-config --cflags --libs raylib) -framework OpenGL -framework Cocoa -framework IOKit
```

### Compile Individual Examples

To manually compile an example:

```bash
cd raylib-examples
gcc core/core_basic_window.c -o core_basic_window $(pkg-config --cflags --libs raylib) -framework OpenGL -framework Cocoa -framework IOKit
./core_basic_window
```

## Project Structure

```
.
├── example_launcher.c          # Main launcher program
├── example_launcher            # Compiled launcher executable
├── raylib-examples/            # 182 primary examples plus 6 others
│   ├── core/                   # Core examples
│   ├── shapes/                 # Shapes examples
│   ├── textures/               # Texture examples
│   ├── text/                   # Text examples
│   ├── models/                 # 3D model examples
│   ├── shaders/                # Shader examples
│   ├── audio/                  # Audio examples
│   ├── examples_list.txt       # Current 187-entry index
│   ├── Makefile                # Build system
│   └── README.md               # Official examples README
└── docs/                       # Repository-level documentation
    ├── README.md               # Documentation index
    ├── PROJECT_OVERVIEW.md     # Code-verified project guide
    └── LAUNCHER_README.md      # This launcher guide
```

## Requirements

- **Raylib** 5.5+ installed via Homebrew: `brew install raylib`
- **GCC** or compatible C compiler
- **macOS** with OpenGL support (other platforms need adjusted compile flags)

## Features

✅ Browse all 181 currently indexed primary examples in one place
✅ Color-coded categories
✅ Difficulty ratings displayed (★☆☆☆ to ★★★★)
✅ One-click compile and run
✅ Source code printing to the terminal
✅ Smooth scrolling navigation
✅ Filtering by category

## Example Difficulty Levels

- ★☆☆☆ - Basic (beginner-friendly)
- ★★☆☆ - Intermediate
- ★★★☆ - Advanced
- ★★★★ - Expert (complex implementations)

## Tips

1. **Start with Core examples** - Learn window management and input first
2. **Follow difficulty progression** - Begin with ★☆☆☆ examples
3. **Read the source code** - Press `C` to print it in the terminal before running
4. **Check resources** - Some examples need resource files (images, models, shaders)

## Troubleshooting

### Compilation Errors

If you get raylib header errors:
```bash
brew install raylib
```

### Resource Not Found

Some examples require resources in the `resources/` folder. Make sure to run examples from the correct directory:
```bash
cd raylib-examples/core
./core_basic_window
```

### Platform-Specific Compilation

**Linux:**
```bash
gcc example.c -o example -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
```

**Windows (MinGW):**
```bash
gcc example.c -o example.exe -lraylib -lopengl32 -lgdi32 -lwinmm
```

## Learning Path

### Recommended Order:

1. **Core basics** → `core_basic_window`, `core_input_keys`, `core_input_mouse`
2. **Drawing shapes** → `shapes_basic_shapes`, `shapes_colors_palette`
3. **Loading textures** → `textures_logo_raylib`, `textures_image_loading`
4. **Text rendering** → `text_font_loading`, `text_writing_anim`
5. **3D graphics** → `models_geometric_shapes`, `models_loading`
6. **Shaders** → `shaders_shapes_textures`, `shaders_basic_lighting`
7. **Audio** → `audio_sound_loading`, `audio_music_stream`

## Resources

- **Official Raylib**: https://www.raylib.com
- **Examples Online**: https://www.raylib.com/examples.html
- **GitHub Repo**: https://github.com/raysan5/raylib
- **Repository documentation**: See [the documentation index](README.md)
- **Examples documentation**: See `raylib-examples/README.md`

## Credits

- **Raylib** by Ramon Santamaria (@raysan5)
- All examples created by the raylib community
- Launcher created for educational purposes

Enjoy learning Raylib! 🎮
