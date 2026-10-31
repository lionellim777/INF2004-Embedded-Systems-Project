#define ROBOCAR_CENTER_LINE 1
#include "../app/robot_firmware/course_control.c"
#include <assert.h>
#include <stdio.h>
static encoder_sample_t enc(int l,int r){return (encoder_sample_t){.left_raw=(uint32_t)-l,.right_raw=(uint32_t)r};}
int main(void){
 center_baseline_t b={0};int32_t median,spread;
 assert(!center_baseline_value(&b,0,&median,&spread));
 for(unsigned i=0;i<31;i++)center_baseline_add(&b,i%10==0?1000:645+i%11,i*10000);
 assert(center_baseline_value(&b,300000,&median,&spread) && median>=645 && median<=655 && spread<11);
 assert(!center_baseline_value(&b,410001,&median,&spread));
 b=(center_baseline_t){0};
 for(unsigned i=0;i<31;i++)center_baseline_add(&b,i%2?800:300,i*10000);
 assert(!center_baseline_value(&b,300000,&median,&spread));
 b=(center_baseline_t){0};
 for(unsigned i=0;i<31;i++)center_baseline_add(&b,100,i*10000);
 assert(!center_baseline_value(&b,300000,&median,&spread));
 /* A new stopped countdown replaces the previous ring completely. */
 for(unsigned i=31;i<62;i++)center_baseline_add(&b,600,i*10000);
 assert(center_baseline_value(&b,610000,&median,&spread) && median==600);
 course_control_t c;course_begin(&c,(course_config_t){'s','s',120},enc(0,0),0);
 course_set_center_reference(&c,650);
 for(unsigned i=1;i<=20;i++){
  course_center_level(&c,i%2?797:637); /* log-like peak then ordinary black */
  line_check_output_t o=course_step(&c,(ir_roles_t){0,true,false},enc(i*6,i*6),i*10000);
  assert(!o.stop && c.center_reference==650 && c.trim_attempts==0);
 }
 /* v5.23 rejected normal637 at ref797. The frozen reference accepts it. */
 course_note_barcode_activity(&c,200000);course_center_level(&c,100);
 line_check_output_t o=course_step(&c,(ir_roles_t){0},enc(120,120),210000);
 assert(!o.stop && c.phase==COURSE_LOST);
 course_center_level(&c,637);
 o=course_step(&c,(ir_roles_t){0},enc(65,175),270000);
 assert(!o.stop && o.left==0 && c.center_search.confirming);
 /* Coast crosses the soft60-count boundary after zero power was requested. */
 o=course_step(&c,(ir_roles_t){0},enc(45,195),290000);
 assert(!o.stop && !o.left && !o.right && !c.center_search.found);
 o=course_step(&c,(ir_roles_t){0},enc(45,195),340000);
 assert(!o.stop && c.recoveries==1 && c.phase==COURSE_FOLLOW);
 assert(c.center_search.coast_l==20 && c.center_search.coast_r==20);
 /* Confirmation never powers another search; losing black cannot succeed. */
 center_search_t s;center_search_begin(&s,1,0,0,0);s.limit=60;
 o=center_search_step(&s,true,55,-55,60000);assert(!o.stop && !o.left && !o.right);
 for(unsigned t=70000;t<=350000;t+=10000){o=center_search_step(&s,false,70,-70,t);assert(!o.stop && !o.left && !o.right);}
 o=center_search_step(&s,false,70,-70,360000);assert(o.stop && !s.found);
 assert(!strcmp(o.state,"CENTER_CONFIRMATION_TIMEOUT"));
 /* Excess coast, late control and new candidates outside the original sweep fail. */
 center_search_begin(&s,1,0,0,0);s.limit=60;
 center_search_step(&s,true,55,-55,60000);
 o=center_search_step(&s,true,136,-55,70000);assert(o.stop && !strcmp(o.state,"CENTER_SETTLE_TRAVEL_LIMIT"));
 center_search_begin(&s,1,0,0,0);s.limit=60;
 o=center_search_step(&s,true,61,-61,60000);assert(!s.confirming && s.reversed && !o.left);
 o=center_search_step(&s,true,-60,60,120000);assert(o.stop && !s.found);
 center_search_begin(&s,1,0,0,0);center_search_step(&s,true,0,0,10000);
 o=center_search_step(&s,true,0,0,270000);assert(o.stop && !strcmp(o.state,"CENTER_SEARCH_CONTROL_LATE"));
 /* Untouched v5.33 pivot stopped with both encoders stationary. Assist
  * only the non-progressing wheel and retain the original1s stall limit. */
 center_search_begin(&s,1,0,0,0);
 for(unsigned t=50000;t<1000000;t+=50000){
  o=center_search_step(&s,false,0,0,t);
  assert(!o.stop && o.left>=0.18f && o.left<=0.28f && o.right==-o.left);
  if(t>200000)assert(o.left>0.18f);
 }
 o=center_search_step(&s,false,0,0,1000000);
 assert(o.stop && !o.left && !o.right && !strcmp(o.state,"CENTER_SEARCH_ENCODER_STALL"));
 center_search_begin(&s,1,0,0,0);
 for(unsigned t=50000;t<=300000;t+=50000)o=center_search_step(&s,false,t/50000,0,t);
 assert(!o.stop && o.left==0.18f && o.right< -0.18f);
 o=center_search_step(&s,false,7,-1,350000);
 assert(!o.stop && o.left==0.18f && o.right==-0.18f);
 puts("PASS stable countdown median, stale/weak/unstable rejection, frozen reference, log-like normal-black recovery, bounded zero-power coast and confirmation failures.");
}
