#include "barcode_acquisition.h"
#include "barcode_signal.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned lo[500],hi[500];
static barcode_sampler_t s;static barcode_capture_t c;static barcode_signal_t f;
static unsigned now;
static void sample(unsigned v){
 now+=BARCODE_SAMPLE_US;barcode_sampler_sample(&s,barcode_signal_update(&f,v),now);
 if(now%10000==0){barcode_batch_t b;barcode_sampler_take(&s,&b);barcode_capture_consume(&c,&b);}
}
int main(int argc,char **argv){
 if(argc!=2)return 1;FILE *fp=fopen(argv[1],"r");if(!fp)return 2;
 for(unsigned i=0,ms;i<500;i++)if(fscanf(fp,"%u %u %u",&ms,&lo[i],&hi[i])!=3)return 3;
 fclose(fp);unsigned accepts=0,candidates=0,maxframes=0;
 for(unsigned on=185;on<=3500;on++)for(unsigned h=2;h<=12;h+=5)for(unsigned mode=0;mode<4;mode++){
  now=0;f=(barcode_signal_t){0};barcode_sampler_begin_levels(&s,on,on-h);
  barcode_capture_begin_stream(&c,0);c.independent_optical=true;c.waiting_gap=true;
  for(unsigned i=0;i<800;i++)sample(180);
  for(unsigned i=0;i<500;i++)for(unsigned j=0;j<20;j++){
   unsigned v=mode==0?(lo[i]+hi[i])/2:mode==1?(j%2?lo[i]:hi[i]):lo[i]+(hi[i]-lo[i])*(mode==2?j:19-j)/19;
   sample(v);
  }
  for(unsigned i=0;i<2000;i++)sample(180);
  candidates++;
  if(c.frames>maxframes)maxframes=c.frames;
  if(c.event_count){accepts++;printf("accepted on=%u off=%u reconstruction=%u letters=",on,on-h,mode);for(unsigned i=0;i<c.event_count;i++)putchar(c.events[i].letter);puts("");}
 }
 printf("profiles=%u accepted_profiles=%u max_full_frame_candidates=%u\n",candidates,accepts,maxframes);
}
