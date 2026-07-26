# Hearthstone Clone - Game Design Document

## Table of Contents
1. [Core Game Loop](#core-game-loop)
2. [Data Structures](#data-structures)
3. [Turn System](#turn-system)
4. [Card Types & Mechanics](#card-types--mechanics)
5. [Combat System](#combat-system)
6. [Mana System](#mana-system)
7. [Card Abilities](#card-abilities)
8. [Game States](#game-states)
9. [AI System](#ai-system)
10. [Implementation Phases](#implementation-phases)

## Core Game Loop

### Game Flow Overview
```
1. Game Initialization
   ├── Load decks (30 cards each player)
   ├── Shuffle decks
   ├── Draw starting hands (3-4 cards)
   ├── Mulligan phase
   └── Determine first player

2. Turn Loop
   ├── Start Turn Phase
   │   ├── Draw card
   │   ├── Increase max mana
   │   ├── Refresh mana
   │   └── Trigger "start of turn" effects
   │
   ├── Main Phase
   │   ├── Play cards from hand
   │   ├── Attack with minions/hero
   │   ├── Use hero power
   │   └── Player actions
   │
   └── End Turn Phase
       ├── Trigger "end of turn" effects
       ├── Reset minion attack availability
       └── Switch to opponent

3. Game End Conditions
   ├── Player health ≤ 0
   ├── Deck exhaustion (fatigue)
   └── Concede/disconnect
```

## Data Structures

### Core Card Structure
```c
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
    CLASS_DEMON_HUNTER,
    CLASS_COUNT
} HeroClass;

typedef struct {
    int id;
    char name[64];
    char description[256];
    int cost;
    CardType type;
    CardRarity rarity;
    HeroClass heroClass;
    
    // Minion-specific
    int attack;
    int health;
    int maxHealth;
    bool canAttack;
    bool taunt;
    bool charge;
    bool divineShield;
    bool stealth;
    bool poisonous;
    bool windfury;
    
    // Spell-specific
    int spellDamage;
    int healing;
    
    // Weapon-specific
    int durability;
    int maxDurability;
    
    // Position and state
    Vector3 position;
    Vector3 targetPosition;
    bool isHovered;
    bool isSelected;
    bool isDragging;
    
    // Game state
    bool inHand;
    bool onBoard;
    int boardPosition;
    int ownerPlayer;
    
    // Effects and abilities
    int effectIds[8];  // Up to 8 different effects
    int effectCount;
} Card;
```

### Player Structure
```c
typedef struct {
    int playerId;
    char name[32];
    HeroClass heroClass;
    
    // Health and mana
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
    
    // Collections
    Card deck[30];
    Card hand[10];
    Card board[7];  // Max 7 minions
    Card graveyard[60];  // Dead cards
    
    // Counts
    int deckCount;
    int handCount;
    int boardCount;
    int graveyardCount;
    
    // Game state
    bool isActivePlayer;
    int turnCount;
    int fatigueDamage;
    
    // Statistics
    int totalDamageDealt;
    int totalHealing;
    int cardsPlayed;
    int minionsKilled;
} Player;
```

### Game State Structure
```c
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

typedef struct {
    GamePhase gamePhase;
    TurnPhase turnPhase;
    
    Player players[2];
    int activePlayer;
    int turnNumber;
    
    // Game history
    struct Action {
        int playerId;
        int actionType;
        int cardId;
        int targetId;
        int timestamp;
    } history[1000];
    int historyCount;
    
    // Combat resolution
    Card *attackingCard;
    Card *defendingCard;
    bool combatInProgress;
    
    // Card targeting
    Card *selectedCard;
    Card *targetCard;
    bool targetingMode;
    
    // Animation and effects
    struct Effect {
        int type;
        Vector3 position;
        float duration;
        float timer;
        Color color;
        bool active;
    } effects[50];
    int activeEffects;
    
    // Random seed for reproducible games
    unsigned int randomSeed;
    
    // Camera and rendering
    Camera3D camera;
    Vector3 boardCenter;
    float cameraShake;
} GameState;
```

## Turn System

### Turn Phases Implementation
```c
typedef enum {
    ACTION_PLAY_CARD,
    ACTION_ATTACK,
    ACTION_USE_HERO_POWER,
    ACTION_END_TURN,
    ACTION_CONCEDE
} ActionType;

void StartTurn(GameState *game) {
    Player *player = &game->players[game->activePlayer];
    
    // 1. Draw card (with fatigue check)
    if (player->deckCount > 0) {
        DrawCardFromDeck(player);
    } else {
        // Fatigue damage
        player->fatigueDamage++;
        DealDamage(player, player->fatigueDamage, NULL);
    }
    
    // 2. Increase mana (max 10)
    if (player->maxMana < 10) {
        player->maxMana++;
    }
    player->mana = player->maxMana;
    
    // 3. Reset hero power
    player->heroPowerUsed = false;
    
    // 4. Reset minion attack availability
    for (int i = 0; i < player->boardCount; i++) {
        player->board[i].canAttack = true;
    }
    
    // 5. Trigger start-of-turn effects
    TriggerStartOfTurnEffects(game);
    
    game->turnPhase = TURN_PHASE_MAIN;
}

void EndTurn(GameState *game) {
    Player *player = &game->players[game->activePlayer];
    
    // 1. Trigger end-of-turn effects
    TriggerEndOfTurnEffects(game);
    
    // 2. Reset card states
    for (int i = 0; i < player->boardCount; i++) {
        player->board[i].canAttack = false;
    }
    
    // 3. Switch active player
    game->activePlayer = 1 - game->activePlayer;
    game->turnNumber++;
    
    // 4. Start next player's turn
    StartTurn(game);
}
```

## Card Types & Mechanics

### Minion Cards
```c
typedef struct {
    // Base stats
    int baseAttack;
    int baseHealth;
    
    // Keywords
    bool battlecry;    // Effect when played
    bool deathrattle;  // Effect when dies
    bool taunt;        // Must be attacked first
    bool charge;       // Can attack immediately
    bool rush;         // Can attack minions immediately
    bool divineShield; // Ignore first damage
    bool stealth;      // Can't be targeted until attacks
    bool poisonous;    // Destroys any minion it damages
    bool windfury;     // Can attack twice per turn
    bool lifesteal;    // Damage heals hero
    
    // Tribes
    enum {
        TRIBE_NONE,
        TRIBE_BEAST,
        TRIBE_DRAGON,
        TRIBE_ELEMENTAL,
        TRIBE_MECH,
        TRIBE_MURLOC,
        TRIBE_PIRATE,
        TRIBE_DEMON
    } tribe;
    
    int attacksThisTurn;
} MinionData;
```

### Spell Cards
```c
typedef struct {
    enum {
        SPELL_TARGET_NONE,       // No target needed
        SPELL_TARGET_ANY,        // Any character
        SPELL_TARGET_MINION,     // Any minion
        SPELL_TARGET_ENEMY,      // Enemy characters
        SPELL_TARGET_FRIENDLY    // Friendly characters
    } targetType;
    
    int damage;
    int healing;
    bool aoe;  // Area of effect
    
    // Special spell types
    bool secret;    // Triggered by opponent actions
    bool quest;     // Long-term objective
    bool counter;   // Counters other spells
} SpellData;
```

### Weapon Cards
```c
typedef struct {
    int attack;
    int durability;
    int maxDurability;
    
    // Weapon abilities
    bool lifesteal;
    bool poisonous;
    bool canTargetMinions;  // Some weapons can hit minions
    
    // Durability loss conditions
    bool losesDurabilityOnAttack;
    bool losesDurabilityOnDefense;
} WeaponData;
```

## Combat System

### Attack Resolution
```c
typedef struct {
    Card *attacker;
    Card *defender;
    int attackerDamage;
    int defenderDamage;
    bool attackerDies;
    bool defenderDies;
    bool overkill;
} CombatResult;

CombatResult ResolveCombat(Card *attacker, Card *defender) {
    CombatResult result = {0};
    result.attacker = attacker;
    result.defender = defender;
    
    // Calculate damage
    result.attackerDamage = attacker->attack;
    result.defenderDamage = defender->attack;
    
    // Apply damage modifiers
    if (attacker->poisonous && defender->type == CARD_TYPE_MINION) {
        result.defenderDies = true;
    }
    
    if (defender->divineShield) {
        defender->divineShield = false;
        result.attackerDamage = 0;  // Divine shield negates damage
    }
    
    // Deal damage
    if (result.attackerDamage > 0) {
        defender->health -= result.attackerDamage;
        if (defender->health <= 0) {
            result.defenderDies = true;
            result.overkill = (-defender->health > 0);
        }
    }
    
    if (result.defenderDamage > 0 && defender->type == CARD_TYPE_MINION) {
        attacker->health -= result.defenderDamage;
        if (attacker->health <= 0) {
            result.attackerDies = true;
        }
    }
    
    // Lifesteal healing
    if (attacker->lifesteal && result.attackerDamage > 0) {
        HealHero(GetCardOwner(attacker), result.attackerDamage);
    }
    
    // Mark attacker as having attacked
    attacker->canAttack = false;
    attacker->attacksThisTurn++;
    
    return result;
}
```

### Damage and Healing
```c
void DealDamage(void *target, int damage, Card *source) {
    if (IsMinion(target)) {
        Card *minion = (Card*)target;
        
        // Check for divine shield
        if (minion->divineShield) {
            minion->divineShield = false;
            return;
        }
        
        minion->health -= damage;
        
        // Check for death
        if (minion->health <= 0) {
            DestroyMinion(minion);
        }
    } else if (IsPlayer(target)) {
        Player *player = (Player*)target;
        
        // Armor absorbs damage first
        int remainingDamage = damage;
        if (player->armor > 0) {
            int armorAbsorbed = (player->armor >= remainingDamage) ? 
                               remainingDamage : player->armor;
            player->armor -= armorAbsorbed;
            remainingDamage -= armorAbsorbed;
        }
        
        player->health -= remainingDamage;
        
        // Check for death
        if (player->health <= 0) {
            EndGame(player);
        }
    }
    
    // Trigger damage-related effects
    TriggerOnDamageEffects(target, damage, source);
}

void HealTarget(void *target, int healing, Card *source) {
    if (IsMinion(target)) {
        Card *minion = (Card*)target;
        minion->health += healing;
        if (minion->health > minion->maxHealth) {
            minion->health = minion->maxHealth;
        }
    } else if (IsPlayer(target)) {
        Player *player = (Player*)target;
        player->health += healing;
        if (player->health > player->maxHealth) {
            player->health = player->maxHealth;
        }
    }
    
    TriggerOnHealEffects(target, healing, source);
}
```

## Mana System

### Mana Management
```c
bool CanPlayCard(Player *player, Card *card) {
    // Check mana cost
    if (player->mana < card->cost) {
        return false;
    }
    
    // Check hand space (shouldn't be issue when playing)
    if (card->type == CARD_TYPE_MINION) {
        if (player->boardCount >= 7) {
            return false;  // Board full
        }
    }
    
    // Check valid targets for spells
    if (card->type == CARD_TYPE_SPELL) {
        if (!HasValidTargets(card)) {
            return false;
        }
    }
    
    return true;
}

void SpendMana(Player *player, int cost) {
    player->mana -= cost;
    if (player->mana < 0) {
        player->mana = 0;  // Safety check
    }
}

void GainMana(Player *player, int amount) {
    player->mana += amount;
    if (player->mana > player->maxMana) {
        player->mana = player->maxMana;
    }
}
```

## Card Abilities

### Effect System
```c
typedef enum {
    EFFECT_BATTLECRY,
    EFFECT_DEATHRATTLE,
    EFFECT_START_OF_TURN,
    EFFECT_END_OF_TURN,
    EFFECT_ON_DAMAGE,
    EFFECT_ON_HEAL,
    EFFECT_ON_SPELL_CAST,
    EFFECT_ON_MINION_SUMMON,
    EFFECT_ON_MINION_DEATH,
    EFFECT_AURA,  // Continuous effect
    EFFECT_SECRET,
    EFFECT_COUNT
} EffectType;

typedef struct {
    int effectId;
    EffectType type;
    int cardId;  // Card that has this effect
    
    // Effect parameters
    int value1;  // Generic values for different effects
    int value2;
    int value3;
    char stringParam[64];
    
    // Targeting
    enum {
        TARGET_SELF,
        TARGET_ALL_MINIONS,
        TARGET_FRIENDLY_MINIONS,
        TARGET_ENEMY_MINIONS,
        TARGET_ALL_CHARACTERS,
        TARGET_ENEMY_CHARACTERS,
        TARGET_RANDOM_ENEMY,
        TARGET_HERO,
        TARGET_ENEMY_HERO
    } targetType;
    
    // Conditions
    bool requiresTarget;
    bool isActive;
    int usesRemaining;  // -1 for unlimited
} CardEffect;

// Example effects
void ExecuteBattlecry(Card *card, Card *target) {
    switch (card->id) {
        case CARD_ELVEN_ARCHER:
            // Deal 1 damage to any target
            if (target) {
                DealDamage(target, 1, card);
            }
            break;
            
        case CARD_ACIDIC_SWAMP_OOZE:
            // Destroy enemy weapon
            Player *opponent = GetOpponent(GetCardOwner(card));
            if (opponent->weapon) {
                DestroyWeapon(opponent->weapon);
            }
            break;
            
        case CARD_NOVICE_ENGINEER:
            // Draw a card
            DrawCard(GetCardOwner(card));
            break;
    }
}

void ExecuteDeathrattle(Card *card) {
    switch (card->id) {
        case CARD_LOOT_HOARDER:
            // Draw a card
            DrawCard(GetCardOwner(card));
            break;
            
        case CARD_HARVEST_GOLEM:
            // Summon a 2/1 Damaged Golem
            SummonMinion(GetCardOwner(card), CARD_DAMAGED_GOLEM, 
                        card->boardPosition);
            break;
    }
}
```

## Game States

### State Management
```c
void UpdateGameState(GameState *game, float deltaTime) {
    switch (game->gamePhase) {
        case GAME_STATE_MULLIGAN:
            UpdateMulliganPhase(game);
            break;
            
        case GAME_STATE_PLAYING:
            UpdatePlayingPhase(game, deltaTime);
            break;
            
        case GAME_STATE_ENDED:
            UpdateEndGamePhase(game);
            break;
    }
    
    // Update animations and effects
    UpdateEffects(game, deltaTime);
    UpdateCardAnimations(game, deltaTime);
    
    // Update camera
    UpdateCamera(game);
}

void UpdatePlayingPhase(GameState *game, float deltaTime) {
    // Handle input
    HandlePlayerInput(game);
    
    // Check for game end conditions
    CheckGameEndConditions(game);
    
    // Update turn timer (if implemented)
    UpdateTurnTimer(game, deltaTime);
    
    // Process pending actions
    ProcessActionQueue(game);
}
```

## AI System

### Basic AI Structure
```c
typedef struct {
    int difficulty;  // 1-5
    
    // AI personality
    bool aggressive;
    bool controlOriented;
    bool faceDamageOriented;
    
    // Decision making
    float cardValueWeights[CARD_COUNT];
    float targetPriorityWeights[10];
    
    // Lookahead
    int maxDepth;
    int simulations;
} AIPlayer;

typedef struct {
    ActionType action;
    int cardIndex;
    int targetIndex;
    float score;
} AIAction;

AIAction CalculateBestAction(GameState *game, AIPlayer *ai) {
    AIAction bestAction = {0};
    float bestScore = -999999.0f;
    
    Player *aiPlayer = &game->players[1];  // Assume AI is player 1
    
    // Evaluate all possible actions
    
    // 1. Playing cards from hand
    for (int i = 0; i < aiPlayer->handCount; i++) {
        if (CanPlayCard(aiPlayer, &aiPlayer->hand[i])) {
            float score = EvaluatePlayCard(game, &aiPlayer->hand[i]);
            if (score > bestScore) {
                bestScore = score;
                bestAction.action = ACTION_PLAY_CARD;
                bestAction.cardIndex = i;
            }
        }
    }
    
    // 2. Attacking with minions
    for (int i = 0; i < aiPlayer->boardCount; i++) {
        if (aiPlayer->board[i].canAttack) {
            // Consider all possible targets
            AIAction attackAction = EvaluateAttacks(game, &aiPlayer->board[i]);
            if (attackAction.score > bestScore) {
                bestScore = attackAction.score;
                bestAction = attackAction;
            }
        }
    }
    
    // 3. Using hero power
    if (!aiPlayer->heroPowerUsed && aiPlayer->mana >= aiPlayer->heroPower.cost) {
        float score = EvaluateHeroPower(game);
        if (score > bestScore) {
            bestScore = score;
            bestAction.action = ACTION_USE_HERO_POWER;
        }
    }
    
    // 4. End turn (always available)
    float endTurnScore = EvaluateEndTurn(game);
    if (endTurnScore > bestScore) {
        bestAction.action = ACTION_END_TURN;
    }
    
    return bestAction;
}

float EvaluateBoardState(GameState *game, int playerId) {
    Player *player = &game->players[playerId];
    Player *opponent = &game->players[1 - playerId];
    
    float score = 0.0f;
    
    // Health difference
    score += (player->health - opponent->health) * 2.0f;
    
    // Board presence
    int playerBoardValue = 0;
    int opponentBoardValue = 0;
    
    for (int i = 0; i < player->boardCount; i++) {
        playerBoardValue += player->board[i].attack + player->board[i].health;
    }
    
    for (int i = 0; i < opponent->boardCount; i++) {
        opponentBoardValue += opponent->board[i].attack + opponent->board[i].health;
    }
    
    score += (playerBoardValue - opponentBoardValue) * 1.5f;
    
    // Hand size advantage
    score += (player->handCount - opponent->handCount) * 3.0f;
    
    // Mana efficiency
    score += player->mana * 0.5f;
    
    return score;
}
```

## Implementation Phases

### Phase 1: Core Infrastructure (Week 1-2)
- [x] Basic 3D card rendering (✓ Complete)
- [ ] Core data structures
- [ ] Basic game state management
- [ ] Turn system implementation
- [ ] Card playing mechanics

### Phase 2: Combat System (Week 3)
- [ ] Minion vs minion combat
- [ ] Hero attacking
- [ ] Damage and healing system
- [ ] Death and cleanup

### Phase 3: Card Types (Week 4-5)
- [ ] Minion cards with keywords
- [ ] Basic spells
- [ ] Hero powers
- [ ] Weapons

### Phase 4: Effects System (Week 6-7)
- [ ] Battlecry effects
- [ ] Deathrattle effects
- [ ] Triggered effects
- [ ] Auras and buffs

### Phase 5: Advanced Features (Week 8-10)
- [ ] Card database and loading
- [ ] Deck building
- [ ] AI opponent
- [ ] Advanced UI/animations

### Phase 6: Polish (Week 11-12)
- [ ] Sound effects and music
- [ ] Particle effects
- [ ] Balancing and testing
- [ ] Performance optimization

## Technical Architecture

### File Structure
```
hearthstone_clone/
├── src/
│   ├── core/
│   │   ├── game_state.c
│   │   ├── card.c
│   │   ├── player.c
│   │   └── turn_system.c
│   ├── combat/
│   │   ├── combat_system.c
│   │   ├── damage.c
│   │   └── effects.c
│   ├── cards/
│   │   ├── card_database.c
│   │   ├── minions.c
│   │   ├── spells.c
│   │   └── weapons.c
│   ├── ai/
│   │   ├── ai_player.c
│   │   ├── evaluation.c
│   │   └── decision_tree.c
│   ├── rendering/
│   │   ├── card_renderer.c
│   │   ├── board_renderer.c
│   │   ├── ui_renderer.c
│   │   └── effects_renderer.c
│   └── main.c
├── data/
│   ├── cards.json
│   ├── heroes.json
│   └── decks.json
├── assets/
│   ├── textures/
│   ├── sounds/
│   └── models/
└── Makefile
```

This comprehensive design provides a solid foundation for building a full Hearthstone clone. Each system is designed to be modular and extensible, allowing for easy addition of new cards, mechanics, and features.

The next step would be to start implementing Phase 1, building on your existing 3D card framework!