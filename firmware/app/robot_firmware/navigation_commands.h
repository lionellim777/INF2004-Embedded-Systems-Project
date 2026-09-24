#ifndef INF2004_NAVIGATION_COMMANDS_H
#define INF2004_NAVIGATION_COMMANDS_H

#include <stdbool.h>

typedef enum {
    NAV_COMMAND_NONE,
    NAV_COMMAND_LEFT,
    NAV_COMMAND_RIGHT,
    NAV_COMMAND_STRAIGHT,
    NAV_COMMAND_UTURN,
    NAV_COMMAND_INVALID
} navigation_command_t;

typedef struct {
    navigation_command_t command;
    float requested_turn_degrees;
    bool requires_turn;
} navigation_manoeuvre_t;

navigation_command_t navigation_decode_symbol(char symbol);
navigation_manoeuvre_t navigation_create_manoeuvre(
    navigation_command_t command);
const char *navigation_command_name(navigation_command_t command);

#endif
