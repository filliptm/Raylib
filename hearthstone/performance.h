#ifndef PERFORMANCE_H
#define PERFORMANCE_H

#include "types.h"
#include "common.h"
#include "game_state.h"
#include <stdbool.h>
#include <stddef.h>

// Performance metrics
typedef struct {
    float frameTime;
    float fps;
    float updateTime;
    float renderTime;
    float networkTime;
    float aiTime;

    // Memory usage
    size_t memoryUsed;
    size_t peakMemoryUsed;

    // Frame timing history
    float frameHistory[60];
    int frameHistoryIndex;

    // Profiling
    bool profilingEnabled;
    float profileStartTime;
} PerformanceMetrics;

// Memory pool for efficient allocation
typedef struct {
    void* memory;
    size_t size;
    size_t used;
    size_t alignment;
} MemoryPool;

// Performance initialization
void InitializePerformance(PerformanceMetrics* metrics);
void CleanupPerformance(PerformanceMetrics* metrics);

// Profiling
void StartProfiling(PerformanceMetrics* metrics, const char* section);
void EndProfiling(PerformanceMetrics* metrics, const char* section);
void UpdatePerformanceMetrics(PerformanceMetrics* metrics, float deltaTime);

// Memory management
bool InitializeMemoryPool(MemoryPool* pool, size_t size);
void* PoolAllocate(MemoryPool* pool, size_t size);
void PoolReset(MemoryPool* pool);
void CleanupMemoryPool(MemoryPool* pool);

// Optimization functions
void OptimizeGameState(GameState* game);
void OptimizeRendering(GameState* game);
void OptimizeMemoryUsage(GameState* game);

// Performance monitoring
float GetAverageFPS(PerformanceMetrics* metrics);
float GetAverageFrameTime(PerformanceMetrics* metrics);
void LogPerformanceStats(PerformanceMetrics* metrics);

// Resource loading optimization
void PreloadResources(GameState* game);
void UnloadUnusedResources(GameState* game);

#endif // PERFORMANCE_H