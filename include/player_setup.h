/**
 * @file player_setup.h
 * @brief US03 player count, character selection, and turn-order setup.
 */
#ifndef RICHMAN_PLAYER_SETUP_H
#define RICHMAN_PLAYER_SETUP_H

#include <stdio.h>

#include "game.h"

#define PLAYER_SETUP_MIN_PLAYERS 2
#define PLAYER_SETUP_MAX_PLAYERS MAX_PLAYERS

typedef enum {
    PLAYER_SETUP_OK = 0,
    PLAYER_SETUP_INVALID_COUNT,
    PLAYER_SETUP_INVALID_CHARACTER,
    PLAYER_SETUP_DUPLICATE_CHARACTER,
    PLAYER_SETUP_IO_ERROR
} PlayerSetupStatus;

typedef struct {
    int selection;
    char id;
    const char *name;
    const char *color;
} PlayerSetupCharacter;

/** Returns immutable character metadata for selection 1-4, or NULL. */
const PlayerSetupCharacter *player_setup_character(int selection);

/** Returns a stable English name for a setup status. */
const char *player_setup_status_name(PlayerSetupStatus status);

/**
 * Applies a compact character sequence such as "12" or "314".
 * Validation completes before game is modified. Sequence order becomes turn
 * order.
 */
PlayerSetupStatus player_setup_apply_sequence(Game *game, const char *choices);

/** Prints the selected players in their game/turn order. */
PlayerSetupStatus player_setup_print_summary(const Game *game, FILE *output);

/** Runs the interactive US03 command-line setup flow. */
PlayerSetupStatus player_setup_run(Game *game, FILE *input, FILE *output);

#endif /* RICHMAN_PLAYER_SETUP_H */
