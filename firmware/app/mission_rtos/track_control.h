#ifndef RTOS_TRACK_CONTROL_H
#define RTOS_TRACK_CONTROL_H
/* Internal target-verified bitfields, never a wire representation.
 * All task reads/writes require local IRQ exclusion; the ADC IRQ is already
 * nonpreemptible by tasks. Do not reuse this contract on a second core.
 * head/tail0..511:9 each; event indices0..7:3 each; enabled:1; divider0..3:2.
 *27 application bits in the minimum four-byte aligned container. */
typedef struct {
    unsigned head:9,tail:9,event_head:3,event_tail:3,enabled:1,divider:2;
    unsigned :5;
} track_control_t;
_Static_assert(sizeof(track_control_t)==4,"Track control layout changed");
_Static_assert(_Alignof(track_control_t)>=4,"Track control needs word alignment");
#endif
