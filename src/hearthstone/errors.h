#ifndef ERRORS_H
#define ERRORS_H

typedef enum {
    GAME_OK = 0,
    GAME_ERROR_OUT_OF_MEMORY,
    GAME_ERROR_FILE_NOT_FOUND,
    GAME_ERROR_INVALID_PARAMETER,
    GAME_ERROR_RESOURCE_NOT_FOUND,
    GAME_ERROR_RESOURCE_ALREADY_EXISTS,
    GAME_ERROR_INVALID_CARD,
    GAME_ERROR_INVALID_TARGET,
    GAME_ERROR_NOT_ENOUGH_MANA,
    GAME_ERROR_BOARD_FULL,
    GAME_ERROR_HAND_FULL,
    GAME_ERROR_DECK_EMPTY,
    GAME_ERROR_INVALID_STATE,
    GAME_ERROR_NETWORK_FAILURE,
    GAME_ERROR_SAVE_FAILED,
    GAME_ERROR_LOAD_FAILED,
    GAME_ERROR_CONFIG_INVALID,
    GAME_ERROR_UNKNOWN
} GameError;

// Error handling utilities
const char* GetErrorString(GameError error);
void LogError(GameError error, const char* context);

// Result type for functions that can fail
#define RESULT(T) struct { T value; GameError error; }

// Common result types
typedef RESULT(int) IntResult;
typedef RESULT(float) FloatResult;
typedef RESULT(void*) PtrResult;

// Macros for error handling
#define IS_OK(result) ((result).error == GAME_OK)
#define IS_ERROR(result) ((result).error != GAME_OK)
#define RETURN_ERROR(error) return (typeof(return)){.error = error}
#define RETURN_OK(value) return (typeof(return)){.value = value, .error = GAME_OK}

#endif