#include "raylib.h"
#include "raymath.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

// ===== CORE ENUMS AND CONSTANTS =====

#define MAX_HAND_SIZE 10
#define MAX_BOARD_SIZE 7
#define MAX_DECK_SIZE 30
#define MAX_EFFECTS 50
#define MAX_HISTORY 1000

typedef enum {
    CARD_TYPE_MINION,
    CARD_TYPE_SPELL,
    CARD_TYPE_WEAPON,
    CARD_TYPE_HERO,
    CARD_TYPE_HERO_POWER
} CardType;

typedef enum {
    RARITY_COMMON,
    RARITY_RARE,
    RARITY_EPIC,
    RARITY_LEGENDARY
} CardRarity;

typedef enum {
    CLASS_NEUTRAL,
    CLASS_MAGE,
    CLASS_PALADIN,
    CLASS_WARRIOR,
    CLASS_ROGUE,
    CLASS_HUNTER,
    CLASS_DRUID,
    CLASS_WARLOCK,
    CLASS_SHAMAN,
    CLASS_PRIEST,
    CLASS_COUNT
} HeroClass;

typedef enum {
    GAME_STATE_MULLIGAN,
    GAME_STATE_PLAYING,
    GAME_STATE_ENDED
} GamePhase;

typedef enum {
    TURN_PHASE_START,
    TURN_PHASE_MAIN,
    TURN_PHASE_END
} TurnPhase;

typedef enum {
    ACTION_PLAY_CARD,
    ACTION_ATTACK,
    ACTION_USE_HERO_POWER,
    ACTION_END_TURN,
    ACTION_CONCEDE
} ActionType;

// ===== CORE DATA STRUCTURES =====

typedef struct Card {
    int id;
    char name[64];
    char description[256];
    int cost;
    CardType type;
    CardRarity rarity;
    HeroClass heroClass;
    
    // Minion stats
    int attack;
    int health;
    int maxHealth;
    bool canAttack;
    bool attackedThisTurn;
    
    // Keywords
    bool taunt;
    bool charge;
    bool rush;
    bool divineShield;
    bool stealth;
    bool poisonous;
    bool windfury;
    bool lifesteal;
    
    // Spell properties
    int spellDamage;
    int healing;
    
    // Weapon properties
    int durability;
    int maxDurability;
    
    // Visual and game state
    Vector3 position;
    Vector3 targetPosition;
    Vector3 size;
    Color color;
    bool isHovered;
    bool isSelected;
    bool isDragging;
    bool inHand;
    bool onBoard;
    int boardPosition;
    int ownerPlayer;
    
    // Effects
    bool hasBattlecry;
    bool hasDeathrattle;
    int battlecryValue;
    int deathrattleValue;
} Card;

typedef struct {
    int playerId;
    char name[32];
    HeroClass heroClass;
    
    // Resources
    int health;
    int maxHealth;
    int armor;
    int mana;
    int maxMana;
    
    // Hero power
    Card heroPower;
    bool heroPowerUsed;
    
    // Weapon
    Card *weapon;
    bool hasWeapon;
    
    // Collections
    Card deck[MAX_DECK_SIZE];
    Card hand[MAX_HAND_SIZE];
    Card board[MAX_BOARD_SIZE];
    
    // Counts
    int deckCount;
    int handCount;
    int boardCount;
    
    // Game state
    bool isActivePlayer;
    int turnCount;
    int fatigueDamage;
    bool isAlive;
} Player;

typedef struct {
    ActionType type;
    int playerId;
    int cardId;
    int targetId;
    int value;
} GameAction;

typedef struct {
    int type;
    Vector3 position;
    float duration;
    float timer;
    Color color;
    bool active;
    char text[64];
} VisualEffect;

typedef struct {
    GamePhase gamePhase;
    TurnPhase turnPhase;
    
    Player players[2];
    int activePlayer;
    int turnNumber;
    
    // Game state
    Card *selectedCard;
    Card *targetCard;
    bool targetingMode;
    bool combatInProgress;
    
    // Visual effects
    VisualEffect effects[MAX_EFFECTS];
    int activeEffectsCount;
    
    // Action queue
    GameAction actionQueue[50];
    int queueCount;
    
    // Random seed
    unsigned int randomSeed;
    
    // Camera and rendering
    Camera3D camera;
    Vector3 boardCenter;
    float cameraShake;
    float turnTimer;
    
    // Game result
    int winner;
    bool gameEnded;
} GameState;

// ===== FORWARD DECLARATIONS =====
void InitializeGame(GameState *game);
void UpdateGame(GameState *game);
void DrawGame(GameState *game);
Card CreateCard(int id, const char* name, int cost, CardType type, int attack, int health);
void DrawCard3D(Card *card, Camera3D camera);
bool CheckCardHit(Card *card, Ray ray);
void PlayCard(GameState *game, Card *card, Card *target);
void StartTurn(GameState *game);
void EndTurn(GameState *game);
bool CanPlayCard(Player *player, Card *card);
void AttackWithCard(GameState *game, Card *attacker, Card *target);
void DealDamage(void *target, int damage, Card *source, GameState *game);
void DrawCardFromDeck(Player *player);
void InitializeDecks(GameState *game);
void AddVisualEffect(GameState *game, int type, Vector3 position, const char* text);

// ===== CARD DATABASE =====

Card CreateCard(int id, const char* name, int cost, CardType type, int attack, int health) {
    Card card = {0};
    card.id = id;
    strncpy(card.name, name, sizeof(card.name) - 1);
    card.cost = cost;
    card.type = type;
    card.attack = attack;
    card.health = health;
    card.maxHealth = health;
    card.canAttack = false;
    card.attackedThisTurn = false;
    card.size = (Vector3){1.6f, 0.1f, 2.4f};
    card.color = WHITE;
    card.isHovered = false;
    card.isSelected = false;
    card.isDragging = false;
    card.inHand = false;
    card.onBoard = false;
    card.boardPosition = -1;
    card.ownerPlayer = -1;
    return card;
}

// Card database - basic cards for testing
Card GetCardById(int id) {
    switch (id) {
        case 1: {
            Card card = CreateCard(1, "Elven Archer", 1, CARD_TYPE_MINION, 1, 1);
            card.hasBattlecry = true;
            card.battlecryValue = 1; // Deal 1 damage
            card.color = GREEN;
            return card;
        }
        case 2: {
            Card card = CreateCard(2, "Boulderfist Ogre", 6, CARD_TYPE_MINION, 6, 7);
            card.color = BROWN;
            return card;
        }
        case 3: {
            Card card = CreateCard(3, "Chillwind Yeti", 4, CARD_TYPE_MINION, 4, 5);
            card.color = SKYBLUE;
            return card;
        }
        case 4: {
            Card card = CreateCard(4, "War Golem", 7, CARD_TYPE_MINION, 7, 7);
            card.color = GRAY;
            return card;
        }
        case 5: {
            Card card = CreateCard(5, "Stormpike Commando", 5, CARD_TYPE_MINION, 4, 2);
            card.hasBattlecry = true;
            card.battlecryValue = 2; // Deal 2 damage
            card.color = ORANGE;
            return card;
        }
        case 6: {
            Card card = CreateCard(6, "Ironforge Rifleman", 3, CARD_TYPE_MINION, 2, 2);
            card.hasBattlecry = true;
            card.battlecryValue = 1; // Deal 1 damage
            card.color = PURPLE;
            return card;
        }
        case 7: {
            Card card = CreateCard(7, "Lord of the Arena", 6, CARD_TYPE_MINION, 6, 5);
            card.taunt = true;
            card.color = GOLD;
            return card;
        }
        case 8: {
            Card card = CreateCard(8, "Wolfrider", 3, CARD_TYPE_MINION, 3, 1);
            card.charge = true;
            card.color = RED;
            return card;
        }
        case 9: {
            Card card = CreateCard(9, "Fireball", 4, CARD_TYPE_SPELL, 0, 0);
            card.spellDamage = 6;
            card.color = ORANGE;
            return card;
        }
        case 10: {
            Card card = CreateCard(10, "Healing Potion", 1, CARD_TYPE_SPELL, 0, 0);
            card.healing = 3;
            card.color = PINK;
            return card;
        }
        default: {
            Card card = CreateCard(0, "Unknown Card", 1, CARD_TYPE_MINION, 1, 1);
            card.color = LIGHTGRAY;
            return card;
        }
    }
}

// ===== GAME INITIALIZATION =====

void InitializeDecks(GameState *game) {
    // Create basic deck for both players
    int deckCards[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
                       1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
                       1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    
    for (int p = 0; p < 2; p++) {
        Player *player = &game->players[p];
        player->deckCount = 30;
        
        for (int i = 0; i < 30; i++) {
            player->deck[i] = GetCardById(deckCards[i]);
            player->deck[i].ownerPlayer = p;
        }
        
        // Shuffle deck
        for (int i = 0; i < 30; i++) {
            int j = rand() % 30;
            Card temp = player->deck[i];
            player->deck[i] = player->deck[j];
            player->deck[j] = temp;
        }
        
        // Initialize player stats
        player->playerId = p;
        sprintf(player->name, "Player %d", p + 1);
        player->health = 30;
        player->maxHealth = 30;
        player->armor = 0;
        player->mana = 0;
        player->maxMana = 0;
        player->handCount = 0;
        player->boardCount = 0;
        player->isActivePlayer = (p == 0);
        player->turnCount = 0;
        player->fatigueDamage = 0;
        player->isAlive = true;
        player->heroPowerUsed = false;
        player->hasWeapon = false;
        
        // Draw starting hand (3 cards)
        for (int i = 0; i < 3; i++) {
            DrawCardFromDeck(player);
        }
    }
}

void InitializeGame(GameState *game) {
    memset(game, 0, sizeof(GameState));
    
    // Set random seed
    srand(time(NULL));
    game->randomSeed = rand();
    
    // Initialize game state
    game->gamePhase = GAME_STATE_PLAYING;
    game->turnPhase = TURN_PHASE_START;
    game->activePlayer = 0;
    game->turnNumber = 1;
    game->selectedCard = NULL;
    game->targetCard = NULL;
    game->targetingMode = false;
    game->combatInProgress = false;
    game->activeEffectsCount = 0;
    game->queueCount = 0;
    game->turnTimer = 0.0f;
    game->winner = -1;
    game->gameEnded = false;
    
    // Setup camera (top-down like Hearthstone)
    game->camera.position = (Vector3){0.0f, 20.0f, 8.0f};
    game->camera.target = (Vector3){0.0f, 0.0f, 0.0f};
    game->camera.up = (Vector3){0.0f, 0.0f, -1.0f};
    game->camera.fovy = 45.0f;
    game->camera.projection = CAMERA_PERSPECTIVE;
    game->boardCenter = (Vector3){0.0f, 0.0f, 0.0f};
    game->cameraShake = 0.0f;
    
    // Initialize decks and hands
    InitializeDecks(game);
    
    // Start first turn
    StartTurn(game);
}

// ===== CARD MANAGEMENT =====

void DrawCardFromDeck(Player *player) {
    if (player->deckCount <= 0) {
        // Fatigue damage
        player->fatigueDamage++;
        player->health -= player->fatigueDamage;
        if (player->health <= 0) {
            player->isAlive = false;
        }
        return;
    }
    
    if (player->handCount >= MAX_HAND_SIZE) {
        // Hand full, burn the card
        player->deckCount--;
        return;
    }
    
    // Draw card
    Card drawnCard = player->deck[player->deckCount - 1];
    player->deckCount--;
    
    drawnCard.inHand = true;
    drawnCard.onBoard = false;
    
    // Position card in hand
    float handSpacing = 2.0f;
    float handStartX = -((player->handCount) * handSpacing) / 2.0f;
    drawnCard.position = (Vector3){
        handStartX + player->handCount * handSpacing,
        0.0f,
        player->playerId == 0 ? 8.0f : -8.0f
    };
    drawnCard.targetPosition = drawnCard.position;
    
    player->hand[player->handCount] = drawnCard;
    player->handCount++;
}

bool CanPlayCard(Player *player, Card *card) {
    if (player->mana < card->cost) return false;
    if (card->type == CARD_TYPE_MINION && player->boardCount >= MAX_BOARD_SIZE) return false;
    return true;
}

// ===== COMBAT SYSTEM =====

void DealDamage(void *target, int damage, Card *source, GameState *game) {
    if (damage <= 0) return;
    
    // Check if target is a card
    Card *targetCard = NULL;
    Player *targetPlayer = NULL;
    
    // Find target in players' boards
    for (int p = 0; p < 2; p++) {
        for (int i = 0; i < game->players[p].boardCount; i++) {
            if (&game->players[p].board[i] == target) {
                targetCard = (Card*)target;
                break;
            }
        }
        if (&game->players[p] == target) {
            targetPlayer = (Player*)target;
            break;
        }
    }
    
    if (targetCard) {
        // Damage to minion
        if (targetCard->divineShield) {
            targetCard->divineShield = false;
            AddVisualEffect(game, 1, targetCard->position, "Divine Shield!");
            return;
        }
        
        targetCard->health -= damage;
        AddVisualEffect(game, 0, targetCard->position, TextFormat("-%d", damage));
        
        if (targetCard->health <= 0) {
            // Minion dies
            AddVisualEffect(game, 2, targetCard->position, "Dies!");
            
            // Execute deathrattle if any
            if (targetCard->hasDeathrattle) {
                // Simple deathrattle implementation
                AddVisualEffect(game, 3, targetCard->position, "Deathrattle!");
            }
            
            // Remove from board
            Player *owner = &game->players[targetCard->ownerPlayer];
            for (int i = 0; i < owner->boardCount; i++) {
                if (&owner->board[i] == targetCard) {
                    // Shift remaining cards
                    for (int j = i; j < owner->boardCount - 1; j++) {
                        owner->board[j] = owner->board[j + 1];
                        owner->board[j].boardPosition = j;
                    }
                    owner->boardCount--;
                    break;
                }
            }
        }
    } else if (targetPlayer) {
        // Damage to player
        targetPlayer->health -= damage;
        AddVisualEffect(game, 0, (Vector3){0, 2, targetPlayer->playerId == 0 ? 6 : -6}, 
                       TextFormat("-%d", damage));
        
        if (targetPlayer->health <= 0) {
            targetPlayer->isAlive = false;
            game->gameEnded = true;
            game->winner = 1 - targetPlayer->playerId;
        }
    }
}

void AttackWithCard(GameState *game, Card *attacker, Card *target) {
    if (!attacker->canAttack || attacker->attackedThisTurn) return;
    
    AddVisualEffect(game, 4, attacker->position, "Attack!");
    
    // Deal damage
    if (target) {
        // Attacking another minion
        DealDamage(target, attacker->attack, attacker, game);
        DealDamage(attacker, target->attack, target, game);
    } else {
        // Attacking enemy hero
        Player *enemy = &game->players[1 - attacker->ownerPlayer];
        
        // Check for taunt minions
        bool hasTaunt = false;
        for (int i = 0; i < enemy->boardCount; i++) {
            if (enemy->board[i].taunt) {
                hasTaunt = true;
                break;
            }
        }
        
        if (!hasTaunt) {
            DealDamage(enemy, attacker->attack, attacker, game);
        }
    }
    
    // Mark as attacked
    attacker->attackedThisTurn = true;
    attacker->canAttack = false;
}

// ===== CARD PLAYING =====

void PlayCard(GameState *game, Card *card, Card *target) {
    Player *player = &game->players[card->ownerPlayer];
    
    if (!CanPlayCard(player, card)) return;
    
    // Spend mana
    player->mana -= card->cost;
    
    // Remove from hand
    for (int i = 0; i < player->handCount; i++) {
        if (&player->hand[i] == card) {
            for (int j = i; j < player->handCount - 1; j++) {
                player->hand[j] = player->hand[j + 1];
            }
            player->handCount--;
            break;
        }
    }
    
    switch (card->type) {
        case CARD_TYPE_MINION: {
            // Place on board
            if (player->boardCount < MAX_BOARD_SIZE) {
                card->onBoard = true;
                card->inHand = false;
                card->boardPosition = player->boardCount;
                card->canAttack = card->charge; // Can attack if has charge
                
                // Position on board
                float boardSpacing = 2.5f;
                float boardStartX = -((player->boardCount) * boardSpacing) / 2.0f;
                card->position = (Vector3){
                    boardStartX + player->boardCount * boardSpacing,
                    0.0f,
                    player->playerId == 0 ? 2.0f : -2.0f
                };
                card->targetPosition = card->position;
                
                player->board[player->boardCount] = *card;
                player->boardCount++;
                
                // Execute battlecry
                if (card->hasBattlecry && target) {
                    DealDamage(target, card->battlecryValue, card, game);
                    AddVisualEffect(game, 5, card->position, "Battlecry!");
                }
                
                AddVisualEffect(game, 6, card->position, "Summoned!");
            }
            break;
        }
        case CARD_TYPE_SPELL: {
            if (card->spellDamage > 0 && target) {
                DealDamage(target, card->spellDamage, card, game);
                AddVisualEffect(game, 7, target ? target->position : (Vector3){0,0,0}, 
                               TextFormat("Spell: -%d", card->spellDamage));
            }
            if (card->healing > 0 && target) {
                // Simple healing implementation
                if (target->health < target->maxHealth) {
                    int healAmount = card->healing;
                    if (target->health + healAmount > target->maxHealth) {
                        healAmount = target->maxHealth - target->health;
                    }
                    target->health += healAmount;
                    AddVisualEffect(game, 8, target->position, TextFormat("+%d", healAmount));
                }
            }
            break;
        }
        default:
            break;
    }
}

// ===== TURN SYSTEM =====

void StartTurn(GameState *game) {
    Player *player = &game->players[game->activePlayer];
    
    game->turnPhase = TURN_PHASE_START;
    
    // Draw card
    DrawCardFromDeck(player);
    
    // Increase mana (max 10)
    if (player->maxMana < 10) {
        player->maxMana++;
    }
    player->mana = player->maxMana;
    
    // Reset hero power
    player->heroPowerUsed = false;
    
    // Reset minion attack availability
    for (int i = 0; i < player->boardCount; i++) {
        player->board[i].canAttack = true;
        player->board[i].attackedThisTurn = false;
    }
    
    game->turnPhase = TURN_PHASE_MAIN;
    game->turnTimer = 0.0f;
    
    AddVisualEffect(game, 9, (Vector3){0, 2, 0}, 
                   TextFormat("%s's Turn", player->name));
}

void EndTurn(GameState *game) {
    game->turnPhase = TURN_PHASE_END;
    
    // Switch active player
    game->activePlayer = 1 - game->activePlayer;
    game->turnNumber++;
    
    // Start next player's turn
    StartTurn(game);
}

// ===== VISUAL EFFECTS =====

void AddVisualEffect(GameState *game, int type, Vector3 position, const char* text) {
    if (game->activeEffectsCount >= MAX_EFFECTS) return;
    
    VisualEffect *effect = &game->effects[game->activeEffectsCount];
    effect->type = type;
    effect->position = position;
    effect->position.y += 1.0f; // Lift above board
    effect->duration = 2.0f;
    effect->timer = 0.0f;
    effect->active = true;
    strncpy(effect->text, text, sizeof(effect->text) - 1);
    
    switch (type) {
        case 0: effect->color = RED; break;     // Damage
        case 1: effect->color = YELLOW; break; // Divine Shield
        case 2: effect->color = PURPLE; break; // Death
        case 3: effect->color = ORANGE; break; // Deathrattle
        case 4: effect->color = GREEN; break;  // Attack
        case 5: effect->color = BLUE; break;   // Battlecry
        case 6: effect->color = WHITE; break;  // Summon
        case 7: effect->color = PINK; break;   // Spell
        case 8: effect->color = LIME; break;   // Heal
        case 9: effect->color = GOLD; break;   // Turn start
        default: effect->color = WHITE; break;
    }
    
    game->activeEffectsCount++;
}

void UpdateEffects(GameState *game, float deltaTime) {
    for (int i = 0; i < game->activeEffectsCount; i++) {
        VisualEffect *effect = &game->effects[i];
        if (effect->active) {
            effect->timer += deltaTime;
            effect->position.y += deltaTime * 2.0f; // Float upward
            
            if (effect->timer >= effect->duration) {
                effect->active = false;
            }
        }
    }
    
    // Remove inactive effects
    int writeIndex = 0;
    for (int i = 0; i < game->activeEffectsCount; i++) {
        if (game->effects[i].active) {
            if (writeIndex != i) {
                game->effects[writeIndex] = game->effects[i];
            }
            writeIndex++;
        }
    }
    game->activeEffectsCount = writeIndex;
}

// ===== CARD RENDERING =====

void UpdateCard(Card *card, float deltaTime) {
    // Smooth movement to target position
    card->position = Vector3Lerp(card->position, card->targetPosition, deltaTime * 8.0f);
    
    // Hover effect - lift card up
    if (card->isHovered && !card->isDragging) {
        Vector3 hoverPos = card->targetPosition;
        hoverPos.y = 0.5f;
        card->position = Vector3Lerp(card->position, hoverPos, deltaTime * 12.0f);
    }
}

bool CheckCardHit(Card *card, Ray ray) {
    if (!card->inHand && !card->onBoard) return false;
    
    Vector3 size = (Vector3){1.6f, 0.1f, 2.4f};
    BoundingBox cardBox = {
        Vector3Subtract(card->position, Vector3Scale(size, 0.5f)),
        Vector3Add(card->position, Vector3Scale(size, 0.5f))
    };
    RayCollision collision = GetRayCollisionBox(ray, cardBox);
    return collision.hit;
}

void DrawCard3D(Card *card, Camera3D camera) {
    if (!card->inHand && !card->onBoard) return;
    
    Vector3 pos = card->position;
    Vector3 size = (Vector3){1.6f, 0.1f, 2.4f};
    
    // Card base
    Color cardColor = card->color;
    if (card->isSelected) {
        cardColor = ColorBrightness(cardColor, 0.3f);
    }
    
    DrawCube(pos, size.x, size.y, size.z, cardColor);
    DrawCubeWires(pos, size.x, size.y, size.z, BLACK);
    
    // Taunt indicator
    if (card->taunt) {
        DrawCube(pos, size.x + 0.2f, size.y + 0.1f, size.z + 0.2f, Fade(GOLD, 0.3f));
    }
    
    // Divine Shield indicator
    if (card->divineShield) {
        DrawCube(pos, size.x + 0.1f, size.y + 0.05f, size.z + 0.1f, Fade(YELLOW, 0.5f));
    }
    
    // Draw card info
    Vector3 textPos = Vector3Add(pos, (Vector3){0, 1.0f, 0});
    Vector2 screenPos = GetWorldToScreen(textPos, camera);
    
    if (screenPos.x >= 0 && screenPos.x <= GetScreenWidth() && 
        screenPos.y >= 0 && screenPos.y <= GetScreenHeight()) {
        
        // Card name
        int nameWidth = MeasureText(card->name, 12);
        DrawText(card->name, screenPos.x - nameWidth/2, screenPos.y - 30, 12, WHITE);
        
        // Cost
        DrawText(TextFormat("%d", card->cost), screenPos.x - 25, screenPos.y - 15, 14, YELLOW);
        
        // Attack/Health for minions
        if (card->type == CARD_TYPE_MINION) {
            DrawText(TextFormat("%d/%d", card->attack, card->health), 
                     screenPos.x + 5, screenPos.y - 15, 14, WHITE);
        }
        
        // Spell damage/healing
        if (card->type == CARD_TYPE_SPELL) {
            if (card->spellDamage > 0) {
                DrawText(TextFormat("DMG:%d", card->spellDamage), 
                         screenPos.x - 15, screenPos.y - 15, 12, RED);
            }
            if (card->healing > 0) {
                DrawText(TextFormat("HEAL:%d", card->healing), 
                         screenPos.x - 15, screenPos.y - 15, 12, GREEN);
            }
        }
        
        // Keywords
        int keywordY = screenPos.y + 5;
        if (card->charge) {
            DrawText("CHARGE", screenPos.x - 20, keywordY, 8, ORANGE);
            keywordY += 10;
        }
        if (card->taunt) {
            DrawText("TAUNT", screenPos.x - 15, keywordY, 8, GOLD);
            keywordY += 10;
        }
        if (card->poisonous) {
            DrawText("POISON", screenPos.x - 20, keywordY, 8, GREEN);
        }
    }
    
    // Selection glow
    if (card->isSelected) {
        DrawCube(pos, size.x + 0.3f, size.y + 0.1f, size.z + 0.3f, Fade(YELLOW, 0.2f));
    }
}

// ===== GAME BOARD RENDERING =====

void DrawGameBoard() {
    // Main board surface
    DrawPlane((Vector3){0, -0.5f, 0}, (Vector2){20, 16}, BROWN);
    
    // Player areas
    DrawPlane((Vector3){0, -0.4f, 6}, (Vector2){16, 4}, Fade(BLUE, 0.3f));
    DrawPlane((Vector3){0, -0.4f, -6}, (Vector2){16, 4}, Fade(RED, 0.3f));
    
    // Board center
    DrawPlane((Vector3){0, -0.4f, 0}, (Vector2){16, 8}, Fade(GREEN, 0.2f));
    
    // Board lines
    for (int i = -7; i <= 7; i++) {
        DrawLine3D((Vector3){i * 2, 0, -8}, (Vector3){i * 2, 0, 8}, DARKGRAY);
    }
    for (int i = -4; i <= 4; i++) {
        DrawLine3D((Vector3){-8, 0, i * 2}, (Vector3){8, 0, i * 2}, DARKGRAY);
    }
}

// ===== MAIN UPDATE LOOP =====

void UpdateGame(GameState *game) {
    float deltaTime = GetFrameTime();
    
    if (game->gameEnded) return;
    
    // Check for game end conditions
    for (int i = 0; i < 2; i++) {
        if (!game->players[i].isAlive) {
            game->gameEnded = true;
            game->winner = 1 - i;
            return;
        }
    }
    
    // Get mouse ray for 3D picking
    Ray mouseRay = GetMouseRay(GetMousePosition(), game->camera);
    
    // Reset hover states
    for (int p = 0; p < 2; p++) {
        for (int i = 0; i < game->players[p].handCount; i++) {
            game->players[p].hand[i].isHovered = false;
        }
        for (int i = 0; i < game->players[p].boardCount; i++) {
            game->players[p].board[i].isHovered = false;
        }
    }
    
    // Check card hovering
    Card *hoveredCard = NULL;
    for (int p = 0; p < 2; p++) {
        for (int i = 0; i < game->players[p].handCount; i++) {
            if (CheckCardHit(&game->players[p].hand[i], mouseRay)) {
                game->players[p].hand[i].isHovered = true;
                hoveredCard = &game->players[p].hand[i];
            }
        }
        for (int i = 0; i < game->players[p].boardCount; i++) {
            if (CheckCardHit(&game->players[p].board[i], mouseRay)) {
                game->players[p].board[i].isHovered = true;
                hoveredCard = &game->players[p].board[i];
            }
        }
    }
    
    // Handle mouse input
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (hoveredCard) {
            if (game->selectedCard) {
                game->selectedCard->isSelected = false;
            }
            game->selectedCard = hoveredCard;
            hoveredCard->isSelected = true;
        } else {
            if (game->selectedCard) {
                game->selectedCard->isSelected = false;
                game->selectedCard = NULL;
            }
        }
    }
    
    // Handle right click for playing cards/attacking
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        if (game->selectedCard && game->selectedCard->ownerPlayer == game->activePlayer) {
            if (game->selectedCard->inHand) {
                // Play card
                PlayCard(game, game->selectedCard, hoveredCard);
                game->selectedCard->isSelected = false;
                game->selectedCard = NULL;
            } else if (game->selectedCard->onBoard && game->selectedCard->canAttack) {
                // Attack
                AttackWithCard(game, game->selectedCard, hoveredCard);
                game->selectedCard->isSelected = false;
                game->selectedCard = NULL;
            }
        }
    }
    
    // Handle end turn
    if (IsKeyPressed(KEY_SPACE)) {
        EndTurn(game);
    }
    
    // Update card animations
    for (int p = 0; p < 2; p++) {
        for (int i = 0; i < game->players[p].handCount; i++) {
            UpdateCard(&game->players[p].hand[i], deltaTime);
        }
        for (int i = 0; i < game->players[p].boardCount; i++) {
            UpdateCard(&game->players[p].board[i], deltaTime);
        }
    }
    
    // Update effects
    UpdateEffects(game, deltaTime);
    
    // Update turn timer
    game->turnTimer += deltaTime;
}

// ===== MAIN DRAW LOOP =====

void DrawGame(GameState *game) {
    BeginMode3D(game->camera);
        // Draw environment
        DrawGameBoard();
        
        // Draw all cards
        for (int p = 0; p < 2; p++) {
            for (int i = 0; i < game->players[p].handCount; i++) {
                DrawCard3D(&game->players[p].hand[i], game->camera);
            }
            for (int i = 0; i < game->players[p].boardCount; i++) {
                DrawCard3D(&game->players[p].board[i], game->camera);
            }
        }
        
        // Draw grid for reference
        DrawGrid(20, 1.0f);
        
    EndMode3D();
    
    // Draw UI
    DrawText("Hearthstone Clone", 10, 10, 24, WHITE);
    
    // Player info
    Player *activePlayer = &game->players[game->activePlayer];
    
    DrawText(TextFormat("Turn %d - %s's Turn", game->turnNumber, activePlayer->name), 
             10, 40, 20, YELLOW);
    
    DrawText(TextFormat("Player 1: %d/%d HP, %d/%d Mana, %d Cards", 
             game->players[0].health, game->players[0].maxHealth,
             game->players[0].mana, game->players[0].maxMana,
             game->players[0].handCount), 
             10, 70, 16, game->activePlayer == 0 ? GREEN : WHITE);
             
    DrawText(TextFormat("Player 2: %d/%d HP, %d/%d Mana, %d Cards", 
             game->players[1].health, game->players[1].maxHealth,
             game->players[1].mana, game->players[1].maxMana,
             game->players[1].handCount), 
             10, 90, 16, game->activePlayer == 1 ? GREEN : WHITE);
    
    // Controls
    DrawText("Left Click: Select Card", 10, 120, 14, LIGHTGRAY);
    DrawText("Right Click: Play/Attack", 10, 140, 14, LIGHTGRAY);
    DrawText("Space: End Turn", 10, 160, 14, LIGHTGRAY);
    
    if (game->selectedCard) {
        DrawText(TextFormat("Selected: %s (%d mana)", 
                 game->selectedCard->name, game->selectedCard->cost), 
                 10, 180, 16, YELLOW);
        
        if (game->selectedCard->type == CARD_TYPE_MINION) {
            DrawText(TextFormat("Attack: %d, Health: %d", 
                     game->selectedCard->attack, game->selectedCard->health), 
                     10, 200, 14, WHITE);
        }
    }
    
    // Draw visual effects
    for (int i = 0; i < game->activeEffectsCount; i++) {
        VisualEffect *effect = &game->effects[i];
        if (effect->active) {
            Vector2 screenPos = GetWorldToScreen(effect->position, game->camera);
            float alpha = 1.0f - (effect->timer / effect->duration);
            Color effectColor = Fade(effect->color, alpha);
            
            int textWidth = MeasureText(effect->text, 16);
            DrawText(effect->text, screenPos.x - textWidth/2, screenPos.y, 16, effectColor);
        }
    }
    
    // Game end screen
    if (game->gameEnded) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.7f));
        const char* winText = TextFormat("Player %d Wins!", game->winner + 1);
        int textWidth = MeasureText(winText, 48);
        DrawText(winText, GetScreenWidth()/2 - textWidth/2, GetScreenHeight()/2 - 24, 
                 48, GOLD);
        
        DrawText("Press R to restart", GetScreenWidth()/2 - 80, GetScreenHeight()/2 + 40, 
                 20, WHITE);
    }
    
    DrawFPS(GetScreenWidth() - 80, 10);
}

// ===== MAIN FUNCTION =====

int main(void) {
    // Initialization
    const int screenWidth = 1400;
    const int screenHeight = 900;
    
    InitWindow(screenWidth, screenHeight, "Hearthstone Clone - Full Implementation");
    SetTargetFPS(60);
    
    GameState game = {0};
    InitializeGame(&game);
    
    // Main game loop
    while (!WindowShouldClose()) {
        // Handle restart
        if (game.gameEnded && IsKeyPressed(KEY_R)) {
            InitializeGame(&game);
        }
        
        // Update
        UpdateGame(&game);
        
        // Draw
        BeginDrawing();
            ClearBackground(DARKGREEN);
            DrawGame(&game);
        EndDrawing();
    }
    
    // Cleanup
    CloseWindow();
    
    return 0;
}