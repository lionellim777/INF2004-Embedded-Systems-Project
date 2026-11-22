#include "barcode_actions.h"
#include "board.h"
#include "hardware/sync.h"
#include <string.h>

/* Nibble code to action character; index 0 is "none". */
static const char nibble_action[12] = {0,   's', 'L', 'R', 'U', 'l',
                                       'r', 'a', 'b', 'c', 'd', 'x'};
enum
{
    UNCHANGED = 15u
};

/* Fixed default. The brief's example is A left, B right, C straight, D U-turn;
 * the demo track's real meanings of A and Z are not known yet, so Z stays
 * straight (as in the accepted A/A/Z runs) and the dashboard can change any of
 * them before a run. Left and right wait for the next junction, so a barcode on
 * a plain straight does not make the car pivot off the line. */
#define DEFAULT_TABLE                                                          \
    {'l', 'r', 's', 'U', 0, 0, 0, 0, 0, 0, 0, 0, 0,                            \
     0,   0,   0,   0,   0, 0, 0, 0, 0, 0, 0, 0, 's'}
static char table[BARCODE_ACTION_LETTERS] = DEFAULT_TABLE;
static uint8_t last_result = BARCODE_RESULT_NONE;

const char *barcode_actions_table(void)
{
    return table;
}

static uint8_t nibble_of(char action)
{
    for (uint8_t i = 0; i < sizeof nibble_action; ++i)
    {
        if (nibble_action[i] == action)
        {
            return i;
        }
    }
    return 0;
}
static uint8_t nibble_at(const uint8_t *packed, unsigned letter)
{
    return (uint8_t)((packed[letter / 2u] >> ((letter & 1u) * 4u)) & 15u);
}
uint8_t barcode_actions_apply(const uint8_t *packed, size_t length)
{
    char next[BARCODE_ACTION_LETTERS];
    uint8_t result = BARCODE_RESULT_ACCEPTED;
    if (packed == NULL || length != BARCODE_ACTION_PACKED_BYTES)
    {
        result = BARCODE_RESULT_INVALID;
    }
    else
    {
        for (unsigned i = 0; i < BARCODE_ACTION_LETTERS; ++i)
        {
            uint8_t code = nibble_at(packed, i);
            if (code == UNCHANGED)
            {
                next[i] = '\0';
                continue;
            }
            if (code >= sizeof nibble_action)
            {
                result = BARCODE_RESULT_INVALID;
                break;
            }
            next[i] = nibble_action[code];
        }
    }
    uint32_t irq = save_and_disable_interrupts();
    if (result == BARCODE_RESULT_ACCEPTED)
    {
        /* The check and the write share one interrupt-off region, so a run
         * cannot arm in between. */
        bool armed;
        uint8_t reason;
        board_gate_snapshot(&armed, &reason);
        if (armed)
        {
            result = BARCODE_RESULT_ARMED;
        }
        else
        {
            for (unsigned i = 0; i < BARCODE_ACTION_LETTERS; ++i)
            {
                if (nibble_at(packed, i) != UNCHANGED)
                {
                    table[i] = next[i];
                }
            }
        }
    }
    last_result = result;
    restore_interrupts(irq);
    return result;
}
void barcode_actions_report(uint8_t out[BARCODE_ACTION_REPORT_BYTES])
{
    uint32_t irq = save_and_disable_interrupts();
    memset(out, 0, BARCODE_ACTION_REPORT_BYTES);
    for (unsigned i = 0; i < BARCODE_ACTION_LETTERS; ++i)
    {
        out[i / 2u] =
            (uint8_t)(out[i / 2u] | (nibble_of(table[i]) << ((i & 1u) * 4u)));
    }
    out[BARCODE_ACTION_PACKED_BYTES] = last_result;
    restore_interrupts(irq);
}
