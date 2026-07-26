#include "game_network.h"
#include <stdlib.h>
#include <stdio.h>

// Initialize network system for game
void InitializeGameNetwork(GameState* game) {
    game->networkSystem = malloc(sizeof(NetworkSystem));
    if (game->networkSystem) {
        if (InitializeNetwork((NetworkSystem*)game->networkSystem)) {
            game->isNetworkGame = true;
            printf("Network system initialized\n");
        } else {
            free(game->networkSystem);
            game->networkSystem = NULL;
            game->isNetworkGame = false;
        }
    }
}

// Initialize as server
void InitializeGameNetworkAsServer(GameState* game, int port) {
    InitializeGameNetwork(game);

    if (game->networkSystem) {
        NetworkSystem* network = (NetworkSystem*)game->networkSystem;
        if (StartServer(network, port)) {
            network->gameState = game;
            printf("Game server started on port %d\n", port);
        } else {
            printf("Failed to start server\n");
            CleanupGameNetwork(game);
        }
    }
}

// Initialize as client
void InitializeGameNetworkAsClient(GameState* game, const char* address, int port) {
    InitializeGameNetwork(game);

    if (game->networkSystem) {
        NetworkSystem* network = (NetworkSystem*)game->networkSystem;
        if (ConnectToServer(network, address, port)) {
            network->gameState = game;
            printf("Connected to game server at %s:%d\n", address, port);
        } else {
            printf("Failed to connect to server\n");
            CleanupGameNetwork(game);
        }
    }
}

// Update network system
void UpdateGameNetwork(GameState* game, float deltaTime) {
    if (!game->isNetworkGame || !game->networkSystem) {
        return;
    }

    NetworkSystem* network = (NetworkSystem*)game->networkSystem;

    if (network->mode == NETWORK_MODE_SERVER) {
        UpdateServer(network, deltaTime);
        SynchronizeGameState(network, game);
    } else if (network->mode == NETWORK_MODE_CLIENT) {
        UpdateClient(network, deltaTime);
    }
}

// Cleanup network system
void CleanupGameNetwork(GameState* game) {
    if (game->networkSystem) {
        CleanupNetwork((NetworkSystem*)game->networkSystem);
        free(game->networkSystem);
        game->networkSystem = NULL;
        game->isNetworkGame = false;
    }
}

// Send player action over network
void NetworkSendPlayerAction(GameState* game, ActionType action, int cardIndex, int targetIndex, bool targetIsPlayer) {
    if (!game->isNetworkGame || !game->networkSystem) {
        return;
    }

    NetworkSystem* network = (NetworkSystem*)game->networkSystem;
    NetworkPlayerAction netAction;

    netAction.actionType = action;
    netAction.playerId = network->localPlayerId;
    netAction.cardIndex = cardIndex;
    netAction.targetIndex = targetIndex;
    netAction.targetIsPlayer = targetIsPlayer;
    netAction.posX = 0.0f;
    netAction.posY = 0.0f;
    netAction.posZ = 0.0f;

    SendPlayerAction(network, &netAction);
}

// Check if an action should be handled by network
bool IsNetworkPlayerAction(GameState* game, int playerId) {
    if (!game->isNetworkGame || !game->networkSystem) {
        return false;
    }

    NetworkSystem* network = (NetworkSystem*)game->networkSystem;

    // Only allow actions from the local player or if we're the server
    return (playerId == network->localPlayerId) ||
           (network->mode == NETWORK_MODE_SERVER);
}