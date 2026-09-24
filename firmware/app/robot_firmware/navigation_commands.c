#include "navigation_commands.h"

navigation_command_t navigation_decode_symbol(char symbol)
{
    navigation_command_t command;

    switch (symbol) {
        case 'A':
        case 'a':
            command = NAV_COMMAND_LEFT;
            break;

        case 'B':
        case 'b':
            command = NAV_COMMAND_RIGHT;
            break;

        case 'C':
        case 'c':
            command = NAV_COMMAND_STRAIGHT;
            break;

        case 'D':
        case 'd':
            command = NAV_COMMAND_UTURN;
            break;

        default:
            command = NAV_COMMAND_INVALID;
            break;
    }

    return command;
}

navigation_manoeuvre_t navigation_create_manoeuvre(
    navigation_command_t command)
{
    navigation_manoeuvre_t manoeuvre = {
        .command = command,
        .requested_turn_degrees = 0.0f,
        .requires_turn = false,
    };

    switch (command) {
        case NAV_COMMAND_LEFT:
            manoeuvre.requested_turn_degrees = -90.0f;
            manoeuvre.requires_turn = true;
            break;

        case NAV_COMMAND_RIGHT:
            manoeuvre.requested_turn_degrees = 90.0f;
            manoeuvre.requires_turn = true;
            break;

        case NAV_COMMAND_STRAIGHT:
            break;

        case NAV_COMMAND_UTURN:
            manoeuvre.requested_turn_degrees = 180.0f;
            manoeuvre.requires_turn = true;
            break;

        case NAV_COMMAND_NONE:
        case NAV_COMMAND_INVALID:
        default:
            break;
    }

    return manoeuvre;
}

const char *navigation_command_name(navigation_command_t command)
{
    const char *name;

    switch (command) {
        case NAV_COMMAND_LEFT:
            name = "LEFT";
            break;

        case NAV_COMMAND_RIGHT:
            name = "RIGHT";
            break;

        case NAV_COMMAND_STRAIGHT:
            name = "STRAIGHT";
            break;

        case NAV_COMMAND_UTURN:
            name = "UTURN";
            break;

        case NAV_COMMAND_NONE:
            name = "NONE";
            break;

        case NAV_COMMAND_INVALID:
        default:
            name = "INVALID";
            break;
    }

    return name;
}
