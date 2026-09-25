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

typedef struct
{
    char payload[COMMAND_PAYLOAD_SIZE];
    size_t expected_length;
    size_t length;
    bool b_is_valid;
} command_frame_t;

bool command_frame_begin(command_frame_t *p_frame, size_t expected_length);
bool command_frame_append(command_frame_t *p_frame,
                          uint8_t const *p_data, size_t length,
                          bool b_is_last);
bool command_parse(char const *p_topic, char const *p_payload,
                   size_t length, command_request_t *p_request);

#endif /* COMMAND_H */
