#ifndef MISSION_BARCODE_ACTIONS_H
#define MISSION_BARCODE_ACTIONS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* What the car does when it reads each barcode letter A..Z.
 *
 * The built-in table below is the default and is what the car uses after every power-up. The dashboard
 * can replace it before a run over MQTT (topic command/set_barcode_actions); the new table is refused
 * while a run is armed and is lost at the next power-up. The car answers on the barcode_actions topic.
 *
 * Action codes, one 4-bit nibble per letter (letter A is the low nibble of byte 0):
 *   0 none (record only)      5 left at next junction     10 skip 2, then right
 *   1 straight (cancel turn)  6 right at next junction    11 stop
 *   2 left 90 now            7 skip 1, then left          15 unchanged (partial updates)
 *   3 right 90 now           8 skip 1, then right
 *   4 U-turn now             9 skip 2, then left
 * Same actions as the AAI10-Calibrate dashboard; see course_config.h for their characters.
 *
 * Report record (14 bytes): the 13 packed nibble bytes, then the result of the latest command:
 * 0 none since power-up, 1 accepted, 2 refused because a run is armed, 3 refused as invalid.
 * 26 nibbles fill the 13 bytes exactly (letter Z is the high nibble of byte 12). */
#define BARCODE_ACTION_LETTERS 26u
#define BARCODE_ACTION_PACKED_BYTES 13u
#define BARCODE_ACTION_REPORT_BYTES 14u
enum { BARCODE_RESULT_NONE, BARCODE_RESULT_ACCEPTED, BARCODE_RESULT_ARMED, BARCODE_RESULT_INVALID };

/* The live table: 26 action characters for course_config_t.letter_actions (0 = record only). */
const char *barcode_actions_table(void);
/* Apply a packed command. Returns the BARCODE_RESULT_* code and changes nothing unless accepted. */
uint8_t barcode_actions_apply(const uint8_t *packed, size_t length);
/* Fill the 14-byte report. */
void barcode_actions_report(uint8_t out[BARCODE_ACTION_REPORT_BYTES]);
#endif
