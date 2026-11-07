#ifndef MISSION_BOARD_H
#define MISSION_BOARD_H
#include "motion_record.h"
#include "safety_gate.h"

/* SDK hardware access stays on physical core0. All output paths are serialized
 * inside this module, including the IRQ's stop override. No network motor API. */
bool board_init(void);
bool board_button_down(void);
void board_note_release(void);
bool board_ready_to_start(void);
bool board_start(uint16_t *token);
bool board_apply(uint16_t token, float left, float right);
void board_stop(uint8_t reason);
void board_gate_snapshot(bool *armed, uint8_t *reason);
bool board_select(uint8_t kind);
uint8_t board_selected(void);
void board_publish(const motion_record_t *record);
void board_read(uint8_t output[MOTION_RECORD_BYTES]);
#endif
