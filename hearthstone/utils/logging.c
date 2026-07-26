#include "logging.h"
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

GameError InitLogger(Logger* logger, LogLevel min_level, bool console_output, bool file_output, const char* log_file_path) {
    if (!logger) return GAME_ERROR_INVALID_PARAMETER;
    
    memset(logger, 0, sizeof(Logger));
    
    logger->min_level = min_level;
    logger->console_output = console_output;
    logger->file_output = file_output;
    logger->entry_count = 0;
    logger->write_index = 0;
    
    if (file_output && log_file_path) {
        strncpy(logger->log_file_path, log_file_path, sizeof(logger->log_file_path) - 1);
        logger->log_file = fopen(log_file_path, "a");
        if (!logger->log_file) {
            return GAME_ERROR_FILE_NOT_FOUND;
        }
    }
    
    logger->initialized = true;
    return GAME_OK;
}

void CleanupLogger(Logger* logger) {
    if (!logger || !logger->initialized) return;
    
    FlushLogger(logger);
    
    if (logger->log_file) {
        fclose(logger->log_file);
        logger->log_file = NULL;
    }
    
    memset(logger, 0, sizeof(Logger));
}

void LogMessage(Logger* logger, LogLevel level, const char* category, const char* file, int line, const char* function, const char* format, ...) {
    if (!logger || !logger->initialized || level < logger->min_level) return;
    
    // Get current log entry
    LogEntry* entry = &logger->entries[logger->write_index];
    
    // Fill entry data
    entry->timestamp = time(NULL);
    entry->level = level;
    entry->file = file;
    entry->line = line;
    entry->function = function;
    
    if (category) {
        strncpy(entry->category, category, sizeof(entry->category) - 1);
    } else {
        strcpy(entry->category, "GENERAL");
    }
    
    // Format message
    va_list args;
    va_start(args, format);
    vsnprintf(entry->message, sizeof(entry->message), format, args);
    va_end(args);
    
    // Update indices
    logger->write_index = (logger->write_index + 1) % MAX_LOG_ENTRIES;
    if (logger->entry_count < MAX_LOG_ENTRIES) {
        logger->entry_count++;
    }
    
    // Output to console
    if (logger->console_output) {
        struct tm* tm_info = localtime(&entry->timestamp);
        char time_buffer[32];
        strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", tm_info);
        
        const char* level_str = LogLevelToString(level);
        const char* filename = strrchr(file, '/');
        filename = filename ? filename + 1 : file;
        
        printf("[%s] [%s] [%s] %s (%s:%d in %s)\n", 
               time_buffer, level_str, entry->category, entry->message, 
               filename, line, function);
    }
    
    // Output to file
    if (logger->file_output && logger->log_file) {
        struct tm* tm_info = localtime(&entry->timestamp);
        char time_buffer[32];
        strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", tm_info);
        
        const char* level_str = LogLevelToString(level);
        const char* filename = strrchr(file, '/');
        filename = filename ? filename + 1 : file;
        
        fprintf(logger->log_file, "[%s] [%s] [%s] %s (%s:%d in %s)\n", 
                time_buffer, level_str, entry->category, entry->message, 
                filename, line, function);
        fflush(logger->log_file);
    }
}

void FlushLogger(Logger* logger) {
    if (!logger || !logger->initialized) return;
    
    if (logger->log_file) {
        fflush(logger->log_file);
    }
}

void SetLogLevel(Logger* logger, LogLevel level) {
    if (!logger || !logger->initialized) return;
    logger->min_level = level;
}

LogLevel GetLogLevel(Logger* logger) {
    if (!logger || !logger->initialized) return LOG_LEVEL_INFO;
    return logger->min_level;
}

const char* LogLevelToString(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_TRACE: return "TRACE";
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_INFO:  return "INFO ";
        case LOG_LEVEL_WARN:  return "WARN ";
        case LOG_LEVEL_ERROR: return "ERROR";
        case LOG_LEVEL_FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

GameError SetLogFile(Logger* logger, const char* file_path) {
    if (!logger || !logger->initialized || !file_path) return GAME_ERROR_INVALID_PARAMETER;
    
    // Close existing file
    if (logger->log_file) {
        fclose(logger->log_file);
        logger->log_file = NULL;
    }
    
    // Open new file
    strncpy(logger->log_file_path, file_path, sizeof(logger->log_file_path) - 1);
    logger->log_file = fopen(file_path, "a");
    if (!logger->log_file) {
        logger->file_output = false;
        return GAME_ERROR_FILE_NOT_FOUND;
    }
    
    logger->file_output = true;
    return GAME_OK;
}

void EnableConsoleOutput(Logger* logger, bool enable) {
    if (!logger || !logger->initialized) return;
    logger->console_output = enable;
}

void EnableFileOutput(Logger* logger, bool enable) {
    if (!logger || !logger->initialized) return;
    logger->file_output = enable;
}

LogEntry* GetLogEntry(Logger* logger, int index) {
    if (!logger || !logger->initialized || index < 0 || index >= logger->entry_count) return NULL;
    
    // Calculate actual index in circular buffer
    int actual_index;
    if (logger->entry_count < MAX_LOG_ENTRIES) {
        actual_index = index;
    } else {
        actual_index = (logger->write_index + index) % MAX_LOG_ENTRIES;
    }
    
    return &logger->entries[actual_index];
}

int GetLogEntryCount(Logger* logger) {
    if (!logger || !logger->initialized) return 0;
    return logger->entry_count;
}

void ClearLogEntries(Logger* logger) {
    if (!logger || !logger->initialized) return;
    
    logger->entry_count = 0;
    logger->write_index = 0;
    memset(logger->entries, 0, sizeof(logger->entries));
}

int FindLogEntries(Logger* logger, LogLevel level, const char* category, LogEntry* results, int max_results) {
    if (!logger || !logger->initialized || !results || max_results <= 0) return 0;
    
    int found = 0;
    for (int i = 0; i < logger->entry_count && found < max_results; i++) {
        LogEntry* entry = GetLogEntry(logger, i);
        if (!entry) continue;
        
        bool level_match = (level == LOG_LEVEL_TRACE || entry->level >= level);
        bool category_match = (!category || strcmp(entry->category, category) == 0);
        
        if (level_match && category_match) {
            results[found] = *entry;
            found++;
        }
    }
    
    return found;
}

int CountLogEntriesByLevel(Logger* logger, LogLevel level) {
    if (!logger || !logger->initialized) return 0;
    
    int count = 0;
    for (int i = 0; i < logger->entry_count; i++) {
        LogEntry* entry = GetLogEntry(logger, i);
        if (entry && entry->level == level) {
            count++;
        }
    }
    
    return count;
}

void LogGameEvent(Logger* logger, const char* event_name, const char* details) {
    if (!logger || !event_name) return;
    
    if (details) {
        LOG_INFO(logger, LOG_CAT_GAME, "Game Event: %s - %s", event_name, details);
    } else {
        LOG_INFO(logger, LOG_CAT_GAME, "Game Event: %s", event_name);
    }
}

void LogPerformanceMetric(Logger* logger, const char* metric_name, double value, const char* unit) {
    if (!logger || !metric_name) return;
    
    if (unit) {
        LOG_INFO(logger, LOG_CAT_PERF, "Performance: %s = %.3f %s", metric_name, value, unit);
    } else {
        LOG_INFO(logger, LOG_CAT_PERF, "Performance: %s = %.3f", metric_name, value);
    }
}

void LogUserAction(Logger* logger, const char* action, const char* details) {
    if (!logger || !action) return;
    
    if (details) {
        LOG_INFO(logger, LOG_CAT_INPUT, "User Action: %s - %s", action, details);
    } else {
        LOG_INFO(logger, LOG_CAT_INPUT, "User Action: %s", action);
    }
}

void LogGameError(Logger* logger, GameError error, const char* context) {
    if (!logger) return;
    
    const char* error_str = GetErrorString(error);
    if (context) {
        LOG_ERROR(logger, LOG_CAT_DEBUG, "Error %d (%s) in context: %s", error, error_str, context);
    } else {
        LOG_ERROR(logger, LOG_CAT_DEBUG, "Error %d (%s)", error, error_str);
    }
}