#include "calibration_core.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static calibration_core_t c;
int main(void)
{
    char out[2048]; uint32_t next, value; bool gap;
    assert(calibration_read(&c,0,out,sizeof(out),&next,&gap)==0 && !gap && next==0);
    calibration_capture(&c,"abc",3); calibration_capture(&c,"def\n",4);
    assert(calibration_read(&c,0,out,3,&next,&gap)==3 && next==3 && !gap);
    assert(memcmp(out,"abc",3)==0);
    assert(calibration_read(&c,next,out,sizeof(out),&next,&gap)==4 && memcmp(out,"def\n",4)==0);
    for (unsigned i=0;i<CAL_LOG_CAPACITY+100;i++) { char ch=(char)('a'+i%26);calibration_capture(&c,&ch,1); }
    size_t n=calibration_read(&c,0,out,sizeof(out),&next,&gap);
    assert(n==sizeof(out) && gap && next==107+sizeof(out));
    for (unsigned i=0;i<n;i++) { assert(out[i]=='a'+(100+i)%26); }
    assert(calibration_read(&c,UINT32_MAX,out,sizeof(out),&next,&gap)==sizeof(out) && gap);
    calibration_capture(&c,"\n",1);
    const char *stop="[MOTION STOP] COURSE_TEST result=BARCODE_ENCODER_STALL counts=200/900 stall_wheel=LEFT no_progress_ms=1000/0 last_target_cps=250/900 last_measured_cps=0/900 last_pwm=0.083/0.200. Motors stopped.\n";
    const char *settled="[MOTION SETTLED] L=3631 R=3627\n";
    for (size_t i=0;i<strlen(stop);i++) calibration_capture(&c,stop+i,1);
    calibration_capture(&c,settled,strlen(settled));
    assert(strcmp(c.stop,stop)==0 && strcmp(c.settled,settled)==0);
    const char *check="[CHECK] samples=4 IR_L=190..3100 edges=2\n";
    const char *timing="[TIMING] max_loop_us=35000 wifi_max_poll_us=4000\n";
    calibration_capture(&c,check,strlen(check));calibration_capture(&c,timing,strlen(timing));
    assert(strcmp(c.summary,check)==0 && strcmp(c.timing,timing)==0);
    const char *barcode="[BARCODE RESULT] decoded=Z valid=1 samples=2000 edges=30\n";
    calibration_capture(&c,barcode,strlen(barcode));assert(strcmp(c.barcode,barcode)==0);
    const char *stream="[BARCODE RESULT] mode=CONTINUOUS reads=2 sequence=AA rejected=0 timeouts=1 paused_fragments=1 last_ended_runs=11 last_ended_widths_us=20000,60000,20000 active_runs=0.\n";
    calibration_capture(&c,stream,strlen(stream));assert(strcmp(c.barcode,stream)==0);
    for(unsigned i=0;i<20000;i++)calibration_capture(&c,"Z",1);
    assert(strcmp(c.stop,stop)==0 && strcmp(c.settled,settled)==0);
    assert(strcmp(c.barcode,stream)==0);
    calibration_capture(&c,"\n[MOTION GO] next\n",18);
    assert(c.stop[0]==0 && c.settled[0]==0 && c.summary[0]==0 && c.timing[0]==0 && c.barcode[0]==0);
    const char *trace1="[SCAN EDGES 1] total=30 time_ms:level=10:B,20:W\n";
    const char *trace2="[SCAN EDGES 2] total=30 time_ms:level=\n";
    calibration_capture(&c,trace1,strlen(trace1));calibration_capture(&c,trace2,strlen(trace2));
    assert(strcmp(c.trace[0],trace1)==0 && strcmp(c.trace[1],trace2)==0);
    calibration_capture(&c,"[SCAN START] new\n",17);
    assert(!c.barcode[0] && !c.trace[0][0] && !c.trace[1][0]);
    assert(!calibration_command(&c,'f'));
    c.selection_allowed=true;
    assert(calibration_command(&c,'v'));assert(calibration_take_command(&c)=='v');
    const char *optical="[OPTICAL RESULT] samples=5000 sampling_ok=1\n";
    calibration_capture(&c,optical,strlen(optical));assert(strcmp(c.optical_result,optical)==0);
    assert(!calibration_command(&c,'z') && !calibration_command(&c,'n'));
    assert(calibration_command(&c,'s'));assert(calibration_take_command(&c)=='s');
    assert(calibration_command(&c,'c'));assert(calibration_take_command(&c)=='c');
    assert(calibration_command(&c,'b'));
    assert(calibration_take_command(&c)=='b');
    assert(calibration_command(&c,'t'));
    assert(calibration_take_command(&c)=='t');
    assert(calibration_command(&c,'f'));
    assert(!calibration_command(&c,'l'));
    assert(calibration_command(&c,'x'));
    assert(!calibration_command(&c,'r'));
    assert(calibration_take_command(&c)=='x' && calibration_take_command(&c)==-1);
    c.selection_allowed=false;assert(!calibration_command(&c,'c'));assert(calibration_command(&c,'x'));
    assert(calibration_take_command(&c)=='x');
    c.selection_allowed=true;
    const char *bad[]={"/mission/Q/s/120","/mission/s/Q/120","/mission/r/l/0","/mission/r/l/251","/mission/r/l/120junk","/mission/r/l/999999999999999999","/mission/r/l/060","/mission/r/l/-60","/course/r/2/120"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++)assert(!calibration_course_command(&c,bad[i]));
    assert(calibration_course_command(&c,"/mission/b/l/120"));
    assert(c.course.y_action=='b' && c.course.z_action=='l' && c.course.advance_mm==120);
    assert(!calibration_course_command(&c,"/mission/L/R/120"));
    assert(calibration_command(&c,'x'));assert(calibration_take_command(&c)=='x');
    const char *options="sLRUlrabcdx";
    for(unsigned i=0;i<strlen(options);i++)for(unsigned j=0;j<strlen(options);j++){
        char path[40];snprintf(path,sizeof(path),"/mission/%c/%c/120",options[i],options[j]);
        assert(calibration_course_command(&c,path));assert(calibration_take_command(&c)=='q');
    }
    assert(calibration_course_command(&c,"/mission/s/s/60"));assert(calibration_take_command(&c)=='q');
    c.selection_allowed=false;assert(!calibration_course_command(&c,"/mission/b/l/120"));
    const char *course_result="[COURSE RESULT] encounters=3 turns=1\n";
    calibration_capture(&c,course_result,strlen(course_result));assert(strcmp(c.course_result,course_result)==0);
    calibration_capture(&c,"[MOTION GO] next\n",17);assert(!c.course_result[0]);
    assert(calibration_parse_cursor("0",&value)&&value==0);
    assert(calibration_parse_cursor("4294967295",&value)&&value==UINT32_MAX);
    assert(!calibration_parse_cursor("4294967296",&value));
    assert(!calibration_parse_cursor("",&value)&&!calibration_parse_cursor("-1",&value));
    assert(!calibration_parse_cursor("1junk",&value));
    puts("Wireless log/cursor/retained-result/command-priority tests passed.");
}
