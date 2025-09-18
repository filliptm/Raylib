# Getting Started with Raylib

This guide will walk you through setting up Raylib on different platforms and creating your first program.

## Table of Contents
1. [Installation](#installation)
   - [Windows](#windows)
   - [macOS](#macos)
   - [Linux](#linux)
   - [Web (Emscripten)](#web-emscripten)
2. [Project Setup](#project-setup)
3. [Hello World Program](#hello-world-program)
4. [Compilation](#compilation)
5. [IDE Setup](#ide-setup)
6. [Troubleshooting](#troubleshooting)

## Installation

### Windows

#### Option 1: Pre-compiled Binaries (Recommended for beginners)
1. Visit [Raylib Releases](https://github.com/raysan5/raylib/releases)
2. Download the latest `raylib_x.x.x_win64_mingw-w64.zip` or `raylib_x.x.x_win32_mingw-w64.zip`
3. Extract the ZIP file to a location like `C:\raylib`
4. The package includes:
   - `include/` - Header files
   - `lib/` - Library files
   - `examples/` - Example projects

#### Option 2: Using MinGW-w64
```bash
# Install MinGW-w64 if not already installed
# Download from: https://www.mingw-w64.org/downloads/

# Clone raylib
git clone https://github.com/raysan5/raylib.git
cd raylib/src

# Compile raylib
mingw32-make PLATFORM=PLATFORM_DESKTOP

# The library will be in raylib/src/libraylib.a
```

#### Option 3: Using Visual Studio
```bash
# Clone raylib
git clone https://github.com/raysan5/raylib.git
cd raylib

# Use the provided Visual Studio solutions
# Open projects/VS2022/raylib.sln
```

#### Option 4: Using CMake
```bash
# Clone raylib
git clone https://github.com/raysan5/raylib.git
cd raylib
mkdir build && cd build

# Generate Visual Studio project files
cmake .. -G "Visual Studio 17 2022"

# Or for MinGW
cmake .. -G "MinGW Makefiles"
mingw32-make
```

### macOS

#### Option 1: Homebrew (Recommended)
```bash
# Install Homebrew if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install raylib
brew install raylib

# Verify installation
pkg-config --libs --cflags raylib
```

#### Option 2: Build from Source
```bash
# Install Xcode Command Line Tools
xcode-select --install

# Clone raylib
git clone https://github.com/raysan5/raylib.git
cd raylib/src

# Compile
make PLATFORM=PLATFORM_DESKTOP

# Optional: Install system-wide
sudo make install
```

#### Option 3: Using CMake
```bash
# Install CMake
brew install cmake

# Clone and build
git clone https://github.com/raysan5/raylib.git
cd raylib
mkdir build && cd build
cmake ..
make
sudo make install
```

### Linux

#### Ubuntu/Debian
```bash
# Install build dependencies
sudo apt update
sudo apt install build-essential git cmake libasound2-dev mesa-common-dev libx11-dev libxrandr-dev libxi-dev xorg-dev libgl1-mesa-dev libglu1-mesa-dev

# Clone raylib
git clone https://github.com/raysan5/raylib.git
cd raylib
mkdir build && cd build
cmake ..
make
sudo make install
```

#### Fedora
```bash
# Install dependencies
sudo dnf install gcc make git cmake mesa-libGL-devel libX11-devel libXrandr-devel libXi-devel libXcursor-devel libXinerama-devel

# Clone and build
git clone https://github.com/raysan5/raylib.git
cd raylib
mkdir build && cd build
cmake ..
make
sudo make install
```

#### Arch Linux
```bash
# Install from AUR
yay -S raylib

# Or from official repositories
sudo pacman -S raylib
```

### Web (Emscripten)

```bash
# Install Emscripten
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh

# Clone raylib
cd ..
git clone https://github.com/raysan5/raylib.git
cd raylib/src

# Compile for Web
make PLATFORM=PLATFORM_WEB -B

# The library will be in raylib/src/libraylib.a
```

## Project Setup

### Basic Project Structure
```
my-raylib-project/
├── src/
│   └── main.c
├── include/
│   └── raylib.h  (if not installed system-wide)
├── lib/
│   └── libraylib.a  (if not installed system-wide)
├── resources/
│   ├── textures/
│   ├── sounds/
│   └── fonts/
├── Makefile
└── README.md
```

### Makefile Example
```makefile
# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -std=c99 -D_DEFAULT_SOURCE -Wno-missing-braces

# Include and library paths (adjust as needed)
INCLUDES = -I./include -I/usr/local/include
LFLAGS = -L./lib -L/usr/local/lib

# Libraries
LIBS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

# macOS specific libraries
ifeq ($(shell uname),Darwin)
    LIBS = -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
endif

# Windows specific libraries
ifeq ($(OS),Windows_NT)
    LIBS = -lraylib -lopengl32 -lgdi32 -lwinmm
endif

# Source files
SRCS = src/main.c

# Object files
OBJS = $(SRCS:.c=.o)

# Executable name
TARGET = my_game

# Default target
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(TARGET) $(OBJS) $(LFLAGS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) src/*.o $(TARGET)

run: $(TARGET)
	./$(TARGET)
```

## Hello World Program

Create a file `src/main.c`:

```c
#include "raylib.h"

int main(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib - Hello World");

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        // TODO: Update your variables here
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawText("Hello, Raylib!", 190, 200, 20, LIGHTGRAY);
            
            // Draw a red circle
            DrawCircle(screenWidth/2, screenHeight/2 + 50, 50, RED);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
```

## Compilation

### Command Line Compilation

#### Linux
```bash
gcc main.c -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -o my_game
```

#### macOS
```bash
gcc main.c -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -o my_game
```

#### Windows (MinGW)
```bash
gcc main.c -lraylib -lopengl32 -lgdi32 -lwinmm -o my_game.exe
```

### Using pkg-config (Linux/macOS)
```bash
gcc main.c $(pkg-config --libs --cflags raylib) -o my_game
```

### Static Linking
```bash
# Linux
gcc main.c -static -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -o my_game

# Note: Static linking may require additional libraries depending on your system
```

### Web Compilation (Emscripten)
```bash
emcc main.c -Os -Wall -s USE_GLFW=3 -s ASYNCIFY -DPLATFORM_WEB -Iinclude -Llib -lraylib -o index.html
```

## IDE Setup

### Visual Studio Code

1. Install the C/C++ extension
2. Create `.vscode/c_cpp_properties.json`:
```json
{
    "configurations": [
        {
            "name": "Linux",
            "includePath": [
                "${workspaceFolder}/**",
                "/usr/local/include"
            ],
            "defines": [],
            "compilerPath": "/usr/bin/gcc",
            "cStandard": "c99",
            "cppStandard": "c++14",
            "intelliSenseMode": "linux-gcc-x64"
        }
    ],
    "version": 4
}
```

3. Create `.vscode/tasks.json`:
```json
{
    "version": "2.0.0",
    "tasks": [
        {
            "type": "cppbuild",
            "label": "Build raylib project",
            "command": "/usr/bin/gcc",
            "args": [
                "-g",
                "${file}",
                "-o",
                "${fileDirname}/${fileBasenameNoExtension}",
                "-lraylib",
                "-lGL",
                "-lm",
                "-lpthread",
                "-ldl",
                "-lrt",
                "-lX11"
            ],
            "options": {
                "cwd": "${workspaceFolder}"
            },
            "problemMatcher": [
                "$gcc"
            ],
            "group": {
                "kind": "build",
                "isDefault": true
            },
            "detail": "compiler: /usr/bin/gcc"
        }
    ]
}
```

4. Create `.vscode/launch.json` for debugging:
```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Debug raylib project",
            "type": "cppdbg",
            "request": "launch",
            "program": "${fileDirname}/${fileBasenameNoExtension}",
            "args": [],
            "stopAtEntry": false,
            "cwd": "${workspaceFolder}",
            "environment": [],
            "externalConsole": false,
            "MIMode": "gdb",
            "preLaunchTask": "Build raylib project"
        }
    ]
}
```

### Visual Studio 2022

1. Create a new Empty C++ Project
2. Right-click on the project → Properties
3. Configuration Properties → VC++ Directories:
   - Include Directories: Add raylib include path
   - Library Directories: Add raylib lib path
4. Linker → Input → Additional Dependencies:
   - Add: `raylib.lib;opengl32.lib;gdi32.lib;winmm.lib`
5. Set the project to use C standard (not C++)

### Code::Blocks

1. Create a new Console Application project
2. Project → Build Options
3. Search Directories:
   - Compiler: Add raylib include path
   - Linker: Add raylib lib path
4. Linker Settings:
   - Add libraries: raylib, GL, m, pthread, dl, rt, X11

## Troubleshooting

### Common Issues

#### "raylib.h: No such file or directory"
- Ensure raylib is properly installed
- Check include paths in your compilation command
- Verify the header file location

#### Undefined reference to raylib functions
- Make sure you're linking with `-lraylib`
- Check library path with `-L` flag
- Ensure the library file exists

#### OpenGL errors
- Update graphics drivers
- Ensure OpenGL development libraries are installed
- On Linux: `sudo apt install libgl1-mesa-dev`

#### Audio initialization failed
- On Linux, install ALSA development files: `sudo apt install libasound2-dev`
- Check audio permissions
- Try running with `sudo` to test permissions

#### Web build issues
- Ensure Emscripten is properly activated: `source emsdk_env.sh`
- Use appropriate flags: `-s USE_GLFW=3 -s ASYNCIFY`
- Check browser console for errors

### Platform-Specific Notes

#### macOS
- May need to allow unsigned apps in Security preferences
- Use `-framework` flags for system frameworks
- Homebrew installation is usually the smoothest

#### Windows
- Prefer MinGW-w64 over MinGW
- Visual Studio requires C mode, not C++
- Windows Defender may flag executables

#### Linux
- Different distributions have different package names
- May need to install development versions of libraries
- Check `pkg-config` availability

### Getting Help

1. Check the [official Wiki](https://github.com/raysan5/raylib/wiki)
2. Visit the [Discord community](https://discord.gg/VkzNHUE)
3. Search [GitHub issues](https://github.com/raysan5/raylib/issues)
4. Review [examples](https://github.com/raysan5/raylib/tree/master/examples)

Remember: Raylib is designed to be simple. Most issues are related to setup rather than the library itself.