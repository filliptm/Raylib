# 3D Graphics in Raylib

This guide covers 3D graphics programming with Raylib, from basic concepts to advanced rendering techniques.

## Table of Contents
1. [3D Fundamentals](#3d-fundamentals)
2. [Camera System](#camera-system)
3. [Basic 3D Shapes](#basic-3d-shapes)
4. [Models and Meshes](#models-and-meshes)
5. [Materials and Textures](#materials-and-textures)
6. [Lighting](#lighting)
7. [3D Collision Detection](#3d-collision-detection)
8. [Billboards and Sprites](#billboards-and-sprites)
9. [Skeletal Animation](#skeletal-animation)
10. [Advanced Techniques](#advanced-techniques)

## 3D Fundamentals

### Coordinate System
- Raylib uses a right-handed coordinate system
- X-axis points right
- Y-axis points up
- Z-axis points towards the viewer (out of the screen)

### Basic 3D Setup
```c
#include "raylib.h"

int main(void)
{
    const int screenWidth = 800;
    const int screenHeight = 450;
    
    InitWindow(screenWidth, screenHeight, "raylib - 3D basics");
    
    // Define the camera
    Camera3D camera = { 0 };
    camera.position = (Vector3){ 10.0f, 10.0f, 10.0f };
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    
    SetTargetFPS(60);
    
    while (!WindowShouldClose())
    {
        BeginDrawing();
            ClearBackground(RAYWHITE);
            
            BeginMode3D(camera);
                DrawGrid(10, 1.0f);
                DrawCube((Vector3){ 0.0f, 1.0f, 0.0f }, 2.0f, 2.0f, 2.0f, RED);
            EndMode3D();
            
            DrawText("Basic 3D Example", 10, 10, 20, DARKGRAY);
            DrawFPS(10, 40);
            
        EndDrawing();
    }
    
    CloseWindow();
    return 0;
}
```

### Vector3 Operations
```c
// Vector3 creation
Vector3 v1 = { 1.0f, 0.0f, 0.0f };
Vector3 v2 = { 0.0f, 1.0f, 0.0f };

// Basic operations
Vector3 sum = Vector3Add(v1, v2);
Vector3 diff = Vector3Subtract(v1, v2);
Vector3 scaled = Vector3Scale(v1, 2.0f);
float length = Vector3Length(v1);
Vector3 normalized = Vector3Normalize(v1);

// Advanced operations
float dot = Vector3DotProduct(v1, v2);
Vector3 cross = Vector3CrossProduct(v1, v2);
float distance = Vector3Distance(v1, v2);
Vector3 lerped = Vector3Lerp(v1, v2, 0.5f);

// Transformations
Matrix transform = MatrixTranslate(1.0f, 2.0f, 3.0f);
Vector3 transformed = Vector3Transform(v1, transform);
```

## Camera System

### Camera Types

```c
// First Person Camera
Camera camera = { 0 };
camera.position = (Vector3){ 0.0f, 10.0f, 10.0f };
camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
camera.fovy = 60.0f;
camera.projection = CAMERA_PERSPECTIVE;

SetCameraMode(camera, CAMERA_FIRST_PERSON);

// Update camera
UpdateCamera(&camera);
```

### Camera Modes
```c
// Available camera modes
SetCameraMode(camera, CAMERA_FREE);         // Free camera
SetCameraMode(camera, CAMERA_ORBITAL);      // Orbital camera
SetCameraMode(camera, CAMERA_FIRST_PERSON); // FPS camera
SetCameraMode(camera, CAMERA_THIRD_PERSON); // Third person

// Custom camera controls
SetCameraMoveControls(KEY_W, KEY_S, KEY_D, KEY_A, KEY_SPACE, KEY_LEFT_CONTROL);
SetCameraAltControl(KEY_LEFT_SHIFT);
SetCameraSmoothZoomControl(KEY_LEFT_CONTROL);
SetCameraPanControl(MOUSE_BUTTON_MIDDLE);
```

### Manual Camera Control
```c
// Custom camera implementation
typedef struct {
    Camera3D camera;
    float moveSpeed;
    float rotSpeed;
    float mouseSpeed;
} FPSCamera;

void UpdateFPSCamera(FPSCamera *cam, float deltaTime) {
    // Keyboard movement
    if (IsKeyDown(KEY_W)) {
        cam->camera.position.x += cam->camera.target.x * cam->moveSpeed * deltaTime;
        cam->camera.position.z += cam->camera.target.z * cam->moveSpeed * deltaTime;
    }
    if (IsKeyDown(KEY_S)) {
        cam->camera.position.x -= cam->camera.target.x * cam->moveSpeed * deltaTime;
        cam->camera.position.z -= cam->camera.target.z * cam->moveSpeed * deltaTime;
    }
    if (IsKeyDown(KEY_A)) {
        Vector3 left = Vector3CrossProduct(cam->camera.target, cam->camera.up);
        left = Vector3Normalize(left);
        cam->camera.position.x -= left.x * cam->moveSpeed * deltaTime;
        cam->camera.position.z -= left.z * cam->moveSpeed * deltaTime;
    }
    if (IsKeyDown(KEY_D)) {
        Vector3 right = Vector3CrossProduct(cam->camera.up, cam->camera.target);
        right = Vector3Normalize(right);
        cam->camera.position.x += right.x * cam->moveSpeed * deltaTime;
        cam->camera.position.z += right.z * cam->moveSpeed * deltaTime;
    }
    
    // Mouse rotation
    Vector2 mouseDelta = GetMouseDelta();
    
    // Horizontal rotation
    Matrix rotation = MatrixRotateY(-mouseDelta.x * cam->mouseSpeed * deltaTime);
    cam->camera.target = Vector3Transform(cam->camera.target, rotation);
    
    // Vertical rotation (with limits)
    float pitch = asinf(cam->camera.target.y);
    pitch -= mouseDelta.y * cam->mouseSpeed * deltaTime;
    pitch = Clamp(pitch, -1.4f, 1.4f);  // ~80 degrees
    
    // Apply pitch
    float horizontalLength = cosf(pitch);
    cam->camera.target.y = sinf(pitch);
    cam->camera.target.x = cam->camera.target.x * horizontalLength;
    cam->camera.target.z = cam->camera.target.z * horizontalLength;
    cam->camera.target = Vector3Normalize(cam->camera.target);
}
```

### Camera Frustum Culling
```c
// Check if object is visible
bool IsObjectInFrustum(Camera camera, BoundingBox box) {
    // Get camera frustum planes
    // This is a simplified version
    Vector3 cameraForward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    
    // Check if bounding box center is in front of camera
    Vector3 boxCenter = {
        box.min.x + (box.max.x - box.min.x) / 2.0f,
        box.min.y + (box.max.y - box.min.y) / 2.0f,
        box.min.z + (box.max.z - box.min.z) / 2.0f
    };
    
    Vector3 toBox = Vector3Subtract(boxCenter, camera.position);
    float distance = Vector3DotProduct(toBox, cameraForward);
    
    return distance > 0;  // Simplified check
}
```

## Basic 3D Shapes

### Lines and Points
```c
// Draw 3D line
DrawLine3D((Vector3){0, 0, 0}, (Vector3){10, 10, 10}, RED);

// Draw 3D point
DrawPoint3D((Vector3){5, 5, 5}, BLUE);

// Draw circle in 3D space
DrawCircle3D((Vector3){0, 0, 0}, 5.0f, (Vector3){1, 0, 0}, 90.0f, GREEN);
```

### Cubes and Boxes
```c
// Draw cube
Vector3 position = { 0.0f, 1.0f, 0.0f };
DrawCube(position, 2.0f, 2.0f, 2.0f, RED);

// Draw cube with vector size
Vector3 size = { 2.0f, 3.0f, 1.0f };
DrawCubeV(position, size, BLUE);

// Draw cube wireframe
DrawCubeWires(position, 2.0f, 2.0f, 2.0f, BLACK);

// Draw textured cube
Texture2D texture = LoadTexture("resources/cubemap.png");
DrawCubeTexture(texture, position, 2.0f, 2.0f, 2.0f, WHITE);

// Draw textured cube with source rectangle
Rectangle source = { 0, 0, texture.width/3, texture.height/4 };
DrawCubeTextureRec(texture, source, position, 2.0f, 2.0f, 2.0f, WHITE);
```

### Spheres
```c
// Draw sphere
Vector3 centerPos = { 0.0f, 0.0f, 0.0f };
DrawSphere(centerPos, 2.0f, RED);

// Draw sphere with detail control
DrawSphereEx(centerPos, 2.0f, 16, 16, GREEN);

// Draw sphere wireframe
DrawSphereWires(centerPos, 2.0f, 16, 16, DARKGREEN);
```

### Cylinders
```c
// Draw cylinder
Vector3 position = { 0.0f, 0.0f, 0.0f };
DrawCylinder(position, 1.0f, 1.0f, 3.0f, 8, RED);

// Draw cylinder with different radii
Vector3 startPos = { 0.0f, 0.0f, 0.0f };
Vector3 endPos = { 0.0f, 3.0f, 0.0f };
DrawCylinderEx(startPos, endPos, 1.0f, 0.5f, 8, BLUE);

// Draw cylinder wireframe
DrawCylinderWires(position, 1.0f, 1.0f, 3.0f, 8, DARKBLUE);
```

### Other Shapes
```c
// Draw plane
Vector3 centerPos = { 0.0f, 0.0f, 0.0f };
Vector2 size = { 10.0f, 10.0f };
DrawPlane(centerPos, size, LIGHTGRAY);

// Draw ray
Ray ray = { { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } };
DrawRay(ray, RED);

// Draw grid
DrawGrid(20, 1.0f);

// Draw bounding box
BoundingBox box = { { -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 1.0f } };
DrawBoundingBox(box, GREEN);
```

## Models and Meshes

### Loading Models
```c
// Load model from file
Model model = LoadModel("resources/model.obj");

// Load model from mesh
Mesh mesh = GenMeshCube(2.0f, 2.0f, 2.0f);
Model modelFromMesh = LoadModelFromMesh(mesh);

// Get model bounding box
BoundingBox bounds = GetModelBoundingBox(model);

// Unload model
UnloadModel(model);
```

### Drawing Models
```c
// Basic model drawing
Vector3 position = { 0.0f, 0.0f, 0.0f };
DrawModel(model, position, 1.0f, WHITE);

// Draw with rotation and scale
Vector3 rotationAxis = { 0.0f, 1.0f, 0.0f };
float rotationAngle = 45.0f;
Vector3 scale = { 2.0f, 2.0f, 2.0f };
DrawModelEx(model, position, rotationAxis, rotationAngle, scale, WHITE);

// Draw wireframe
DrawModelWires(model, position, 1.0f, BLACK);
DrawModelWiresEx(model, position, rotationAxis, rotationAngle, scale, BLACK);
```

### Mesh Generation
```c
// Generate basic meshes
Mesh plane = GenMeshPlane(10.0f, 10.0f, 5, 5);
Mesh cube = GenMeshCube(2.0f, 2.0f, 2.0f);
Mesh sphere = GenMeshSphere(1.0f, 16, 16);
Mesh hemisphere = GenMeshHemiSphere(1.0f, 16, 16);
Mesh cylinder = GenMeshCylinder(1.0f, 2.0f, 16);
Mesh cone = GenMeshCone(1.0f, 2.0f, 16);
Mesh torus = GenMeshTorus(0.5f, 1.0f, 16, 16);
Mesh knot = GenMeshKnot(1.0f, 2.0f, 16, 128);

// Generate from heightmap
Image heightmap = LoadImage("resources/heightmap.png");
Mesh terrain = GenMeshHeightmap(heightmap, (Vector3){16, 8, 16});

// Generate from cubicmap
Image cubicmap = LoadImage("resources/cubicmap.png");
Mesh cubicMesh = GenMeshCubicmap(cubicmap, (Vector3){1.0f, 1.0f, 1.0f});

// Generate mesh tangents (for normal mapping)
GenMeshTangents(&mesh);
```

### Mesh Manipulation
```c
// Upload mesh to GPU
UploadMesh(&mesh, false);  // false = static, true = dynamic

// Update mesh buffer
float vertices[] = { /* new vertex data */ };
UpdateMeshBuffer(mesh, 0, vertices, sizeof(vertices), 0);

// Get mesh bounding box
BoundingBox meshBounds = GetMeshBoundingBox(mesh);

// Export mesh
ExportMesh(mesh, "exported_mesh.obj");

// Draw mesh directly
Material material = LoadMaterialDefault();
Matrix transform = MatrixIdentity();
DrawMesh(mesh, material, transform);

// Instanced rendering
Matrix transforms[100];
for (int i = 0; i < 100; i++) {
    transforms[i] = MatrixTranslate(i * 3.0f, 0, 0);
}
DrawMeshInstanced(mesh, material, transforms, 100);
```

## Materials and Textures

### Materials
```c
// Load default material
Material material = LoadMaterialDefault();

// Load materials from model
Model model = LoadModel("resources/model.obj");
// model.materials is an array of materials

// Material properties
material.shader = shader;  // Custom shader

// Material maps
material.maps[MATERIAL_MAP_ALBEDO].texture = texture;
material.maps[MATERIAL_MAP_ALBEDO].color = WHITE;

// Available material maps:
// MATERIAL_MAP_ALBEDO (MATERIAL_MAP_DIFFUSE)
// MATERIAL_MAP_METALNESS (MATERIAL_MAP_SPECULAR)
// MATERIAL_MAP_NORMAL
// MATERIAL_MAP_ROUGHNESS
// MATERIAL_MAP_OCCLUSION
// MATERIAL_MAP_EMISSION
// MATERIAL_MAP_HEIGHT
// MATERIAL_MAP_CUBEMAP
// MATERIAL_MAP_IRRADIANCE
// MATERIAL_MAP_PREFILTER
// MATERIAL_MAP_BRDF

// Set material texture
SetMaterialTexture(&material, MATERIAL_MAP_ALBEDO, texture);

// Set material shader
SetMaterialShader(&material, shader);
```

### Model Textures
```c
// Load model with textures
Model model = LoadModel("resources/model.obj");
Texture2D texture = LoadTexture("resources/model_diffuse.png");
model.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = texture;

// Set model mesh material
SetModelMeshMaterial(&model, 0, 0);  // mesh 0, material 0
```

### Texture Coordinates
```c
// Generate mesh with custom UVs
Mesh CreateTexturedPlane(float width, float height) {
    Mesh mesh = { 0 };
    mesh.vertexCount = 4;
    mesh.triangleCount = 2;
    
    // Vertices
    float vertices[] = {
        -width/2, 0, -height/2,
         width/2, 0, -height/2,
         width/2, 0,  height/2,
        -width/2, 0,  height/2
    };
    
    // Texture coordinates
    float texcoords[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f
    };
    
    // Normals
    float normals[] = {
        0, 1, 0,
        0, 1, 0,
        0, 1, 0,
        0, 1, 0
    };
    
    // Indices
    unsigned short indices[] = {
        0, 1, 2,
        0, 2, 3
    };
    
    mesh.vertices = (float *)MemAlloc(sizeof(vertices));
    memcpy(mesh.vertices, vertices, sizeof(vertices));
    
    mesh.texcoords = (float *)MemAlloc(sizeof(texcoords));
    memcpy(mesh.texcoords, texcoords, sizeof(texcoords));
    
    mesh.normals = (float *)MemAlloc(sizeof(normals));
    memcpy(mesh.normals, normals, sizeof(normals));
    
    mesh.indices = (unsigned short *)MemAlloc(sizeof(indices));
    memcpy(mesh.indices, indices, sizeof(indices));
    
    UploadMesh(&mesh, false);
    
    return mesh;
}
```

## Lighting

### Basic Lighting with Custom Shader
```c
// Basic lighting shader example
#define GLSL_VERSION 330

// Vertex shader
const char *lightingVS = "#version 330\n"
    "in vec3 vertexPosition;\n"
    "in vec3 vertexNormal;\n"
    "in vec2 vertexTexCoord;\n"
    "uniform mat4 mvp;\n"
    "uniform mat4 matModel;\n"
    "uniform mat4 matNormal;\n"
    "out vec3 fragPosition;\n"
    "out vec3 fragNormal;\n"
    "out vec2 fragTexCoord;\n"
    "void main() {\n"
    "    fragPosition = vec3(matModel * vec4(vertexPosition, 1.0));\n"
    "    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 1.0)));\n"
    "    fragTexCoord = vertexTexCoord;\n"
    "    gl_Position = mvp * vec4(vertexPosition, 1.0);\n"
    "}";

// Fragment shader
const char *lightingFS = "#version 330\n"
    "in vec3 fragPosition;\n"
    "in vec3 fragNormal;\n"
    "in vec2 fragTexCoord;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "uniform vec3 lightPos;\n"
    "uniform vec3 lightColor;\n"
    "uniform vec3 viewPos;\n"
    "out vec4 finalColor;\n"
    "void main() {\n"
    "    vec4 texelColor = texture(texture0, fragTexCoord);\n"
    "    vec3 lightDir = normalize(lightPos - fragPosition);\n"
    "    float diff = max(dot(fragNormal, lightDir), 0.0);\n"
    "    vec3 diffuse = diff * lightColor;\n"
    "    vec3 ambient = 0.1 * lightColor;\n"
    "    vec3 result = (ambient + diffuse) * texelColor.rgb;\n"
    "    finalColor = vec4(result, texelColor.a) * colDiffuse;\n"
    "}";

// Load and use shader
Shader lightingShader = LoadShaderFromMemory(lightingVS, lightingFS);

// Set shader uniforms
Vector3 lightPos = { 5.0f, 5.0f, 5.0f };
SetShaderValue(lightingShader, GetShaderLocation(lightingShader, "lightPos"), 
               &lightPos, SHADER_UNIFORM_VEC3);

Vector3 lightColor = { 1.0f, 1.0f, 1.0f };
SetShaderValue(lightingShader, GetShaderLocation(lightingShader, "lightColor"), 
               &lightColor, SHADER_UNIFORM_VEC3);

// Update view position each frame
SetShaderValue(lightingShader, GetShaderLocation(lightingShader, "viewPos"), 
               &camera.position, SHADER_UNIFORM_VEC3);
```

### Multiple Lights System
```c
// Light structure
typedef struct {
    int enabled;
    int type;  // LIGHT_POINT, LIGHT_DIRECTIONAL, LIGHT_SPOT
    Vector3 position;
    Vector3 target;
    Vector4 color;
    float attenuation;
    
    // Shader locations
    int enabledLoc;
    int typeLoc;
    int positionLoc;
    int targetLoc;
    int colorLoc;
    int attenuationLoc;
} Light;

// Create light
Light CreateLight(int type, Vector3 position, Vector3 target, Color color, 
                  Shader shader, int lightIndex) {
    Light light = { 0 };
    light.enabled = 1;
    light.type = type;
    light.position = position;
    light.target = target;
    light.color = (Vector4){ color.r/255.0f, color.g/255.0f, 
                            color.b/255.0f, color.a/255.0f };
    light.attenuation = 1.0f;
    
    // Get shader locations
    char name[32] = { 0 };
    sprintf(name, "lights[%i].enabled", lightIndex);
    light.enabledLoc = GetShaderLocation(shader, name);
    
    sprintf(name, "lights[%i].type", lightIndex);
    light.typeLoc = GetShaderLocation(shader, name);
    
    sprintf(name, "lights[%i].position", lightIndex);
    light.positionLoc = GetShaderLocation(shader, name);
    
    sprintf(name, "lights[%i].target", lightIndex);
    light.targetLoc = GetShaderLocation(shader, name);
    
    sprintf(name, "lights[%i].color", lightIndex);
    light.colorLoc = GetShaderLocation(shader, name);
    
    sprintf(name, "lights[%i].attenuation", lightIndex);
    light.attenuationLoc = GetShaderLocation(shader, name);
    
    // Update shader
    UpdateLightValues(shader, light);
    
    return light;
}

// Update light values in shader
void UpdateLightValues(Shader shader, Light light) {
    SetShaderValue(shader, light.enabledLoc, &light.enabled, SHADER_UNIFORM_INT);
    SetShaderValue(shader, light.typeLoc, &light.type, SHADER_UNIFORM_INT);
    SetShaderValue(shader, light.positionLoc, &light.position, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, light.targetLoc, &light.target, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, light.colorLoc, &light.color, SHADER_UNIFORM_VEC4);
    SetShaderValue(shader, light.attenuationLoc, &light.attenuation, SHADER_UNIFORM_FLOAT);
}
```

## 3D Collision Detection

### Basic Collisions
```c
// Sphere collision
Vector3 center1 = { 0.0f, 0.0f, 0.0f };
float radius1 = 1.0f;
Vector3 center2 = { 2.0f, 0.0f, 0.0f };
float radius2 = 1.5f;
bool collision = CheckCollisionSpheres(center1, radius1, center2, radius2);

// Box collision
BoundingBox box1 = { { -1, -1, -1 }, { 1, 1, 1 } };
BoundingBox box2 = { { 0, 0, 0 }, { 2, 2, 2 } };
bool boxCollision = CheckCollisionBoxes(box1, box2);

// Box-Sphere collision
bool boxSphereCollision = CheckCollisionBoxSphere(box1, center1, radius1);
```

### Ray Collision
```c
// Create ray from mouse
Ray ray = GetMouseRay(GetMousePosition(), camera);

// Ray-Sphere collision
RayCollision sphereHit = GetRayCollisionSphere(ray, center, radius);
if (sphereHit.hit) {
    DrawSphere(sphereHit.point, 0.1f, RED);
    DrawText(TextFormat("Distance: %.2f", sphereHit.distance), 10, 10, 20, BLACK);
}

// Ray-Box collision
RayCollision boxHit = GetRayCollisionBox(ray, box);

// Ray-Model collision
RayCollision modelHit = GetRayCollisionModel(ray, model);

// Ray-Mesh collision
Matrix transform = MatrixIdentity();
RayCollision meshHit = GetRayCollisionMesh(ray, mesh, transform);

// Ray-Triangle collision
Vector3 p1 = { 0, 0, 0 };
Vector3 p2 = { 1, 0, 0 };
Vector3 p3 = { 0, 1, 0 };
RayCollision triangleHit = GetRayCollisionTriangle(ray, p1, p2, p3);

// Ray-Quad collision
Vector3 p4 = { 1, 1, 0 };
RayCollision quadHit = GetRayCollisionQuad(ray, p1, p2, p3, p4);
```

### Advanced Collision Detection
```c
// Object picking with mouse
typedef struct {
    Model model;
    BoundingBox bounds;
    Vector3 position;
    bool selected;
} GameObject;

GameObject objects[10];

// In update loop
if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    Ray ray = GetMouseRay(GetMousePosition(), camera);
    
    float closestDistance = FLT_MAX;
    int closestObject = -1;
    
    for (int i = 0; i < 10; i++) {
        // Transform bounding box
        BoundingBox worldBounds = {
            Vector3Add(objects[i].bounds.min, objects[i].position),
            Vector3Add(objects[i].bounds.max, objects[i].position)
        };
        
        RayCollision hit = GetRayCollisionBox(ray, worldBounds);
        if (hit.hit && hit.distance < closestDistance) {
            closestDistance = hit.distance;
            closestObject = i;
        }
    }
    
    // Select closest object
    if (closestObject >= 0) {
        objects[closestObject].selected = true;
    }
}
```

## Billboards and Sprites

### Basic Billboards
```c
// Draw billboard (always faces camera)
Texture2D billboard = LoadTexture("resources/billboard.png");
Vector3 position = { 0.0f, 2.0f, 0.0f };
DrawBillboard(camera, billboard, position, 2.0f, WHITE);

// Draw billboard with source rectangle
Rectangle source = { 0, 0, billboard.width/4, billboard.height };
Vector2 size = { 2.0f, 2.0f };
DrawBillboardRec(camera, billboard, source, position, size, WHITE);

// Draw billboard with more control
Vector3 up = { 0.0f, 1.0f, 0.0f };
Vector2 origin = { 0.5f, 0.5f };  // Center
DrawBillboardPro(camera, billboard, source, position, up, size, origin, 0, WHITE);
```

### Billboard System
```c
// Particle billboard system
typedef struct {
    Vector3 position;
    Vector2 size;
    Color color;
    float rotation;
    Rectangle sourceRec;
    bool active;
} BillboardParticle;

#define MAX_PARTICLES 1000
BillboardParticle particles[MAX_PARTICLES];
Texture2D particleTexture;

// Initialize particles
void InitParticles() {
    particleTexture = LoadTexture("resources/particle.png");
    
    for (int i = 0; i < MAX_PARTICLES; i++) {
        particles[i].position = (Vector3){
            GetRandomValue(-10, 10),
            GetRandomValue(0, 10),
            GetRandomValue(-10, 10)
        };
        particles[i].size = (Vector2){ 0.5f, 0.5f };
        particles[i].color = WHITE;
        particles[i].rotation = GetRandomValue(0, 360);
        particles[i].sourceRec = (Rectangle){ 0, 0, 
            particleTexture.width, particleTexture.height };
        particles[i].active = true;
    }
}

// Draw all particles
void DrawParticles(Camera camera) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].active) {
            DrawBillboardPro(camera, particleTexture, 
                particles[i].sourceRec, particles[i].position,
                (Vector3){0, 1, 0}, particles[i].size,
                (Vector2){0.5f, 0.5f}, particles[i].rotation,
                particles[i].color);
        }
    }
}
```

## Skeletal Animation

### Model Animation
```c
// Load animated model
Model model = LoadModel("resources/character.iqm");
Texture2D texture = LoadTexture("resources/character.png");
model.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = texture;

// Load animations
unsigned int animCount = 0;
ModelAnimation *animations = LoadModelAnimations("resources/character.iqm", &animCount);

// Animation state
int currentAnim = 0;
int currentFrame = 0;

// Update animation
void UpdateModelAnimation() {
    currentFrame++;
    if (currentFrame >= animations[currentAnim].frameCount) currentFrame = 0;
    
    UpdateModelAnimation(model, animations[currentAnim], currentFrame);
}

// Draw animated model
DrawModel(model, position, 1.0f, WHITE);

// Change animation
if (IsKeyPressed(KEY_SPACE)) {
    currentAnim = (currentAnim + 1) % animCount;
    currentFrame = 0;
}

// Clean up
for (int i = 0; i < animCount; i++) UnloadModelAnimation(animations[i]);
RL_FREE(animations);
```

### Animation Blending
```c
// Simple animation blending
typedef struct {
    ModelAnimation *animations;
    int animCount;
    int currentAnim;
    int nextAnim;
    float blendFactor;
    int currentFrame;
} AnimationController;

void UpdateAnimationBlend(Model *model, AnimationController *controller, 
                         float deltaTime) {
    if (controller->blendFactor < 1.0f) {
        controller->blendFactor += deltaTime * 2.0f;  // Blend speed
        if (controller->blendFactor > 1.0f) {
            controller->blendFactor = 1.0f;
            controller->currentAnim = controller->nextAnim;
        }
    }
    
    controller->currentFrame++;
    int maxFrame = controller->animations[controller->currentAnim].frameCount;
    if (controller->currentFrame >= maxFrame) controller->currentFrame = 0;
    
    // Apply current animation
    UpdateModelAnimation(*model, 
        controller->animations[controller->currentAnim], 
        controller->currentFrame);
    
    // Blend with next animation if transitioning
    if (controller->blendFactor < 1.0f) {
        int nextFrame = controller->currentFrame % 
            controller->animations[controller->nextAnim].frameCount;
        
        // This is simplified - real blending would interpolate bone transforms
        UpdateModelAnimation(*model, 
            controller->animations[controller->nextAnim], 
            nextFrame);
    }
}

// Trigger animation transition
void ChangeAnimation(AnimationController *controller, int newAnim) {
    if (newAnim != controller->currentAnim) {
        controller->nextAnim = newAnim;
        controller->blendFactor = 0.0f;
    }
}
```

## Advanced Techniques

### Instanced Rendering
```c
// Draw many objects efficiently
#define INSTANCE_COUNT 10000

// Generate transform matrices
Matrix *transforms = (Matrix *)MemAlloc(sizeof(Matrix) * INSTANCE_COUNT);
for (int i = 0; i < INSTANCE_COUNT; i++) {
    float x = (float)(i % 100) * 2.0f;
    float z = (float)(i / 100) * 2.0f;
    transforms[i] = MatrixMultiply(
        MatrixScale(0.5f, 0.5f, 0.5f),
        MatrixTranslate(x, 0, z)
    );
}

// Draw instanced
Mesh cube = GenMeshCube(1.0f, 1.0f, 1.0f);
Material material = LoadMaterialDefault();
DrawMeshInstanced(cube, material, transforms, INSTANCE_COUNT);

MemFree(transforms);
```

### Shadow Mapping (Conceptual)
```c
// Simple shadow mapping setup
typedef struct {
    RenderTexture2D shadowMap;
    Shader shadowShader;
    Matrix lightViewProj;
    Vector3 lightPos;
} ShadowSystem;

ShadowSystem InitShadowSystem(int shadowMapSize) {
    ShadowSystem shadow = { 0 };
    
    // Create shadow map render texture
    shadow.shadowMap = LoadRenderTexture(shadowMapSize, shadowMapSize);
    
    // Load shadow shaders
    shadow.shadowShader = LoadShader("shaders/shadow.vs", "shaders/shadow.fs");
    
    // Setup light camera
    shadow.lightPos = (Vector3){ 10, 10, 10 };
    Matrix lightView = MatrixLookAt(shadow.lightPos, 
        (Vector3){0, 0, 0}, (Vector3){0, 1, 0});
    Matrix lightProj = MatrixOrtho(-10, 10, -10, 10, 0.1f, 100.0f);
    shadow.lightViewProj = MatrixMultiply(lightView, lightProj);
    
    return shadow;
}

// Render shadow map
void RenderShadowMap(ShadowSystem *shadow, Model *models, int modelCount) {
    BeginTextureMode(shadow->shadowMap);
        ClearBackground(WHITE);
        
        BeginShaderMode(shadow->shadowShader);
            SetShaderValueMatrix(shadow->shadowShader, 
                GetShaderLocation(shadow->shadowShader, "lightSpaceMatrix"),
                shadow->lightViewProj);
            
            for (int i = 0; i < modelCount; i++) {
                DrawModel(models[i], (Vector3){0, 0, 0}, 1.0f, WHITE);
            }
        EndShaderMode();
    EndTextureMode();
}
```

### Level of Detail (LOD)
```c
typedef struct {
    Model models[3];  // Different detail levels
    float distances[3];  // Distance thresholds
} LODModel;

void DrawLODModel(LODModel *lod, Vector3 position, Camera camera) {
    float distance = Vector3Distance(camera.position, position);
    
    int lodLevel = 0;
    for (int i = 0; i < 3; i++) {
        if (distance > lod->distances[i]) lodLevel = i;
    }
    
    DrawModel(lod->models[lodLevel], position, 1.0f, WHITE);
}
```

### Post-processing
```c
// Simple post-processing pipeline
typedef struct {
    RenderTexture2D sceneBuffer;
    Shader postShader;
} PostProcessor;

PostProcessor InitPostProcessor(int width, int height) {
    PostProcessor post = { 0 };
    post.sceneBuffer = LoadRenderTexture(width, height);
    post.postShader = LoadShader(0, "shaders/postprocess.fs");
    return post;
}

// Usage
PostProcessor post = InitPostProcessor(screenWidth, screenHeight);

// Render scene to buffer
BeginTextureMode(post.sceneBuffer);
    ClearBackground(SKYBLUE);
    BeginMode3D(camera);
        // Draw 3D scene
    EndMode3D();
EndTextureMode();

// Apply post-processing
BeginDrawing();
    ClearBackground(BLACK);
    
    BeginShaderMode(post.postShader);
        // Set shader parameters
        float time = GetTime();
        SetShaderValue(post.postShader, 
            GetShaderLocation(post.postShader, "time"), 
            &time, SHADER_UNIFORM_FLOAT);
        
        DrawTextureRec(post.sceneBuffer.texture,
            (Rectangle){0, 0, post.sceneBuffer.texture.width, 
                       -post.sceneBuffer.texture.height},
            (Vector2){0, 0}, WHITE);
    EndShaderMode();
EndDrawing();
```

### Performance Tips

1. **Frustum Culling**
   - Don't draw objects outside camera view
   - Use bounding boxes for quick tests

2. **Level of Detail**
   - Use simpler models for distant objects
   - Reduce texture resolution with distance

3. **Batching**
   - Use instanced rendering for repeated objects
   - Group draw calls by material/shader

4. **Optimize Shaders**
   - Minimize texture lookups
   - Pre-calculate values in vertex shader
   - Use simpler shading for distant objects

5. **Memory Management**
   - Load models and textures once
   - Share materials between objects
   - Unload unused resources

6. **Profiling**
   ```c
   double startTime = GetTime();
   // Render code
   double renderTime = GetTime() - startTime;
   DrawText(TextFormat("Render: %.2f ms", renderTime * 1000), 10, 10, 20, BLACK);
   ```

Remember: Start with simple implementations and optimize based on actual performance needs.