#include "performance.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Initialize performance metrics
void InitializePerformance(PerformanceMetrics* metrics) {
    memset(metrics, 0, sizeof(PerformanceMetrics));
    metrics->profilingEnabled = true;
}

// Cleanup performance metrics
void CleanupPerformance(PerformanceMetrics* metrics) {
    // Nothing to cleanup currently
    (void)metrics;
}

// Start profiling a section
void StartProfiling(PerformanceMetrics* metrics, const char* section) {
    if (!metrics->profilingEnabled) return;

    metrics->profileStartTime = GetTime();
    // Could store section name for detailed profiling
    (void)section;
}

// End profiling a section
void EndProfiling(PerformanceMetrics* metrics, const char* section) {
    if (!metrics->profilingEnabled) return;

    float endTime = GetTime();
    float duration = endTime - metrics->profileStartTime;

    // Update specific timing based on section
    if (strcmp(section, "update") == 0) {
        metrics->updateTime = duration;
    } else if (strcmp(section, "render") == 0) {
        metrics->renderTime = duration;
    } else if (strcmp(section, "network") == 0) {
        metrics->networkTime = duration;
    } else if (strcmp(section, "ai") == 0) {
        metrics->aiTime = duration;
    }
}

// Update performance metrics
void UpdatePerformanceMetrics(PerformanceMetrics* metrics, float deltaTime) {
    metrics->frameTime = deltaTime;
    metrics->fps = 1.0f / deltaTime;

    // Update frame history
    metrics->frameHistory[metrics->frameHistoryIndex] = deltaTime;
    metrics->frameHistoryIndex = (metrics->frameHistoryIndex + 1) % 60;

    // Simple memory tracking (would need OS-specific implementation for real tracking)
    metrics->memoryUsed = 0; // Placeholder
}

// Initialize memory pool
bool InitializeMemoryPool(MemoryPool* pool, size_t size) {
    pool->memory = malloc(size);
    if (!pool->memory) {
        return false;
    }

    pool->size = size;
    pool->used = 0;
    pool->alignment = sizeof(void*); // Default alignment

    return true;
}

// Allocate from memory pool
void* PoolAllocate(MemoryPool* pool, size_t size) {
    // Align size to boundary
    size_t alignedSize = (size + pool->alignment - 1) & ~(pool->alignment - 1);

    if (pool->used + alignedSize > pool->size) {
        return NULL; // Pool exhausted
    }

    void* ptr = (char*)pool->memory + pool->used;
    pool->used += alignedSize;

    return ptr;
}

// Reset memory pool
void PoolReset(MemoryPool* pool) {
    pool->used = 0;
}

// Cleanup memory pool
void CleanupMemoryPool(MemoryPool* pool) {
    free(pool->memory);
    pool->memory = NULL;
    pool->size = 0;
    pool->used = 0;
}

// Optimize game state
void OptimizeGameState(GameState* game) {
    // Remove dead effects
    for (int i = game->activeEffectsCount - 1; i >= 0; i--) {
        if (game->effects[i].lifetime <= 0.0f) {
            // Remove effect by swapping with last
            if (i < game->activeEffectsCount - 1) {
                game->effects[i] = game->effects[game->activeEffectsCount - 1];
            }
            game->activeEffectsCount--;
        }
    }

    // Optimize player data
    for (int p = 0; p < 2; p++) {
        Player* player = &game->players[p];

        // Compact hand if needed
        int writeIndex = 0;
        for (int i = 0; i < player->handCount; i++) {
            if (player->hand[i].id != 0) { // Valid card
                if (writeIndex != i) {
                    player->hand[writeIndex] = player->hand[i];
                }
                writeIndex++;
            }
        }
        player->handCount = writeIndex;

        // Compact board if needed
        writeIndex = 0;
        for (int i = 0; i < player->boardCount; i++) {
            if (player->board[i].id != 0) { // Valid card
                if (writeIndex != i) {
                    player->board[writeIndex] = player->board[i];
                    player->board[writeIndex].boardPosition = writeIndex;
                }
                writeIndex++;
            }
        }
        player->boardCount = writeIndex;
    }
}

// Optimize rendering
void OptimizeRendering(GameState* game) {
    // Frustum culling for cards
    for (int p = 0; p < 2; p++) {
        Player* player = &game->players[p];

        // Mark cards as visible/invisible based on camera
        for (int i = 0; i < player->handCount; i++) {
            Card* card = &player->hand[i];
            // Simple distance check for now
            float distance = Vector3Distance(card->position, game->camera.position);
            card->isVisible = distance < 50.0f;
        }

        for (int i = 0; i < player->boardCount; i++) {
            Card* card = &player->board[i];
            float distance = Vector3Distance(card->position, game->camera.position);
            card->isVisible = distance < 50.0f;
        }
    }

    // Level of detail for distant cards
    for (int p = 0; p < 2; p++) {
        Player* player = &game->players[p];

        for (int i = 0; i < player->boardCount; i++) {
            Card* card = &player->board[i];
            float distance = Vector3Distance(card->position, game->camera.position);

            if (distance > 20.0f) {
                card->lodLevel = 2; // Low detail
            } else if (distance > 10.0f) {
                card->lodLevel = 1; // Medium detail
            } else {
                card->lodLevel = 0; // High detail
            }
        }
    }
}

// Optimize memory usage
void OptimizeMemoryUsage(GameState* game) {
    // Clear action queue if it gets too full
    if (game->queueCount > 40) {
        game->queueCount = 0; // Reset queue
    }

    // Limit effect count
    if (game->activeEffectsCount > MAX_EFFECTS - 5) {
        // Remove oldest effects
        for (int i = 0; i < 5; i++) {
            if (game->activeEffectsCount > 0) {
                // Remove first effect and shift others
                for (int j = 0; j < game->activeEffectsCount - 1; j++) {
                    game->effects[j] = game->effects[j + 1];
                }
                game->activeEffectsCount--;
            }
        }
    }
}

// Get average FPS
float GetAverageFPS(PerformanceMetrics* metrics) {
    float totalTime = 0.0f;
    int count = 0;

    for (int i = 0; i < 60; i++) {
        if (metrics->frameHistory[i] > 0.0f) {
            totalTime += metrics->frameHistory[i];
            count++;
        }
    }

    if (count == 0) return 0.0f;

    float avgFrameTime = totalTime / count;
    return 1.0f / avgFrameTime;
}

// Get average frame time
float GetAverageFrameTime(PerformanceMetrics* metrics) {
    float totalTime = 0.0f;
    int count = 0;

    for (int i = 0; i < 60; i++) {
        if (metrics->frameHistory[i] > 0.0f) {
            totalTime += metrics->frameHistory[i];
            count++;
        }
    }

    return (count > 0) ? totalTime / count : 0.0f;
}

// Log performance statistics
void LogPerformanceStats(PerformanceMetrics* metrics) {
    printf("=== Performance Stats ===\n");
    printf("Current FPS: %.1f\n", metrics->fps);
    printf("Average FPS: %.1f\n", GetAverageFPS(metrics));
    printf("Frame Time: %.3fms\n", metrics->frameTime * 1000.0f);
    printf("Update Time: %.3fms\n", metrics->updateTime * 1000.0f);
    printf("Render Time: %.3fms\n", metrics->renderTime * 1000.0f);
    printf("Network Time: %.3fms\n", metrics->networkTime * 1000.0f);
    printf("AI Time: %.3fms\n", metrics->aiTime * 1000.0f);
    printf("Memory Used: %zu bytes\n", metrics->memoryUsed);
    printf("=========================\n");
}

// Preload resources
void PreloadResources(GameState* game) {
    // Preload common card textures
    // This would load textures into VRAM ahead of time
    (void)game; // Placeholder
}

// Unload unused resources
void UnloadUnusedResources(GameState* game) {
    // Unload textures for cards not currently visible
    // This would free VRAM for unused textures
    (void)game; // Placeholder
}