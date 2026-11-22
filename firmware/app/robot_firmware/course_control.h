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
#define COURSE_LIMIT_COUNTS 432000
/* A two-sensor crossing is a candidate, not proof of a geometric junction. */
typedef enum
{
    COURSE_FOLLOW,
    COURSE_CENTER,
    COURSE_BRAKE,
    COURSE_TURN,
    COURSE_SETTLE,
    COURSE_SEEK,
    COURSE_FAILED,
    COURSE_LOST
} course_phase_t;
typedef struct
{
    course_config_t config;
#if ROBOCAR_COMPACT_CONTROL_STATE
    /* Eight phases plus 19 Boolean decisions = 22 bits in three bytes.
     * All course/drive/search/trim fields are owned by the Motion task. */
    unsigned char phase : 3;
    bool center_search_started : 1;
    bool center_level_valid : 1;
    bool center_visible : 1;
    bool barcode_seen : 1;
    bool bend_disabled : 1;
    bool bend_ready : 1;
    bool bend_active : 1;
    bool flow_follow : 1;
    bool right_level_valid : 1;
    bool right_preview : 1;
    bool right_preview_tracking : 1;
    bool loss_coast : 1;
    bool ignore_current_crossing : 1;
    bool deferred : 1;
    bool candidate : 1;
    bool latched : 1;
    bool clear_tracking : 1;
    bool seek_reverse : 1;
    bool curve_follow : 1; /* long centre+side overlap is a curve, set by
                              course_set_curve_follow() */
#else
    course_phase_t phase;
#endif
    motion_control_t reference;
    motion_control_t turn;
    barcode_progress_t drive;
    center_search_t center_search;
    center_trim_t center_trim;
    int32_t center_reference;
    unsigned cruise_limit_cps;
    unsigned
        recovery_limit_counts; /* 0: normal120; rejoin may request up to240 */
    uint64_t steady_line_us;
    unsigned trim_attempts;
    unsigned trim_reversals;
    unsigned trim_no_responses;
#if !ROBOCAR_COMPACT_CONTROL_STATE
    bool center_search_started;
    bool center_level_valid;
    bool center_visible;
    bool barcode_seen;
#endif
    int32_t center_level;
    uint64_t barcode_activity_us;
    unsigned recoveries;
    unsigned confirmation_retries;
    unsigned wide_searches;
#if !ROBOCAR_COMPACT_CONTROL_STATE
    bool bend_disabled;
    bool bend_ready;
    bool bend_active;
#endif
    int bend_direction;
    uint64_t bend_hint_us;
    uint64_t bend_ready_us;
    uint64_t bend_start_us;
    uint64_t bend_strong_us;
    int32_t bend_ready_l;
    int32_t bend_ready_r;
    int32_t bend_l;
    int32_t bend_r;
    unsigned bend_attempts;
    unsigned bend_completed;
    unsigned bend_fallbacks;
    /* Current mounting is centre+right, not a symmetric straddling pair. */
#if !ROBOCAR_COMPACT_CONTROL_STATE
    bool flow_follow;
    bool right_level_valid;
    bool right_preview;
    bool right_preview_tracking;
    bool loss_coast;
#endif
    int32_t right_level;
    int32_t coast_l;
    int32_t coast_r;
    uint64_t right_preview_since;
    uint64_t coast_us;
    unsigned right_previews;
    unsigned loss_coasts;
    const char *bend_source;
    const char *bend_exit;
    uint64_t start_us;
    uint64_t last_us;
    uint64_t segment_us;
    uint64_t phase_us;
    uint64_t both_us;
    uint64_t clear_us;
    uint64_t seek_us;
    int32_t segment_l;
    int32_t segment_r;
    int32_t entry_l;
    int32_t entry_r;
    int32_t exit_l;
    int32_t exit_r;
    int32_t seek_l;
    int32_t seek_r;
    unsigned junctions;
    unsigned turns;
    unsigned route_updates;
    unsigned replacements;
    unsigned deferred_replacements;
    char pending_action;
    char pending_source;
    char turn_action;
    char turn_source;
    char deferred_action;
    char deferred_source;
    unsigned remaining_junctions;
#if !ROBOCAR_COMPACT_CONTROL_STATE
    bool ignore_current_crossing;
    bool deferred;
    bool candidate;
    bool latched;
    bool clear_tracking;
    bool seek_reverse;
#endif
    const char *fault;
    /* Optional per-letter table, set by course_set_letter_actions(); NULL keeps
     * the Y/Z-only behaviour. */
    const char *letter_actions;
    /* Set by course_set_curve_follow(): a long centre+side black overlap is a
     * curve, not a crossing. */
#if !ROBOCAR_COMPACT_CONTROL_STATE
    bool curve_follow;
#endif
    /* Multiplier on the centre-trim steering (0 = 1.0, the validated amount).
     */
    float trim_gain;
} course_control_t;
#if ROBOCAR_COMPACT_CONTROL_STATE
_Static_assert(COURSE_LOST == 7,
               "Review compact course phase width when phases change");
#endif
void course_begin(course_control_t *c, course_config_t config,
                  encoder_sample_t enc, uint64_t now);
line_check_output_t course_step(course_control_t *c, ir_roles_t ir,
                                encoder_sample_t enc, uint64_t now);
/* Safe to call only in foreground; route decisions never write PWM here.
 * New reads replace waiting instructions. Committed manoeuvres defer newest
 * instruction, except STOP which always wins. Other letters do nothing. */
const char *course_on_barcode(course_control_t *c, char letter,
                              encoder_sample_t enc, uint64_t now,
                              bool on_crossing);
bool course_reads_barcodes(const course_control_t *c);
/* Optional table of 26 action characters for the letters A..Z (0 = record
 * only), used instead of the Y/Z config. Call right after course_begin(); the
 * table must not change while the run is armed. */
void course_set_letter_actions(course_control_t *c, const char *actions);
/* Off by default (the tested crossing-too-wide stop is unchanged). When on, a
 * centre+side overlap that lasts longer than a crossing steers gently toward
 * the side sensor, and only a much longer one stops the run. */
void course_set_curve_follow(course_control_t *c, bool enabled);
/* Optional higher breakaway cap (duty 0..0.5) and faster launch ramp; 0 keeps
 * the validated defaults. */
void course_set_launch_cap(course_control_t *c, float cap_pwm);
/* Scale the small centre-trim steering so it can move wheels near their dead
 * zone; 0 keeps the default. */
void course_set_trim_gain(course_control_t *c, float gain);
/* Foreground sensor context. Normalised GP28: white=0, calibrated black=1000.
 * Barcode activity includes raw confirmed edges, not just decoded symbols. */
void course_center_level(course_control_t *c, int32_t level);
/* Set once from the stopped countdown; never adjusted by running samples. */
void course_set_center_reference(course_control_t *c, int32_t level);
/* Optional 450..1000 cps straight cruise. Default remains 600; caller supplies
 * clearance permission. Optical stability and recovery always take priority. */
void course_set_cruise_limit(course_control_t *c, unsigned cps);
void course_note_barcode_activity(course_control_t *c, uint64_t now);
void course_enable_flow_follow(course_control_t *c, bool enabled);
void course_right_level(course_control_t *c, int32_t level);
#endif
