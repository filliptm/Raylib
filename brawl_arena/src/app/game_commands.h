#ifndef BRAWL_GAME_COMMANDS_H
#define BRAWL_GAME_COMMANDS_H

#include "app_types.h"

typedef enum {
    GAME_COMMAND_SET_BOT_COUNT = 0,
    GAME_COMMAND_RESPAWN_BOTS,
    GAME_COMMAND_KILL_BOTS,
    GAME_COMMAND_HEAL_BOTS,
    GAME_COMMAND_SET_PLAYER_CLASS,
    GAME_COMMAND_CHARGE_PLAYER_SUPER,
    GAME_COMMAND_HEAL_PLAYER,
    GAME_COMMAND_RESPAWN_PLAYER,
    GAME_COMMAND_RESET_OBJECTIVE,
    GAME_COMMAND_SPAWN_GEM,
    GAME_COMMAND_CLEAR_GEMS,
    GAME_COMMAND_SYNC_CLASS_HEALTH,
    GAME_COMMAND_RESET_SCORE
} GameCommandType;

typedef struct GameCommand {
    GameCommandType type;
    int value;
} GameCommand;

// The single mutation gateway used by UI/devtools. Returns false when a command or
// payload is invalid for the current session.
bool GameCommandExecute(App *app, GameCommand command);

#endif
