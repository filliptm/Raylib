# 2D Graphics in Raylib

This guide covers all aspects of 2D graphics programming with Raylib, from basic shapes to advanced techniques.

## Table of Contents
1. [Basic Concepts](#basic-concepts)
2. [Drawing Shapes](#drawing-shapes)
3. [Working with Colors](#working-with-colors)
4. [Textures and Images](#textures-and-images)
5. [Sprites and Animation](#sprites-and-animation)
6. [Text Rendering](#text-rendering)
7. [2D Camera](#2d-camera)
8. [Collision Detection](#collision-detection)
9. [Particle Systems](#particle-systems)
10. [Advanced Techniques](#advanced-techniques)

## Basic Concepts

### Coordinate System
- Origin (0,0) is at the top-left corner
- X increases to the right
- Y increases downward
- All positions are in pixels

### Drawing Order
- Raylib draws in the order you call the functions
- Later draws appear on top of earlier ones
- Use this for layering effects

### Basic Drawing Structure
```c
BeginDrawing();
    ClearBackground(RAYWHITE);
    
    // Your drawing code here
    
EndDrawing();
```

## Drawing Shapes

### Points and Pixels

```c
// Draw a single pixel
DrawPixel(100, 100, RED);

// Draw pixel with Vector2
Vector2 position = { 150, 150 };
DrawPixelV(position, BLUE);
```

### Lines

```c
// Basic line
DrawLine(0, 0, 100, 100, BLACK);

// Line with Vector2
Vector2 start = { 50, 50 };
Vector2 end = { 200, 150 };
DrawLineV(start, end, GREEN);

// Thick line
DrawLineEx(start, end, 5.0f, PURPLE);

// Bezier line (curved)
DrawLineBezier(start, end, 2.0f, RED);

// Multiple connected lines
Vector2 points[] = { {100, 100}, {200, 100}, {200, 200}, {100, 200} };
DrawLineStrip(points, 4, BLUE);
```

### Circles

```c
// Filled circle
DrawCircle(400, 300, 50, RED);

// Circle with Vector2
Vector2 center = { 400, 300 };
DrawCircleV(center, 50, RED);

// Gradient circle
DrawCircleGradient(400, 300, 50, RED, BLUE);

// Circle outline
DrawCircleLines(400, 300, 50, BLACK);

// Circle sector (pie slice)
DrawCircleSector(center, 50, 0, 90, 32, GREEN);

// Circle sector outline
DrawCircleSectorLines(center, 50, 0, 90, 32, GREEN);
```

### Rectangles

```c
// Basic rectangle
DrawRectangle(100, 100, 200, 150, BLUE);

// Rectangle with Vector2
Vector2 position = { 100, 100 };
Vector2 size = { 200, 150 };
DrawRectangleV(position, size, BLUE);

// Rectangle with Rectangle struct
Rectangle rec = { 100, 100, 200, 150 };
DrawRectangleRec(rec, BLUE);

// Rotated rectangle
Vector2 origin = { 100, 75 };  // Rotation point relative to rectangle
DrawRectanglePro(rec, origin, 45.0f, RED);

// Gradient rectangles
DrawRectangleGradientV(100, 100, 200, 150, RED, BLUE);  // Vertical
DrawRectangleGradientH(100, 100, 200, 150, RED, BLUE);  // Horizontal

// Four-color gradient
DrawRectangleGradientEx(rec, RED, BLUE, GREEN, YELLOW);

// Rectangle outline
DrawRectangleLines(100, 100, 200, 150, BLACK);

// Thick rectangle outline
DrawRectangleLinesEx(rec, 3.0f, BLACK);

// Rounded rectangle
DrawRectangleRounded(rec, 0.2f, 10, BLUE);

// Rounded rectangle outline
DrawRectangleRoundedLines(rec, 0.2f, 10, 2.0f, BLACK);
```

### Triangles

```c
// Filled triangle
Vector2 v1 = { 200, 100 };
Vector2 v2 = { 100, 200 };
Vector2 v3 = { 300, 200 };
DrawTriangle(v1, v2, v3, VIOLET);

// Triangle outline
DrawTriangleLines(v1, v2, v3, BLACK);

// Triangle fan (multiple triangles sharing first vertex)
Vector2 points[] = { {200, 200}, {250, 150}, {300, 200}, {250, 250} };
DrawTriangleFan(points, 4, ORANGE);

// Triangle strip (connected triangles)
DrawTriangleStrip(points, 4, PURPLE);
```

### Polygons

```c
// Regular polygon
Vector2 center = { 400, 300 };
DrawPoly(center, 6, 80, 0, BROWN);  // Hexagon

// Polygon outline
DrawPolyLines(center, 6, 80, 0, BLACK);

// Thick polygon outline
DrawPolyLinesEx(center, 6, 80, 0, 3.0f, BLACK);
```

### Rings

```c
// Ring (donut shape)
Vector2 center = { 400, 300 };
DrawRing(center, 40, 60, 0, 360, 32, RED);

// Ring outline
DrawRingLines(center, 40, 60, 0, 360, 32, BLACK);
```

## Working with Colors

### Predefined Colors
```c
// Raylib includes many predefined colors
LIGHTGRAY, GRAY, DARKGRAY, YELLOW, GOLD, ORANGE, PINK, RED,
MAROON, GREEN, LIME, DARKGREEN, SKYBLUE, BLUE, DARKBLUE,
PURPLE, VIOLET, DARKPURPLE, BEIGE, BROWN, DARKBROWN,
WHITE, BLACK, BLANK, MAGENTA, RAYWHITE
```

### Creating Custom Colors
```c
// RGBA color (red, green, blue, alpha)
Color myColor = { 255, 128, 0, 255 };  // Orange

// Using color functions
Color fadeRed = Fade(RED, 0.5f);  // 50% transparent red
Color darkBlue = ColorAlpha(BLUE, 0.7f);  // 70% opaque blue

// Color conversion
int hexColor = ColorToInt(RED);
Color fromHex = GetColor(0xff0000ff);  // Red from hex

// HSV color
Vector3 hsv = ColorToHSV(RED);
Color fromHSV = ColorFromHSV(hsv.x, hsv.y, hsv.z);
```

### Color Manipulation
```c
// Tint colors
Color tinted = ColorTint(WHITE, RED);  // White tinted with red

// Brightness
Color bright = ColorBrightness(BLUE, 0.5f);  // 50% brightness
Color contrast = ColorContrast(GRAY, 0.2f);  // Adjust contrast

// Alpha blending
Color blend = ColorAlphaBlend(BLUE, RED, WHITE);
```

## Textures and Images

### Loading and Drawing Textures

```c
// Load texture from file
Texture2D texture = LoadTexture("resources/image.png");

// Basic texture drawing
DrawTexture(texture, 100, 100, WHITE);

// Draw with Vector2 position
Vector2 position = { 100, 100 };
DrawTextureV(texture, position, WHITE);

// Draw with rotation and scale
DrawTextureEx(texture, position, 45.0f, 2.0f, WHITE);

// Draw part of texture
Rectangle source = { 0, 0, 32, 32 };  // Part to draw
Vector2 position = { 100, 100 };
DrawTextureRec(texture, source, position, WHITE);

// Draw with more control
Rectangle source = { 0, 0, 32, 32 };
Rectangle dest = { 100, 100, 64, 64 };  // Scale 2x
Vector2 origin = { 32, 32 };  // Rotation center
DrawTexturePro(texture, source, dest, origin, 45.0f, WHITE);

// Don't forget to unload
UnloadTexture(texture);
```

### Image Manipulation

```c
// Load image
Image image = LoadImage("resources/image.png");

// Image operations
ImageResize(&image, 64, 64);  // Resize
ImageFlipVertical(&image);     // Flip vertically
ImageFlipHorizontal(&image);   // Flip horizontally
ImageRotateCW(&image);         // Rotate 90° clockwise
ImageColorTint(&image, RED);   // Tint with color

// Convert formats
ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

// Generate mipmaps
ImageMipmaps(&image);

// Create texture from modified image
Texture2D texture = LoadTextureFromImage(image);

// Clean up
UnloadImage(image);
UnloadTexture(texture);
```

### Image Generation

```c
// Generate images
Image checked = GenImageChecked(256, 256, 32, 32, RED, BLUE);
Image white = GenImageColor(256, 256, WHITE);
Image gradient = GenImageGradientV(256, 256, RED, BLUE);
Image radial = GenImageGradientRadial(256, 256, 0.0f, WHITE, BLACK);

// Create texture from generated image
Texture2D checkerboard = LoadTextureFromImage(checked);
UnloadImage(checked);
```

### Render Textures (Drawing to textures)

```c
// Create render texture
RenderTexture2D target = LoadRenderTexture(256, 256);

// Draw to render texture
BeginTextureMode(target);
    ClearBackground(BLANK);
    DrawCircle(128, 128, 64, RED);
    DrawRectangle(64, 64, 128, 128, Fade(BLUE, 0.5f));
EndTextureMode();

// Use render texture in main drawing
BeginDrawing();
    ClearBackground(RAYWHITE);
    
    // Draw render texture (note: it's flipped)
    DrawTextureRec(target.texture, 
        (Rectangle){ 0, 0, target.texture.width, -target.texture.height },
        (Vector2){ 0, 0 }, WHITE);
        
EndDrawing();

// Clean up
UnloadRenderTexture(target);
```

## Sprites and Animation

### Basic Sprite Animation

```c
typedef struct {
    Texture2D texture;
    Rectangle frameRec;
    int currentFrame;
    int framesCounter;
    int framesSpeed;    // Animation speed
    int frameCount;
} AnimatedSprite;

// Initialize animated sprite
AnimatedSprite sprite = {0};
sprite.texture = LoadTexture("resources/sprite_sheet.png");
sprite.frameCount = 8;  // 8 frames in the sprite sheet
sprite.frameRec = (Rectangle){ 0, 0, sprite.texture.width/8, sprite.texture.height };
sprite.currentFrame = 0;
sprite.framesCounter = 0;
sprite.framesSpeed = 8;  // Change frame every 8 screen frames

// Update animation
sprite.framesCounter++;
if (sprite.framesCounter >= (60/sprite.framesSpeed)) {
    sprite.framesCounter = 0;
    sprite.currentFrame++;
    
    if (sprite.currentFrame >= sprite.frameCount) sprite.currentFrame = 0;
    
    sprite.frameRec.x = (float)sprite.currentFrame * sprite.frameRec.width;
}

// Draw animated sprite
DrawTextureRec(sprite.texture, sprite.frameRec, position, WHITE);
```

### Sprite Batch Rendering

```c
// Example: Drawing many sprites efficiently
typedef struct {
    Vector2 position;
    Vector2 velocity;
    Color color;
} Particle;

#define MAX_PARTICLES 1000
Particle particles[MAX_PARTICLES];

// Initialize particles
for (int i = 0; i < MAX_PARTICLES; i++) {
    particles[i].position = (Vector2){ GetRandomValue(0, screenWidth), 
                                       GetRandomValue(0, screenHeight) };
    particles[i].velocity = (Vector2){ GetRandomValue(-100, 100)/100.0f,
                                       GetRandomValue(-100, 100)/100.0f };
    particles[i].color = (Color){ GetRandomValue(0, 255), 
                                  GetRandomValue(0, 255),
                                  GetRandomValue(0, 255), 255 };
}

// Draw all particles
for (int i = 0; i < MAX_PARTICLES; i++) {
    DrawPixelV(particles[i].position, particles[i].color);
}
```

## Text Rendering

### Basic Text Drawing

```c
// Simple text
DrawText("Hello Raylib!", 100, 100, 20, BLACK);

// Formatted text
DrawText(TextFormat("Score: %d", score), 10, 10, 20, RED);

// Measure text
int textWidth = MeasureText("Hello!", 20);

// Center text
const char *text = "Centered Text";
int fontSize = 30;
int textWidth = MeasureText(text, fontSize);
DrawText(text, screenWidth/2 - textWidth/2, screenHeight/2, fontSize, BLACK);
```

### Custom Fonts

```c
// Load custom font
Font font = LoadFont("resources/custom_font.ttf");

// Alternative loading with size
Font fontBig = LoadFontEx("resources/custom_font.ttf", 96, 0, 0);

// Draw with custom font
DrawTextEx(font, "Custom Font Text", (Vector2){ 100, 100 }, 32, 2, BLACK);

// Draw with rotation
Vector2 position = { 200, 200 };
Vector2 origin = { 0, 0 };
DrawTextPro(font, "Rotated Text", position, origin, 45.0f, 32, 2, RED);

// Measure custom font text
Vector2 textSize = MeasureTextEx(font, "Measure This", 32, 2);

// Unload fonts
UnloadFont(font);
UnloadFont(fontBig);
```

### Text Effects

```c
// Typewriter effect
void DrawTextTypewriter(const char *text, int posX, int posY, int fontSize, 
                       float charsRevealed, Color color) {
    int length = TextLength(text);
    int charsToDraw = (int)(charsRevealed * length);
    
    char *partial = (char *)MemAlloc(charsToDraw + 1);
    memcpy(partial, text, charsToDraw);
    partial[charsToDraw] = '\0';
    
    DrawText(partial, posX, posY, fontSize, color);
    MemFree(partial);
}

// Wave text effect
void DrawTextWave(const char *text, int posX, int posY, int fontSize, Color color) {
    float time = GetTime();
    for (int i = 0; text[i] != '\0'; i++) {
        char letter[2] = { text[i], '\0' };
        float offsetY = sinf(time * 3.0f + i * 0.5f) * 5.0f;
        DrawText(letter, posX + i * (fontSize/2), posY + offsetY, fontSize, color);
    }
}
```

## 2D Camera

### Basic Camera Setup

```c
// Define camera
Camera2D camera = { 0 };
camera.target = (Vector2){ player.x, player.y };
camera.offset = (Vector2){ screenWidth/2.0f, screenHeight/2.0f };
camera.rotation = 0.0f;
camera.zoom = 1.0f;

// Use camera
BeginMode2D(camera);
    // Draw your game world here
    // Everything will be transformed by the camera
EndMode2D();

// Draw UI elements here (not affected by camera)
```

### Camera Controls

```c
// Camera following player smoothly
void UpdateCameraSmooth(Camera2D *camera, Vector2 target, float deltaTime) {
    float minSpeed = 30;
    float minEffectLength = 10;
    float fractionSpeed = 0.8f;
    
    camera->offset = (Vector2){ screenWidth/2.0f, screenHeight/2.0f };
    Vector2 diff = Vector2Subtract(target, camera->target);
    float length = Vector2Length(diff);
    
    if (length > minEffectLength) {
        float speed = fmaxf(fractionSpeed * length, minSpeed);
        camera->target = Vector2Add(camera->target, 
            Vector2Scale(diff, speed * deltaTime / length));
    }
}

// Camera boundaries
void UpdateCameraWithBounds(Camera2D *camera, Vector2 target, 
                           Rectangle bounds, int screenWidth, int screenHeight) {
    camera->target = target;
    camera->offset = (Vector2){ screenWidth/2.0f, screenHeight/2.0f };
    
    float minX = bounds.x + screenWidth/2.0f/camera->zoom;
    float minY = bounds.y + screenHeight/2.0f/camera->zoom;
    float maxX = bounds.x + bounds.width - screenWidth/2.0f/camera->zoom;
    float maxY = bounds.y + bounds.height - screenHeight/2.0f/camera->zoom;
    
    camera->target.x = fmaxf(minX, fminf(camera->target.x, maxX));
    camera->target.y = fmaxf(minY, fminf(camera->target.y, maxY));
}

// Mouse world position
Vector2 GetMouseWorldPos(Camera2D camera) {
    return GetScreenToWorld2D(GetMousePosition(), camera);
}
```

### Camera Effects

```c
// Screen shake effect
typedef struct {
    float trauma;
    float maxAngle;
    float maxOffset;
} CameraShake;

void UpdateCameraShake(Camera2D *camera, CameraShake *shake, float deltaTime) {
    if (shake->trauma > 0) {
        shake->trauma -= deltaTime;
        if (shake->trauma < 0) shake->trauma = 0;
        
        float amount = shake->trauma * shake->trauma;
        camera->rotation = shake->maxAngle * amount * GetRandomValue(-100, 100) / 100.0f;
        camera->offset.x += shake->maxOffset * amount * GetRandomValue(-100, 100) / 100.0f;
        camera->offset.y += shake->maxOffset * amount * GetRandomValue(-100, 100) / 100.0f;
    }
}

// Add trauma for shake
void AddCameraTrauma(CameraShake *shake, float trauma) {
    shake->trauma = fminf(shake->trauma + trauma, 1.0f);
}
```

## Collision Detection

### Basic Collision

```c
// Rectangle collision
Rectangle rect1 = { 100, 100, 80, 80 };
Rectangle rect2 = { 150, 150, 80, 80 };
bool collision = CheckCollisionRecs(rect1, rect2);

// Circle collision
Vector2 center1 = { 200, 200 };
float radius1 = 50;
Vector2 center2 = { 250, 250 };
float radius2 = 60;
bool circleCollision = CheckCollisionCircles(center1, radius1, center2, radius2);

// Circle-Rectangle collision
bool circleRectCollision = CheckCollisionCircleRec(center1, radius1, rect1);

// Point in rectangle
Vector2 point = GetMousePosition();
bool pointInRect = CheckCollisionPointRec(point, rect1);

// Point in circle
bool pointInCircle = CheckCollisionPointCircle(point, center1, radius1);

// Point in triangle
Vector2 p1 = { 100, 100 };
Vector2 p2 = { 200, 100 };
Vector2 p3 = { 150, 200 };
bool pointInTriangle = CheckCollisionPointTriangle(point, p1, p2, p3);

// Line collision
Vector2 startPos = { 0, 0 };
Vector2 endPos = { 100, 100 };
bool lineCircle = CheckCollisionPointLine(point, startPos, endPos, 10);

// Get collision rectangle
Rectangle collision = GetCollisionRec(rect1, rect2);
```

### Advanced Collision

```c
// Pixel-perfect collision
bool PixelPerfectCollision(Texture2D tex1, Rectangle rec1, 
                          Texture2D tex2, Rectangle rec2) {
    Rectangle collision = GetCollisionRec(rec1, rec2);
    if (collision.width > 0 && collision.height > 0) {
        // Check pixels in collision area
        // This is a simplified example - real implementation would check actual pixels
        return true;
    }
    return false;
}

// Swept collision (for fast moving objects)
typedef struct {
    Rectangle bounds;
    Vector2 velocity;
} MovingRect;

bool SweptRectCollision(MovingRect rect1, MovingRect rect2, float deltaTime, 
                       float *collisionTime) {
    // Calculate relative velocity
    Vector2 relVel = Vector2Subtract(rect1.velocity, rect2.velocity);
    
    // Calculate entry and exit times
    // ... (implementation details)
    
    return false;  // Simplified
}
```

## Particle Systems

### Basic Particle System

```c
typedef struct {
    Vector2 position;
    Vector2 velocity;
    Color color;
    float lifetime;
    float size;
    bool active;
} Particle;

typedef struct {
    Particle *particles;
    int maxParticles;
    Vector2 emitPosition;
    float emitRate;
    float accumulator;
} ParticleSystem;

// Initialize particle system
ParticleSystem InitParticleSystem(int maxParticles) {
    ParticleSystem ps = {0};
    ps.maxParticles = maxParticles;
    ps.particles = (Particle *)MemAlloc(sizeof(Particle) * maxParticles);
    ps.emitRate = 10.0f;  // Particles per second
    
    for (int i = 0; i < maxParticles; i++) {
        ps.particles[i].active = false;
    }
    
    return ps;
}

// Emit particle
void EmitParticle(ParticleSystem *ps) {
    for (int i = 0; i < ps->maxParticles; i++) {
        if (!ps->particles[i].active) {
            Particle *p = &ps->particles[i];
            p->active = true;
            p->position = ps->emitPosition;
            p->velocity = (Vector2){ 
                GetRandomValue(-100, 100) / 100.0f * 100,
                GetRandomValue(-200, -100) / 100.0f * 100 
            };
            p->color = (Color){ 255, GetRandomValue(0, 255), 0, 255 };
            p->lifetime = 2.0f;
            p->size = GetRandomValue(2, 6);
            break;
        }
    }
}

// Update particle system
void UpdateParticleSystem(ParticleSystem *ps, float deltaTime) {
    // Emit new particles
    ps->accumulator += deltaTime;
    float particlesToEmit = ps->emitRate * deltaTime;
    while (ps->accumulator >= 1.0f / ps->emitRate) {
        EmitParticle(ps);
        ps->accumulator -= 1.0f / ps->emitRate;
    }
    
    // Update existing particles
    for (int i = 0; i < ps->maxParticles; i++) {
        if (ps->particles[i].active) {
            Particle *p = &ps->particles[i];
            
            // Update physics
            p->velocity.y += 200 * deltaTime;  // Gravity
            p->position = Vector2Add(p->position, 
                Vector2Scale(p->velocity, deltaTime));
            
            // Update lifetime
            p->lifetime -= deltaTime;
            if (p->lifetime <= 0) {
                p->active = false;
            }
            
            // Fade out
            p->color.a = (unsigned char)(255 * (p->lifetime / 2.0f));
        }
    }
}

// Draw particle system
void DrawParticleSystem(ParticleSystem *ps) {
    for (int i = 0; i < ps->maxParticles; i++) {
        if (ps->particles[i].active) {
            DrawCircleV(ps->particles[i].position, 
                       ps->particles[i].size, 
                       ps->particles[i].color);
        }
    }
}
```

## Advanced Techniques

### Blend Modes

```c
// Set blend mode
BeginBlendMode(BLEND_ADDITIVE);
    DrawCircle(100, 100, 50, Fade(RED, 0.5f));
    DrawCircle(150, 100, 50, Fade(BLUE, 0.5f));
EndBlendMode();

// Available blend modes:
// BLEND_ALPHA (default)
// BLEND_ADDITIVE
// BLEND_MULTIPLIED
// BLEND_ADD_COLORS
// BLEND_SUBTRACT_COLORS
// BLEND_ALPHA_PREMULTIPLY
// BLEND_CUSTOM
// BLEND_CUSTOM_SEPARATE
```

### Scissor Mode (Clipping)

```c
// Draw only within specified rectangle
BeginScissorMode(100, 100, 200, 200);
    // Everything drawn here will be clipped to the rectangle
    DrawCircle(150, 150, 100, RED);  // Circle will be clipped
EndScissorMode();
```

### Post-processing Effects

```c
// Simple blur effect using render texture
RenderTexture2D target = LoadRenderTexture(screenWidth, screenHeight);
Shader blur = LoadShader(0, "shaders/blur.fs");

// Render scene to texture
BeginTextureMode(target);
    ClearBackground(RAYWHITE);
    DrawScene();  // Your normal drawing
EndTextureMode();

// Apply blur shader
BeginDrawing();
    ClearBackground(BLACK);
    
    BeginShaderMode(blur);
        DrawTextureRec(target.texture, 
            (Rectangle){ 0, 0, target.texture.width, -target.texture.height },
            (Vector2){ 0, 0 }, WHITE);
    EndShaderMode();
    
EndDrawing();
```

### Metaballs Effect

```c
// Simple metaballs using distance fields
void DrawMetaballs(Vector2 *positions, int count, float radius) {
    for (int y = 0; y < screenHeight; y += 4) {
        for (int x = 0; x < screenWidth; x += 4) {
            float sum = 0;
            
            for (int i = 0; i < count; i++) {
                float dist = Vector2Distance((Vector2){x, y}, positions[i]);
                sum += radius / (dist * dist);
            }
            
            if (sum >= 1.0f) {
                DrawRectangle(x, y, 4, 4, RED);
            }
        }
    }
}
```

### Trail Effect

```c
// Create trail effect using render texture
RenderTexture2D trail = LoadRenderTexture(screenWidth, screenHeight);

// In update loop:
BeginTextureMode(trail);
    // Fade previous frame
    DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.1f));
    
    // Draw new position
    DrawCircle(mouseX, mouseY, 20, WHITE);
EndTextureMode();

// Draw trail
BeginDrawing();
    ClearBackground(BLACK);
    
    DrawTextureRec(trail.texture,
        (Rectangle){ 0, 0, trail.texture.width, -trail.texture.height },
        (Vector2){ 0, 0 }, WHITE);
        
EndDrawing();
```

### Tilemap Rendering

```c
typedef struct {
    int *data;
    int width;
    int height;
    int tileSize;
    Texture2D tileset;
} Tilemap;

void DrawTilemap(Tilemap *map, Camera2D camera) {
    // Calculate visible area
    Vector2 topLeft = GetScreenToWorld2D((Vector2){0, 0}, camera);
    Vector2 bottomRight = GetScreenToWorld2D(
        (Vector2){GetScreenWidth(), GetScreenHeight()}, camera);
    
    int startX = (int)(topLeft.x / map->tileSize) - 1;
    int startY = (int)(topLeft.y / map->tileSize) - 1;
    int endX = (int)(bottomRight.x / map->tileSize) + 1;
    int endY = (int)(bottomRight.y / map->tileSize) + 1;
    
    // Clamp to map bounds
    startX = Clamp(startX, 0, map->width - 1);
    startY = Clamp(startY, 0, map->height - 1);
    endX = Clamp(endX, 0, map->width - 1);
    endY = Clamp(endY, 0, map->height - 1);
    
    // Draw only visible tiles
    for (int y = startY; y <= endY; y++) {
        for (int x = startX; x <= endX; x++) {
            int tileId = map->data[y * map->width + x];
            if (tileId > 0) {
                Rectangle source = GetTileSourceRec(tileId, map->tileset);
                Rectangle dest = { x * map->tileSize, y * map->tileSize, 
                                  map->tileSize, map->tileSize };
                DrawTexturePro(map->tileset, source, dest, (Vector2){0, 0}, 0, WHITE);
            }
        }
    }
}
```

### Performance Tips

1. **Batch Similar Draw Calls**
   - Group shapes by type and color
   - Use texture atlases for sprites
   - Minimize texture switches

2. **Use Appropriate Functions**
   - `DrawTexturePro()` for complex transforms
   - `DrawTextureRec()` for simple sprite sheets
   - `DrawTexture()` for basic drawing

3. **Optimize Collision Detection**
   - Use spatial partitioning (quadtree, grid)
   - Broad phase before narrow phase
   - Early exit conditions

4. **Manage Memory**
   - Load resources once
   - Unload unused resources
   - Use object pooling for particles

5. **Profile and Measure**
   ```c
   double startTime = GetTime();
   // Your code here
   double elapsed = GetTime() - startTime;
   DrawText(TextFormat("Time: %.3f ms", elapsed * 1000), 10, 10, 20, BLACK);
   ```

Remember: Start simple and optimize only when needed. Raylib is already quite efficient for most 2D games.