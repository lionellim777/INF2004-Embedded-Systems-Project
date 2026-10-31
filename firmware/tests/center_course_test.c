#define ROBOCAR_CENTER_LINE 1
#include "../app/robot_firmware/course_control.c"
#include <assert.h>
#include <stdio.h>
static course_control_t c;
static uint64_t now;
static int32_t l,r;
static encoder_sample_t enc(void){return (encoder_sample_t){.left_raw=(uint32_t)-l,.right_raw=(uint32_t)r};}
static void reset(void){now=1000000;l=r=0;course_begin(&c,(course_config_t){'s','x',120},enc(),now);c.drive.launch_complete=true;} /* Isolate steering here; launch has separate plant tests. */
static line_check_output_t step(bool center,bool right,int dl,int dr,unsigned us)
{now+=us;l+=dl;r+=dr;return course_step(&c,(ir_roles_t){0,center,right},enc(),now);}
int main(void){
 reset();
 for(int i=0;i<20;i++){line_check_output_t o=step(true,false,60,60,100000);assert(!o.stop&&o.left>=0&&o.right>=0);assert(c.drive.left_target<=600&&c.drive.right_target<=600);}
 assert(c.junctions==0 && course_reads_barcodes(&c));
 /* Centre loss immediately brakes and pauses barcode acquisition. GP27
  * selects the first search direction; centre confirmation is mandatory. */
 for(int side=0;side<2;side++){
  reset();line_check_output_t o=step(false,side,0,0,10000);
  assert(!o.stop&&o.left==0&&o.right==0&&c.phase==COURSE_LOST&&!course_reads_barcodes(&c));
  o=step(false,side,0,0,50000);assert(!o.stop&&(side?o.left>0:o.left<0)&&o.left==-o.right);
  int dir=side?1:-1;
  o=step(true,false,dir*10,-dir*10,10000);assert(!o.stop&&o.left==0&&c.phase==COURSE_LOST);
  o=step(false,false,0,0,10000);assert(!o.stop&&c.phase==COURSE_LOST); /* flicker does not confirm */
  o=step(true,false,0,0,10000);assert(!o.stop&&c.phase==COURSE_LOST);
  o=step(true,false,0,0,60000);assert(!o.stop&&c.phase==COURSE_FOLLOW&&c.recoveries==1&&!c.turns);
  assert(course_reads_barcodes(&c));
  o=step(true,false,0,0,10000);assert(!o.stop&&o.left>=0&&o.right>=0);
 }
 reset();step(false,false,0,0,10000);
 line_check_output_t o=step(false,false,-120,120,100000);assert(!o.stop&&o.left==0&&c.center_search.reversed);
 o=step(false,false,240,-240,100000);assert(o.stop&&!strcmp(o.state,"CENTER_LINE_NOT_REACQUIRED"));
 reset();step(false,true,0,0,10000);
 o=step(false,true,-65,65,10000);assert(o.stop&&!strcmp(o.state,"CENTER_SEARCH_WRONG_DIRECTION"));
 reset();step(false,false,0,0,10000);
 for(int i=0;i<10;i++)o=step(false,false,0,10,100000);
 assert(o.stop&&!strcmp(o.state,"CENTER_SEARCH_ENCODER_STALL"));
 reset();step(false,false,0,0,10000);
 for(int i=0;i<30;i++)o=step(true,false,0,0,100000); /* cannot time out into success: first confirms */
 assert(c.phase==COURSE_FAILED); /* stationary resumed forward motion still stalls */
 reset();step(false,false,0,0,10000);
 assert(!strcmp(course_on_barcode(&c,'Z',enc(),now,false),"STOP_NOW"));
 assert(step(true,false,0,0,10000).stop);
 reset();step(false,false,0,0,10000);
 assert(!strcmp(course_on_barcode(&c,'Y',enc(),now,false),"DEFERRED_UNTIL_MANOEUVRE_COMPLETE"));
 step(true,false,0,0,10000);step(true,false,0,0,60000);
 assert(c.phase==COURSE_FOLLOW&&!c.deferred);
 /* Both black is a crossing candidate; it goes straight without a pivot. */
 reset();step(true,true,6,6,10000);o=step(true,true,12,12,20000);
 assert(!o.stop&&c.junctions==1&&!c.recoveries&&o.left>=0&&o.right>=0);
 /* Continuous sensor flicker cannot extend the total search indefinitely. */
 center_search_t s;center_search_begin(&s,1,0,0,0);
 for(unsigned t=10000;t<=3000000;t+=10000){
  int sign=s.direction;int32_t dl=s.origin_l+sign*10,dr=s.origin_r-sign*10;
  o=center_search_step(&s,(t/10000)%2,dl,dr,t);
  if(o.stop)break;
 }
 assert(o.stop&&!s.found);
 /* Weak-edge hysteresis must avoid repeated recovery triggers. */
 reset();course_center_level(&c,500);step(true,false,0,0,10000);
 for(int i=0;i<12;i++){
  course_center_level(&c,i%2?220:260);
  o=step(false,false,6,6,10000);assert(!o.stop && c.phase==COURSE_FOLLOW);
 }
 course_center_level(&c,199);o=step(true,false,0,0,10000);
 assert(c.phase==COURSE_LOST && !o.stop);
 /* Ordinary black/edge is insufficient: keep searching until strong black. */
 course_center_level(&c,300);o=step(true,false,-10,10,50000);
 assert(!o.stop && c.phase==COURSE_LOST && o.left<0);
 course_center_level(&c,399);o=step(true,false,-10,10,40000);
 assert(!o.stop && c.phase==COURSE_LOST && !c.center_search.confirming);
 course_center_level(&c,425);o=step(true,false,-10,10,10000);
 assert(o.left==0 && !c.center_search.found);
 course_center_level(&c,299);step(true,false,0,0,10000);
 course_center_level(&c,450);step(true,false,0,0,10000);
 o=step(true,false,0,0,60000);assert(!o.stop && c.recoveries==1);
 /* Barcode activity permits a local correction, never a new broad budget. */
 reset();course_note_barcode_activity(&c,now);
 o=step(false,false,0,0,10000);
 assert(!o.stop && !o.left && !o.right && c.center_search.limit==60);
 o=step(false,false,-20,20,60000);assert(!o.stop && o.left<0);
 o=step(true,false,-10,10,10000);assert(!o.stop);
 o=step(true,false,0,0,60000);assert(!o.stop && c.recoveries==1);
 reset();step(false,false,0,0,10000);
 course_note_barcode_activity(&c,now);
 o=step(false,false,-60,60,60000);
 assert(!o.stop && c.center_search.limit==60 && c.center_search.reversed);
 course_note_barcode_activity(&c,now); /* more barcode edges must not extend */
 o=step(true,false,120,-120,100000);
 assert(o.stop && !strcmp(o.state,"CENTER_LOCAL_RECOVERY_EXHAUSTED"));
 reset();course_note_barcode_activity(&c,now);
 for(int i=0;i<20;i++)step(true,false,60,60,100000);
 o=step(false,false,0,0,10000);assert(!o.stop && c.center_search.limit==120);
 /* Optical steering happens BEFORE line loss and keeps barcode capture on. */
 reset();course_set_center_reference(&c,650);course_center_level(&c,650);step(true,false,0,0,10000);
 course_center_level(&c,490);course_note_barcode_activity(&c,now);
 o=step(true,false,60,60,100000);
 assert(!o.stop && !strcmp(o.state,"CENTER_TRIM_LEFT"));
 assert(c.phase==COURSE_FOLLOW && course_reads_barcodes(&c));
 assert(c.drive.left_target<c.drive.right_target && c.drive.left_target>=300);
 assert(c.recoveries==0 && c.trim_attempts==1);
 /* If the trial makes reflectance worse after real yaw, reverse once. */
 course_center_level(&c,400);o=step(true,false,20,50,100000);
 assert(!o.stop && c.drive.center_steer>0 && c.trim_reversals==1);
 course_center_level(&c,600);o=step(true,false,60,30,100000);
 assert(!o.stop && !c.center_trim.active && c.center_trim.last_good==1);
 assert(course_reads_barcodes(&c) && !c.recoveries);
 /* GP27 black plus weak centre suggests right, not a false crossing. */
 reset();course_set_center_reference(&c,650);course_center_level(&c,650);step(true,false,0,0,10000);
 course_center_level(&c,490);o=step(true,true,60,60,100000);
 assert(!o.stop && c.drive.center_steer>0 && !c.junctions && !c.recoveries);
 /* Time/differential-travel limits stop endless weak-edge steering. */
 center_trim_t t={0};
 assert(center_trim_step(&t,450,650,false,0,0,0)<0);
 assert(center_trim_step(&t,450,650,false,0,120,100000)>0);
 assert(center_trim_step(&t,450,650,false,120,120,200000)==0 && t.exhausted);
 t=(center_trim_t){0};center_trim_step(&t,450,650,false,0,0,0);
 center_trim_step(&t,450,650,false,0,0,500000);
 center_trim_step(&t,450,650,false,0,0,1000000);assert(t.exhausted);
 /* Recorded v5.28 run1: raw1243 ->1351 ->1538 improves during a left
  * trim, yet the old500ms rule reversed it. Replay normalized GP28 levels. */
 t=(center_trim_t){0};
 assert(center_trim_step(&t,(1243-178)*1000/2730,635,false,0,0,0)<0);
 float strong=-center_trim_step(&t,(1351-178)*1000/2730,635,false,80,110,250000);
 float gentle=-center_trim_step(&t,(1538-178)*1000/2730,635,false,160,220,500000);
 assert(gentle>0 && gentle<strong && !t.reversed && !t.exhausted);
 assert(center_trim_step(&t,(1800-178)*1000/2730,635,false,260,340,750000)==0);
 assert(!t.active && t.last_good==-1);
 /* Measurable improvement also survives120 differential counts, but the
  * absolute240-count limit never grows, even if reflectance improves. */
 t=(center_trim_t){0};center_trim_step(&t,400,800,true,0,0,0);
 assert(center_trim_step(&t,450,800,true,120,0,250000)>0 && !t.reversed);
 assert(center_trim_step(&t,500,800,true,240,0,500000)==0 && t.exhausted);
 /* Falling from a useful peak is deterioration even above the entry value. */
 t=(center_trim_t){0};center_trim_step(&t,400,800,false,0,0,0);
 assert(center_trim_step(&t,500,800,false,0,30,100000)<0);
 assert(center_trim_step(&t,450,800,false,0,60,200000)>0 && t.reversed);
 /* Frequent improvements must not defeat the overall time budget. */
 t=(center_trim_t){0};
 for(int i=0;i<15;i++)assert(center_trim_step(&t,300+i*20,1000,false,0,i*3,i*100000u)<0);
 assert(center_trim_step(&t,600,1000,false,0,45,1500000)==0 && t.exhausted);
 /* Weak-edge steering has both forward targets positive and at most600;
  * ordinary centred travel returns to600 through the existing ramp. */
 reset();course_set_center_reference(&c,650);course_center_level(&c,650);step(true,false,0,0,10000);
 course_center_level(&c,540);o=step(true,false,60,60,100000);
 assert(c.phase==COURSE_FOLLOW && course_reads_barcodes(&c));
 assert(c.drive.left_target>0 && c.drive.left_target<c.drive.right_target && c.drive.right_target==600);
 course_center_level(&c,650);o=step(true,false,35,45,100000);
 assert(!c.center_trim.active && c.drive.left_target<=600 && c.drive.right_target<=600 && !o.stop);
 /* Latest physical failure: requested left but encoders move right.
  * That is a drive failure, not evidence that left was the wrong choice. */
 t=(center_trim_t){0};center_trim_step(&t,500,670,false,0,0,0);
 assert(center_trim_step(&t,400,670,false,50,10,100000)<0 && !t.reversed);
 for(unsigned i=2;i<10;i++)center_trim_step(&t,400,670,false,50,10,i*100000);
 assert(center_trim_step(&t,400,670,false,50,10,1000000)==0 && t.no_response && !t.reversed);
 /* Replay the important v5.32 failure shape: weak-but-visible tape,
  * both wheels advance equally, hence no requested yaw for1s. It must brake
  * into bounded recovery, not latch a terminal steering fault. */
 reset();course_set_center_reference(&c,756);course_center_level(&c,630);
 for(int i=0;i<11;i++)o=step(true,false,50,50,100000);
 assert(!o.stop && !o.left && !o.right && c.phase==COURSE_LOST);
 assert(c.trim_no_responses==1 && c.center_search.limit==120);
 course_center_level(&c,700);o=step(true,false,0,0,10000);
 o=step(true,false,0,0,60000);
 assert(!o.stop && c.phase==COURSE_FOLLOW && c.recoveries==1 && c.trim_no_responses==1);
 /* A recovery retains bounded learned wheel effort; it resets velocity and
  * requires another balanced launch rather than forgetting motor deadband. */
 c.drive.left_integral=0.09f;c.drive.right_integral=0.14f;
 new_segment(&c,100,100,now);
 assert(!c.drive.launch_complete && !c.drive.started && !c.drive.speed_valid);
 assert(c.drive.launch_left_pwm>0.16f && c.drive.launch_right_pwm>0.20f);
 puts("PASS centred sensor: early optical steering and reversal, bounded local recovery beside barcodes, strong confirmation, travel/stall bounds, stop/defer policy and crossings.");
}
