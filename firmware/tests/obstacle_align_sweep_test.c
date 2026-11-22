/* Regression for the rejoin line-up sweep. Field log (tiles): a 60 degree
 * return, sweeps at about 44 deg/s crossed the line at 87 to 96 degrees and the
 * controller never locked. The sweep must lock onto a line crossed anywhere in
 * its bounded range whose level reaches 70% of the stopped-line reference. */
#define ROBOCAR_CENTER_LINE 1
#include "../app/robot_firmware/obstacle_bypass.c"
#include <assert.h>
#include <stdio.h>

static int run(int reference, double tape_deg, double tape_halfwidth_deg,
               int peak_level, double sweep_dps)
{
    static obstacle_bypass_t a;
    encoder_sample_t enc = {0};
    uint64_t now = 1000000;
    memset(&a, 0, sizeof a);
    obstacle_bypass_begin(&a, enc, now, reference);
    a.rejoin_good = 2;
    a.align_reversed = false;
    a.align_direction = -1;
    a.angled_probe = true;
    a.return_deg = 60;
    a.range_us = now;
    enter(&a, AVOID_ALIGN_TURN, enc, now);
    a.align_origin = obstacle_encoder_origin(enc);
    double heading = 0.0;
    for (unsigned i = 0; i < 1000; i++)
    {
        now += 10000;
        heading += sweep_dps * 0.01;
        /* Left turn: left_raw rises, right_raw rises, 2000 counts = 90 deg. */
        int32_t counts = (int32_t)(heading * 2000.0 / 90.0 / 2.0);
        enc.left_raw = (uint32_t)counts;
        enc.right_raw = (uint32_t)counts;
        int level = 0;
        double d = heading - tape_deg;
        if (d < 0)
        {
            d = -d;
        }
        if (d <= tape_halfwidth_deg)
        {
            level = peak_level;
        }
        line_check_output_t o = obstacle_bypass_step(
            &a, (ir_roles_t){false, level >= 250, false}, level, enc, now,
            i % 12 == 0, true, 100.0f);
        (void)o;
        if (a.phase == AVOID_ALIGN_CONFIRM)
        {
            return (int)i;
        }
        if (a.phase == AVOID_FAILED)
        {
            return -2;
        }
        if (a.phase == AVOID_WAIT)
        {
            return -3;
        }
    }
    return -1;
}

int main(void)
{
    static const int refs[] = {500, 700, 900, 1000};
    for (unsigned r = 0; r < 4; r++)
    {
        for (double tape = 60.0; tape <= 105.0; tape += 15.0)
        {
            for (double hw = 2.0; hw <= 8.0; hw += 3.0)
            {
                int peak = refs[r] * 80 / 100;
                assert(run(refs[r], tape, hw, peak, 44.0) > 0);
                /* Below 70% of the reference the ink is not accepted. */
                assert(run(refs[r], tape, hw, refs[r] * 50 / 100, 44.0) < 0);
            }
        }
    }
    puts("PASS align sweep locks onto a line crossed at 60 to 105 degrees");
    return 0;
}
