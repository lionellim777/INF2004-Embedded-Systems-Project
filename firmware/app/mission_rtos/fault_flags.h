#ifndef RTOS_FAULT_FLAGS_H
#define RTOS_FAULT_FLAGS_H
#include <stdint.h>

/* One shared fault bitmask instead of a separate variable per fault. Eight active bits and eight
 * latched ("seen since boot") bits live in ONE 16-bit word, so a task or an IRQ updates both with a
 * single read-modify-write under local interrupt exclusion. Cortex-M0+ has no exclusive load/store,
 * so interrupt masking is the atomic primitive here (the same one the stop gate and the network
 * snapshots use). Reads are a single aligned halfword load and need no exclusion.
 * Writers: Motion (stall, encoder), Observer (IMU), sonar poll in Motion (no echo), Network (Wi-Fi,
 * MQTT). Any writer may use set/clear from a task; no writer needs to know the others exist. */
enum {
    FAULT_MOTOR_STALL = 1u << 0, /* a wheel stopped turning while driven */
    FAULT_ENCODER     = 1u << 1, /* invalid encoder transitions */
    FAULT_IMU         = 1u << 2, /* accelerometer missing or read errors */
    FAULT_NO_ECHO     = 1u << 3, /* ultrasonic sensor gave no echo at all (not a long-range clear) */
    FAULT_WIFI_LOSS   = 1u << 4, /* Wi-Fi link down after boot */
    FAULT_MQTT_LOSS   = 1u << 5, /* broker connection lost */
    FAULT_ALL         = FAULT_MOTOR_STALL | FAULT_ENCODER | FAULT_IMU | FAULT_NO_ECHO | FAULT_WIFI_LOSS | FAULT_MQTT_LOSS
};
void fault_set(uint8_t bits);      /* raises the active bits and latches them */
void fault_clear(uint8_t bits);    /* clears active bits; the latched copy stays until reboot */
uint8_t fault_active(void);
uint8_t fault_latched(void);
#endif
