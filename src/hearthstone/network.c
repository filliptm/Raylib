#include "network.h"
#include "gameplay.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Platform-specific networking
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define SOCKET_ERROR_CODE WSAGetLastError()
    #define CLOSE_SOCKET closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <errno.h>
    #define SOCKET_ERROR_CODE errno
    #define CLOSE_SOCKET close
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
#endif

#define MAGIC_NUMBER 0x48435348  // "HCSH" - HearthStone Clone Header

// Initialize network system
bool InitializeNetwork(NetworkSystem* network) {
    memset(network, 0, sizeof(NetworkSystem));

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return false;
    }
#endif

    network->mode = NETWORK_MODE_NONE;
    network->localPlayerId = -1;
    network->connectedPlayers = 0;
    network->serverRunning = false;
    network->waitingForSync = false;
    network->syncTimer = 0.0f;
    network->pingTimer = 0.0f;

    // Initialize connections
    for (int i = 0; i < MAX_PLAYERS; i++) {
        network->connections[i].socket = INVALID_SOCKET;
        network->connections[i].connected = false;
        network->connections[i].sequenceNumber = 0;
    }

    return true;
}

// Cleanup network system
void CleanupNetwork(NetworkSystem* network) {
    if (network->mode == NETWORK_MODE_SERVER) {
        StopServer(network);
    } else if (network->mode == NETWORK_MODE_CLIENT) {
        DisconnectFromServer(network);
    }

#ifdef _WIN32
    WSACleanup();
#endif
}

// Start server
bool StartServer(NetworkSystem* network, int port) {
    if (network->mode != NETWORK_MODE_NONE) {
        return false;
    }

    // Create server socket
    network->serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (network->serverSocket == INVALID_SOCKET) {
        return false;
    }

    // Set socket options
    int opt = 1;
    setsockopt(network->serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    // Bind socket
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(network->serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        CLOSE_SOCKET(network->serverSocket);
        return false;
    }

    // Listen for connections
    if (listen(network->serverSocket, MAX_PLAYERS) == SOCKET_ERROR) {
        CLOSE_SOCKET(network->serverSocket);
        return false;
    }

    network->mode = NETWORK_MODE_SERVER;
    network->serverRunning = true;
    network->localPlayerId = 0; // Server is always player 0

    printf("Server started on port %d\n", port);
    return true;
}

// Update server
void UpdateServer(NetworkSystem* network, float deltaTime) {
    if (!network->serverRunning) return;

    // Accept new connections (non-blocking)
    if (network->connectedPlayers < MAX_PLAYERS) {
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        int clientSocket = accept(network->serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);

        if (clientSocket != INVALID_SOCKET) {
            // Find free connection slot
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (!network->connections[i].connected) {
                    network->connections[i].socket = clientSocket;
                    network->connections[i].connected = true;
                    network->connections[i].connectionTime = 0.0f;
                    strcpy(network->connections[i].address, inet_ntoa(clientAddr.sin_addr));
                    network->connections[i].port = ntohs(clientAddr.sin_port);
                    network->connectedPlayers++;

                    printf("Client connected from %s:%d\n",
                           network->connections[i].address,
                           network->connections[i].port);

                    // Send handshake
                    SendMessage(network, i, NET_MSG_HANDSHAKE, &i, sizeof(int));
                    break;
                }
            }
        }
    }

    // Update connections
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (network->connections[i].connected) {
            network->connections[i].connectionTime += deltaTime;
        }
    }

    // Handle ping/pong
    network->pingTimer += deltaTime;
    if (network->pingTimer >= 5.0f) {
        // Send ping to all clients
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (network->connections[i].connected) {
                uint32_t timestamp = (uint32_t)time(NULL);
                SendMessage(network, i, NET_MSG_PING, &timestamp, sizeof(timestamp));
            }
        }
        network->pingTimer = 0.0f;
    }

    // Process incoming messages
    NetworkPacket packet;
    while (ReceiveMessage(network, &packet)) {
        // Find which connection sent this message
        // For simplicity, we'll process it as connection 0
        ProcessMessage(network, &packet, 0);
    }
}

// Stop server
void StopServer(NetworkSystem* network) {
    if (!network->serverRunning) return;

    // Disconnect all clients
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (network->connections[i].connected) {
            CLOSE_SOCKET(network->connections[i].socket);
            network->connections[i].connected = false;
        }
    }

    CLOSE_SOCKET(network->serverSocket);
    network->serverRunning = false;
    network->mode = NETWORK_MODE_NONE;
    network->connectedPlayers = 0;

    printf("Server stopped\n");
}

// Connect to server
bool ConnectToServer(NetworkSystem* network, const char* address, int port) {
    if (network->mode != NETWORK_MODE_NONE) {
        return false;
    }

    // Create client socket
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        return false;
    }

    // Connect to server
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, address, &serverAddr.sin_addr);

    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        CLOSE_SOCKET(clientSocket);
        return false;
    }

    // Store connection info
    network->connections[0].socket = clientSocket;
    network->connections[0].connected = true;
    network->connections[0].connectionTime = 0.0f;
    strcpy(network->connections[0].address, address);
    network->connections[0].port = port;

    network->mode = NETWORK_MODE_CLIENT;
    network->connectedPlayers = 1;

    printf("Connected to server %s:%d\n", address, port);
    return true;
}

// Update client
void UpdateClient(NetworkSystem* network, float deltaTime) {
    if (network->mode != NETWORK_MODE_CLIENT || !network->connections[0].connected) {
        return;
    }

    network->connections[0].connectionTime += deltaTime;

    // Process incoming messages
    NetworkPacket packet;
    while (ReceiveMessage(network, &packet)) {
        ProcessMessage(network, &packet, 0);
    }

    // Send periodic pings
    network->pingTimer += deltaTime;
    if (network->pingTimer >= 5.0f) {
        uint32_t timestamp = (uint32_t)time(NULL);
        SendMessage(network, 0, NET_MSG_PING, &timestamp, sizeof(timestamp));
        network->pingTimer = 0.0f;
    }
}

// Disconnect from server
void DisconnectFromServer(NetworkSystem* network) {
    if (network->mode != NETWORK_MODE_CLIENT) return;

    if (network->connections[0].connected) {
        SendMessage(network, 0, NET_MSG_DISCONNECT, NULL, 0);
        CLOSE_SOCKET(network->connections[0].socket);
        network->connections[0].connected = false;
    }

    network->mode = NETWORK_MODE_NONE;
    network->connectedPlayers = 0;

    printf("Disconnected from server\n");
}

// Send message
void SendMessage(NetworkSystem* network, int connectionId, NetworkMessageType type, const void* data, int dataSize) {
    if (connectionId < 0 || connectionId >= MAX_PLAYERS || !network->connections[connectionId].connected) {
        return;
    }

    NetworkPacket packet;
    packet.magic = MAGIC_NUMBER;
    packet.type = type;
    packet.length = dataSize;
    packet.sequence = network->connections[connectionId].sequenceNumber++;
    packet.timestamp = (uint32_t)time(NULL);

    if (data && dataSize > 0 && dataSize <= MAX_MESSAGE_SIZE) {
        memcpy(packet.data, data, dataSize);
    }

    // Convert to network byte order and send
    uint8_t buffer[MAX_PACKET_SIZE];
    PacketToBuffer(&packet, buffer);

    int totalSize = sizeof(packet) - MAX_MESSAGE_SIZE + dataSize;
    send(network->connections[connectionId].socket, (char*)buffer, totalSize, 0);
}

// Broadcast message to all connected clients
void BroadcastMessage(NetworkSystem* network, NetworkMessageType type, const void* data, int dataSize) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (network->connections[i].connected) {
            SendMessage(network, i, type, data, dataSize);
        }
    }
}

// Receive message (simplified - should be non-blocking in real implementation)
bool ReceiveMessage(NetworkSystem* network, NetworkPacket* packet) {
    // For simplicity, only check first connection
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!network->connections[i].connected) continue;

        uint8_t buffer[MAX_PACKET_SIZE];
        int received = recv(network->connections[i].socket, (char*)buffer, sizeof(buffer), 0);

        if (received > 0) {
            BufferToPacket(buffer, packet);
            if (IsValidPacket(packet)) {
                return true;
            }
        }
    }
    return false;
}

// Process received message
void ProcessMessage(NetworkSystem* network, const NetworkPacket* packet, int connectionId) {
    switch (packet->type) {
        case NET_MSG_HANDSHAKE:
            if (network->mode == NETWORK_MODE_CLIENT) {
                // Server assigned us a player ID
                network->localPlayerId = *(int*)packet->data;
                printf("Assigned player ID: %d\n", network->localPlayerId);
            }
            break;

        case NET_MSG_PLAYER_ACTION:
            if (packet->length == sizeof(NetworkPlayerAction)) {
                NetworkPlayerAction* action = (NetworkPlayerAction*)packet->data;
                HandlePlayerAction(network, action);
            }
            break;

        case NET_MSG_GAME_STATE_SYNC:
            if (packet->length == sizeof(NetworkGameStateSync)) {
                // Update local game state from network
                NetworkGameStateSync* sync = (NetworkGameStateSync*)packet->data;
                if (network->gameState) {
                    network->gameState->activePlayer = sync->activePlayer;
                    network->gameState->turnNumber = sync->turnNumber;
                    network->gameState->gamePhase = sync->gamePhase;
                    network->gameState->turnPhase = sync->turnPhase;
                    network->gameState->gameEnded = sync->gameEnded;
                    network->gameState->winner = sync->winner;
                }
            }
            break;

        case NET_MSG_PING:
            // Respond with pong
            SendMessage(network, connectionId, NET_MSG_PONG, packet->data, packet->length);
            break;

        case NET_MSG_PONG: {
            // Calculate ping time
            uint32_t sentTime = *(uint32_t*)packet->data;
            uint32_t currentTime = (uint32_t)time(NULL);
            network->connections[connectionId].lastPingTime = (float)(currentTime - sentTime);
            break;
        }

        case NET_MSG_DISCONNECT:
            network->connections[connectionId].connected = false;
            network->connectedPlayers--;
            printf("Player %d disconnected\n", connectionId);
            break;

        default:
            break;
    }
}

// Send game state synchronization
void SendGameStateSync(NetworkSystem* network) {
    if (!network->gameState) return;

    NetworkGameStateSync sync;
    sync.activePlayer = network->gameState->activePlayer;
    sync.turnNumber = network->gameState->turnNumber;
    sync.gamePhase = network->gameState->gamePhase;
    sync.turnPhase = network->gameState->turnPhase;
    sync.gameEnded = network->gameState->gameEnded;
    sync.winner = network->gameState->winner;

    // Copy simplified player states
    for (int i = 0; i < 2; i++) {
        sync.players[i].health = network->gameState->players[i].health;
        sync.players[i].mana = network->gameState->players[i].mana;
        sync.players[i].handCount = network->gameState->players[i].handCount;
        sync.players[i].boardCount = network->gameState->players[i].boardCount;
        sync.players[i].isAlive = network->gameState->players[i].isAlive;
    }

    BroadcastMessage(network, NET_MSG_GAME_STATE_SYNC, &sync, sizeof(sync));
}

// Send player action
void SendPlayerAction(NetworkSystem* network, const NetworkPlayerAction* action) {
    if (network->mode == NETWORK_MODE_CLIENT) {
        SendMessage(network, 0, NET_MSG_PLAYER_ACTION, action, sizeof(NetworkPlayerAction));
    } else {
        BroadcastMessage(network, NET_MSG_PLAYER_ACTION, action, sizeof(NetworkPlayerAction));
    }
}

// Handle received player action
void HandlePlayerAction(NetworkSystem* network, const NetworkPlayerAction* action) {
    if (!network->gameState) return;

    // Execute the action on local game state
    Player* player = &network->gameState->players[action->playerId];

    switch (action->actionType) {
        case ACTION_PLAY_CARD:
            if (action->cardIndex >= 0 && action->cardIndex < player->handCount) {
                // Convert position back to target if needed
                void* target = NULL; // Simplified - would need proper target resolution
                PlayCard(network->gameState, player, action->cardIndex, target);
            }
            break;

        case ACTION_ATTACK:
            if (action->cardIndex >= 0 && action->cardIndex < player->boardCount) {
                void* target = action->targetIsPlayer ?
                    (void*)&network->gameState->players[1 - action->playerId] : NULL;
                AttackWithMinion(network->gameState, player, action->cardIndex, target);
            }
            break;

        case ACTION_END_TURN:
            EndPlayerTurn(network->gameState);
            break;

        default:
            break;
    }
}

// Synchronize game state
void SynchronizeGameState(NetworkSystem* network, GameState* game) {
    network->gameState = game;

    if (network->mode == NETWORK_MODE_SERVER) {
        // Server broadcasts state updates
        network->syncTimer += GetFrameTime();
        if (network->syncTimer >= 0.1f) { // Update 10 times per second
            SendGameStateSync(network);
            network->syncTimer = 0.0f;
        }
    }
}

// Utility functions
void PacketToBuffer(const NetworkPacket* packet, uint8_t* buffer) {
    // Convert to network byte order - simplified implementation
    memcpy(buffer, packet, sizeof(NetworkPacket));
}

void BufferToPacket(const uint8_t* buffer, NetworkPacket* packet) {
    // Convert from network byte order - simplified implementation
    memcpy(packet, buffer, sizeof(NetworkPacket));
}

uint32_t CalculateChecksum(const uint8_t* data, int length) {
    uint32_t checksum = 0;
    for (int i = 0; i < length; i++) {
        checksum += data[i];
    }
    return checksum;
}

bool IsValidPacket(const NetworkPacket* packet) {
    return packet->magic == MAGIC_NUMBER &&
           packet->length <= MAX_MESSAGE_SIZE;
}

// Network diagnostics
float GetPing(NetworkSystem* network, int connectionId) {
    if (connectionId >= 0 && connectionId < MAX_PLAYERS) {
        return network->connections[connectionId].lastPingTime;
    }
    return -1.0f;
}

float GetConnectionTime(NetworkSystem* network, int connectionId) {
    if (connectionId >= 0 && connectionId < MAX_PLAYERS) {
        return network->connections[connectionId].connectionTime;
    }
    return -1.0f;
}

int GetPacketLoss(NetworkSystem* network, int connectionId) {
    // Simplified - would need packet tracking for real implementation
    return 0;
}