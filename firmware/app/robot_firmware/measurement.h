#ifndef INF2004_MEASUREMENT_H
#define INF2004_MEASUREMENT_H
/* Idle-only measurement helper for the dashboard "Measurements" tab:
 * encoder counts per millimetre and IMU orientation / level baseline.
 *
 * Read-only. It never touches motor, servo or PWM pins and never starts motion.
 * It stays completely silent (no I2C traffic, no output) until the browser
 * activates it with 'm', and it switches itself off when a run starts, when the
 * browser stops sending keep-alives, or on 'e'.
 *
 * Web/serial commands (lower case, only accepted while the car is idle):
 *   m  activate (also the keep-alive)      e  deactivate
 *   g  zero counts and start a distance run
 *   h  latch the distance run and report it
 *   p  capture the next IMU pose (car must be still for one second)
 *   w  restart the 7-pose sequence
 */
#include <stdbool.h>
#include <stdint.h>

#ifdef ROBOCAR_MEASUREMENTS
void measurement_init(void);
/* Returns true if ch was a measurement command (consumed, even if refused). */
bool measurement_command(int ch, bool idle, uint64_t now_us);
/* Call every main-loop iteration. Cheap when inactive. */
void measurement_poll(bool idle, uint64_t now_us);
#else
static inline void measurement_init(void) {}
static inline bool measurement_command(int ch, bool idle, uint64_t now_us)
{
    (void)ch; (void)idle; (void)now_us; return false;
}
static inline void measurement_poll(bool idle, uint64_t now_us) { (void)idle; (void)now_us; }
#endif
#endif
