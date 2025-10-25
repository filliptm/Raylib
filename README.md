# Raylib Examples Collection

Complete collection of all 182 official raylib examples with a graphical launcher.

## 🎮 Quick Start

```bash
./example_launcher
```

This opens a graphical menu where you can:
- Browse all 182 examples by category
- View source code
- Compile and run examples with one keypress

## 📚 What's Included

### Examples by Category

| Category | Count | Description |
|----------|-------|-------------|
| **Core** | 45 | Window management, input, cameras, timing |
| **Shapes** | 32 | 2D drawing, collisions, animations |
| **Textures** | 26 | Image loading, sprites, particles |
| **Text** | 15 | Fonts, text rendering, input boxes |
| **Models** | 25 | 3D models, meshes, animations |
| **Shaders** | 31 | GLSL shaders, lighting, effects |
| **Audio** | 8 | Sound effects, music streaming |
| **Total** | **182** | Complete raylib example collection |

## 🎯 Features

✅ **Graphical Launcher** - Browse all examples in an interactive menu
✅ **One-Click Compile & Run** - Press ENTER to compile and execute
✅ **Source Code Viewer** - Press C to view example code
✅ **Color-Coded Categories** - Easy visual organization
✅ **Difficulty Ratings** - From ★☆☆☆ (basic) to ★★★★ (advanced)
✅ **Complete Documentation** - Learn raylib step-by-step

## 🎨 Launcher Controls

| Key | Action |
|-----|--------|
| `↑/↓` | Navigate examples |
| `PAGE UP/DOWN` | Jump 20 examples |
| `HOME/END` | First/Last example |
| `ENTER` | Compile & Run |
| `C` | View source code |
| `ESC` | Exit launcher |

## 📂 Project Structure

```
Raylib/
├── example_launcher.c        # Launcher source code
├── example_launcher          # Compiled launcher
├── LAUNCHER_README.md        # Detailed launcher docs
├── README.md                 # This file
│
├── raylib-examples/          # All 182 examples
│   ├── core/                 # 45 core examples
│   ├── shapes/               # 32 shapes examples
│   ├── textures/             # 26 texture examples
│   ├── text/                 # 15 text examples
│   ├── models/               # 25 3D model examples
│   ├── shaders/              # 31 shader examples
│   ├── audio/                # 8 audio examples
│   ├── examples_list.txt     # Complete index
│   ├── Makefile              # Build system
│   └── resources/            # Assets for examples
│
└── documentation/
    └── raylib/
        ├── README.md         # Raylib overview
        ├── GETTING_STARTED.md
        ├── GRAPHICS_2D.md
        ├── GRAPHICS_3D.md
        ├── AUDIO.md
        └── INPUT_HANDLING.md
```

## 🚀 Running Examples

### Method 1: Using the Launcher (Recommended)

```bash
./example_launcher
```

Navigate with arrow keys and press ENTER to run.

### Method 2: Manual Compilation

```bash
cd raylib-examples/core
gcc core_basic_window.c -o example $(pkg-config --cflags --libs raylib) \
    -framework OpenGL -framework Cocoa -framework IOKit
./example
```

### Method 3: Using Make

```bash
cd raylib-examples
make core/core_basic_window
./core/core_basic_window
```

## 📖 Learning Path

### Beginner (★☆☆☆)
1. `core_basic_window` - Create your first window
2. `core_input_keys` - Handle keyboard input
3. `core_input_mouse` - Mouse interaction
4. `shapes_basic_shapes` - Draw 2D shapes
5. `textures_logo_raylib` - Load and display images

### Intermediate (★★☆☆)
1. `core_2d_camera` - Camera controls
2. `shapes_collision_area` - Collision detection
3. `textures_sprite_animation` - Animated sprites
4. `text_input_box` - Text input handling
5. `models_geometric_shapes` - 3D basics

### Advanced (★★★☆)
1. `core_2d_camera_platformer` - Platformer camera
2. `shapes_top_down_lights` - 2D lighting
3. `textures_bunnymark` - Performance optimization
4. `models_mesh_picking` - 3D picking
5. `shaders_postprocessing` - Shader effects

### Expert (★★★★)
1. `core_2d_camera_split_screen` - Split-screen rendering
2. `shapes_digital_clock` - Complex animations
3. `textures_image_kernel` - Image processing
4. `models_rlgl_solar_system` - Advanced 3D
5. `shaders_deferred_rendering` - Advanced rendering

## 🛠️ Requirements

- **Raylib 5.5+** - `brew install raylib`
- **GCC or Clang** - C compiler
- **macOS/Linux/Windows** - Cross-platform

### Installation

**macOS:**
```bash
brew install raylib
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt install libraylib-dev
```

**Windows:**
Download from [raylib releases](https://github.com/raysan5/raylib/releases)

## 🎓 Resources

- **Official Website**: https://www.raylib.com
- **Examples Online**: https://www.raylib.com/examples.html
- **GitHub**: https://github.com/raysan5/raylib
- **Discord**: https://discord.gg/raylib
- **Cheatsheet**: https://www.raylib.com/cheatsheet/cheatsheet.html

## 📝 Example Categories

### Core (45 examples)
Window management, input handling, cameras, file I/O, timing, VR, automation

### Shapes (32 examples)
2D drawing primitives, collisions, easing, recursion, lighting, particles

### Textures (26 examples)
Image loading, manipulation, sprites, particles, blending, tiling

### Text (15 examples)
Font loading, text rendering, Unicode, 3D text, styling, input boxes

### Models (25 examples)
3D shapes, model loading (OBJ, GLTF, VOX), animations, skybox, heightmaps

### Shaders (31 examples)
GLSL shaders, lighting, raymarching, fog, post-processing, PBR, deferred rendering

### Audio (8 examples)
Sound loading, music streaming, multi-channel audio, audio effects

## 🎨 Color Coding

The launcher uses colors to identify categories:

- 🔵 **Core** - Sky Blue
- 🔴 **Shapes** - Red
- 🟢 **Textures** - Green
- 🟡 **Text** - Yellow
- 🟣 **Models** - Purple
- 🟠 **Shaders** - Orange
- 🩷 **Audio** - Pink

## 💡 Tips

1. **Start simple** - Begin with ★☆☆☆ examples
2. **Read the code** - Press 'C' in launcher to view source
3. **Modify examples** - Best way to learn is by experimenting
4. **Check resources** - Some examples need assets from `resources/` folder
5. **Use the docs** - See `documentation/raylib/` for detailed guides

## 🐛 Troubleshooting

**Compilation errors:**
```bash
# Make sure raylib is installed
brew install raylib

# Verify installation
pkg-config --cflags --libs raylib
```

**Missing resources:**
Some examples need resources. Run from the example's directory:
```bash
cd raylib-examples/textures
gcc textures_logo_raylib.c -o example $(pkg-config --cflags --libs raylib) \
    -framework OpenGL -framework Cocoa -framework IOKit
./example
```

**Launcher not loading examples:**
Ensure `raylib-examples/examples_list.txt` exists.

## 📜 License

All examples are from the official raylib repository and follow raylib's license (zlib/libpng).

The launcher program is created for educational purposes.

## 🙏 Credits

- **Raylib** by Ramon Santamaria (@raysan5)
- All individual example authors (see source files)
- Raylib community contributors

---

**Enjoy learning game development with Raylib! 🎮✨**

For detailed launcher documentation, see [LAUNCHER_README.md](LAUNCHER_README.md)
