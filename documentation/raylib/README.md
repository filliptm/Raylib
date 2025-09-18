# Raylib Documentation

## Table of Contents
1. [Introduction](#introduction)
2. [Getting Started](#getting-started)
3. [Core Module](#core-module)
4. [Graphics](#graphics)
   - [2D Graphics](#2d-graphics)
   - [3D Graphics](#3d-graphics)
5. [Audio](#audio)
6. [Shaders](#shaders)
7. [Input Handling](#input-handling)
8. [Best Practices](#best-practices)
9. [Examples](#examples)
10. [Resources](#resources)

## Introduction

Raylib is a simple and easy-to-use library to enjoy videogames programming. It is highly inspired by Borland BGI graphics library and by XNA framework and it's specially well suited for prototyping, tooling, graphical applications, embedded systems and education.

### Key Features
- **No external dependencies** - All required libraries are bundled into raylib
- **Multiplatform** - Windows, Linux, MacOS, RPI, Android, HTML5 and more
- **Written in C99** - Bindings available for 60+ other languages
- **Hardware accelerated** - OpenGL (1.1, 2.1, 3.3 or ES 2.0)
- **Very simple API** - Just ~470 functions for everything
- **Complete** - Fonts, textures, audio, 3d models, shaders

### Philosophy
Raylib is designed for:
- **Pure spartan-programmers** who enjoy coding from scratch
- **Learning** - Reading and understanding code is the best way to learn
- **Simplicity** - No complex abstractions, just straightforward functions
- **Minimalism** - Only what's necessary, nothing more

## Getting Started

### Installation

#### Windows
```bash
# Option 1: Download pre-compiled binaries
# Visit https://github.com/raysan5/raylib/releases

# Option 2: Build from source with CMake
git clone https://github.com/raysan5/raylib.git
cd raylib
mkdir build && cd build
cmake ..
make
```

#### macOS
```bash
# Using Homebrew
brew install raylib

# Or build from source
git clone https://github.com/raysan5/raylib.git
cd raylib/src
make PLATFORM=PLATFORM_DESKTOP
```

#### Linux
```bash
# Ubuntu/Debian
sudo apt install build-essential git cmake
git clone https://github.com/raysan5/raylib.git
cd raylib
mkdir build && cd build
cmake ..
make
sudo make install

# Arch Linux
sudo pacman -S raylib
```

### Your First Raylib Program

```c
#include "raylib.h"

int main(void)
{
    // Initialization
    const int screenWidth = 800;
    const int screenHeight = 450;
    
    InitWindow(screenWidth, screenHeight, "raylib - basic window");
    
    SetTargetFPS(60);  // Set our game to run at 60 frames-per-second
    
    // Main game loop
    while (!WindowShouldClose())  // Detect window close button or ESC key
    {
        // Update
        // TODO: Update your variables here
        
        // Draw
        BeginDrawing();
            ClearBackground(RAYWHITE);
            DrawText("Congrats! You created your first window!", 190, 200, 20, LIGHTGRAY);
        EndDrawing();
    }
    
    // De-Initialization
    CloseWindow();  // Close window and OpenGL context
    
    return 0;
}
```

### Compilation

```bash
# Basic compilation
gcc main.c -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

# macOS
gcc main.c -lraylib -framework OpenGL -framework Cocoa -framework IOKit

# Windows with MinGW
gcc main.c -lraylib -lopengl32 -lgdi32 -lwinmm
```

## Core Module

The core module provides essential functions for window management, timing, and basic operations.

### Window Management

```c
// Initialize window
InitWindow(int width, int height, const char *title);

// Check if window should close
bool WindowShouldClose(void);

// Close window and unload OpenGL context
CloseWindow(void);

// Check if window is ready
bool IsWindowReady(void);

// Check if window is fullscreen mode
bool IsWindowFullscreen(void);

// Check if window is hidden
bool IsWindowHidden(void);

// Check if window is minimized
bool IsWindowMinimized(void);

// Check if window is maximized
bool IsWindowMaximized(void);

// Check if window is focused
bool IsWindowFocused(void);

// Check if window has been resized
bool IsWindowResized(void);

// Set window configuration state
void SetWindowState(unsigned int flags);

// Clear window configuration state
void ClearWindowState(unsigned int flags);

// Toggle window fullscreen
void ToggleFullscreen(void);

// Set window title
void SetWindowTitle(const char *title);

// Set window position on screen
void SetWindowPosition(int x, int y);

// Set window minimum dimensions
void SetWindowMinSize(int width, int height);

// Set window dimensions
void SetWindowSize(int width, int height);
```

### Timing Functions

```c
// Set target FPS (frames per second)
void SetTargetFPS(int fps);

// Get current FPS
int GetFPS(void);

// Get time in seconds for last frame drawn
float GetFrameTime(void);

// Get elapsed time in seconds since InitWindow()
double GetTime(void);
```

### Drawing Functions

```c
// Begin drawing
void BeginDrawing(void);

// End drawing and swap buffers
void EndDrawing(void);

// Begin 2D mode with custom camera
void BeginMode2D(Camera2D camera);

// End 2D mode
void EndMode2D(void);

// Begin 3D mode with custom camera
void BeginMode3D(Camera3D camera);

// End 3D mode
void EndMode3D(void);

// Begin render texture mode
void BeginTextureMode(RenderTexture2D target);

// End render texture mode
void EndTextureMode(void);

// Begin scissor mode (define screen area for following drawing)
void BeginScissorMode(int x, int y, int width, int height);

// End scissor mode
void EndScissorMode(void);
```

### Screen-space Functions

```c
// Clear background with color
void ClearBackground(Color color);

// Get current screen width
int GetScreenWidth(void);

// Get current screen height
int GetScreenHeight(void);
```

## Graphics

### 2D Graphics

#### Basic Shapes

```c
// Pixel operations
void DrawPixel(int posX, int posY, Color color);
void DrawPixelV(Vector2 position, Color color);

// Line drawing
void DrawLine(int startPosX, int startPosY, int endPosX, int endPosY, Color color);
void DrawLineV(Vector2 startPos, Vector2 endPos, Color color);
void DrawLineEx(Vector2 startPos, Vector2 endPos, float thick, Color color);
void DrawLineBezier(Vector2 startPos, Vector2 endPos, float thick, Color color);
void DrawLineStrip(Vector2 *points, int pointCount, Color color);

// Circle drawing
void DrawCircle(int centerX, int centerY, float radius, Color color);
void DrawCircleSector(Vector2 center, float radius, float startAngle, float endAngle, int segments, Color color);
void DrawCircleSectorLines(Vector2 center, float radius, float startAngle, float endAngle, int segments, Color color);
void DrawCircleGradient(int centerX, int centerY, float radius, Color color1, Color color2);
void DrawCircleV(Vector2 center, float radius, Color color);
void DrawCircleLines(int centerX, int centerY, float radius, Color color);

// Rectangle drawing
void DrawRectangle(int posX, int posY, int width, int height, Color color);
void DrawRectangleV(Vector2 position, Vector2 size, Color color);
void DrawRectangleRec(Rectangle rec, Color color);
void DrawRectanglePro(Rectangle rec, Vector2 origin, float rotation, Color color);
void DrawRectangleGradientV(int posX, int posY, int width, int height, Color color1, Color color2);
void DrawRectangleGradientH(int posX, int posY, int width, int height, Color color1, Color color2);
void DrawRectangleGradientEx(Rectangle rec, Color col1, Color col2, Color col3, Color col4);
void DrawRectangleLines(int posX, int posY, int width, int height, Color color);
void DrawRectangleLinesEx(Rectangle rec, float lineThick, Color color);
void DrawRectangleRounded(Rectangle rec, float roundness, int segments, Color color);
void DrawRectangleRoundedLines(Rectangle rec, float roundness, int segments, float lineThick, Color color);

// Triangle drawing
void DrawTriangle(Vector2 v1, Vector2 v2, Vector2 v3, Color color);
void DrawTriangleLines(Vector2 v1, Vector2 v2, Vector2 v3, Color color);
void DrawTriangleFan(Vector2 *points, int pointCount, Color color);
void DrawTriangleStrip(Vector2 *points, int pointCount, Color color);

// Polygon drawing
void DrawPoly(Vector2 center, int sides, float radius, float rotation, Color color);
void DrawPolyLines(Vector2 center, int sides, float radius, float rotation, Color color);
void DrawPolyLinesEx(Vector2 center, int sides, float radius, float rotation, float lineThick, Color color);
```

#### Texture Operations

```c
// Image loading functions
Image LoadImage(const char *fileName);
Image LoadImageRaw(const char *fileName, int width, int height, int format, int headerSize);
Image LoadImageAnim(const char *fileName, int *frames);
Image LoadImageFromMemory(const char *fileType, const unsigned char *fileData, int dataSize);
Image LoadImageFromTexture(Texture2D texture);
Image LoadImageFromScreen(void);
void UnloadImage(Image image);
bool ExportImage(Image image, const char *fileName);

// Image manipulation
Image ImageCopy(Image image);
Image ImageFromImage(Image image, Rectangle rec);
Image ImageText(const char *text, int fontSize, Color color);
Image ImageTextEx(Font font, const char *text, float fontSize, float spacing, Color tint);
void ImageFormat(Image *image, int newFormat);
void ImageToPOT(Image *image, Color fill);
void ImageCrop(Image *image, Rectangle crop);
void ImageAlphaCrop(Image *image, float threshold);
void ImageAlphaClear(Image *image, Color color, float threshold);
void ImageAlphaMask(Image *image, Image alphaMask);
void ImageAlphaPremultiply(Image *image);
void ImageResize(Image *image, int newWidth, int newHeight);
void ImageResizeNN(Image *image, int newWidth, int newHeight);
void ImageResizeCanvas(Image *image, int newWidth, int newHeight, int offsetX, int offsetY, Color fill);
void ImageMipmaps(Image *image);
void ImageDither(Image *image, int rBpp, int gBpp, int bBpp, int aBpp);
void ImageFlipVertical(Image *image);
void ImageFlipHorizontal(Image *image);

// Texture loading/unloading functions
Texture2D LoadTexture(const char *fileName);
Texture2D LoadTextureFromImage(Image image);
TextureCubemap LoadTextureCubemap(Image image, int layout);
RenderTexture2D LoadRenderTexture(int width, int height);
void UnloadTexture(Texture2D texture);
void UnloadRenderTexture(RenderTexture2D target);
void UpdateTexture(Texture2D texture, const void *pixels);
void UpdateTextureRec(Texture2D texture, Rectangle rec, const void *pixels);

// Texture drawing functions
void DrawTexture(Texture2D texture, int posX, int posY, Color tint);
void DrawTextureV(Texture2D texture, Vector2 position, Color tint);
void DrawTextureEx(Texture2D texture, Vector2 position, float rotation, float scale, Color tint);
void DrawTextureRec(Texture2D texture, Rectangle source, Vector2 position, Color tint);
void DrawTextureQuad(Texture2D texture, Vector2 tiling, Vector2 offset, Rectangle quad, Color tint);
void DrawTextureTiled(Texture2D texture, Rectangle source, Rectangle dest, Vector2 origin, float rotation, float scale, Color tint);
void DrawTexturePro(Texture2D texture, Rectangle source, Rectangle dest, Vector2 origin, float rotation, Color tint);
void DrawTextureNPatch(Texture2D texture, NPatchInfo nPatchInfo, Rectangle dest, Vector2 origin, float rotation, Color tint);
```

#### Text Rendering

```c
// Font loading/unloading functions
Font GetFontDefault(void);
Font LoadFont(const char *fileName);
Font LoadFontEx(const char *fileName, int fontSize, int *fontChars, int glyphCount);
Font LoadFontFromImage(Image image, Color key, int firstChar);
Font LoadFontFromMemory(const char *fileType, const unsigned char *fileData, int dataSize, int fontSize, int *fontChars, int glyphCount);
void UnloadFont(Font font);

// Text drawing functions
void DrawText(const char *text, int posX, int posY, int fontSize, Color color);
void DrawTextEx(Font font, const char *text, Vector2 position, float fontSize, float spacing, Color tint);
void DrawTextPro(Font font, const char *text, Vector2 position, Vector2 origin, float rotation, float fontSize, float spacing, Color tint);
void DrawTextCodepoint(Font font, int codepoint, Vector2 position, float fontSize, Color tint);

// Text measurement functions
int MeasureText(const char *text, int fontSize);
Vector2 MeasureTextEx(Font font, const char *text, float fontSize, float spacing);
```

### 3D Graphics

#### Basic 3D Shapes

```c
// Basic geometric 3D shapes drawing functions
void DrawLine3D(Vector3 startPos, Vector3 endPos, Color color);
void DrawPoint3D(Vector3 position, Color color);
void DrawCircle3D(Vector3 center, float radius, Vector3 rotationAxis, float rotationAngle, Color color);
void DrawTriangle3D(Vector3 v1, Vector3 v2, Vector3 v3, Color color);
void DrawTriangleStrip3D(Vector3 *points, int pointCount, Color color);
void DrawCube(Vector3 position, float width, float height, float length, Color color);
void DrawCubeV(Vector3 position, Vector3 size, Color color);
void DrawCubeWires(Vector3 position, float width, float height, float length, Color color);
void DrawCubeWiresV(Vector3 position, Vector3 size, Color color);
void DrawCubeTexture(Texture2D texture, Vector3 position, float width, float height, float length, Color color);
void DrawCubeTextureRec(Texture2D texture, Rectangle source, Vector3 position, float width, float height, float length, Color color);
void DrawSphere(Vector3 centerPos, float radius, Color color);
void DrawSphereEx(Vector3 centerPos, float radius, int rings, int slices, Color color);
void DrawSphereWires(Vector3 centerPos, float radius, int rings, int slices, Color color);
void DrawCylinder(Vector3 position, float radiusTop, float radiusBottom, float height, int slices, Color color);
void DrawCylinderEx(Vector3 startPos, Vector3 endPos, float startRadius, float endRadius, int sides, Color color);
void DrawCylinderWires(Vector3 position, float radiusTop, float radiusBottom, float height, int slices, Color color);
void DrawCylinderWiresEx(Vector3 startPos, Vector3 endPos, float startRadius, float endRadius, int sides, Color color);
void DrawPlane(Vector3 centerPos, Vector2 size, Color color);
void DrawRay(Ray ray, Color color);
void DrawGrid(int slices, float spacing);
```

#### Model Loading and Drawing

```c
// Model loading/unloading functions
Model LoadModel(const char *fileName);
Model LoadModelFromMesh(Mesh mesh);
void UnloadModel(Model model);
void UnloadModelKeepMeshes(Model model);
BoundingBox GetModelBoundingBox(Model model);

// Model drawing functions
void DrawModel(Model model, Vector3 position, float scale, Color tint);
void DrawModelEx(Model model, Vector3 position, Vector3 rotationAxis, float rotationAngle, Vector3 scale, Color tint);
void DrawModelWires(Model model, Vector3 position, float scale, Color tint);
void DrawModelWiresEx(Model model, Vector3 position, Vector3 rotationAxis, float rotationAngle, Vector3 scale, Color tint);
void DrawBoundingBox(BoundingBox box, Color color);
void DrawBillboard(Camera camera, Texture2D texture, Vector3 position, float size, Color tint);
void DrawBillboardRec(Camera camera, Texture2D texture, Rectangle source, Vector3 position, Vector2 size, Color tint);
void DrawBillboardPro(Camera camera, Texture2D texture, Rectangle source, Vector3 position, Vector3 up, Vector2 size, Vector2 origin, float rotation, Color tint);

// Mesh manipulation functions
void UploadMesh(Mesh *mesh, bool dynamic);
void UpdateMeshBuffer(Mesh mesh, int index, void *data, int dataSize, int offset);
void UnloadMesh(Mesh mesh);
void DrawMesh(Mesh mesh, Material material, Matrix transform);
void DrawMeshInstanced(Mesh mesh, Material material, Matrix *transforms, int instances);
bool ExportMesh(Mesh mesh, const char *fileName);
BoundingBox GetMeshBoundingBox(Mesh mesh);
void GenMeshTangents(Mesh *mesh);
void GenMeshBinormals(Mesh *mesh);

// Mesh generation functions
Mesh GenMeshPoly(int sides, float radius);
Mesh GenMeshPlane(float width, float length, int resX, int resZ);
Mesh GenMeshCube(float width, float height, float length);
Mesh GenMeshSphere(float radius, int rings, int slices);
Mesh GenMeshHemiSphere(float radius, int rings, int slices);
Mesh GenMeshCylinder(float radius, float height, int slices);
Mesh GenMeshCone(float radius, float height, int slices);
Mesh GenMeshTorus(float radius, float size, int radSeg, int sides);
Mesh GenMeshKnot(float radius, float size, int radSeg, int sides);
Mesh GenMeshHeightmap(Image heightmap, Vector3 size);
Mesh GenMeshCubicmap(Image cubicmap, Vector3 cubeSize);
```

#### Camera System

```c
// Camera modes
typedef enum {
    CAMERA_CUSTOM = 0,
    CAMERA_FREE,
    CAMERA_ORBITAL,
    CAMERA_FIRST_PERSON,
    CAMERA_THIRD_PERSON
} CameraMode;

// Camera projection
typedef enum {
    CAMERA_PERSPECTIVE = 0,
    CAMERA_ORTHOGRAPHIC
} CameraProjection;

// Camera structure
typedef struct Camera3D {
    Vector3 position;       // Camera position
    Vector3 target;         // Camera target it looks-at
    Vector3 up;             // Camera up vector (rotation over its axis)
    float fovy;             // Camera field-of-view apperture in Y (degrees)
    int projection;         // Camera projection: CAMERA_PERSPECTIVE or CAMERA_ORTHOGRAPHIC
} Camera3D;

typedef Camera3D Camera;   // Camera type fallback, defaults to Camera3D

// Camera functions
void SetCameraMode(Camera camera, int mode);
void UpdateCamera(Camera *camera);
void SetCameraPanControl(int keyPan);
void SetCameraAltControl(int keyAlt);
void SetCameraSmoothZoomControl(int keySmoothZoom);
void SetCameraMoveControls(int keyFront, int keyBack, int keyRight, int keyLeft, int keyUp, int keyDown);
```

#### Collision Detection 3D

```c
// Basic collision detection functions
bool CheckCollisionSpheres(Vector3 center1, float radius1, Vector3 center2, float radius2);
bool CheckCollisionBoxes(BoundingBox box1, BoundingBox box2);
bool CheckCollisionBoxSphere(BoundingBox box, Vector3 center, float radius);
RayCollision GetRayCollisionSphere(Ray ray, Vector3 center, float radius);
RayCollision GetRayCollisionBox(Ray ray, BoundingBox box);
RayCollision GetRayCollisionModel(Ray ray, Model model);
RayCollision GetRayCollisionMesh(Ray ray, Mesh mesh, Matrix transform);
RayCollision GetRayCollisionTriangle(Ray ray, Vector3 p1, Vector3 p2, Vector3 p3);
RayCollision GetRayCollisionQuad(Ray ray, Vector3 p1, Vector3 p2, Vector3 p3, Vector3 p4);
```

## Audio

### Audio Device Management

```c
// Audio device management functions
void InitAudioDevice(void);                                     // Initialize audio device and context
void CloseAudioDevice(void);                                    // Close the audio device and context
bool IsAudioDeviceReady(void);                                  // Check if audio device has been initialized successfully
void SetMasterVolume(float volume);                             // Set master volume (listener)
```

### Wave/Sound Loading and Playing

```c
// Wave/Sound loading/unloading functions
Wave LoadWave(const char *fileName);                            // Load wave data from file
Wave LoadWaveFromMemory(const char *fileType, const unsigned char *fileData, int dataSize); // Load wave from memory buffer
Sound LoadSound(const char *fileName);                          // Load sound from file
Sound LoadSoundFromWave(Wave wave);                             // Load sound from wave data
void UpdateSound(Sound sound, const void *data, int sampleCount); // Update sound buffer with new data
void UnloadWave(Wave wave);                                     // Unload wave data
void UnloadSound(Sound sound);                                  // Unload sound
bool ExportWave(Wave wave, const char *fileName);               // Export wave data to file
bool ExportWaveAsCode(Wave wave, const char *fileName);         // Export wave sample data to code (.h)

// Wave/Sound management functions
void PlaySound(Sound sound);                                    // Play a sound
void StopSound(Sound sound);                                    // Stop playing a sound
void PauseSound(Sound sound);                                   // Pause a sound
void ResumeSound(Sound sound);                                  // Resume a paused sound
void PlaySoundMulti(Sound sound);                               // Play a sound (using multichannel buffer pool)
void StopSoundMulti(void);                                      // Stop any sound playing (using multichannel buffer pool)
int GetSoundsPlaying(void);                                     // Get number of sounds playing in the multichannel
bool IsSoundPlaying(Sound sound);                               // Check if a sound is currently playing
void SetSoundVolume(Sound sound, float volume);                 // Set volume for a sound (1.0 is max level)
void SetSoundPitch(Sound sound, float pitch);                   // Set pitch for a sound (1.0 is base level)
void WaveFormat(Wave *wave, int sampleRate, int sampleSize, int channels); // Convert wave data to desired format
Wave WaveCopy(Wave wave);                                       // Copy a wave to a new wave
void WaveCrop(Wave *wave, int initSample, int finalSample);     // Crop a wave to defined samples range
float *LoadWaveSamples(Wave wave);                              // Load samples data from wave as a floats array
void UnloadWaveSamples(float *samples);                         // Unload samples data loaded with LoadWaveSamples()
```

### Music Streaming

```c
// Music management functions
Music LoadMusicStream(const char *fileName);                    // Load music stream from file
Music LoadMusicStreamFromMemory(const char *fileType, unsigned char *data, int dataSize); // Load music stream from memory
void UnloadMusicStream(Music music);                            // Unload music stream
void PlayMusicStream(Music music);                              // Start music playing
bool IsMusicStreamPlaying(Music music);                         // Check if music is playing
void UpdateMusicStream(Music music);                            // Updates buffers for music streaming
void StopMusicStream(Music music);                              // Stop music playing
void PauseMusicStream(Music music);                             // Pause music playing
void ResumeMusicStream(Music music);                            // Resume playing paused music
void SeekMusicStream(Music music, float position);              // Seek music to a position (in seconds)
void SetMusicVolume(Music music, float volume);                 // Set volume for music (1.0 is max level)
void SetMusicPitch(Music music, float pitch);                   // Set pitch for a music (1.0 is base level)
float GetMusicTimeLength(Music music);                          // Get music time length (in seconds)
float GetMusicTimePlayed(Music music);                          // Get current music time played (in seconds)
```

### Audio Processing

```c
// AudioStream management functions
AudioStream LoadAudioStream(unsigned int sampleRate, unsigned int sampleSize, unsigned int channels); // Load audio stream
void UnloadAudioStream(AudioStream stream);                     // Unload audio stream and free memory
void UpdateAudioStream(AudioStream stream, const void *data, int frameCount); // Update audio stream buffers
bool IsAudioStreamProcessed(AudioStream stream);                // Check if any audio stream buffers requires refill
void PlayAudioStream(AudioStream stream);                       // Play audio stream
void PauseAudioStream(AudioStream stream);                      // Pause audio stream
void ResumeAudioStream(AudioStream stream);                     // Resume audio stream
bool IsAudioStreamPlaying(AudioStream stream);                  // Check if audio stream is playing
void StopAudioStream(AudioStream stream);                       // Stop audio stream
void SetAudioStreamVolume(AudioStream stream, float volume);    // Set volume for audio stream (1.0 is max level)
void SetAudioStreamPitch(AudioStream stream, float pitch);      // Set pitch for audio stream (1.0 is base level)
void SetAudioStreamBufferSizeDefault(int size);                 // Default size for new audio streams
```

## Shaders

### Shader Loading and Management

```c
// Shader loading/unloading functions
Shader LoadShader(const char *vsFileName, const char *fsFileName);   // Load shader from files and bind default locations
Shader LoadShaderFromMemory(const char *vsCode, const char *fsCode); // Load shader from code strings
int GetShaderLocation(Shader shader, const char *uniformName);       // Get shader uniform location
int GetShaderLocationAttrib(Shader shader, const char *attribName);  // Get shader attribute location
void SetShaderValue(Shader shader, int locIndex, const void *value, int uniformType); // Set shader uniform value
void SetShaderValueV(Shader shader, int locIndex, const void *value, int uniformType, int count); // Set shader uniform value vector
void SetShaderValueMatrix(Shader shader, int locIndex, Matrix mat);  // Set shader uniform value (matrix 4x4)
void SetShaderValueTexture(Shader shader, int locIndex, Texture2D texture); // Set shader uniform value for texture
void UnloadShader(Shader shader);                                    // Unload shader from GPU memory (VRAM)
```

### Screen-space Shaders

```c
// Screen-space-related functions
Shader LoadShaderFromMemory(const char *vsCode, const char *fsCode); // Load shader from code strings
void BeginShaderMode(Shader shader);                            // Begin custom shader drawing
void EndShaderMode(void);                                        // End custom shader drawing (use default shader)
```

### Shader Configuration

```c
// Shader configuration functions
void SetShaderValue(Shader shader, int locIndex, const void *value, int uniformType);
void SetShaderValueV(Shader shader, int locIndex, const void *value, int uniformType, int count);
void SetShaderValueMatrix(Shader shader, int locIndex, Matrix mat);
void SetShaderValueTexture(Shader shader, int locIndex, Texture2D texture);
```

### GLSL Shader Example

```glsl
// Vertex shader (example.vs)
#version 330

// Input vertex attributes
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

// Input uniform values
uniform mat4 mvp;

// Output vertex attributes (to fragment shader)
out vec2 fragTexCoord;
out vec4 fragColor;

void main()
{
    // Send vertex attributes to fragment shader
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    
    // Calculate final vertex position
    gl_Position = mvp*vec4(vertexPosition, 1.0);
}
```

```glsl
// Fragment shader (example.fs)
#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Output fragment color
out vec4 finalColor;

void main()
{
    // Texel color fetching from texture sampler
    vec4 texelColor = texture(texture0, fragTexCoord);
    
    // Calculate final fragment color
    finalColor = texelColor*colDiffuse*fragColor;
}
```

### Using Shaders in Raylib

```c
// Example: Loading and using a custom shader
#include "raylib.h"

int main(void)
{
    InitWindow(800, 450, "raylib - shaders example");
    
    // Load shader from files
    Shader shader = LoadShader("shaders/base.vs", "shaders/grayscale.fs");
    
    // Load a texture
    Texture2D texture = LoadTexture("resources/raylib_logo.png");
    
    SetTargetFPS(60);
    
    while (!WindowShouldClose())
    {
        BeginDrawing();
            ClearBackground(RAYWHITE);
            
            // Enable shader
            BeginShaderMode(shader);
                // Draw with shader
                DrawTexture(texture, 0, 0, WHITE);
            EndShaderMode();
            
        EndDrawing();
    }
    
    UnloadShader(shader);
    UnloadTexture(texture);
    CloseWindow();
    
    return 0;
}
```

## Input Handling

### Keyboard Input

```c
// Input-related functions: keyboard
bool IsKeyPressed(int key);                             // Check if a key has been pressed once
bool IsKeyDown(int key);                                // Check if a key is being pressed
bool IsKeyReleased(int key);                            // Check if a key has been released once
bool IsKeyUp(int key);                                  // Check if a key is NOT being pressed
void SetExitKey(int key);                               // Set a custom key to exit program (default is ESC)
int GetKeyPressed(void);                                // Get key pressed (keycode), call it multiple times for keys queued
int GetCharPressed(void);                               // Get char pressed (unicode), call it multiple times for chars queued

// Keyboard keys (US keyboard layout)
typedef enum {
    KEY_NULL            = 0,        // Key: NULL, used for no key pressed
    // Alphanumeric keys
    KEY_APOSTROPHE      = 39,       // Key: '
    KEY_COMMA           = 44,       // Key: ,
    KEY_MINUS           = 45,       // Key: -
    KEY_PERIOD          = 46,       // Key: .
    KEY_SLASH           = 47,       // Key: /
    KEY_ZERO            = 48,       // Key: 0
    KEY_ONE             = 49,       // Key: 1
    KEY_TWO             = 50,       // Key: 2
    KEY_THREE           = 51,       // Key: 3
    KEY_FOUR            = 52,       // Key: 4
    KEY_FIVE            = 53,       // Key: 5
    KEY_SIX             = 54,       // Key: 6
    KEY_SEVEN           = 55,       // Key: 7
    KEY_EIGHT           = 56,       // Key: 8
    KEY_NINE            = 57,       // Key: 9
    KEY_SEMICOLON       = 59,       // Key: ;
    KEY_EQUAL           = 61,       // Key: =
    KEY_A               = 65,       // Key: A | a
    KEY_B               = 66,       // Key: B | b
    KEY_C               = 67,       // Key: C | c
    // ... (and so on for all keys)
    KEY_SPACE           = 32,       // Key: Space
    KEY_ESCAPE          = 256,      // Key: Esc
    KEY_ENTER           = 257,      // Key: Enter
    KEY_TAB             = 258,      // Key: Tab
    KEY_BACKSPACE       = 259,      // Key: Backspace
    KEY_INSERT          = 260,      // Key: Ins
    KEY_DELETE          = 261,      // Key: Del
    KEY_RIGHT           = 262,      // Key: Cursor right
    KEY_LEFT            = 263,      // Key: Cursor left
    KEY_DOWN            = 264,      // Key: Cursor down
    KEY_UP              = 265,      // Key: Cursor up
    // ... (function keys, etc.)
} KeyboardKey;
```

### Mouse Input

```c
// Input-related functions: mouse
bool IsMouseButtonPressed(int button);                  // Check if a mouse button has been pressed once
bool IsMouseButtonDown(int button);                     // Check if a mouse button is being pressed
bool IsMouseButtonReleased(int button);                 // Check if a mouse button has been released once
bool IsMouseButtonUp(int button);                       // Check if a mouse button is NOT being pressed
int GetMouseX(void);                                    // Get mouse position X
int GetMouseY(void);                                    // Get mouse position Y
Vector2 GetMousePosition(void);                         // Get mouse position XY
Vector2 GetMouseDelta(void);                            // Get mouse delta between frames
void SetMousePosition(int x, int y);                    // Set mouse position XY
void SetMouseOffset(int offsetX, int offsetY);          // Set mouse offset
void SetMouseScale(float scaleX, float scaleY);         // Set mouse scaling
float GetMouseWheelMove(void);                          // Get mouse wheel movement Y
Vector2 GetMouseWheelMoveV(void);                       // Get mouse wheel movement for both X and Y
void SetMouseCursor(int cursor);                        // Set mouse cursor

// Mouse buttons
typedef enum {
    MOUSE_BUTTON_LEFT    = 0,       // Mouse button left
    MOUSE_BUTTON_RIGHT   = 1,       // Mouse button right
    MOUSE_BUTTON_MIDDLE  = 2,       // Mouse button middle (pressed wheel)
    MOUSE_BUTTON_SIDE    = 3,       // Mouse button side (advanced mouse device)
    MOUSE_BUTTON_EXTRA   = 4,       // Mouse button extra (advanced mouse device)
    MOUSE_BUTTON_FORWARD = 5,       // Mouse button forward (advanced mouse device)
    MOUSE_BUTTON_BACK    = 6,       // Mouse button back (advanced mouse device)
} MouseButton;

// Mouse cursor
typedef enum {
    MOUSE_CURSOR_DEFAULT       = 0,     // Default pointer shape
    MOUSE_CURSOR_ARROW         = 1,     // Arrow shape
    MOUSE_CURSOR_IBEAM         = 2,     // Text writing cursor shape
    MOUSE_CURSOR_CROSSHAIR     = 3,     // Cross shape
    MOUSE_CURSOR_POINTING_HAND = 4,     // Pointing hand cursor
    MOUSE_CURSOR_RESIZE_EW     = 5,     // Horizontal resize/move arrow shape
    MOUSE_CURSOR_RESIZE_NS     = 6,     // Vertical resize/move arrow shape
    MOUSE_CURSOR_RESIZE_NWSE   = 7,     // Top-left to bottom-right diagonal resize/move arrow shape
    MOUSE_CURSOR_RESIZE_NESW   = 8,     // The top-right to bottom-left diagonal resize/move arrow shape
    MOUSE_CURSOR_RESIZE_ALL    = 9,     // The omnidirectional resize/move cursor shape
    MOUSE_CURSOR_NOT_ALLOWED   = 10     // The operation-not-allowed shape
} MouseCursor;
```

### Gamepad Input

```c
// Input-related functions: gamepads
bool IsGamepadAvailable(int gamepad);                   // Check if a gamepad is available
const char *GetGamepadName(int gamepad);                // Get gamepad internal name id
bool IsGamepadButtonPressed(int gamepad, int button);   // Check if a gamepad button has been pressed once
bool IsGamepadButtonDown(int gamepad, int button);      // Check if a gamepad button is being pressed
bool IsGamepadButtonReleased(int gamepad, int button);  // Check if a gamepad button has been released once
bool IsGamepadButtonUp(int gamepad, int button);        // Check if a gamepad button is NOT being pressed
int GetGamepadButtonPressed(void);                      // Get the last gamepad button pressed
int GetGamepadAxisCount(int gamepad);                   // Get gamepad axis count for a gamepad
float GetGamepadAxisMovement(int gamepad, int axis);    // Get axis movement value for a gamepad axis
int SetGamepadMappings(const char *mappings);           // Set internal gamepad mappings (SDL_GameControllerDB)

// Gamepad buttons
typedef enum {
    GAMEPAD_BUTTON_UNKNOWN = 0,         // Unknown button, just for error checking
    GAMEPAD_BUTTON_LEFT_FACE_UP,        // Gamepad left DPAD up button
    GAMEPAD_BUTTON_LEFT_FACE_RIGHT,     // Gamepad left DPAD right button
    GAMEPAD_BUTTON_LEFT_FACE_DOWN,      // Gamepad left DPAD down button
    GAMEPAD_BUTTON_LEFT_FACE_LEFT,      // Gamepad left DPAD left button
    GAMEPAD_BUTTON_RIGHT_FACE_UP,       // Gamepad right button up (i.e. PS3: Triangle, Xbox: Y)
    GAMEPAD_BUTTON_RIGHT_FACE_RIGHT,    // Gamepad right button right (i.e. PS3: Square, Xbox: X)
    GAMEPAD_BUTTON_RIGHT_FACE_DOWN,     // Gamepad right button down (i.e. PS3: Cross, Xbox: A)
    GAMEPAD_BUTTON_RIGHT_FACE_LEFT,     // Gamepad right button left (i.e. PS3: Circle, Xbox: B)
    GAMEPAD_BUTTON_LEFT_TRIGGER_1,      // Gamepad top/back trigger left (first), it could be a trailing button
    GAMEPAD_BUTTON_LEFT_TRIGGER_2,      // Gamepad top/back trigger left (second), it could be a trailing button
    GAMEPAD_BUTTON_RIGHT_TRIGGER_1,     // Gamepad top/back trigger right (one), it could be a trailing button
    GAMEPAD_BUTTON_RIGHT_TRIGGER_2,     // Gamepad top/back trigger right (second), it could be a trailing button
    GAMEPAD_BUTTON_MIDDLE_LEFT,         // Gamepad center buttons, left one (i.e. PS3: Select)
    GAMEPAD_BUTTON_MIDDLE,              // Gamepad center buttons, middle one (i.e. PS3: PS, Xbox: XBOX)
    GAMEPAD_BUTTON_MIDDLE_RIGHT,        // Gamepad center buttons, right one (i.e. PS3: Start)
    GAMEPAD_BUTTON_LEFT_THUMB,          // Gamepad joystick pressed button left
    GAMEPAD_BUTTON_RIGHT_THUMB          // Gamepad joystick pressed button right
} GamepadButton;

// Gamepad axis
typedef enum {
    GAMEPAD_AXIS_LEFT_X        = 0,     // Gamepad left stick X axis
    GAMEPAD_AXIS_LEFT_Y        = 1,     // Gamepad left stick Y axis
    GAMEPAD_AXIS_RIGHT_X       = 2,     // Gamepad right stick X axis
    GAMEPAD_AXIS_RIGHT_Y       = 3,     // Gamepad right stick Y axis
    GAMEPAD_AXIS_LEFT_TRIGGER  = 4,     // Gamepad back trigger left, pressure level: [1..-1]
    GAMEPAD_AXIS_RIGHT_TRIGGER = 5      // Gamepad back trigger right, pressure level: [1..-1]
} GamepadAxis;
```

### Touch Input

```c
// Input-related functions: touch
int GetTouchX(void);                                    // Get touch position X for touch point 0 (relative to screen size)
int GetTouchY(void);                                    // Get touch position Y for touch point 0 (relative to screen size)
Vector2 GetTouchPosition(int index);                    // Get touch position XY for a touch point index (relative to screen size)
int GetTouchPointId(int index);                         // Get touch point identifier for given index
int GetTouchPointCount(void);                           // Get number of touch points
```

## Best Practices

### Code Organization

1. **Initialize Properly**
   ```c
   InitWindow(800, 450, "My Game");
   InitAudioDevice();
   SetTargetFPS(60);
   ```

2. **Resource Management**
   - Always unload resources when done
   - Load resources outside the game loop
   - Use resource pools for frequently used assets

3. **Game Loop Structure**
   ```c
   while (!WindowShouldClose())
   {
       // Update
       UpdateGame();
       
       // Draw
       BeginDrawing();
           ClearBackground(RAYWHITE);
           DrawGame();
       EndDrawing();
   }
   ```

### Performance Tips

1. **Batch Drawing Operations**
   - Group similar draw calls together
   - Use texture atlases for sprites
   - Minimize state changes

2. **Optimize Updates**
   - Use delta time for frame-independent movement
   - Implement spatial partitioning for collision detection
   - Profile your code to find bottlenecks

3. **Memory Management**
   - Reuse objects instead of creating new ones
   - Clear unused resources
   - Monitor memory usage

### Common Patterns

1. **State Management**
   ```c
   typedef enum GameScreen { LOGO, TITLE, GAMEPLAY, ENDING } GameScreen;
   
   GameScreen currentScreen = LOGO;
   
   switch(currentScreen)
   {
       case LOGO:
           // Update/Draw LOGO screen
           break;
       case TITLE:
           // Update/Draw TITLE screen
           break;
       case GAMEPLAY:
           // Update/Draw GAMEPLAY screen
           break;
       case ENDING:
           // Update/Draw ENDING screen
           break;
   }
   ```

2. **Animation System**
   ```c
   typedef struct {
       Texture2D texture;
       Rectangle frameRec;
       int currentFrame;
       int framesCounter;
       int framesSpeed;
   } Animation;
   
   void UpdateAnimation(Animation *anim)
   {
       anim->framesCounter++;
       
       if (anim->framesCounter >= (60/anim->framesSpeed))
       {
           anim->framesCounter = 0;
           anim->currentFrame++;
           
           if (anim->currentFrame > 5) anim->currentFrame = 0;
           
           anim->frameRec.x = (float)anim->currentFrame*(float)anim->texture.width/6;
       }
   }
   ```

3. **Simple Physics**
   ```c
   typedef struct Player {
       Vector2 position;
       Vector2 speed;
       float jumpSpeed;
       bool canJump;
   } Player;
   
   void UpdatePlayer(Player *player)
   {
       if (IsKeyPressed(KEY_SPACE) && player->canJump)
       {
           player->speed.y = player->jumpSpeed;
           player->canJump = false;
       }
       
       player->position.x += player->speed.x;
       player->position.y += player->speed.y;
       
       // Apply gravity
       player->speed.y += 0.5f;
       
       // Ground check
       if (player->position.y >= 400)
       {
           player->position.y = 400;
           player->speed.y = 0;
           player->canJump = true;
       }
   }
   ```

## Examples

### Example 1: Basic Window with Shapes

```c
#include "raylib.h"

int main(void)
{
    const int screenWidth = 800;
    const int screenHeight = 450;
    
    InitWindow(screenWidth, screenHeight, "raylib - basic shapes");
    SetTargetFPS(60);
    
    while (!WindowShouldClose())
    {
        BeginDrawing();
            ClearBackground(RAYWHITE);
            
            DrawText("some basic shapes available on raylib", 20, 20, 20, DARKGRAY);
            
            // Circle shapes and lines
            DrawCircle(screenWidth/5, 120, 35, DARKBLUE);
            DrawCircleGradient(screenWidth/5, 220, 60, GREEN, SKYBLUE);
            DrawCircleLines(screenWidth/5, 340, 80, DARKBLUE);
            
            // Rectangle shapes and lines
            DrawRectangle(screenWidth/4*2 - 60, 100, 120, 60, RED);
            DrawRectangleGradientH(screenWidth/4*2 - 90, 170, 180, 130, MAROON, GOLD);
            DrawRectangleLines(screenWidth/4*2 - 40, 320, 80, 60, ORANGE);
            
            // Triangle shapes and lines
            DrawTriangle((Vector2){screenWidth/4*3, 80},
                         (Vector2){screenWidth/4*3 - 60, 150},
                         (Vector2){screenWidth/4*3 + 60, 150}, VIOLET);
            
            DrawTriangleLines((Vector2){screenWidth/4*3, 160},
                              (Vector2){screenWidth/4*3 - 20, 230},
                              (Vector2){screenWidth/4*3 + 20, 230}, DARKBLUE);
            
            // Polygon shapes and lines
            DrawPoly((Vector2){screenWidth/4*3, 330}, 6, 80, 0, BROWN);
            DrawPolyLinesEx((Vector2){screenWidth/4*3, 330}, 6, 90, 0, 6, BEIGE);
            
        EndDrawing();
    }
    
    CloseWindow();
    
    return 0;
}
```

### Example 2: Texture Loading and Drawing

```c
#include "raylib.h"

int main(void)
{
    const int screenWidth = 800;
    const int screenHeight = 450;
    
    InitWindow(screenWidth, screenHeight, "raylib - texture loading and drawing");
    
    // NOTE: Textures MUST be loaded after Window initialization (OpenGL context is required)
    Texture2D texture = LoadTexture("resources/raylib_logo.png");
    
    SetTargetFPS(60);
    
    while (!WindowShouldClose())
    {
        BeginDrawing();
            ClearBackground(RAYWHITE);
            
            DrawTexture(texture, screenWidth/2 - texture.width/2, screenHeight/2 - texture.height/2, WHITE);
            
            DrawText("this IS a texture!", 360, 370, 10, GRAY);
            
        EndDrawing();
    }
    
    // De-Initialization
    UnloadTexture(texture);
    CloseWindow();
    
    return 0;
}
```

### Example 3: 3D Camera

```c
#include "raylib.h"

int main(void)
{
    const int screenWidth = 800;
    const int screenHeight = 450;
    
    InitWindow(screenWidth, screenHeight, "raylib - 3d camera free");
    
    // Define the camera to look into our 3d world
    Camera3D camera = { 0 };
    camera.position = (Vector3){ 10.0f, 10.0f, 10.0f };
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    
    Vector3 cubePosition = { 0.0f, 0.0f, 0.0f };
    
    SetCameraMode(camera, CAMERA_FREE);
    SetTargetFPS(60);
    
    while (!WindowShouldClose())
    {
        // Update
        UpdateCamera(&camera);
        
        if (IsKeyDown('Z')) camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
        
        // Draw
        BeginDrawing();
            ClearBackground(RAYWHITE);
            
            BeginMode3D(camera);
                DrawCube(cubePosition, 2.0f, 2.0f, 2.0f, RED);
                DrawCubeWires(cubePosition, 2.0f, 2.0f, 2.0f, MAROON);
                DrawGrid(10, 1.0f);
            EndMode3D();
            
            DrawText("Free camera default controls:", 20, 20, 10, BLACK);
            DrawText("- Mouse Wheel to Zoom in-out", 40, 40, 10, DARKGRAY);
            DrawText("- Mouse Wheel Pressed to Pan", 40, 60, 10, DARKGRAY);
            DrawText("- Alt + Mouse Wheel Pressed to Rotate", 40, 80, 10, DARKGRAY);
            DrawText("- Alt + Ctrl + Mouse Wheel Pressed for Smooth Zoom", 40, 100, 10, DARKGRAY);
            DrawText("- Z to zoom to (0, 0, 0)", 40, 120, 10, DARKGRAY);
            
        EndDrawing();
    }
    
    CloseWindow();
    
    return 0;
}
```

### Example 4: Audio Playback

```c
#include "raylib.h"

int main(void)
{
    const int screenWidth = 800;
    const int screenHeight = 450;
    
    InitWindow(screenWidth, screenHeight, "raylib - audio playing");
    InitAudioDevice();
    
    Sound fxWav = LoadSound("resources/sound.wav");
    Sound fxOgg = LoadSound("resources/target.ogg");
    
    SetTargetFPS(60);
    
    while (!WindowShouldClose())
    {
        // Update
        if (IsKeyPressed(KEY_SPACE)) PlaySound(fxWav);
        if (IsKeyPressed(KEY_ENTER)) PlaySound(fxOgg);
        
        // Draw
        BeginDrawing();
            ClearBackground(RAYWHITE);
            DrawText("Press SPACE to PLAY the WAV sound!", 200, 180, 20, LIGHTGRAY);
            DrawText("Press ENTER to PLAY the OGG sound!", 200, 220, 20, LIGHTGRAY);
        EndDrawing();
    }
    
    // De-Initialization
    UnloadSound(fxWav);
    UnloadSound(fxOgg);
    CloseAudioDevice();
    CloseWindow();
    
    return 0;
}
```

### Example 5: Basic Game Structure

```c
#include "raylib.h"

// Game screens
typedef enum GameScreen { LOGO = 0, TITLE, GAMEPLAY, ENDING } GameScreen;

int main(void)
{
    // Initialization
    const int screenWidth = 800;
    const int screenHeight = 450;
    
    InitWindow(screenWidth, screenHeight, "raylib - game screens");
    
    GameScreen currentScreen = LOGO;
    
    int framesCounter = 0;          // Useful to count frames
    
    SetTargetFPS(60);               // Set desired framerate
    
    // Main game loop
    while (!WindowShouldClose())
    {
        // Update
        switch(currentScreen)
        {
            case LOGO:
            {
                framesCounter++;    // Count frames
                
                // Wait for 2 seconds (120 frames) before jumping to TITLE screen
                if (framesCounter > 120)
                {
                    currentScreen = TITLE;
                }
            } break;
            case TITLE:
            {
                // Press enter to change to GAMEPLAY screen
                if (IsKeyPressed(KEY_ENTER) || IsGestureDetected(GESTURE_TAP))
                {
                    currentScreen = GAMEPLAY;
                }
            } break;
            case GAMEPLAY:
            {
                // Press enter to change to ENDING screen
                if (IsKeyPressed(KEY_ENTER) || IsGestureDetected(GESTURE_TAP))
                {
                    currentScreen = ENDING;
                }
            } break;
            case ENDING:
            {
                // Press enter to return to TITLE screen
                if (IsKeyPressed(KEY_ENTER) || IsGestureDetected(GESTURE_TAP))
                {
                    currentScreen = TITLE;
                }
            } break;
            default: break;
        }
        
        // Draw
        BeginDrawing();
            
            ClearBackground(RAYWHITE);
            
            switch(currentScreen)
            {
                case LOGO:
                {
                    DrawText("LOGO SCREEN", 20, 20, 40, LIGHTGRAY);
                    DrawText("WAIT for 2 SECONDS...", 290, 220, 20, GRAY);
                    
                } break;
                case TITLE:
                {
                    DrawRectangle(0, 0, screenWidth, screenHeight, GREEN);
                    DrawText("TITLE SCREEN", 20, 20, 40, DARKGREEN);
                    DrawText("PRESS ENTER or TAP to JUMP to GAMEPLAY SCREEN", 120, 220, 20, DARKGREEN);
                    
                } break;
                case GAMEPLAY:
                {
                    DrawRectangle(0, 0, screenWidth, screenHeight, PURPLE);
                    DrawText("GAMEPLAY SCREEN", 20, 20, 40, MAROON);
                    DrawText("PRESS ENTER or TAP to JUMP to ENDING SCREEN", 130, 220, 20, MAROON);
                    
                } break;
                case ENDING:
                {
                    DrawRectangle(0, 0, screenWidth, screenHeight, BLUE);
                    DrawText("ENDING SCREEN", 20, 20, 40, DARKBLUE);
                    DrawText("PRESS ENTER or TAP to RETURN to TITLE SCREEN", 120, 220, 20, DARKBLUE);
                    
                } break;
                default: break;
            }
            
        EndDrawing();
    }
    
    // De-Initialization
    CloseWindow();
    
    return 0;
}
```

## Resources

### Official Resources
- **Website**: [www.raylib.com](https://www.raylib.com)
- **GitHub**: [github.com/raysan5/raylib](https://github.com/raysan5/raylib)
- **Wiki**: [github.com/raysan5/raylib/wiki](https://github.com/raysan5/raylib/wiki)
- **Cheatsheet**: [www.raylib.com/cheatsheet/cheatsheet.html](https://www.raylib.com/cheatsheet/cheatsheet.html)
- **Examples**: [github.com/raysan5/raylib/tree/master/examples](https://github.com/raysan5/raylib/tree/master/examples)

### Community Resources
- **Discord Server**: [discord.gg/VkzNHUE](https://discord.gg/VkzNHUE)
- **Reddit**: [r/raylib](https://www.reddit.com/r/raylib/)
- **YouTube Channel**: [Raylib YouTube](https://www.youtube.com/c/raylib)

### Learning Materials
- **raylib Tutorial Series**: Various community tutorials on YouTube
- **Game Development with raylib**: Community-created courses
- **raylib Projects Showcase**: See what others have created

### Tools and Extensions
- **rGuiStyler**: A tool to create custom styles for raygui
- **rTexPacker**: A simple and easy-to-use textures packer
- **rFXGen**: A simple and easy-to-use fx sounds generator

### Language Bindings
Raylib has been ported to over 60 programming languages. Some popular bindings include:
- **C++**: raylib-cpp
- **Python**: pyraylib, raylib-python-cffi
- **C#**: Raylib-cs
- **Go**: raylib-go
- **Rust**: raylib-rs
- **JavaScript**: raylib.js (for web)
- **Lua**: raylib-lua
- **Ruby**: raylib-ruby

### Tips for Learning Raylib
1. Start with the basic examples
2. Read the cheatsheet for quick reference
3. Explore the source code - it's well-commented
4. Join the community for help and inspiration
5. Build small projects to practice
6. Don't hesitate to ask questions on Discord

Remember: Raylib is designed to be simple. If something seems complicated, there's probably an easier way to do it!