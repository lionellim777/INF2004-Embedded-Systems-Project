#include "track_runtime.h"
#include "track_sensor.h"
#include "network_service.h"
#include <string.h>
#ifdef RTOS_OBSTACLE
#include "obstacle_bypass.h"
#include "obstacle_io.h"
/* The Motion task alone reads and writes the controller. */
static obstacle_bypass_t obstacle;
#endif

static int32_t level(uint16_t raw,int white,int black)
{
    int32_t n=((int32_t)raw-white)*1000/(black-white);
    return n<0?0:n>1000?1000:n;
}
static bool sensors(track_ir_t *ir,uint64_t now)
{
    track_sensor_read(ir);
    bool valid=ir->ready && ir->age_us<=TRACK_FRESH_US;
    int32_t levels[3]={level(ir->raw[0],191,3164),level(ir->raw[1],178,2908),level(ir->raw[2],141,842)};
    uint8_t pattern=0;for(unsigned i=0;i<3;i++)if(levels[i]>=IR_BLACK_LEVEL)pattern|=(uint8_t)(1u<<i);
    (void)network_line((uint32_t)(now/1000u)-ir->age_us/1000u,ir->raw,pattern,valid);
    return valid;
}
void track_runtime_idle(track_runtime_t *t,uint64_t now)
{
    track_ir_t ir;
    if(sensors(&ir,now))center_baseline_add(&t->baseline,level(ir.raw[1],178,2908),now);
}
void track_runtime_observe(uint64_t now)
{ track_ir_t ir;(void)sensors(&ir,now); }
bool track_runtime_begin(track_runtime_t *t,uint16_t token,encoder_sample_t enc,uint64_t now)
{
    track_ir_t ir;int32_t baseline,spread;
#ifdef RTOS_OBSTACLE
    t->obstacle_enabled=false;
#endif
    t->status="TRACK_START_NOT_ALIGNED";t->reads=0;t->letter=0;
    if(!sensors(&ir,now) || !center_baseline_value(&t->baseline,now,&baseline,&spread) ||
       level(ir.raw[1],178,2908)<IR_BLACK_LEVEL)return false;
    /* Reuse exact validated follow/recovery algorithm. First integration
     * milestone scans routes; Y/Z actions remain straight as in the AAZ test. */
    course_begin(&t->course,(course_config_t){'s','s',120},enc,now);
    course_enable_flow_follow(&t->course,true);
    course_set_center_reference(&t->course,baseline);
    t->token=token;t->requested_us=now;t->state=0;t->reads=0;t->letter=0;
    t->status="TRACK_DECODER_ARMING";track_request(token);return true;
}
#ifdef RTOS_OBSTACLE
bool track_runtime_begin_obstacle(track_runtime_t *t,uint16_t token,encoder_sample_t enc,uint64_t now)
{
    if(!track_runtime_begin(t,token,enc,now))return false;
    obstacle_bypass_begin(&obstacle,enc,now,t->course.center_reference);
    obstacle_io_begin();
    t->obstacle_enabled=true;
    return true;
}
#endif
line_check_output_t track_runtime_step(track_runtime_t *t,encoder_sample_t enc,uint64_t now)
{
    track_ir_t ir;
    if(!sensors(&ir,now)){t->status="TRACK_ADC_STALE";return (line_check_output_t){0,0,true,t->status};}
    if(track_faulted()){t->status="TRACK_CAPTURE_FAULT";return (line_check_output_t){0,0,true,t->status};}
    if(!t->state){
        if(!track_ready(t->token))return (line_check_output_t){0,0,now-t->requested_us>=100000u,"TRACK_DECODER_ARMING"};
        t->state=1;
    }
    if(track_recording())course_note_barcode_activity(&t->course,now);
    track_event_t event;
    while(track_event_take(t->token,&event)){
        t->reads++;t->letter=event.letter;
#ifdef RTOS_OBSTACLE
        if(!t->obstacle_enabled)
#endif
            (void)course_on_barcode(&t->course,event.letter,enc,now,event.crossing);
    }
    int32_t levels[3]={level(ir.raw[0],191,3164),level(ir.raw[1],178,2908),level(ir.raw[2],141,842)};
#ifdef RTOS_OBSTACLE
    if(t->obstacle_enabled){
        obstacle_range_t range=obstacle_io_poll();
        line_check_output_t out=obstacle_bypass_guarded_step(&obstacle,ir_roles(levels),levels[1],
            enc,now,range.fresh,range.valid,range.cm,range.error,range.no_return);
        obstacle_io_angle(obstacle.servo_angle);
        track_recoveries(obstacle.line.recoveries);
        t->status=out.state;
        return out;
    }
#endif
    course_center_level(&t->course,levels[1]);course_right_level(&t->course,levels[2]);
    line_check_output_t out=course_step(&t->course,ir_roles(levels),enc,now);
    track_recoveries(t->course.recoveries);
    t->status=out.state;return out;
}
void track_runtime_end(void)
{
#ifdef RTOS_OBSTACLE
    obstacle_io_end();
#endif
    track_stop();
}
