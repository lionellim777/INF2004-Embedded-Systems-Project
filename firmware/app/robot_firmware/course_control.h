#ifndef COURSE_CONTROL_H
#define COURSE_CONTROL_H
#include "course_config.h"
#include "barcode_drive.h"
#include "motion_control.h"
#include "center_search.h"
#include "center_trim.h"
#include "center_baseline.h"
#ifndef ROBOCAR_CENTER_LINE
#define ROBOCAR_CENTER_LINE 0
#endif
#define COURSE_LIMIT_COUNTS 160000
/* Nominal wheel geometry, as used by motion_control.c: CPR2543, 60 mm wheel. */
#define COURSE_WHEEL_CPR 2543
#define COURSE_WHEEL_CIRCUMFERENCE_MM_X100 18850
/* Barcode hold, centre-line build only. While GP26 is part way through a
 * symbol the car drives straight: no trim, bend or line search, because each
 * pivots the offset sensor across or off the stripes. The default covers the
 * 235 mm large calibration print plus margin. Past the limit the hold ends
 * and normal line recovery runs. 0 disables the hold. */
#define COURSE_BARCODE_HOLD_MM 250u
#define COURSE_BARCODE_HOLD_MAX_MM 400u
/* Straight barcode run (web test): drive straight this far, ignoring the
 * line, reading barcodes the same way as the course, then stop. Spec barcode
 * is ~141 mm, so 400 mm leaves room to start and finish on white. */
#define COURSE_STRAIGHT_MM 400u
#define COURSE_STRAIGHT_MAX_MM 700u
static inline int32_t course_mm_counts(unsigned mm)
{return (int32_t)((int64_t)mm*COURSE_WHEEL_CPR*100/COURSE_WHEEL_CIRCUMFERENCE_MM_X100);}
/* A two-sensor crossing is a candidate, not proof of a geometric junction. */
typedef enum { COURSE_FOLLOW, COURSE_CENTER, COURSE_BRAKE, COURSE_TURN,
               COURSE_SETTLE, COURSE_SEEK, COURSE_FAILED, COURSE_LOST } course_phase_t;
typedef struct {
    course_config_t config;
    course_phase_t phase;
    motion_control_t reference, turn;
    barcode_progress_t drive;
    center_search_t center_search;
    center_trim_t center_trim;
    int32_t center_reference;
    unsigned cruise_limit_cps;
    unsigned recovery_limit_counts; /* 0: normal120; rejoin may request up to240 */
    uint64_t steady_line_us;
    unsigned trim_attempts,trim_reversals,trim_no_responses;
    bool center_search_started;
    bool center_level_valid,center_visible,barcode_seen;
    int32_t center_level;
    uint64_t barcode_activity_us;
    unsigned recoveries;
    bool bend_disabled, bend_ready, bend_active;
    int bend_direction;
    uint64_t bend_hint_us, bend_ready_us, bend_start_us, bend_strong_us;
    int32_t bend_ready_l, bend_ready_r, bend_l, bend_r;
    unsigned bend_attempts, bend_completed, bend_fallbacks;
    int32_t hold_limit_counts,hold_l,hold_r;
    bool hold_requested,hold_active,hold_spent,hold_blind;
    unsigned holds,hold_limits,hold_blind_holds;
    int32_t straight_counts; /* 0: normal line following */
    uint64_t start_us,last_us,segment_us,phase_us,both_us,clear_us,seek_us;
    int32_t segment_l,segment_r,entry_l,entry_r,exit_l,exit_r,seek_l,seek_r;
    unsigned junctions,turns,route_updates,replacements,deferred_replacements;
    char pending_action,pending_source,turn_action,turn_source,deferred_action,deferred_source;
    unsigned remaining_junctions;
    bool ignore_current_crossing,deferred;
    bool candidate,latched,clear_tracking,seek_reverse;
    const char *fault;
} course_control_t;
void course_begin(course_control_t *c,course_config_t config,encoder_sample_t enc,uint64_t now);
line_check_output_t course_step(course_control_t *c,ir_roles_t ir,encoder_sample_t enc,uint64_t now);
/* Safe to call only in foreground; route decisions never write PWM here.
 * New reads replace waiting instructions. Committed manoeuvres defer newest
 * instruction, except STOP which always wins. Other letters do nothing. */
const char *course_on_barcode(course_control_t *c,char letter,encoder_sample_t enc,
                             uint64_t now,bool on_crossing);
bool course_reads_barcodes(const course_control_t *c);
/* Foreground sensor context. Normalised GP28: white=0, calibrated black=1000.
 * Barcode activity includes raw confirmed edges, not just decoded symbols. */
void course_center_level(course_control_t *c,int32_t level);
/* Set once from the stopped countdown; never adjusted by running samples. */
void course_set_center_reference(course_control_t *c,int32_t level);
/* Optional 450..1000 cps straight cruise. Default remains 600; caller supplies
 * clearance permission. Optical stability and recovery always take priority. */
void course_set_cruise_limit(course_control_t *c,unsigned cps);
void course_note_barcode_activity(course_control_t *c,uint64_t now);
/* 0..COURSE_BARCODE_HOLD_MAX_MM; 0 disables. course_begin sets the default. */
void course_set_barcode_hold_mm(course_control_t *c,unsigned mm);
/* Foreground, before course_step: true while the barcode reader is part way
 * through a symbol. A spent hold renews only after capturing goes false. */
void course_barcode_capturing(course_control_t *c,bool capturing);
/* After course_begin. 1..COURSE_STRAIGHT_MAX_MM: straight run, stops with
 * STRAIGHT_DONE after that distance on both wheels. 0 restores following. */
void course_set_straight_mm(course_control_t *c,unsigned mm);
#endif
