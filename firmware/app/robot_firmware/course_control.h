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
    unsigned recoveries,confirmation_retries,wide_searches;
    bool bend_disabled, bend_ready, bend_active;
    int bend_direction;
    uint64_t bend_hint_us, bend_ready_us, bend_start_us, bend_strong_us;
    int32_t bend_ready_l, bend_ready_r, bend_l, bend_r;
    unsigned bend_attempts, bend_completed, bend_fallbacks;
    /* Current mounting is centre+right, not a symmetric straddling pair. */
    bool flow_follow, right_level_valid, right_preview, right_preview_tracking, loss_coast;
    int32_t right_level,coast_l,coast_r;
    uint64_t right_preview_since,coast_us;
    unsigned right_previews,loss_coasts;
    const char *bend_source,*bend_exit;
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
void course_enable_flow_follow(course_control_t *c,bool enabled);
void course_right_level(course_control_t *c,int32_t level);
#endif
