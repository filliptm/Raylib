#include "errors.h"
#include <stdio.h>

const char* GetErrorString(GameError error) {
    switch (error) {
        case GAME_OK: return "Success";
        case GAME_ERROR_OUT_OF_MEMORY: return "Out of memory";
        case GAME_ERROR_FILE_NOT_FOUND: return "File not found";
        case GAME_ERROR_INVALID_PARAMETER: return "Invalid parameter";
        case GAME_ERROR_RESOURCE_NOT_FOUND: return "Resource not found";
        case GAME_ERROR_RESOURCE_ALREADY_EXISTS: return "Resource already exists";
        case GAME_ERROR_INVALID_CARD: return "Invalid card";
        case GAME_ERROR_INVALID_TARGET: return "Invalid target";
        case GAME_ERROR_NOT_ENOUGH_MANA: return "Not enough mana";
        case GAME_ERROR_BOARD_FULL: return "Board is full";
        case GAME_ERROR_HAND_FULL: return "Hand is full";
        case GAME_ERROR_DECK_EMPTY: return "Deck is empty";
        case GAME_ERROR_INVALID_STATE: return "Invalid game state";
        case GAME_ERROR_NETWORK_FAILURE: return "Network failure";
        case GAME_ERROR_SAVE_FAILED: return "Save failed";
        case GAME_ERROR_LOAD_FAILED: return "Load failed";
        case GAME_ERROR_CONFIG_INVALID: return "Invalid configuration";
        case GAME_ERROR_UNKNOWN: return "Unknown error";
        default: return "Undefined error";
    }
}

void LogError(GameError error, const char* context) {
    if (error != GAME_OK) {
        fprintf(stderr, "[ERROR] %s: %s\n", context, GetErrorString(error));
    }
}