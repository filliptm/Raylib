/*******************************************************************************************
*   SQUAD RUNNER - A hybrid 3D/2D mobile-style runner game
*
*   3D world with 2D billboard sprites for characters
*   Built with raylib
********************************************************************************************/

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

//------------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------------
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 900
#define MAX_SQUAD_UNITS 100
#define MAX_ENEMIES 50
#define MAX_BULLETS 200
#define MAX_COLLECTIBLES 20
#define MAX_PARTICLES 500
#define MAX_FLOAT_TEXTS 30
#define ROAD_WIDTH 10.0f
#define ROAD_LENGTH 200.0f
#define SPAWN_DISTANCE 50.0f
#define DESPAWN_DISTANCE 10.0f

//------------------------------------------------------------------------------------
// Types
//------------------------------------------------------------------------------------
typedef struct Unit {
    Vector3 position;
    Vector3 velocity;
    Vector3 targetOffset;
    float shootTimer;
    float bobPhase;         // For walking animation
    float scale;            // For spawn/death animation
    float hitFlash;         // Flash white when hit
    bool active;
    bool dying;             // Death animation in progress
} Unit;

typedef struct Enemy {
    Vector3 position;
    Vector3 velocity;
    int health;
    int maxHealth;
    float shootTimer;
    float bobPhase;
    float scale;
    float hitFlash;
    bool active;
    bool dying;
} Enemy;

typedef struct Bullet {
    Vector3 position;
    Vector3 velocity;
    Vector3 prevPosition;   // For trail effect
    float life;
    bool isPlayerBullet;
    bool active;
} Bullet;

typedef struct Collectible {
    Vector3 position;
    int value;
    bool isMultiplier;
    float rotation;
    float bobOffset;
    float scale;
    float glowPulse;
    bool active;
    bool collected;         // Collection animation
} Collectible;

typedef struct Particle {
    Vector3 position;
    Vector3 velocity;
    Color color;
    float life;
    float maxLife;
    float size;
    int type;               // 0=spark, 1=smoke, 2=muzzle, 3=trail
    bool active;
} Particle;

typedef struct FloatText {
    Vector3 worldPos;
    char text[16];
    Color color;
    float life;
    float yOffset;
    float scale;
    bool active;
} FloatText;

typedef struct CameraState {
    Vector3 targetPosition;
    Vector3 targetLookAt;
    Vector3 currentPosition;
    Vector3 currentLookAt;
    float zoom;
    float targetZoom;
} CameraState;

typedef struct GameState {
    // Player squad
    Unit squad[MAX_SQUAD_UNITS];
    int squadCount;
    Vector3 squadCenter;
    Vector3 squadVelocity;
    float squadSpeed;

    // Enemies
    Enemy enemies[MAX_ENEMIES];
    int enemyCount;

    // Bullets
    Bullet bullets[MAX_BULLETS];

    // Collectibles
    Collectible collectibles[MAX_COLLECTIBLES];

    // Particles
    Particle particles[MAX_PARTICLES];

    // Floating text
    FloatText floatTexts[MAX_FLOAT_TEXTS];

    // Game progress
    float distance;
    int score;
    int displayScore;       // Smoothly animated score
    int combo;
    float comboTimer;
    bool gameOver;
    float gameOverTimer;

    // Spawning
    float nextEnemySpawn;
    float nextCollectibleSpawn;

    // Global time for animations
    float time;
} GameState;

//------------------------------------------------------------------------------------
// Global Variables
//------------------------------------------------------------------------------------
static GameState game;
static Camera3D camera;
static CameraState camState;

// Colors
static Color ROAD_COLOR = { 55, 55, 65, 255 };
static Color ROAD_STRIPE_COLOR = { 90, 90, 100, 255 };
static Color ROAD_LINE_COLOR = { 255, 220, 100, 255 };
static Color PLAYER_BODY_COLOR = { 70, 130, 220, 255 };
static Color PLAYER_DARK_COLOR = { 40, 80, 160, 255 };
static Color ENEMY_BODY_COLOR = { 220, 80, 80, 255 };
static Color ENEMY_DARK_COLOR = { 160, 40, 40, 255 };
static Color BULLET_PLAYER_COLOR = { 255, 230, 100, 255 };
static Color BULLET_ENEMY_COLOR = { 255, 120, 120, 255 };
static Color MUZZLE_COLOR = { 255, 200, 50, 255 };

//------------------------------------------------------------------------------------
// Function Declarations
//------------------------------------------------------------------------------------
void InitGame(void);
void UpdateGame(void);
void DrawGame(void);
void DrawCharacter(Vector3 position, float scale, float bobPhase, float hitFlash, bool isPlayer, bool isDying);
void DrawShadow(Vector3 position, float radius);
void DrawRoad(void);
void SpawnEnemy(Vector3 position, int health);
void SpawnCollectible(Vector3 position, int value, bool isMultiplier);
void SpawnBullet(Vector3 position, Vector3 velocity, bool isPlayer);
void SpawnParticle(Vector3 position, Vector3 velocity, Color color, float life, float size, int type);
void SpawnFloatText(Vector3 position, const char* text, Color color);
void SpawnDeathEffect(Vector3 position, Color color);
void SpawnMuzzleFlash(Vector3 position);
void SpawnImpactEffect(Vector3 position, bool isPlayer);
void AddSquadUnits(int count);
void UpdateSquadFormation(void);
float EaseOutElastic(float t);
float EaseOutBack(float t);
Color ColorLerp(Color a, Color b, float t);

//------------------------------------------------------------------------------------
// Main Entry Point
//------------------------------------------------------------------------------------
int main(void)
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Squad Runner");
    SetTargetFPS(60);

    InitGame();

    while (!WindowShouldClose())
    {
        UpdateGame();

        BeginDrawing();
            ClearBackground((Color){ 25, 28, 40, 255 });
            DrawGame();
        EndDrawing();
    }

    CloseWindow();
    return 0;
}

//------------------------------------------------------------------------------------
// Initialize Game
//------------------------------------------------------------------------------------
void InitGame(void)
{
    // Initialize camera state
    camState = (CameraState){
        .targetPosition = { 0.0f, 14.0f, -10.0f },
        .targetLookAt = { 0.0f, 0.0f, 12.0f },
        .currentPosition = { 0.0f, 14.0f, -10.0f },
        .currentLookAt = { 0.0f, 0.0f, 12.0f },
        .zoom = 1.0f,
        .targetZoom = 1.0f
    };

    camera.position = camState.currentPosition;
    camera.target = camState.currentLookAt;
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 55.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Initialize game state
    game = (GameState){ 0 };
    game.squadCenter = (Vector3){ 0.0f, 0.0f, 0.0f };
    game.squadSpeed = 10.0f;
    game.nextEnemySpawn = 15.0f;
    game.nextCollectibleSpawn = 10.0f;
    game.displayScore = 0;

    // Start with 5 units
    AddSquadUnits(5);
}

//------------------------------------------------------------------------------------
// Easing functions for juicy animations
//------------------------------------------------------------------------------------
float EaseOutElastic(float t)
{
    if (t == 0 || t == 1) return t;
    float p = 0.3f;
    return powf(2, -10 * t) * sinf((t - p/4) * (2 * PI) / p) + 1;
}

float EaseOutBack(float t)
{
    float c1 = 1.70158f;
    float c3 = c1 + 1;
    return 1 + c3 * powf(t - 1, 3) + c1 * powf(t - 1, 2);
}

Color ColorLerp(Color a, Color b, float t)
{
    return (Color){
        (unsigned char)(a.r + (b.r - a.r) * t),
        (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t),
        (unsigned char)(a.a + (b.a - a.a) * t)
    };
}

//------------------------------------------------------------------------------------
// Add units to squad with spawn animation
//------------------------------------------------------------------------------------
void AddSquadUnits(int count)
{
    int added = 0;
    for (int i = 0; i < MAX_SQUAD_UNITS && added < count; i++)
    {
        if (!game.squad[i].active && !game.squad[i].dying)
        {
            game.squad[i].active = true;
            game.squad[i].dying = false;
            game.squad[i].scale = 0.0f;  // Start small for pop-in animation
            game.squad[i].shootTimer = GetRandomValue(0, 100) / 100.0f;
            game.squad[i].bobPhase = GetRandomValue(0, 628) / 100.0f;  // Random phase
            game.squad[i].hitFlash = 0.0f;
            game.squad[i].position = game.squadCenter;
            game.squadCount++;
            added++;
        }
    }
    UpdateSquadFormation();
}

//------------------------------------------------------------------------------------
// Update squad formation
//------------------------------------------------------------------------------------
void UpdateSquadFormation(void)
{
    int count = game.squadCount;
    if (count == 0) return;

    int ring = 0;
    int ringCount = 1;
    int ringIdx = 0;

    for (int i = 0; i < MAX_SQUAD_UNITS; i++)
    {
        if (!game.squad[i].active) continue;

        if (ring == 0)
        {
            game.squad[i].targetOffset = (Vector3){ 0, 0, 0 };
        }
        else
        {
            float angle = (ringIdx / (float)ringCount) * PI * 2.0f + ring * 0.5f;
            float radius = ring * 0.7f;
            game.squad[i].targetOffset = (Vector3){
                cosf(angle) * radius,
                0,
                sinf(angle) * radius * 0.4f
            };
        }

        ringIdx++;
        if (ringIdx >= ringCount)
        {
            ring++;
            ringCount = ring * 6;
            ringIdx = 0;
        }
    }
}

//------------------------------------------------------------------------------------
// Particle spawning
//------------------------------------------------------------------------------------
void SpawnParticle(Vector3 position, Vector3 velocity, Color color, float life, float size, int type)
{
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        if (!game.particles[i].active)
        {
            game.particles[i].position = position;
            game.particles[i].velocity = velocity;
            game.particles[i].color = color;
            game.particles[i].life = life;
            game.particles[i].maxLife = life;
            game.particles[i].size = size;
            game.particles[i].type = type;
            game.particles[i].active = true;
            return;
        }
    }
}

void SpawnMuzzleFlash(Vector3 position)
{
    for (int i = 0; i < 5; i++)
    {
        Vector3 vel = {
            GetRandomValue(-100, 100) / 100.0f,
            GetRandomValue(0, 200) / 100.0f,
            GetRandomValue(-50, 150) / 100.0f
        };
        SpawnParticle(position, vel, MUZZLE_COLOR, 0.15f, 0.15f, 2);
    }
}

void SpawnImpactEffect(Vector3 position, bool isPlayer)
{
    Color color = isPlayer ? BULLET_PLAYER_COLOR : BULLET_ENEMY_COLOR;
    for (int i = 0; i < 8; i++)
    {
        Vector3 vel = {
            GetRandomValue(-300, 300) / 100.0f,
            GetRandomValue(100, 400) / 100.0f,
            GetRandomValue(-300, 300) / 100.0f
        };
        SpawnParticle(position, vel, color, 0.3f, 0.1f, 0);
    }
}

void SpawnDeathEffect(Vector3 position, Color color)
{
    // Burst of particles
    for (int i = 0; i < 15; i++)
    {
        Vector3 vel = {
            GetRandomValue(-400, 400) / 100.0f,
            GetRandomValue(200, 600) / 100.0f,
            GetRandomValue(-400, 400) / 100.0f
        };
        Color c = ColorLerp(color, WHITE, GetRandomValue(0, 50) / 100.0f);
        SpawnParticle(position, vel, c, 0.5f + GetRandomValue(0, 30) / 100.0f, 0.15f, 0);
    }
}

//------------------------------------------------------------------------------------
// Floating text
//------------------------------------------------------------------------------------
void SpawnFloatText(Vector3 position, const char* text, Color color)
{
    for (int i = 0; i < MAX_FLOAT_TEXTS; i++)
    {
        if (!game.floatTexts[i].active)
        {
            game.floatTexts[i].worldPos = position;
            snprintf(game.floatTexts[i].text, 16, "%s", text);
            game.floatTexts[i].color = color;
            game.floatTexts[i].life = 1.0f;
            game.floatTexts[i].yOffset = 0.0f;
            game.floatTexts[i].scale = 0.0f;
            game.floatTexts[i].active = true;
            return;
        }
    }
}

//------------------------------------------------------------------------------------
// Spawn functions
//------------------------------------------------------------------------------------
void SpawnEnemy(Vector3 position, int health)
{
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        if (!game.enemies[i].active && !game.enemies[i].dying)
        {
            game.enemies[i].position = position;
            game.enemies[i].velocity = (Vector3){ 0, 0, 0 };
            game.enemies[i].health = health;
            game.enemies[i].maxHealth = health;
            game.enemies[i].shootTimer = GetRandomValue(80, 200) / 100.0f;
            game.enemies[i].bobPhase = GetRandomValue(0, 628) / 100.0f;
            game.enemies[i].scale = 0.0f;  // Pop-in animation
            game.enemies[i].hitFlash = 0.0f;
            game.enemies[i].active = true;
            game.enemies[i].dying = false;
            game.enemyCount++;
            return;
        }
    }
}

void SpawnCollectible(Vector3 position, int value, bool isMultiplier)
{
    for (int i = 0; i < MAX_COLLECTIBLES; i++)
    {
        if (!game.collectibles[i].active)
        {
            game.collectibles[i].position = position;
            game.collectibles[i].value = value;
            game.collectibles[i].isMultiplier = isMultiplier;
            game.collectibles[i].rotation = 0.0f;
            game.collectibles[i].bobOffset = GetRandomValue(0, 628) / 100.0f;
            game.collectibles[i].scale = 0.0f;
            game.collectibles[i].glowPulse = 0.0f;
            game.collectibles[i].active = true;
            game.collectibles[i].collected = false;
            return;
        }
    }
}

void SpawnBullet(Vector3 position, Vector3 velocity, bool isPlayer)
{
    for (int i = 0; i < MAX_BULLETS; i++)
    {
        if (!game.bullets[i].active)
        {
            game.bullets[i].position = position;
            game.bullets[i].prevPosition = position;
            game.bullets[i].velocity = velocity;
            game.bullets[i].life = 3.0f;
            game.bullets[i].isPlayerBullet = isPlayer;
            game.bullets[i].active = true;

            // Spawn muzzle flash
            SpawnMuzzleFlash(position);
            return;
        }
    }
}

//------------------------------------------------------------------------------------
// Update Game
//------------------------------------------------------------------------------------
void UpdateGame(void)
{
    float dt = GetFrameTime();
    game.time += dt;

    // Update display score (smooth counting)
    if (game.displayScore < game.score)
    {
        int diff = game.score - game.displayScore;
        game.displayScore += (diff > 10) ? diff / 5 : 1;
        if (game.displayScore > game.score) game.displayScore = game.score;
    }

    // Update combo timer
    if (game.combo > 0)
    {
        game.comboTimer -= dt;
        if (game.comboTimer <= 0)
        {
            game.combo = 0;
        }
    }

    // Game over state
    if (game.gameOver)
    {
        game.gameOverTimer += dt;
        if (IsKeyPressed(KEY_R) && game.gameOverTimer > 0.5f) InitGame();

        // Still update particles and floating text
        goto updateEffects;
    }

    // Move squad forward
    game.squadCenter.z += game.squadSpeed * dt;
    game.distance += game.squadSpeed * dt;

    // Player horizontal movement with smoothing
    float targetX = game.squadCenter.x;
    float moveSpeed = 8.0f;

    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) targetX -= moveSpeed * dt;
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) targetX += moveSpeed * dt;

    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
    {
        float mouseX = GetMouseX();
        float mouseTargetX = (mouseX - SCREEN_WIDTH / 2.0f) / (SCREEN_WIDTH / 2.0f) * (ROAD_WIDTH / 2.0f - 1.5f);
        targetX = Lerp(game.squadCenter.x, mouseTargetX, 5.0f * dt);
    }

    targetX = Clamp(targetX, -ROAD_WIDTH/2 + 2.0f, ROAD_WIDTH/2 - 2.0f);
    game.squadVelocity.x = (targetX - game.squadCenter.x) / dt;
    game.squadCenter.x = targetX;

    // Update camera with smooth follow
    camState.targetPosition = (Vector3){
        game.squadCenter.x * 0.4f,
        14.0f,
        game.squadCenter.z - 10.0f
    };
    camState.targetLookAt = (Vector3){
        game.squadCenter.x * 0.6f,
        0.0f,
        game.squadCenter.z + 12.0f
    };

    // Smooth camera movement
    float camSmooth = 4.0f * dt;
    camState.currentPosition = Vector3Lerp(camState.currentPosition, camState.targetPosition, camSmooth);
    camState.currentLookAt = Vector3Lerp(camState.currentLookAt, camState.targetLookAt, camSmooth);

    camera.position = camState.currentPosition;
    camera.target = camState.currentLookAt;

    // Dynamic zoom based on squad size
    camState.targetZoom = 1.0f + (game.squadCount - 5) * 0.01f;
    camState.targetZoom = Clamp(camState.targetZoom, 0.9f, 1.3f);
    camState.zoom = Lerp(camState.zoom, camState.targetZoom, 2.0f * dt);
    camera.fovy = 55.0f / camState.zoom;

    // Update squad units
    for (int i = 0; i < MAX_SQUAD_UNITS; i++)
    {
        if (!game.squad[i].active && !game.squad[i].dying) continue;

        // Death animation
        if (game.squad[i].dying)
        {
            game.squad[i].scale -= dt * 4.0f;
            if (game.squad[i].scale <= 0)
            {
                game.squad[i].dying = false;
                game.squad[i].scale = 0;
            }
            continue;
        }

        // Spawn animation (pop in with overshoot)
        if (game.squad[i].scale < 1.0f)
        {
            game.squad[i].scale += dt * 5.0f;
            if (game.squad[i].scale > 1.0f) game.squad[i].scale = 1.0f;
        }

        // Hit flash decay
        if (game.squad[i].hitFlash > 0)
        {
            game.squad[i].hitFlash -= dt * 5.0f;
            if (game.squad[i].hitFlash < 0) game.squad[i].hitFlash = 0;
        }

        // Walking bob animation
        game.squad[i].bobPhase += dt * 12.0f;

        // Move toward target position with slight lag
        Vector3 target = Vector3Add(game.squadCenter, game.squad[i].targetOffset);
        game.squad[i].position = Vector3Lerp(game.squad[i].position, target, 8.0f * dt);

        // Shooting
        game.squad[i].shootTimer -= dt;
        if (game.squad[i].shootTimer <= 0)
        {
            game.squad[i].shootTimer = 0.2f + GetRandomValue(0, 15) / 100.0f;

            // Find nearest enemy
            float nearestDist = 999.0f;
            int nearestEnemy = -1;
            for (int e = 0; e < MAX_ENEMIES; e++)
            {
                if (!game.enemies[e].active || game.enemies[e].dying) continue;
                float dist = game.enemies[e].position.z - game.squad[i].position.z;
                if (dist > 0 && dist < nearestDist && dist < 25.0f)
                {
                    nearestDist = dist;
                    nearestEnemy = e;
                }
            }

            if (nearestEnemy >= 0)
            {
                Vector3 dir = Vector3Normalize(Vector3Subtract(
                    game.enemies[nearestEnemy].position,
                    game.squad[i].position
                ));
                // Add slight inaccuracy for visual interest
                dir.x += GetRandomValue(-5, 5) / 100.0f;
                Vector3 vel = Vector3Scale(dir, 30.0f);
                Vector3 spawnPos = game.squad[i].position;
                spawnPos.y = 0.6f;
                spawnPos.z += 0.3f;
                SpawnBullet(spawnPos, vel, true);
            }
        }
    }

    // Update enemies
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        if (!game.enemies[i].active && !game.enemies[i].dying) continue;

        // Death animation
        if (game.enemies[i].dying)
        {
            game.enemies[i].scale -= dt * 5.0f;
            if (game.enemies[i].scale <= 0)
            {
                game.enemies[i].dying = false;
                game.enemies[i].active = false;
                game.enemies[i].scale = 0;
            }
            continue;
        }

        // Spawn animation
        if (game.enemies[i].scale < 1.0f)
        {
            game.enemies[i].scale += dt * 4.0f;
            if (game.enemies[i].scale > 1.0f) game.enemies[i].scale = 1.0f;
        }

        // Hit flash decay
        if (game.enemies[i].hitFlash > 0)
        {
            game.enemies[i].hitFlash -= dt * 8.0f;
            if (game.enemies[i].hitFlash < 0) game.enemies[i].hitFlash = 0;
        }

        // Bob animation
        game.enemies[i].bobPhase += dt * 8.0f;

        // Despawn if passed
        if (game.enemies[i].position.z < game.squadCenter.z - DESPAWN_DISTANCE)
        {
            game.enemies[i].active = false;
            game.enemyCount--;
            continue;
        }

        // Enemy shooting
        game.enemies[i].shootTimer -= dt;
        if (game.enemies[i].shootTimer <= 0 && game.enemies[i].scale >= 1.0f)
        {
            game.enemies[i].shootTimer = 1.2f + GetRandomValue(0, 80) / 100.0f;

            Vector3 dir = Vector3Normalize(Vector3Subtract(game.squadCenter, game.enemies[i].position));
            dir.x += GetRandomValue(-10, 10) / 100.0f;
            Vector3 vel = Vector3Scale(dir, 18.0f);
            Vector3 spawnPos = game.enemies[i].position;
            spawnPos.y = 0.6f;
            SpawnBullet(spawnPos, vel, false);
        }
    }

    // Update bullets
    for (int i = 0; i < MAX_BULLETS; i++)
    {
        if (!game.bullets[i].active) continue;

        game.bullets[i].prevPosition = game.bullets[i].position;
        game.bullets[i].position = Vector3Add(
            game.bullets[i].position,
            Vector3Scale(game.bullets[i].velocity, dt)
        );
        game.bullets[i].life -= dt;

        // Spawn trail particles occasionally
        if (GetRandomValue(0, 100) < 30)
        {
            Color trailColor = game.bullets[i].isPlayerBullet ?
                (Color){ 255, 200, 50, 150 } : (Color){ 255, 100, 100, 150 };
            SpawnParticle(game.bullets[i].position, (Vector3){0, 0, 0}, trailColor, 0.15f, 0.08f, 3);
        }

        // Check bounds / life
        if (game.bullets[i].life <= 0 ||
            game.bullets[i].position.z < game.squadCenter.z - 15.0f ||
            game.bullets[i].position.z > game.squadCenter.z + 60.0f)
        {
            game.bullets[i].active = false;
            continue;
        }

        // Player bullets hit enemies
        if (game.bullets[i].isPlayerBullet)
        {
            for (int e = 0; e < MAX_ENEMIES; e++)
            {
                if (!game.enemies[e].active || game.enemies[e].dying) continue;

                float dist = Vector3Distance(game.bullets[i].position, game.enemies[e].position);
                if (dist < 0.9f)
                {
                    game.enemies[e].health--;
                    game.enemies[e].hitFlash = 1.0f;
                    game.bullets[i].active = false;

                    SpawnImpactEffect(game.bullets[i].position, true);

                    if (game.enemies[e].health <= 0)
                    {
                        game.enemies[e].dying = true;
                        game.enemyCount--;

                        // Score with combo
                        game.combo++;
                        game.comboTimer = 2.0f;
                        int points = 10 * game.combo;
                        game.score += points;

                        SpawnDeathEffect(game.enemies[e].position, ENEMY_BODY_COLOR);
                        SpawnFloatText(game.enemies[e].position,
                            TextFormat("+%d", points),
                            game.combo > 1 ? GOLD : WHITE);
                    }
                    break;
                }
            }
        }
        // Enemy bullets hit squad
        else
        {
            for (int s = 0; s < MAX_SQUAD_UNITS; s++)
            {
                if (!game.squad[s].active || game.squad[s].dying) continue;

                float dist = Vector3Distance(game.bullets[i].position, game.squad[s].position);
                if (dist < 0.7f)
                {
                    game.squad[s].dying = true;
                    game.squad[s].active = false;
                    game.squadCount--;
                    game.bullets[i].active = false;

                    SpawnDeathEffect(game.squad[s].position, PLAYER_BODY_COLOR);
                    SpawnImpactEffect(game.bullets[i].position, false);
                    UpdateSquadFormation();

                    if (game.squadCount <= 0)
                    {
                        game.gameOver = true;
                        game.gameOverTimer = 0;
                    }
                    break;
                }
            }
        }
    }

    // Update collectibles
    for (int i = 0; i < MAX_COLLECTIBLES; i++)
    {
        if (!game.collectibles[i].active) continue;

        // Animations
        game.collectibles[i].rotation += dt * 2.0f;
        game.collectibles[i].glowPulse += dt * 4.0f;

        // Spawn scale animation
        if (!game.collectibles[i].collected && game.collectibles[i].scale < 1.0f)
        {
            game.collectibles[i].scale += dt * 3.0f;
            if (game.collectibles[i].scale > 1.0f) game.collectibles[i].scale = 1.0f;
        }

        // Collection animation
        if (game.collectibles[i].collected)
        {
            game.collectibles[i].scale += dt * 8.0f;
            if (game.collectibles[i].scale > 2.0f)
            {
                game.collectibles[i].active = false;
            }
            continue;
        }

        // Despawn if passed
        if (game.collectibles[i].position.z < game.squadCenter.z - 3.0f)
        {
            game.collectibles[i].active = false;
            continue;
        }

        // Check collision with squad
        float dist = fabsf(game.collectibles[i].position.x - game.squadCenter.x);
        float distZ = game.collectibles[i].position.z - game.squadCenter.z;

        if (dist < 1.8f && distZ > -1.0f && distZ < 3.0f)
        {
            if (game.collectibles[i].isMultiplier)
            {
                int newCount = game.squadCount * game.collectibles[i].value;
                AddSquadUnits(newCount - game.squadCount);
            }
            else
            {
                AddSquadUnits(game.collectibles[i].value);
            }

            game.collectibles[i].collected = true;

            // Spawn floating text
            const char* text = game.collectibles[i].isMultiplier ?
                TextFormat("x%d!", game.collectibles[i].value) :
                TextFormat("+%d", game.collectibles[i].value);
            SpawnFloatText(game.collectibles[i].position, text,
                game.collectibles[i].isMultiplier ? GOLD : GREEN);
        }
    }

    // Spawn enemies
    if (game.distance >= game.nextEnemySpawn)
    {
        int groupSize = GetRandomValue(3, 6);
        float spawnZ = game.squadCenter.z + SPAWN_DISTANCE;
        float spawnX = GetRandomValue(-25, 25) / 10.0f;

        for (int i = 0; i < groupSize; i++)
        {
            float offsetX = GetRandomValue(-15, 15) / 10.0f;
            float offsetZ = i * 1.2f + GetRandomValue(0, 5) / 10.0f;
            SpawnEnemy(
                (Vector3){ spawnX + offsetX, 0, spawnZ + offsetZ },
                2 + (int)(game.distance / 100.0f)  // Enemies get tougher
            );
        }
        game.nextEnemySpawn = game.distance + GetRandomValue(12, 25);
    }

    // Spawn collectibles
    if (game.distance >= game.nextCollectibleSpawn)
    {
        float spawnZ = game.squadCenter.z + SPAWN_DISTANCE;

        bool isMulti = GetRandomValue(0, 100) < 25;
        int val = isMulti ? 2 : GetRandomValue(5, 12);
        SpawnCollectible((Vector3){ -2.5f, 0, spawnZ }, val, isMulti);

        isMulti = GetRandomValue(0, 100) < 25;
        val = isMulti ? 2 : GetRandomValue(3, 8);
        SpawnCollectible((Vector3){ 2.5f, 0, spawnZ }, val, isMulti);

        game.nextCollectibleSpawn = game.distance + GetRandomValue(18, 35);
    }

updateEffects:
    // Update particles
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        if (!game.particles[i].active) continue;

        game.particles[i].life -= dt;
        if (game.particles[i].life <= 0)
        {
            game.particles[i].active = false;
            continue;
        }

        // Apply gravity to some particle types
        if (game.particles[i].type == 0)  // Sparks
        {
            game.particles[i].velocity.y -= 15.0f * dt;
        }

        game.particles[i].position = Vector3Add(
            game.particles[i].position,
            Vector3Scale(game.particles[i].velocity, dt)
        );
    }

    // Update floating texts
    for (int i = 0; i < MAX_FLOAT_TEXTS; i++)
    {
        if (!game.floatTexts[i].active) continue;

        game.floatTexts[i].life -= dt;
        if (game.floatTexts[i].life <= 0)
        {
            game.floatTexts[i].active = false;
            continue;
        }

        game.floatTexts[i].yOffset += dt * 3.0f;

        // Scale animation (pop in then fade)
        float lifeRatio = game.floatTexts[i].life;
        if (lifeRatio > 0.8f)
        {
            game.floatTexts[i].scale = EaseOutBack((1.0f - lifeRatio) / 0.2f);
        }
        else
        {
            game.floatTexts[i].scale = 1.0f;
        }
    }
}

//------------------------------------------------------------------------------------
// Draw shadow under character
//------------------------------------------------------------------------------------
void DrawShadow(Vector3 position, float radius)
{
    rlDisableDepthMask();
    DrawCylinder(
        (Vector3){ position.x, 0.01f, position.z },
        radius, radius, 0.01f, 16,
        (Color){ 0, 0, 0, 60 }
    );
    rlEnableDepthMask();
}

//------------------------------------------------------------------------------------
// Draw character (soldier billboard with body shape)
//------------------------------------------------------------------------------------
void DrawCharacter(Vector3 position, float scale, float bobPhase, float hitFlash, bool isPlayer, bool isDying)
{
    if (scale <= 0.01f) return;

    Color bodyColor = isPlayer ? PLAYER_BODY_COLOR : ENEMY_BODY_COLOR;
    Color darkColor = isPlayer ? PLAYER_DARK_COLOR : ENEMY_DARK_COLOR;
    Color helmetColor = isPlayer ? BLUE : RED;

    // Flash white when hit
    if (hitFlash > 0)
    {
        bodyColor = ColorLerp(bodyColor, WHITE, hitFlash);
        darkColor = ColorLerp(darkColor, WHITE, hitFlash);
        helmetColor = ColorLerp(helmetColor, WHITE, hitFlash);
    }

    // Apply scale
    float s = scale * (isDying ? EaseOutBack(scale) : 1.0f);

    // Bob offset for walking
    float bob = sinf(bobPhase) * 0.05f * s;
    float lean = sinf(bobPhase * 0.5f) * 0.02f;

    Vector3 pos = position;
    pos.y += bob;

    // Shadow
    DrawShadow(position, 0.35f * s);

    // Body (simple capsule shape using cylinders and spheres)
    Vector3 bodyPos = { pos.x + lean, 0.4f * s + pos.y, pos.z };
    DrawCapsule(
        (Vector3){ bodyPos.x, 0.25f * s, bodyPos.z },
        (Vector3){ bodyPos.x, 0.6f * s, bodyPos.z },
        0.25f * s, 8, 4, bodyColor
    );

    // Head
    Vector3 headPos = { pos.x, 0.85f * s + pos.y + bob * 0.5f, pos.z };
    DrawSphere(headPos, 0.2f * s, bodyColor);

    // Helmet
    Vector3 helmetPos = { pos.x, 0.95f * s + pos.y + bob * 0.5f, pos.z };
    DrawSphere(helmetPos, 0.18f * s, helmetColor);

    // Gun (small rectangle pointing forward)
    if (!isDying && scale > 0.5f)
    {
        Vector3 gunPos = { pos.x + 0.15f * s, 0.45f * s, pos.z + 0.2f * s };
        DrawCube(gunPos, 0.08f * s, 0.08f * s, 0.3f * s, darkColor);
    }
}

//------------------------------------------------------------------------------------
// Draw the road with lanes
//------------------------------------------------------------------------------------
void DrawRoad(void)
{
    float roadStart = game.squadCenter.z - 15.0f;
    float roadEnd = game.squadCenter.z + SPAWN_DISTANCE + 30.0f;
    float roadCenter = (roadStart + roadEnd) / 2.0f;
    float roadLen = roadEnd - roadStart;

    // Main road surface
    DrawPlane(
        (Vector3){ 0, -0.05f, roadCenter },
        (Vector2){ ROAD_WIDTH, roadLen },
        ROAD_COLOR
    );

    // Side stripes
    float stripeWidth = 0.3f;
    DrawCube((Vector3){ -ROAD_WIDTH/2 + stripeWidth/2, 0.0f, roadCenter },
             stripeWidth, 0.02f, roadLen, ROAD_STRIPE_COLOR);
    DrawCube((Vector3){  ROAD_WIDTH/2 - stripeWidth/2, 0.0f, roadCenter },
             stripeWidth, 0.02f, roadLen, ROAD_STRIPE_COLOR);

    // Dashed center lines (two lanes)
    float dashLen = 2.5f;
    float gapLen = 1.5f;

    for (float z = roadStart; z < roadEnd; z += dashLen + gapLen)
    {
        // Left lane divider
        DrawCube((Vector3){ -ROAD_WIDTH/4, 0.01f, z + dashLen/2 },
                 0.12f, 0.02f, dashLen, ROAD_LINE_COLOR);
        // Right lane divider
        DrawCube((Vector3){  ROAD_WIDTH/4, 0.01f, z + dashLen/2 },
                 0.12f, 0.02f, dashLen, ROAD_LINE_COLOR);
    }

    // Edge barriers (simple blocks)
    Color barrierColor = { 80, 80, 90, 255 };
    for (float z = roadStart; z < roadEnd; z += 4.0f)
    {
        DrawCube((Vector3){ -ROAD_WIDTH/2 - 0.4f, 0.3f, z }, 0.5f, 0.6f, 1.5f, barrierColor);
        DrawCube((Vector3){  ROAD_WIDTH/2 + 0.4f, 0.3f, z }, 0.5f, 0.6f, 1.5f, barrierColor);
    }

    // Fog effect in distance (gradient planes)
    Color fogColor = { 25, 28, 40, 200 };
    for (int i = 0; i < 5; i++)
    {
        float fogZ = game.squadCenter.z + SPAWN_DISTANCE + i * 5.0f;
        fogColor.a = 50 + i * 30;
        DrawPlane((Vector3){ 0, 1.0f, fogZ }, (Vector2){ ROAD_WIDTH + 2, 0.1f }, fogColor);
    }
}

//------------------------------------------------------------------------------------
// Draw collectible (barrel/gate)
//------------------------------------------------------------------------------------
void DrawCollectible(Collectible* col)
{
    if (!col->active) return;

    float s = col->scale;
    if (col->collected) s = 2.0f - col->scale;  // Shrink when collected

    Vector3 pos = col->position;
    float bob = sinf(game.time * 3.0f + col->bobOffset) * 0.1f;
    pos.y = 0.75f * s + bob;

    // Glow effect
    float glow = (sinf(col->glowPulse) + 1.0f) * 0.5f;

    // Base color based on type
    Color baseColor = col->isMultiplier ? GOLD : GREEN;
    Color glowColor = ColorLerp(baseColor, WHITE, glow * 0.3f);

    // Draw barrel
    DrawCylinder(pos, 0.7f * s, 0.7f * s, 1.5f * s, 12, glowColor);
    DrawCylinderWires(pos, 0.72f * s, 0.72f * s, 1.52f * s, 12, WHITE);

    // Draw stripes on barrel
    Color stripeColor = col->isMultiplier ? ORANGE : DARKGREEN;
    DrawCylinder((Vector3){ pos.x, pos.y - 0.4f * s, pos.z }, 0.72f * s, 0.72f * s, 0.15f * s, 12, stripeColor);
    DrawCylinder((Vector3){ pos.x, pos.y + 0.4f * s, pos.z }, 0.72f * s, 0.72f * s, 0.15f * s, 12, stripeColor);

    // Shadow
    DrawShadow(col->position, 0.6f * s);
}

//------------------------------------------------------------------------------------
// Draw Game
//------------------------------------------------------------------------------------
void DrawGame(void)
{
    BeginMode3D(camera);

        // Draw road
        DrawRoad();

        // Draw collectibles
        for (int i = 0; i < MAX_COLLECTIBLES; i++)
        {
            if (game.collectibles[i].active)
            {
                DrawCollectible(&game.collectibles[i]);
            }
        }

        // Draw enemies
        for (int i = 0; i < MAX_ENEMIES; i++)
        {
            if (game.enemies[i].active || game.enemies[i].dying)
            {
                DrawCharacter(
                    game.enemies[i].position,
                    game.enemies[i].scale,
                    game.enemies[i].bobPhase,
                    game.enemies[i].hitFlash,
                    false,
                    game.enemies[i].dying
                );
            }
        }

        // Draw player squad
        for (int i = 0; i < MAX_SQUAD_UNITS; i++)
        {
            if (game.squad[i].active || game.squad[i].dying)
            {
                DrawCharacter(
                    game.squad[i].position,
                    game.squad[i].scale,
                    game.squad[i].bobPhase,
                    game.squad[i].hitFlash,
                    true,
                    game.squad[i].dying
                );
            }
        }

        // Draw bullets with trails
        for (int i = 0; i < MAX_BULLETS; i++)
        {
            if (!game.bullets[i].active) continue;

            Color bulletColor = game.bullets[i].isPlayerBullet ? BULLET_PLAYER_COLOR : BULLET_ENEMY_COLOR;

            // Trail
            DrawLine3D(game.bullets[i].prevPosition, game.bullets[i].position, bulletColor);

            // Bullet glow
            DrawSphere(game.bullets[i].position, 0.12f, bulletColor);
            DrawSphere(game.bullets[i].position, 0.08f, WHITE);
        }

        // Draw particles
        for (int i = 0; i < MAX_PARTICLES; i++)
        {
            if (!game.particles[i].active) continue;

            float lifeRatio = game.particles[i].life / game.particles[i].maxLife;
            Color c = game.particles[i].color;
            c.a = (unsigned char)(c.a * lifeRatio);

            float size = game.particles[i].size * (0.5f + lifeRatio * 0.5f);

            DrawSphere(game.particles[i].position, size, c);
        }

    EndMode3D();

    // Draw floating texts (screen space)
    for (int i = 0; i < MAX_FLOAT_TEXTS; i++)
    {
        if (!game.floatTexts[i].active) continue;

        Vector3 worldPos = game.floatTexts[i].worldPos;
        worldPos.y += game.floatTexts[i].yOffset + 1.5f;
        Vector2 screenPos = GetWorldToScreen(worldPos, camera);

        if (screenPos.x > 0 && screenPos.x < SCREEN_WIDTH &&
            screenPos.y > 0 && screenPos.y < SCREEN_HEIGHT)
        {
            float s = game.floatTexts[i].scale;
            int fontSize = (int)(32 * s);
            Color c = game.floatTexts[i].color;
            c.a = (unsigned char)(255 * game.floatTexts[i].life);

            int textWidth = MeasureText(game.floatTexts[i].text, fontSize);
            DrawText(game.floatTexts[i].text,
                     (int)(screenPos.x - textWidth/2),
                     (int)(screenPos.y - fontSize/2),
                     fontSize, c);
        }
    }

    // Draw collectible labels (screen space)
    for (int i = 0; i < MAX_COLLECTIBLES; i++)
    {
        if (!game.collectibles[i].active || game.collectibles[i].collected) continue;

        Vector3 worldPos = game.collectibles[i].position;
        worldPos.y = 2.2f + sinf(game.time * 3.0f + game.collectibles[i].bobOffset) * 0.1f;
        Vector2 screenPos = GetWorldToScreen(worldPos, camera);

        if (screenPos.x > 0 && screenPos.x < SCREEN_WIDTH &&
            screenPos.y > 60 && screenPos.y < SCREEN_HEIGHT)
        {
            const char* text = game.collectibles[i].isMultiplier ?
                TextFormat("x%d", game.collectibles[i].value) :
                TextFormat("+%d", game.collectibles[i].value);

            int fontSize = 28;
            int textWidth = MeasureText(text, fontSize);

            // Background
            Color bgColor = game.collectibles[i].isMultiplier ?
                (Color){ 180, 120, 0, 220 } : (Color){ 0, 120, 50, 220 };
            DrawRectangleRounded(
                (Rectangle){ screenPos.x - textWidth/2 - 8, screenPos.y - 16, textWidth + 16, 36 },
                0.3f, 8, bgColor
            );

            // Text
            DrawText(text, (int)(screenPos.x - textWidth/2), (int)(screenPos.y - 12), fontSize, WHITE);
        }
    }

    //--- UI Overlay ---

    // Top bar background
    DrawRectangleGradientV(0, 0, SCREEN_WIDTH, 70, (Color){ 0, 0, 0, 200 }, (Color){ 0, 0, 0, 0 });

    // Score with smooth counting
    DrawText(TextFormat("SCORE"), 20, 8, 14, GRAY);
    DrawText(TextFormat("%d", game.displayScore), 20, 24, 32, WHITE);

    // Combo indicator
    if (game.combo > 1)
    {
        float comboScale = 1.0f + sinf(game.time * 10.0f) * 0.1f;
        int fontSize = (int)(24 * comboScale);
        const char* comboText = TextFormat("x%d COMBO!", game.combo);
        DrawText(comboText, 20, 58, fontSize, GOLD);
    }

    // Squad count with icon
    const char* squadText = TextFormat("%d", game.squadCount);
    int squadTextWidth = MeasureText(squadText, 32);
    DrawText("SQUAD", SCREEN_WIDTH - squadTextWidth - 50, 8, 14, GRAY);
    DrawRectangle(SCREEN_WIDTH - squadTextWidth - 55, 26, 20, 20, PLAYER_BODY_COLOR);
    DrawText(squadText, SCREEN_WIDTH - squadTextWidth - 25, 24, 32, WHITE);

    // Distance meter
    const char* distText = TextFormat("%.0fm", game.distance);
    int distWidth = MeasureText(distText, 24);
    DrawText(distText, SCREEN_WIDTH/2 - distWidth/2, 24, 24, WHITE);

    // Progress bar at very top
    float progressInSegment = fmodf(game.distance, 100.0f) / 100.0f;
    DrawRectangle(0, 0, SCREEN_WIDTH, 4, (Color){ 50, 50, 60, 255 });
    DrawRectangle(0, 0, (int)(SCREEN_WIDTH * progressInSegment), 4, GOLD);

    // Controls hint (fades out)
    if (game.distance < 40.0f)
    {
        int alpha = (int)(255 * (1.0f - game.distance / 40.0f));
        DrawText("A/D or Arrow Keys to move  |  Click and drag to steer",
                 SCREEN_WIDTH/2 - 230, SCREEN_HEIGHT - 45, 18, (Color){ 255, 255, 255, alpha });
    }

    // Game over screen
    if (game.gameOver)
    {
        // Darken background
        float fadeIn = Clamp(game.gameOverTimer * 2.0f, 0, 1);
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){ 0, 0, 0, (unsigned char)(200 * fadeIn) });

        if (game.gameOverTimer > 0.3f)
        {
            float slideIn = EaseOutBack(Clamp((game.gameOverTimer - 0.3f) * 3.0f, 0, 1));

            // Game over text
            const char* gameOverText = "GAME OVER";
            int textWidth = MeasureText(gameOverText, 60);
            int yPos = (int)(SCREEN_HEIGHT/2 - 80 + (1.0f - slideIn) * 100);
            DrawText(gameOverText, SCREEN_WIDTH/2 - textWidth/2, yPos, 60, RED);

            // Final score
            if (game.gameOverTimer > 0.6f)
            {
                const char* finalText = TextFormat("Final Score: %d", game.score);
                textWidth = MeasureText(finalText, 30);
                DrawText(finalText, SCREEN_WIDTH/2 - textWidth/2, SCREEN_HEIGHT/2, 30, WHITE);

                const char* distanceText = TextFormat("Distance: %.0fm", game.distance);
                textWidth = MeasureText(distanceText, 24);
                DrawText(distanceText, SCREEN_WIDTH/2 - textWidth/2, SCREEN_HEIGHT/2 + 40, 24, GRAY);
            }

            // Restart hint
            if (game.gameOverTimer > 1.0f)
            {
                float pulse = (sinf(game.time * 4.0f) + 1.0f) * 0.5f;
                Color hintColor = ColorLerp(GRAY, WHITE, pulse);
                const char* restartText = "Press R to Restart";
                textWidth = MeasureText(restartText, 24);
                DrawText(restartText, SCREEN_WIDTH/2 - textWidth/2, SCREEN_HEIGHT/2 + 100, 24, hintColor);
            }
        }
    }
}
