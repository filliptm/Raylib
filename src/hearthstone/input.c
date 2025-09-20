#include "input.h"
#include "game_state.h"
#include "combat.h"
#include "effects.h"
#include <stddef.h>
#include <stdio.h>

// Main input handling function
void HandleInput(GameState* game) {
    if (game->gameEnded) {
        // Only handle restart in game over state
        if (IsKeyPressed(KEY_R)) {
            InitializeGame(game);
        }
        return;
    }
    
    HandleMouseInput(game);
    HandleKeyboardInput(game);
}

// Handle mouse input
void HandleMouseInput(GameState* game) {
    Ray mouseRay = GetMouseRay(GetMousePosition(), game->camera);
    
    // Update card hover states
    HandleCardHover(game, mouseRay);
    
    // Handle left click - start drag
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Card* clickedCard = GetCardUnderMouse(game, mouseRay);
        
        if (clickedCard && clickedCard->ownerPlayer == game->activePlayer) {
            // Only allow dragging own cards
            HandleCardSelection(game, clickedCard);
        } else {
            // Deselect if clicking empty space or enemy cards
            if (game->selectedCard) {
                game->selectedCard->isSelected = false;
                game->selectedCard = NULL;
            }
        }
    }
    
    // Handle drag
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && game->selectedCard) {
        HandleCardDrag(game);
    }
    
    // Handle drop - this is where actions happen
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && game->selectedCard && game->selectedCard->isDragging) {
        HandleCardDrop(game);
    }
}

// Handle keyboard input
void HandleKeyboardInput(GameState* game) {
    // End turn
    if (IsKeyPressed(KEY_SPACE)) {
        HandleEndTurn(game);
    }
    
    // Cancel selection
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (game->selectedCard) {
            game->selectedCard->isSelected = false;
            game->selectedCard = NULL;
        }
        if (game->targetingMode) {
            EndTargeting(game);
        }
    }
}

// Handle card selection
void HandleCardSelection(GameState* game, Card* card) {
    // Deselect previous card
    if (game->selectedCard) {
        game->selectedCard->isSelected = false;
    }
    
    // Select new card
    game->selectedCard = card;
    card->isSelected = true;
    
    // If this requires targeting, start targeting mode
    if (card->inHand && (card->hasBattlecry || card->type == CARD_TYPE_SPELL)) {
        if (card->spellDamage > 0 || card->healing > 0 || card->battlecryValue > 0) {
            StartTargeting(game, card);
        }
    }
}

// Handle card dragging
void HandleCardDrag(GameState* game) {
    if (!game->selectedCard) return;
    
    // Allow dragging for both hand cards and board cards (for attacking)
    if (!game->selectedCard->inHand && !game->selectedCard->onBoard) return;
    
    game->selectedCard->isDragging = true;
    
    // Update card position to follow mouse
    Ray mouseRay = GetMouseRay(GetMousePosition(), game->camera);
    RayCollision groundHit = GetRayCollisionQuad(mouseRay,
        (Vector3){-10, 0, -10}, (Vector3){-10, 0, 10},
        (Vector3){10, 0, 10}, (Vector3){10, 0, -10});
    
    if (groundHit.hit) {
        game->selectedCard->position = groundHit.point;
        game->selectedCard->position.y = 1.0f;
    }
}

// Handle card drop
void HandleCardDrop(GameState* game) {
    if (!game->selectedCard) return;
    
    Card* droppedCard = game->selectedCard;
    droppedCard->isDragging = false;
    
    // Get what's under the drop position
    Ray dropRay = GetMouseRay(GetMousePosition(), game->camera);
    void* dropTarget = GetTargetUnderMouse(game, dropRay);
    
    if (droppedCard->inHand) {
        // PLAYING A CARD FROM HAND
        
        // Check if dropped on valid play area (board zone)
        bool droppedOnBoard = (droppedCard->position.z > -1 && droppedCard->position.z < 1);
        
        if (droppedCard->type == CARD_TYPE_MINION && droppedCard->hasBattlecry && dropTarget) {
            // Battlecry minion with target - cast battlecry on target
            HandlePlayCard(game, droppedCard, dropTarget);
        } else if (droppedCard->type == CARD_TYPE_MINION && droppedOnBoard) {
            // Regular minion or battlecry minion without target - play to board
            HandlePlayCard(game, droppedCard, NULL);
        } else if (droppedCard->type == CARD_TYPE_SPELL && dropTarget) {
            // Cast spell on target
            HandlePlayCard(game, droppedCard, dropTarget);
        } else {
            // Invalid drop - return to hand
            UpdateHandPositions(&game->players[droppedCard->ownerPlayer]);
        }
        
    } else if (droppedCard->onBoard) {
        // ATTACKING WITH A MINION
        
        if (dropTarget && CanAttack(droppedCard)) {
            // Attack the target
            HandleAttack(game, droppedCard, dropTarget);
        } else {
            // Invalid attack - return to board position
            UpdateBoardPositions(&game->players[droppedCard->ownerPlayer]);
        }
    }
    
    // Clear selection
    droppedCard->isSelected = false;
    game->selectedCard = NULL;
}

// Update card hover states
void HandleCardHover(GameState* game, Ray mouseRay) {
    // Reset all hover states
    for (int p = 0; p < 2; p++) {
        for (int i = 0; i < game->players[p].handCount; i++) {
            game->players[p].hand[i].isHovered = false;
        }
        for (int i = 0; i < game->players[p].boardCount; i++) {
            game->players[p].board[i].isHovered = false;
        }
    }
    
    // Check for card under mouse
    Card* hoveredCard = GetCardUnderMouse(game, mouseRay);
    if (hoveredCard) {
        hoveredCard->isHovered = true;
    }
}

// Handle playing a card
void HandlePlayCard(GameState* game, Card* card, void* target) {
    Player* player = &game->players[card->ownerPlayer];
    
    if (!CanPlayCard(player, card)) {
        // Show error effect
        AddVisualEffect(game, EFFECT_DAMAGE, card->position, "Not enough mana!");
        return;
    }
    
    // Check if we need a target
    bool needsTarget = false;
    if (card->type == CARD_TYPE_SPELL && (card->spellDamage > 0 || card->healing > 0)) {
        needsTarget = true;
    }
    if (card->type == CARD_TYPE_MINION && card->hasBattlecry && card->battlecryValue > 0) {
        needsTarget = true;
    }
    
    if (needsTarget && !target) {
        StartTargeting(game, card);
        return;
    }
    
    // Validate target
    if (needsTarget && !IsValidPlayTarget(card, target)) {
        return;
    }
    
    // Spend mana
    SpendMana(player, card->cost);
    
    // Copy card data before removing from hand (to avoid pointer invalidation)
    Card cardCopy = *card;
    
    // Remove from hand
    for (int i = 0; i < player->handCount; i++) {
        if (&player->hand[i] == card) {
            RemoveCardFromHand(player, i);
            break;
        }
    }
    
    // Play the card based on type (using the copy)
    switch (cardCopy.type) {
        case CARD_TYPE_MINION: {
            if (AddCardToBoard(player, cardCopy)) {
                Card* boardCard = &player->board[player->boardCount - 1];
                CreateSummonEffect(game, boardCard->position);
                
                // Execute battlecry if applicable
                if (boardCard->hasBattlecry) {
                    ExecuteBattlecry(game, boardCard, target);
                }
            }
            break;
        }
        case CARD_TYPE_SPELL: {
            CastSpell(game, &cardCopy, target);
            break;
        }
        default:
            break;
    }
    
    // Deselect card
    game->selectedCard = NULL;
    EndTargeting(game);
}

// Handle attacking with a card
void HandleAttack(GameState* game, Card* attacker, void* target) {
    if (!CanAttack(attacker)) {
        AddVisualEffect(game, EFFECT_DAMAGE, attacker->position, "Can't attack!");
        return;
    }
    
    // Check if we clicked on an enemy
    Card* targetCard = NULL;
    Player* targetPlayer = NULL;
    
    // Try to identify the target
    for (int p = 0; p < 2; p++) {
        if (&game->players[p] == target) {
            targetPlayer = &game->players[p];
            break;
        }
        
        for (int i = 0; i < game->players[p].boardCount; i++) {
            if (&game->players[p].board[i] == target) {
                targetCard = &game->players[p].board[i];
                break;
            }
        }
    }
    
    // Attack the target
    if (targetCard) {
        if (IsValidTarget(attacker, targetCard, game)) {
            AttackWithCard(game, attacker, targetCard);
        } else {
            // Check why the attack is invalid and give feedback
            Player* enemyPlayer = &game->players[1 - attacker->ownerPlayer];
            if (HasTauntMinions(enemyPlayer) && !targetCard->taunt) {
                AddVisualEffect(game, EFFECT_DAMAGE, attacker->position, "Must attack TAUNT first!");
            } else if (targetCard->ownerPlayer == attacker->ownerPlayer) {
                AddVisualEffect(game, EFFECT_DAMAGE, attacker->position, "Can't attack your own minions!");
            } else {
                AddVisualEffect(game, EFFECT_DAMAGE, attacker->position, "Invalid target!");
            }
        }
    } else if (targetPlayer && targetPlayer->playerId != attacker->ownerPlayer) {
        if (IsValidTarget(attacker, targetPlayer, game)) {
            AttackPlayer(game, attacker, targetPlayer);
        } else {
            // Player attack blocked by taunt
            if (HasTauntMinions(targetPlayer)) {
                AddVisualEffect(game, EFFECT_DAMAGE, attacker->position, "Must attack TAUNT first!");
            } else {
                AddVisualEffect(game, EFFECT_DAMAGE, attacker->position, "Can't attack player!");
            }
        }
    } else if (targetPlayer) {
        // Trying to attack own player - show debug info
        char debugMsg[64];
        sprintf(debugMsg, "Own player! Target:%d Attacker:%d", targetPlayer->playerId, attacker->ownerPlayer);
        AddVisualEffect(game, EFFECT_DAMAGE, attacker->position, debugMsg);
    } else {
        // No valid target, start targeting mode
        StartTargeting(game, attacker);
    }
    
    // Deselect after attacking
    if (game->selectedCard) {
        game->selectedCard->isSelected = false;
        game->selectedCard = NULL;
    }
}

// Handle end turn
void HandleEndTurn(GameState* game) {
    // Players can only end their own turn when it's the main phase
    EndTurn(game);
}

// Handle concede
void HandleConcede(GameState* game) {
    SetWinner(game, 1 - game->activePlayer);
}

// Start targeting mode
void StartTargeting(GameState* game, Card* card) {
    game->targetingMode = true;
    game->targetCard = card;
}

// Update targeting
void UpdateTargeting(GameState* game, Ray mouseRay) {
    if (!game->targetingMode) return;
    
    // Highlight valid targets
    void* target = GetTargetUnderMouse(game, mouseRay);
    if (target && IsValidPlayTarget(game->targetCard, target)) {
        // Visual feedback for valid target
    }
}

// End targeting mode
void EndTargeting(GameState* game) {
    game->targetingMode = false;
    game->targetCard = NULL;
}

// Check if a target is valid for playing a card
bool IsValidPlayTarget(Card* card, void* target) {
    if (!target) return false;
    
    // For now, any target is valid
    // This would be expanded with more complex targeting rules
    return true;
}

// Get card under mouse cursor
Card* GetCardUnderMouse(GameState* game, Ray mouseRay) {
    // Check cards in reverse order (top to bottom)
    for (int p = 1; p >= 0; p--) {
        // Check hand
        for (int i = game->players[p].handCount - 1; i >= 0; i--) {
            if (CheckCardHit(&game->players[p].hand[i], mouseRay)) {
                return &game->players[p].hand[i];
            }
        }
        
        // Check board
        for (int i = game->players[p].boardCount - 1; i >= 0; i--) {
            if (CheckCardHit(&game->players[p].board[i], mouseRay)) {
                return &game->players[p].board[i];
            }
        }
    }
    
    return NULL;
}

// Get any valid target under mouse
void* GetTargetUnderMouse(GameState* game, Ray mouseRay) {
    // First check for cards
    Card* card = GetCardUnderMouse(game, mouseRay);
    if (card) return card;
    
    // Check for player portraits
    Player* player = GetPlayerUnderMouse(game, mouseRay);
    if (player) return player;
    
    return NULL;
}

// Check if it's the player's turn
bool IsPlayerTurn(GameState* game, int playerId) {
    return game->activePlayer == playerId && game->turnPhase == TURN_PHASE_MAIN;
}

// Check if a ray hits a player portrait
bool CheckPlayerPortraitHit(Player* player, Ray ray) {
    Vector3 portraitPos = (Vector3){
        7.0f, 
        0.2f, 
        player->playerId == 0 ? 6.0f : -6.0f
    };
    Vector3 portraitSize = (Vector3){2.0f, 0.2f, 2.0f};
    
    BoundingBox portraitBox = {
        Vector3Subtract(portraitPos, Vector3Scale(portraitSize, 0.5f)),
        Vector3Add(portraitPos, Vector3Scale(portraitSize, 0.5f))
    };
    RayCollision collision = GetRayCollisionBox(ray, portraitBox);
    return collision.hit;
}

// Get player under mouse cursor
Player* GetPlayerUnderMouse(GameState* game, Ray mouseRay) {
    for (int i = 0; i < 2; i++) {
        if (CheckPlayerPortraitHit(&game->players[i], mouseRay)) {
            return &game->players[i];
        }
    }
    return NULL;
}