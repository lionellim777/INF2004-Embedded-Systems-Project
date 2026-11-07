/* Letter-to-action table: A-X configurable, Y/Z unchanged, unset letters
 * record only. Also checks the Wi-Fi /mission path that carries the table. */
#include "course_control.h"
#include "wireless/calibration_core.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static course_control_t c;
static calibration_core_t core;

static void begin(course_config_t config)
{
    course_begin(&c,config,(encoder_sample_t){0},1000000);
    assert(c.phase!=COURSE_FAILED);
}

static bool mission(const char *path)
{
    memset(&core,0,sizeof(core));
    core.selection_allowed=true;
    return calibration_course_command(&core,path);
}

int main(void)
{
    /* Default table: only Y/Z act, everything else is record only. */
    course_config_t config={'s','x',120,{0}};
    assert(course_config_valid(config));
    begin(config);
    assert(!strcmp(course_on_barcode(&c,'A',(encoder_sample_t){0},1000000,false),"IGNORED_UNMAPPED"));
    assert(!strcmp(course_on_barcode(&c,'Q',(encoder_sample_t){0},1000000,false),"IGNORED_UNMAPPED"));
    assert(!strcmp(course_on_barcode(&c,'Z',(encoder_sample_t){0},1000000,false),"STOP_NOW"));

    /* A mapped: stop, right at next junction, U-turn now. */
    config=(course_config_t){'s','s',120,{0}};
    config.letter_action['A'-'A']='x';
    config.letter_action['B'-'A']='r';
    config.letter_action['D'-'A']='U';
    assert(course_config_valid(config));
    assert(course_config_action(&config,'A')=='x');
    assert(course_config_action(&config,'B')=='r');
    assert(course_config_action(&config,'C')==0);
    assert(course_config_action(&config,'Y')=='s' && course_config_action(&config,'Z')=='s');
    assert(course_config_action(&config,'a')==0 && course_config_action(&config,'*')==0);
    begin(config);
    assert(!strcmp(course_on_barcode(&c,'C',(encoder_sample_t){0},1000000,false),"IGNORED_UNMAPPED"));
    assert(!strcmp(course_on_barcode(&c,'B',(encoder_sample_t){0},1000000,false),"WAIT_FOR_FUTURE_JUNCTION"));
    assert(c.pending_source=='B' && c.remaining_junctions==1);
    begin(config);
    assert(!strcmp(course_on_barcode(&c,'D',(encoder_sample_t){0},1000000,false),"TURN_NOW"));
    assert(c.turn_action=='U' && c.turn_source=='D');
    begin(config);
    assert(!strcmp(course_on_barcode(&c,'A',(encoder_sample_t){0},1000000,false),"STOP_NOW"));

    /* An invalid entry makes the whole table invalid. */
    config.letter_action[5]='Q';
    assert(!course_config_valid(config));

    /* Wi-Fi path: old format still works and clears the table. */
    assert(mission("/mission/b/l/120"));
    assert(core.course.y_action=='b' && core.course.z_action=='l' && core.course.advance_mm==120);
    for(unsigned i=0;i<24;i++)assert(core.course.letter_action[i]==0);
    assert(mission("/mission/s/s/60"));
    /* New format: 24 actions for A-X, '-' = record only. */
    assert(mission("/mission/s/x/120/lr-U--------------------"));
    assert(core.course.letter_action[0]=='l' && core.course.letter_action[1]=='r');
    assert(core.course.letter_action[2]==0 && core.course.letter_action[3]=='U');
    assert(core.course.z_action=='x');
    assert(mission("/mission/s/s/60/-----------------------x"));
    assert(core.course.letter_action[23]=='x');
    const char *bad[]={
        "/mission/s/s/120/lr-U",                       /* too short */
        "/mission/s/s/120/lr-U---------------------",   /* 25 letters */
        "/mission/s/s/120/lr-Q--------------------",    /* invalid action */
        "/mission/s/s/120/",                            /* empty table */
        "/mission/s/s/12/------------------------",     /* advance too small */
        "/mission/s/s/0120/------------------------",   /* leading zero */
        "/mission/s/s/1x0/------------------------",    /* not a number */
        "/mission/s/s",
        "/m",
    };
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++)assert(!mission(bad[i]));

    puts("PASS course letters: A-X table, Y/Z unchanged, unset letters record only, Wi-Fi mission path old and new formats.");
    return 0;
}
