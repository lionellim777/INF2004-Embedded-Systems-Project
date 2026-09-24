#ifndef COMMAND_H
#define COMMAND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define COMMAND_PAYLOAD_SIZE 32U
#define COMMAND_PERIOD_DEFAULT_MS 200U
#define COMMAND_PERIOD_MIN_MS 200U
#define COMMAND_PERIOD_MAX_MS 5000U

typedef enum
{
    COMMAND_INVALID,
    COMMAND_GET_STATUS,
    COMMAND_SET_PERIOD
} command_kind_t;

typedef struct
{
    command_kind_t kind;
    uint32_t period_ms;
} command_request_t;

bool command_parse(char const *p_topic, char const *p_payload,
                   size_t length, command_request_t *p_request);

#endif /* COMMAND_H */
