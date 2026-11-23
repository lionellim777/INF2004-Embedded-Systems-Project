#include "motion_runtime.h"
#include "board.h"
#include <string.h>
#if ROBOCAR_COMPACT_CONTROL_STATE
_Static_assert(MOTION_TRACK_FAULT<8,"Track fault no longer fits compact motion fault state");
#endif
enum { RAW = 1u, STABLE = 2u, PENDING = 4u };
static int32_t delta(uint32_t now, uint32_t previous)
{
    uint32_t d = now - previous;
    return d <= INT32_MAX ? (int32_t)d : (int32_t)((int64_t)d - INT64_C(4294967296));
}
static bool speed(int64_t counts, uint32_t us, int32_t *out)
{
    if (us == 0 || us > 1000000 || counts < -1000 || counts > 1000) return false;
    /* Nominal 188.496mm circumference / CPR2543; bounded int64 intermediates. */
    int64_t result = counts * INT64_C(188496000) / ((int64_t)2543 * us);
    if (result < -500 || result > 500) return false;
    *out = (int32_t)result;
    return true;
}
void motion_runtime_init(motion_runtime_t *r, encoder_sample_t enc, uint32_t us, bool down)
{
    memset(r, 0, sizeof *r);
    motion_init(&r->motion);
    r->previous_left = enc.left_raw; r->previous_right = enc.right_raw;
    r->previous_us = r->changed_us = us;
    /* Treat boot as held until a debounced release. A held boot cannot arm. */
    r->buttons = (uint8_t)(STABLE | (down ? RAW : 0));
}
void motion_runtime_step(motion_runtime_t *r, encoder_sample_t enc, uint64_t now, bool down)
{
    uint32_t us = (uint32_t)now, dt = us - r->previous_us;
    if (dt > r->max_gap_us) r->max_gap_us = dt;
    bool armed; uint8_t reason;
    board_gate_snapshot(&armed, &reason);
    if (!armed && r->motion.phase == MOTION_ACTIVE) {
        motion_abort(&r->motion);
        r->buttons &= (uint8_t)~PENDING;
#ifdef RTOS_TRACK
        if(r->motion.kind==MOTION_COURSE){track_runtime_end();r->track.status="TRACK_STOPPED";}
#endif
    }
#ifdef RTOS_TRACK
    if(!armed)track_runtime_idle(&r->track,now);
    else if(r->motion.kind!=MOTION_COURSE)track_runtime_observe(now);
#endif
    if (down != ((r->buttons & RAW) != 0)) {
        r->buttons ^= RAW;
        r->changed_us = us;
    }
    if ((uint32_t)(us - r->changed_us) >= 30000u) {
        if (((r->buttons & STABLE) != 0) != down) {
            r->buttons ^= STABLE;
            if (down) {
                if (board_ready_to_start()) r->buttons |= PENDING;
                else r->buttons &= (uint8_t)~PENDING;
            } else if (r->buttons & PENDING) {
                r->buttons &= (uint8_t)~PENDING;
                /* Check before release prepares a FUTURE gesture: a stop
                 * during this press must cancel its pending start. */
                if (board_ready_to_start() && board_start(&r->token)) {
                    bool began;
#ifdef RTOS_TRACK
                    uint8_t selected=board_selected();
                    if(selected==MOTION_COURSE
#ifdef RTOS_OBSTACLE
                       || selected==BOARD_OBSTACLE_SELECT
#endif
                    ){
                        motion_init(&r->motion);r->motion.kind=MOTION_COURSE;
                        r->motion.start=enc;r->motion.start_us=now;r->motion.phase=MOTION_ACTIVE;
#ifdef RTOS_OBSTACLE
                        if(selected==BOARD_OBSTACLE_SELECT)
                            began=track_runtime_begin_obstacle(&r->track,r->token,enc,now);
                        else
#endif
                            began=track_runtime_begin(&r->track,r->token,enc,now);
                        if(!began){motion_abort(&r->motion);track_runtime_end();}
                    }else
#endif
                    began=motion_begin(&r->motion,(motion_kind_t)board_selected(),enc,now);
                    if(!began)board_stop(STOP_MOTION);
                }
            }
        }
        if (!down) board_note_release();
    }
    board_gate_snapshot(&armed, &reason);
    if (armed && r->motion.phase == MOTION_ACTIVE) {
#ifdef RTOS_TRACK
        if(r->motion.kind==MOTION_COURSE){
            line_check_output_t out=track_runtime_step(&r->track,enc,now);
            if(now-r->motion.start_us>=180000000u){
                r->track.status="TRACK_TIME_LIMIT";
                r->motion.phase=MOTION_DONE;board_stop(STOP_COMPLETE);track_runtime_end();
            }else if(out.stop){
                r->motion.phase=MOTION_FAULT;r->motion.fault=(motion_fault_t)MOTION_TRACK_FAULT;
                board_stop(STOP_MOTION);track_runtime_end();
            }else (void)board_apply(r->token,out.left,out.right);
        }else
#endif
        {
        motion_poll(&r->motion, enc, now);
        if (r->motion.phase == MOTION_ACTIVE)
            (void)board_apply(r->token, r->motion.left.throttle, r->motion.right.throttle);
        else board_stop(r->motion.phase == MOTION_DONE ? STOP_COMPLETE : STOP_MOTION);
        }
    }
    motion_record_t record = {.captured_ms = (uint32_t)(now / 1000u),
        .kind = (uint8_t)r->motion.kind, .phase = (uint8_t)r->motion.phase,
        .fault = (uint8_t)r->motion.fault};
    board_gate_snapshot(&armed, &record.stop_reason);
    record.stop_latched = !armed;
    record.left_counts = motion_left_position(&r->motion, enc);
    record.right_counts = motion_right_position(&r->motion, enc);
    if (r->motion.kind == MOTION_NONE) record.left_counts = record.right_counts = 0;
    int32_t count_limit=r->motion.kind==MOTION_COURSE?MOTION_TRACK_COUNTS:30000;
    record.valid = speed(-(int64_t)delta(enc.left_raw, r->previous_left), dt, &record.left_mm_s) &&
        speed(delta(enc.right_raw, r->previous_right), dt, &record.right_mm_s) &&
        record.left_counts >= -count_limit && record.left_counts <= count_limit &&
        record.right_counts >= -count_limit && record.right_counts <= count_limit;
    if (!record.valid) {
        if (armed && dt != 0) board_stop(STOP_RANGE);
        record.left_counts = record.right_counts = record.left_mm_s = record.right_mm_s = 0;
        board_gate_snapshot(&armed, &record.stop_reason);
        record.stop_latched = !armed;
    }
    board_publish(&record);
    r->previous_left = enc.left_raw; r->previous_right = enc.right_raw; r->previous_us = us;
}
