#ifndef OBSTACLE_PLAN_H
#define OBSTACLE_PLAN_H
#include <stdbool.h>
#include <math.h>
/* Five settled range rays, 50/70/90/110/125 servo degrees. Convention:
 * 50 is right, 125 is left. The finite beam does not measure exact width.
 * Unknown right edge -> perpendicular departure, never an invented edge. */
typedef struct {
    unsigned angle_deg, lateral_mm, travel_mm, forward_mm, observed_width_mm;
    bool right_edge_known, width_bounded;
} obstacle_plan_t;
static inline obstacle_plan_t obstacle_plan(const float cm[5],const unsigned good[5],float front_cm)
{
    const int servo[5]={50,70,90,110,125};
    const float rad=0.01745329252f;
    obstacle_plan_t p={90,250,250,0,0,false,false};
    float limit=fminf(45.0f,fmaxf(30.0f,front_cm+15.0f));
    float xmin=10000,xmax=-10000,turn=0;
    int first=-1,last=-1;
    for(int i=0;i<5;i++){
        if(good[i]<2||!isfinite(cm[i])||cm[i]<2||cm[i]>limit)continue;
        float bearing=(90-servo[i])*rad,r=cm[i]*10;
        float x=r*sinf(bearing),y=r*cosf(bearing);
        /* 60mm half-body +40mm buffer; 15deg beam/heading uncertainty. */
        float needed=atan2f(x+100,y)/rad+15;
        if(needed>turn)turn=needed;
        if(x<xmin)xmin=x;
        if(x>xmax)xmax=x;
        if(first<0)first=i;
        last=i;
    }
    if(first<0)return p;
    p.observed_width_mm=(unsigned)ceilf(xmax-xmin);
    p.right_edge_known=first>0&&good[first-1]>=2&&isfinite(cm[first-1])&&cm[first-1]<=400&&cm[first-1]>limit;
    p.width_bounded=p.right_edge_known&&last<4&&good[last+1]>=2&&isfinite(cm[last+1])&&cm[last+1]<=400&&cm[last+1]>limit;
    if(!p.right_edge_known)return p;
    /* The edge lies BETWEEN rays, not at the last hit's centre. Include the
     * adjacent far-ray bearing plus beam uncertainty before choosing clearance. */
    float y=cm[first]*10*cosf((90-servo[first])*rad);
    float edge_bearing=fminf(75,(90-servo[first-1])+15);
    float edge_x=y*tanf(edge_bearing*rad);
    if(edge_x>xmax)xmax=edge_x;
    float edge_turn=atan2f(edge_x+100,y)/rad;
    if(edge_turn>turn)turn=edge_turn;
    p.angle_deg=(unsigned)(ceilf(fmaxf(45,turn)/5)*5);
    if(p.angle_deg>85)p.angle_deg=90;
    /* Include the ~143mm front-tip pivot radius when turning parallel.
     * This is a clearance bound, not an exact object width/depth estimate. */
    p.lateral_mm=(unsigned)ceilf(fmaxf(200,xmax+150));
    if(p.lateral_mm>600){p.right_edge_known=false;p.angle_deg=90;p.lateral_mm=250;}
    p.travel_mm=(unsigned)ceilf(p.lateral_mm/sinf(p.angle_deg*rad));
    p.forward_mm=(unsigned)fmaxf(0,floorf(p.travel_mm*cosf(p.angle_deg*rad)));
    return p;
}
#endif
