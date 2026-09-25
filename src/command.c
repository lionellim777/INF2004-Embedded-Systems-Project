#include "command.h"

#include <stdbool.h>
#include <string.h>

bool
command_frame_begin(command_frame_t *p_frame, size_t expected_length)
{
    bool b_is_valid = false;

    if (NULL != p_frame)
    {
        memset(p_frame, 0, sizeof(*p_frame));
        p_frame->expected_length = expected_length;
        p_frame->b_is_valid = (expected_length < COMMAND_PAYLOAD_SIZE);
        b_is_valid = p_frame->b_is_valid;
    }

    return (b_is_valid);
}

bool
command_frame_append(command_frame_t *p_frame, uint8_t const *p_data,
                     size_t length, bool b_is_last)
{
    bool b_is_valid = false;

    if ((NULL != p_frame) && (true == p_frame->b_is_valid))
    {
        if (((0U == length) || (NULL != p_data)) &&
            (length <= (p_frame->expected_length - p_frame->length)))
        {
            if (0U < length)
            {
                memcpy(&p_frame->payload[p_frame->length], p_data,
                       length);
                p_frame->length += length;
            }
            if ((false == b_is_last) ||
                (p_frame->expected_length == p_frame->length))
            {
                p_frame->payload[p_frame->length] = '\0';
                b_is_valid = true;
            }
        }
        p_frame->b_is_valid = b_is_valid;
    }

    return (b_is_valid);
}

bool
command_parse(char const *p_topic, char const *p_payload,
              size_t length, command_request_t *p_request)
{
    bool b_is_valid = false;
    uint32_t period_ms = 0U;
    size_t index = 0U;

    if ((NULL != p_topic) && (NULL != p_payload) &&
        (NULL != p_request) && (length < COMMAND_PAYLOAD_SIZE))
    {
        p_request->kind = COMMAND_INVALID;
        p_request->period_ms = 0U;

        if ((0 == strcmp("get_status", p_topic)) && (0U == length))
        {
            p_request->kind = COMMAND_GET_STATUS;
            b_is_valid = true;
        }
        else if ((0 == strcmp("set_period_ms", p_topic)) &&
                 (0U < length) && (5U >= length))
        {
            b_is_valid = true;
            for (index = 0U; index < length; index++)
            {
                if (('0' > p_payload[index]) ||
                    ('9' < p_payload[index]))
                {
                    b_is_valid = false;
                    break;
                }
                period_ms = (period_ms * 10U) +
                            (uint32_t)(p_payload[index] - '0');
            }

            if ((true == b_is_valid) &&
                (COMMAND_PERIOD_MIN_MS <= period_ms) &&
                (COMMAND_PERIOD_MAX_MS >= period_ms))
            {
                p_request->kind = COMMAND_SET_PERIOD;
                p_request->period_ms = period_ms;
            }
            else
            {
                b_is_valid = false;
            }
        }
        else
        {
            /* Unknown diagnostic command. */
        }
    }

    return (b_is_valid);
}
