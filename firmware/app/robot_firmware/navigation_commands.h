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

/*
 * 3-bit barcode value for telemetry (agreed with Buddy 1):
 * 0 none, 1-4 A-D, 5 valid letter not used as a command (E-Z),
 * 6 unreadable (29 bars captured but not valid Code 39), 7 spare.
 */
typedef enum {
    BARCODE_VALUE_NONE = 0,
    BARCODE_VALUE_A = 1,
    BARCODE_VALUE_B = 2,
    BARCODE_VALUE_C = 3,
    BARCODE_VALUE_D = 4,
    BARCODE_VALUE_UNUSED_LETTER = 5,
    BARCODE_VALUE_UNREADABLE = 6
} barcode_value_t;

enum { BARCODE_VALUE_BITS = 3 };

navigation_command_t navigation_decode_symbol(char symbol);
barcode_value_t navigation_barcode_value(char letter, bool unreadable);
navigation_manoeuvre_t navigation_create_manoeuvre(
    navigation_command_t command);
const char *navigation_command_name(navigation_command_t command);

#endif
