#include "run_diagnostics.h"
#include <string.h>
void run_diagnostics_begin(run_diagnostics_t *d,uint32_t humps,uint32_t errors,uint32_t sequence)
{
    memset(d,0,sizeof(*d));d->active=true;d->start_humps=humps;d->start_errors=errors;
    d->last_pitch_sequence=sequence;
    for(unsigned i=0;i<3;i++) d->ir_min[i]=UINT16_MAX;
}
void run_diagnostics_sample(run_diagnostics_t *d,const uint16_t raw[3],const bool black[3],
 bool sonar_fresh,bool sonar_valid,float sonar_cm,bool pitch_valid,uint32_t sequence,float pitch_deg)
{
    if(!d->active)return;
    for(unsigned i=0;i<3;i++){
        if(raw[i]<d->ir_min[i]) d->ir_min[i]=raw[i];
        if(raw[i]>d->ir_max[i]) d->ir_max[i]=raw[i];
        if(d->samples && black[i]!=d->ir_previous[i]) d->ir_edges[i]++;
        d->ir_previous[i]=black[i];
    }
    d->samples++;
    if(sonar_fresh){
        if(sonar_valid){
            if(!d->sonar_valid || sonar_cm<d->sonar_min)d->sonar_min=sonar_cm;
            if(!d->sonar_valid || sonar_cm>d->sonar_max)d->sonar_max=sonar_cm;
            d->sonar_valid++;
        }else d->sonar_no_echo++;
    }
    if(pitch_valid && sequence!=d->last_pitch_sequence){
        if(!d->pitch_samples || pitch_deg<d->pitch_min)d->pitch_min=pitch_deg;
        if(!d->pitch_samples || pitch_deg>d->pitch_max)d->pitch_max=pitch_deg;
        d->pitch_samples++;d->last_pitch_sequence=sequence;
    }
}
