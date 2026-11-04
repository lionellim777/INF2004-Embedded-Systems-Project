#ifndef OBSTACLE_PLAN_H
#define OBSTACLE_PLAN_H
#include <stdbool.h>
#include <math.h>
/* Servo 90 faces forward; decreasing angles look right. A ray needs two
 * matching valid returns. Three completed no-returns can suggest an edge,
 * but the subsequent path scan must measure valid clearance. */
enum { OBSTACLE_PLAN_RAYS=9 };
static const unsigned obstacle_plan_angles[OBSTACLE_PLAN_RAYS]={90,85,80,75,70,65,60,55,50};
typedef struct {
    unsigned angle_deg, lateral_mm, travel_mm, forward_mm, observed_width_mm;
    unsigned edge_servo_deg, clear_servo_deg;
    bool right_edge_known, edge_from_no_return, width_bounded;
} obstacle_plan_t;
static inline obstacle_plan_t obstacle_plan(const float cm[OBSTACLE_PLAN_RAYS],
    const unsigned near[OBSTACLE_PLAN_RAYS],const unsigned far[OBSTACLE_PLAN_RAYS],
    const unsigned no_return[OBSTACLE_PLAN_RAYS])
{
    const float rad=0.01745329252f;
    obstacle_plan_t p={0};
    float xmin=10000,xmax=-10000,turn=0;
    int edge=-1;
    for(int i=0;i<OBSTACLE_PLAN_RAYS;i++){
        if(near[i]<2||!isfinite(cm[i])||cm[i]<2||cm[i]>400)continue;
        float bearing=(90-(int)obstacle_plan_angles[i])*rad,r=cm[i]*10;
        float x=r*sinf(bearing),y=r*cosf(bearing);
        /* 60 mm half body, 40 mm gap and 15 degrees beam/heading error. */
        float needed=atan2f(x+100,y)/rad+15;
        if(needed>turn)turn=needed;
        if(x<xmin)xmin=x;
        if(x>xmax)xmax=x;
        edge=i;
    }
    if(edge<0||edge==OBSTACLE_PLAN_RAYS-1)return p;
    p.right_edge_known=far[edge+1]>=2;
    p.edge_from_no_return=!p.right_edge_known&&no_return[edge+1]>=3;
    if(!p.right_edge_known&&!p.edge_from_no_return)return p;
    p.edge_servo_deg=obstacle_plan_angles[edge];
    p.clear_servo_deg=obstacle_plan_angles[edge+1];
    p.observed_width_mm=(unsigned)ceilf(xmax-xmin);
    /* The edge may lie anywhere between the last near ray and first far ray.
     * Include its far-side bearing, beam uncertainty and an extra 5 degrees. */
    float y=cm[edge]*10*cosf((90-(int)p.edge_servo_deg)*rad);
    float edge_bearing=fminf(75,90-(int)p.clear_servo_deg+15);
    float edge_x=y*tanf(edge_bearing*rad);
    if(edge_x>xmax)xmax=edge_x;
    float edge_turn=atan2f(edge_x+100,y)/rad;
    if(edge_turn>turn)turn=edge_turn;
    p.angle_deg=(unsigned)(ceilf((fmaxf(40,turn)+5)/5)*5);
    if(p.angle_deg>85)p.angle_deg=90;
    /* Leave room for the front tip when turning parallel to the route. */
    p.lateral_mm=(unsigned)ceilf(fmaxf(200,xmax+150));
    if(p.lateral_mm>600){p.right_edge_known=p.edge_from_no_return=false;return p;}
    p.travel_mm=(unsigned)ceilf(p.lateral_mm/sinf(p.angle_deg*rad));
    p.forward_mm=(unsigned)fmaxf(0,floorf(p.travel_mm*cosf(p.angle_deg*rad)));
    return p;
}
#endif
