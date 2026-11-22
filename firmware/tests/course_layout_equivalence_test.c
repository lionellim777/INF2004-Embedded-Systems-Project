/* Differential run for the course controller: identical pseudo-random sensor
 * streams, barcode letters and curve-follow settings through course_step.
 * Prints a hash of every output and of the decision state. Build once with
 * -DROBOCAR_COMPACT_CONTROL_STATE=0 and once with =1 and compare the lines. */
#define ROBOCAR_CENTER_LINE 1
#include "../app/robot_firmware/course_control.c"
#include <stdio.h>
#include <string.h>

static uint64_t rng;
static uint32_t rnd(void)
{
    rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;
    return (uint32_t)(rng >> 33);
}
static uint64_t h = 1469598103934665603ULL;
static void mix(uint64_t v)
{
    h ^= v;
    h *= 1099511628211ULL;
}
static void mixs(const char *s)
{
    if (s == NULL)
    {
        mix(0);
        return;
    }
    while (*s != 0)
    {
        mix((unsigned char)*s++);
    }
    mix(255);
}

int main(void)
{
    static course_control_t c;
    unsigned long long steps = 0;
    unsigned long long stops = 0;
    unsigned long long turns = 0;
    unsigned long long phases = 0;
    static const char letters[] = {'Y', 'Z', 'A', 'B', 'C', 'D', 'Q'};
    static const char *const table = "lrsUlrsUlrsUlrsUlrsUlrsUlr";
    for (unsigned seed = 1; seed <= 1500; seed++)
    {
        rng = seed * 7919u;
        encoder_sample_t enc = {0};
        uint64_t now = 1000000;
        double l = 0;
        double r = 0;
        memset(&c, 0, sizeof c);
        course_begin(&c, (course_config_t){'l', 's', 120}, enc, now);
        c.drive.launch_complete = true;
        course_set_curve_follow(&c, (seed & 1u) != 0u);
        if ((seed & 2u) != 0u)
        {
            course_set_letter_actions(&c, table);
        }
        line_check_output_t out = {0};
        bool center = true;
        bool side = false;
        unsigned hold = 0;
        for (unsigned i = 0; i < 4000 && !out.stop; i++)
        {
            now += 5000 + (rnd() % 4 == 0 ? rnd() % 40000 : 0);
            l += out.left * 4000 * .005;
            r += out.right * 4000 * .005;
            enc.left_raw = (uint32_t)-(int32_t)l;
            enc.right_raw = (uint32_t)(int32_t)r;
            if (hold == 0)
            {
                /* Straight tape, junction crossings, long curve overlaps and
                 * line loss, in random lengths. */
                switch (rnd() % 6)
                {
                case 0:
                    center = false;
                    side = false;
                    break;
                case 1:
                case 2:
                    center = true;
                    side = false;
                    break;
                default:
                    center = true;
                    side = true;
                    break;
                }
                hold = 1 + rnd() % (rnd() % 5 == 0 ? 300 : 30);
            }
            else
            {
                hold--;
            }
            if (rnd() % 400 == 0)
            {
                const char *res = course_on_barcode(
                    &c, letters[rnd() % sizeof letters], enc, now, side);
                mixs(res);
            }
            course_center_level(&c, center ? 700 : 100);
            out = course_step(&c, (ir_roles_t){false, center, side}, enc, now);
            mix((uint32_t)(int32_t)(out.left * 1000));
            mix((uint32_t)(int32_t)(out.right * 1000));
            mix(out.stop);
            mixs(out.state);
            mix(c.phase);
            mix(c.junctions);
            mix(c.turns);
            mix(c.remaining_junctions);
            mix((unsigned char)c.pending_action);
            mix((unsigned char)c.turn_action);
            mix(c.center_search_started);
            mix(c.center_level_valid);
            mix(c.center_visible);
            mix(c.barcode_seen);
            mix(c.bend_disabled);
            mix(c.bend_ready);
            mix(c.bend_active);
            mix(c.flow_follow);
            mix(c.right_level_valid);
            mix(c.right_preview);
            mix(c.right_preview_tracking);
            mix(c.loss_coast);
            mix(c.ignore_current_crossing);
            mix(c.deferred);
            mix(c.candidate);
            mix(c.latched);
            mix(c.clear_tracking);
            mix(c.seek_reverse);
            mix(c.curve_follow);
            mix(c.center_search.confirmation_retries);
            mix(c.drive.left_target);
            mix(c.drive.right_target);
            phases |= 1ULL << c.phase;
            steps++;
        }
        stops += out.stop;
        turns += c.turns;
    }
    printf("course layout hash %016llx steps=%llu stops=%llu turns=%llu "
           "phases=0x%llx\n",
           (unsigned long long)h, steps, stops, turns, phases);
    return 0;
}
