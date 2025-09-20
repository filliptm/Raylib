#ifndef NETWORK_H
#define NETWORK_H

#include "types.h"
#include "common.h"
#include "game_state.h"
#include <stdint.h>
#include <stdbool.h>

// Network constants
#define MAX_PACKET_SIZE 1024
#define MAX_MESSAGE_SIZE 512
#define DEFAULT_PORT 7777
#define MAX_PLAYERS 2
#define NETWORK_TIMEOUT 30.0f

// Network modes
typedef enum {
    NETWORK_MODE_NONE,
    NETWORK_MODE_SERVER,
    NETWORK_MODE_CLIENT
} NetworkMode;

// Network message types
typedef enum {
    NET_MSG_NONE = 0,
    NET_MSG_HANDSHAKE,
    NET_MSG_GAME_STATE_SYNC,
    NET_MSG_PLAYER_ACTION,
    NET_MSG_CARD_PLAYED,
    NET_MSG_ATTACK,
    NET_MSG_END_TURN,
    NET_MSG_CHAT,
    NET_MSG_DISCONNECT,
    NET_MSG_PING,
    NET_MSG_PONG,
    NET_MSG_GAME_START,
    NET_MSG_GAME_END
} NetworkMessageType;

// Network packet structure
typedef struct {
    uint32_t magic;          // Packet validation
    uint16_t type;           // Message type
    uint16_t length;         // Data length
    uint32_t sequence;       // Sequence number
    uint32_t timestamp;      // Timestamp
    uint8_t data[MAX_MESSAGE_SIZE]; // Message data
} NetworkPacket;

// Player action network message
typedef struct {
    ActionType actionType;
    int playerId;
    int cardIndex;
    int targetIndex;
    bool targetIsPlayer;
    float posX, posY, posZ;  // Target position
} NetworkPlayerAction;

// Game state sync message (simplified)
typedef struct {
    int activePlayer;
    int turnNumber;
    GamePhase gamePhase;
    TurnPhase turnPhase;
    bool gameEnded;
    int winner;

    // Player states
    struct {
        int health;
        int mana;
        int handCount;
        int boardCount;
        bool isAlive;
    } players[2];
} NetworkGameStateSync;

// Connection info
typedef struct {
    char address[64];
    int port;
    int socket;
    bool connected;
    float lastPingTime;
    float connectionTime;
    uint32_t sequenceNumber;
} NetworkConnection;

// Main network system
typedef struct {
    NetworkMode mode;
    NetworkConnection connections[MAX_PLAYERS];
    int localPlayerId;
    int connectedPlayers;

    // Server specific
    int serverSocket;
    bool serverRunning;

    // Message handling
    NetworkPacket incomingPackets[32];
    int incomingPacketCount;
    NetworkPacket outgoingPackets[32];
    int outgoingPacketCount;

    // Synchronization
    bool waitingForSync;
    float syncTimer;
    float pingTimer;

    // Game reference
    GameState* gameState;
} NetworkSystem;

// Network initialization and cleanup
bool InitializeNetwork(NetworkSystem* network);
void CleanupNetwork(NetworkSystem* network);

// Server functions
bool StartServer(NetworkSystem* network, int port);
void UpdateServer(NetworkSystem* network, float deltaTime);
void StopServer(NetworkSystem* network);

// Client functions
bool ConnectToServer(NetworkSystem* network, const char* address, int port);
void UpdateClient(NetworkSystem* network, float deltaTime);
void DisconnectFromServer(NetworkSystem* network);

// Message handling
void SendMessage(NetworkSystem* network, int connectionId, NetworkMessageType type, const void* data, int dataSize);
void BroadcastMessage(NetworkSystem* network, NetworkMessageType type, const void* data, int dataSize);
bool ReceiveMessage(NetworkSystem* network, NetworkPacket* packet);
void ProcessMessage(NetworkSystem* network, const NetworkPacket* packet, int connectionId);

// Game synchronization
void SendGameStateSync(NetworkSystem* network);
void SendPlayerAction(NetworkSystem* network, const NetworkPlayerAction* action);
void HandlePlayerAction(NetworkSystem* network, const NetworkPlayerAction* action);
void SynchronizeGameState(NetworkSystem* network, GameState* game);

// Utility functions
void PacketToBuffer(const NetworkPacket* packet, uint8_t* buffer);
void BufferToPacket(const uint8_t* buffer, NetworkPacket* packet);
uint32_t CalculateChecksum(const uint8_t* data, int length);
bool IsValidPacket(const NetworkPacket* packet);

// Network diagnostics
float GetPing(NetworkSystem* network, int connectionId);
float GetConnectionTime(NetworkSystem* network, int connectionId);
int GetPacketLoss(NetworkSystem* network, int connectionId);

#endif // NETWORK_H