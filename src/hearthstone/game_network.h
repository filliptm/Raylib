#ifndef GAME_NETWORK_H
#define GAME_NETWORK_H

#include "game_state.h"
#include "network.h"

// Network game management functions
void InitializeGameNetwork(GameState* game);
void InitializeGameNetworkAsServer(GameState* game, int port);
void InitializeGameNetworkAsClient(GameState* game, const char* address, int port);
void UpdateGameNetwork(GameState* game, float deltaTime);
void CleanupGameNetwork(GameState* game);

// Network action forwarding
void NetworkSendPlayerAction(GameState* game, ActionType action, int cardIndex, int targetIndex, bool targetIsPlayer);
bool IsNetworkPlayerAction(GameState* game, int playerId);

#endif // GAME_NETWORK_H