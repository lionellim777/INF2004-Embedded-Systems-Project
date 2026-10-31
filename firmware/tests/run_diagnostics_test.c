#include "run_diagnostics.h"
#include "motor_guard.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    volatile motor_guard_t g={0};
    assert(motor_guard_refresh(&g,1000,true));assert(!motor_guard_tick(&g,150999));
    assert(motor_guard_tick(&g,151000));assert(g.tripped && !g.armed);
    assert(!motor_guard_refresh(&g,200000,true));assert(!g.armed);
    motor_guard_reset(&g);assert(motor_guard_refresh(&g,UINT32_MAX-100,true));
    assert(!motor_guard_tick(&g,1000));assert(motor_guard_tick(&g,150000));
    motor_guard_reset(&g);assert(motor_guard_refresh(&g,0,false));assert(!motor_guard_tick(&g,1000000));
    motor_guard_reset(&g);assert(motor_guard_refresh(&g,0,true));
    assert(motor_guard_refresh(&g,100000,true));assert(!motor_guard_tick(&g,200000));assert(motor_guard_tick(&g,250000));
    run_diagnostics_t d={0};uint16_t white[3]={190,180,140},black[3]={3100,2800,800};
    bool off[3]={false,false,false},on[3]={true,true,true};
    run_diagnostics_begin(&d,3,1,10);
    run_diagnostics_sample(&d,white,off,true,true,120,true,10,999); /* old pitch ignored */
    run_diagnostics_sample(&d,black,on,false,true,0,true,11,-2);
    run_diagnostics_sample(&d,black,on,true,false,0,true,11,-99); /* stale measurements ignored */
    run_diagnostics_sample(&d,white,off,true,true,75,true,12,4);
    assert(d.samples==4 && d.sonar_valid==2 && d.sonar_no_echo==1);
    assert(d.sonar_min==75 && d.sonar_max==120);
    assert(d.pitch_samples==2 && d.pitch_min==-2 && d.pitch_max==4);
    for(unsigned i=0;i<3;i++){assert(d.ir_edges[i]==2 && d.ir_min[i]==white[i] && d.ir_max[i]==black[i]);}
    assert(d.start_humps==3 && d.start_errors==1);
    d.active=false;run_diagnostics_sample(&d,white,off,true,true,1,true,13,88);assert(d.samples==4);
    puts("PASS motor guard timeout/latch/rearm/wrap/idle and multi-sensor aggregation/freshness tests.");
}
