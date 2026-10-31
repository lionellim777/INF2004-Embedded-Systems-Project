#include "barcode_acquisition.h"
#include "barcode_signal.h"
#include <assert.h>
#include <stdio.h>
static barcode_sampler_t s;
static barcode_capture_t c;
static barcode_signal_t filter;
static uint32_t now;
static const uint16_t letters[]={0x109,0x049,0x148,0x019,0x118,0x058,0x00d,0x10c,0x04c,0x01c,0x103,0x043,0x142,0x013,0x112,0x052,0x007,0x106,0x046,0x016,0x181,0x0c1,0x1c0,0x091,0x190,0x0d0};
static void begin(uint32_t origin){
 now=origin;filter=(barcode_signal_t){0};
 barcode_sampler_begin_levels(&s,BARCODE_DRIVE_BLACK_ON,BARCODE_DRIVE_BLACK_OFF);
 barcode_capture_begin_stream(&c,origin);c.independent_optical=true;c.waiting_gap=true;
}
static void level(bool black,unsigned us){
 for(unsigned t=0;t<us;t+=BARCODE_SAMPLE_US){
  now+=BARCODE_SAMPLE_US;
  barcode_sampler_sample(&s,barcode_signal_update(&filter,black?2100:190),now);
  /* Deliberately contradictory encoder metadata must not reset optical work. */
  s.stats.motion_breaks++;s.stats.position=0;
  if(now%10000u==0){barcode_batch_t b;barcode_sampler_take(&s,&b);barcode_capture_consume(&c,&b);}
 }
}
static void frame(unsigned letter,unsigned narrow,bool reverse,unsigned count){
 unsigned w[29],n=0;uint16_t pats[]={0x094,letters[letter],0x094};
 for(unsigned ch=0;ch<3;ch++){
  for(int bit=8;bit>=0;bit--)w[n++]=(pats[ch]&(1u<<bit))?narrow*3:narrow;
  if(ch<2)w[n++]=narrow;
 }
 for(unsigned i=0;i<count;i++)level(!(i%2),w[reverse?28-i:i]);
 level(false,400000);
}
int main(void){
 for(unsigned a=0;a<26;a++)for(unsigned rev=0;rev<2;rev++)for(unsigned ms=15;ms<=60;ms*=2){
  begin(0);level(false,400000);
  /* Two unrelated bars in front of a complete framed letter. */
  level(true,23000);level(false,41000);level(true,19000);level(false,53000);
  frame(a,ms*1000,rev,29);
  assert(c.event_count==1 && c.events[0].letter==(char)('A'+a));
  assert(!c.motion_aborts && !c.zero_distance_aborts && !c.integrity_fault);
 }
 begin(0);level(false,400000);frame(0,30000,false,29);frame(25,15000,false,29);
 assert(c.event_count==2 && c.events[0].letter=='A' && c.events[1].letter=='Z');
 for(unsigned n=1;n<29;n++){
  begin(0);level(false,400000);frame(0,30000,false,n);level(false,900000);
  assert(!c.event_count); /* no missing-bar guessing */
 }
 begin(0);level(false,400000);
 for(unsigned i=0;i<100;i++){level(true,20000);level(false,20000);}
 assert(!c.event_count && !c.integrity_fault);
 /* Clock wrap and an overlong incomplete capture must not poison next scan. */
 begin(UINT32_MAX-199999u);level(false,400000);level(true,900000);level(false,400000);
 frame(25,30000,true,29);assert(c.event_count==1 && c.events[0].letter=='Z');
 puts("PASS independent optical A-Z at15/30/60ms, both directions, prefix resync, no encoder coupling, AZ sequence, incomplete/noise rejection, timeout rearm and clock wrap.");
}
