#ifndef LOGGING_H
#define LOGGING_H

#include "../common.h"
#include "../errors.h"
#include <stdio.h>
#include <time.h>
#include <stdbool.h>

#define MAX_LOG_MESSAGE 512
#define MAX_LOG_CATEGORY 32
#define MAX_LOG_FILE_PATH 256
#define MAX_LOG_ENTRIES 1000

typedef enum {
    LOG_LEVEL_TRACE = 0,
    LOG_LEVEL_DEBUG = 1,
    LOG_LEVEL_INFO = 2,
    LOG_LEVEL_WARN = 3,
    LOG_LEVEL_ERROR = 4,
    LOG_LEVEL_FATAL = 5
} LogLevel;

typedef struct {
    time_t timestamp;
    LogLevel level;
    char category[MAX_LOG_CATEGORY];
    char message[MAX_LOG_MESSAGE];
    const char* file;
    int line;
    const char* function;
} LogEntry;

typedef struct {
    LogEntry entries[MAX_LOG_ENTRIES];
    int entry_count;
    int write_index;
    LogLevel min_level;
    bool console_output;
    bool file_output;
    char log_file_path[MAX_LOG_FILE_PATH];
    FILE* log_file;
    bool initialized;
} Logger;

// Logger initialization and cleanup
GameError InitLogger(Logger* logger, LogLevel min_level, bool console_output, bool file_output, const char* log_file_path);
void CleanupLogger(Logger* logger);

// Core logging functions
void LogMessage(Logger* logger, LogLevel level, const char* category, const char* file, int line, const char* function, const char* format, ...);
void FlushLogger(Logger* logger);

// Log level management
void SetLogLevel(Logger* logger, LogLevel level);
LogLevel GetLogLevel(Logger* logger);
const char* LogLevelToString(LogLevel level);

// Log output control
GameError SetLogFile(Logger* logger, const char* file_path);
void EnableConsoleOutput(Logger* logger, bool enable);
void EnableFileOutput(Logger* logger, bool enable);

// Log entry retrieval
LogEntry* GetLogEntry(Logger* logger, int index);
int GetLogEntryCount(Logger* logger);
void ClearLogEntries(Logger* logger);

// Log filtering and searching
int FindLogEntries(Logger* logger, LogLevel level, const char* category, LogEntry* results, int max_results);
int CountLogEntriesByLevel(Logger* logger, LogLevel level);

// Performance and analytics logging
void LogGameEvent(Logger* logger, const char* event_name, const char* details);
void LogPerformanceMetric(Logger* logger, const char* metric_name, double value, const char* unit);
void LogUserAction(Logger* logger, const char* action, const char* details);
void LogGameError(Logger* logger, GameError error, const char* context);

// Convenience macros
#define LOG_TRACE(logger, category, ...) LogMessage(logger, LOG_LEVEL_TRACE, category, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_DEBUG(logger, category, ...) LogMessage(logger, LOG_LEVEL_DEBUG, category, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_INFO(logger, category, ...) LogMessage(logger, LOG_LEVEL_INFO, category, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_WARN(logger, category, ...) LogMessage(logger, LOG_LEVEL_WARN, category, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_ERROR(logger, category, ...) LogMessage(logger, LOG_LEVEL_ERROR, category, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_FATAL(logger, category, ...) LogMessage(logger, LOG_LEVEL_FATAL, category, __FILE__, __LINE__, __func__, __VA_ARGS__)

// Game-specific logging categories
#define LOG_CAT_GAME "GAME"
#define LOG_CAT_COMBAT "COMBAT"
#define LOG_CAT_CARDS "CARDS"
#define LOG_CAT_PLAYER "PLAYER"
#define LOG_CAT_AI "AI"
#define LOG_CAT_RENDER "RENDER"
#define LOG_CAT_AUDIO "AUDIO"
#define LOG_CAT_INPUT "INPUT"
#define LOG_CAT_SAVE "SAVE"
#define LOG_CAT_CONFIG "CONFIG"
#define LOG_CAT_PERF "PERFORMANCE"
#define LOG_CAT_DEBUG "DEBUG"

#endif // LOGGING_H