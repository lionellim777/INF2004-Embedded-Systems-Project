/* Differential run: identical pseudo-random sensor streams through the obstacle bypass controller.
 * Prints a hash of every output and the visited-phase set. Build once per layout and compare. */
#include "obstacle_bypass.h"
#include <stdio.h>
#include <string.h>
static uint64_t rng;
static uint32_t rnd(void){rng=rng*6364136223846793005ULL+1442695040888963407ULL;return (uint32_t)(rng>>33);}
static uint64_t h;
static void mix(uint64_t v){h^=v;h*=1099511628211ULL;}
static void mixs(const char*s){if(!s){mix(0);return;}while(*s)mix((unsigned char)*s++);mix(255);}
int main(void){
    unsigned long long phases=0,steps=0,stops=0,completed=0;static obstacle_bypass_t a;
    for(unsigned seed=1;seed<=600;seed++){
        rng=seed*7919u;encoder_sample_t enc={0};uint64_t now=1000000;double l=0,r=0;
        memset(&a,0,sizeof a);obstacle_bypass_begin(&a,enc,now,700);
        line_check_output_t out={0};float cm=180;bool valid=true,nr=false;int center=700;bool rb=false;
        unsigned hold=0;
        for(unsigned i=0;i<6000&&!out.stop&&!a.completed;i++){
            now+=10000;l+=out.left*4000*.01;r+=out.right*4000*.01;
            enc.left_raw=(uint32_t)-(int32_t)l;enc.right_raw=(uint32_t)(int32_t)r;
            if(!hold){static const float cms[]={8,12,15,20,25,30,40,45,60,90,150,200};
                cm=cms[rnd()%12];valid=rnd()%9;nr=!valid&&rnd()%2;center=rnd()%4?0:700;rb=rnd()%7==0;hold=1+rnd()%60;
                if(rnd()%3==0&&a.phase==AVOID_FOLLOW){cm=200;valid=true;center=700;}
                if(a.phase==AVOID_FOLLOW&&rnd()%40==0){cm=35;valid=true;center=700;}}
            else hold--;
            bool fresh=i%12==0;
            ir_roles_t ir={false,center>=250,rb};
            out=obstacle_bypass_guarded_step(&a,ir,center,enc,now,fresh,valid,cm,rnd()%500==0?"ECHO":0,nr);
            mix((uint32_t)(int32_t)(out.left*1000));mix((uint32_t)(int32_t)(out.right*1000));mix(out.stop);mixs(out.state);
            mix(a.phase);mix(a.next);mix(a.detected);mix(a.completed);mix(a.close_count);mix(a.moving_invalid);
            mix(a.clear_checks);mix(a.clearance_adjustments);mix(a.approach_pending);mix(a.approach_warning);
            mix(a.rejoin_armed);mix(a.no_return);mix(a.sonar_wait);mix(a.probe_black);mix(a.probe_gap);
            mix(a.contact_set);mix(a.edge_shift_active);mix(a.angled_probe);mix(a.line.curve_follow);
            phases|=1ULL<<a.phase;steps++;
        }
        stops+=out.stop;completed+=a.completed;
    }

    /* Phase-reactive scenarios (same environment model as the repository's controller test) with random perturbation. */
    for(unsigned seed=1;seed<=420;seed++){
        unsigned scenario=seed%21;rng=seed*104729u;encoder_sample_t enc={0};uint64_t now=1000000;double l=0,r=0;
        memset(&a,0,sizeof a);obstacle_bypass_begin(&a,enc,now,700);
        line_check_output_t out={0};
        for(unsigned i=0;i<25000&&!out.stop&&!a.completed;i++){
            now+=10000;l+=out.left*4000*.01;r+=out.right*4000*.01;
            enc.left_raw=(uint32_t)-(int32_t)l;enc.right_raw=(uint32_t)(int32_t)r;
            bool fresh=i%12==0,valid=true;float cm=180;
            int center=(a.phase==AVOID_FOLLOW||a.phase==AVOID_VERIFY||a.phase==AVOID_CHECK_TAPE||a.phase==AVOID_TAPE_PROBE)?700:0;
            if(a.phase==AVOID_FOLLOW)cm=25;
            if(a.phase==AVOID_SCAN_INITIAL)cm=a.servo_angle<=100?20:100;
            if(a.phase==AVOID_SCAN_SIDE||a.phase==AVOID_CHECK_LEFT)cm=a.along_mm<=300?28:180;
            if(scenario==1&&a.phase==AVOID_MOVE_OUT)cm=10;
            if(scenario==2&&a.phase==AVOID_SCAN_FORWARD)cm=20;
            if(scenario==4&&a.phase==AVOID_MOVE_OUT)fresh=false;
            if(scenario==5&&a.phase==AVOID_MOVE_ALONG)cm=10;
            if(scenario==8&&a.phase==AVOID_SCAN_FORWARD&&a.offset_mm<450)cm=22;
            if(scenario==10&&a.phase==AVOID_SCAN_FORWARD&&a.servo_angle==90)valid=false;
            if(scenario==12&&a.phase==AVOID_SCAN_FORWARD&&a.offset_mm<340)cm=20;
            if(scenario==18&&a.phase==AVOID_SCAN_FORWARD){valid=a.servo_angle!=90;a.no_return=!valid;}
            if(scenario==19&&a.phase==AVOID_SCAN_FORWARD)valid=a.servo_angle==125;
            if(a.phase==AVOID_RETURN_LINE||a.phase==AVOID_MOVE_ALONG){
                float mm=(motion_left_position(&a.leg,enc)+motion_right_position(&a.leg,enc))/(2*13.491f);
                center=mm>35?700:0;if(scenario==13||scenario==9)center=0;if(scenario==14)center=700;}
            if(scenario==15&&a.phase==AVOID_CHECK_TAPE)center=0;
            if(scenario==16&&a.phase==AVOID_VERIFY)center=0;
            if(rnd()%97==0){cm=(float)(5+rnd()%200);valid=rnd()%5;}
            if(rnd()%211==0)center=rnd()%2?700:0;
            ir_roles_t ir={false,center>=250,false};
            out=obstacle_bypass_guarded_step(&a,ir,center,enc,now,fresh,valid,cm,0,!valid&&a.no_return);
            mix((uint32_t)(int32_t)(out.left*1000));mix((uint32_t)(int32_t)(out.right*1000));mix(out.stop);mixs(out.state);
            mix(a.phase);mix(a.next);mix(a.detected);mix(a.completed);mix(a.close_count);mix(a.moving_invalid);
            mix(a.clear_checks);mix(a.clearance_adjustments);mix(a.approach_pending);mix(a.approach_warning);
            mix(a.rejoin_armed);mix(a.no_return);mix(a.sonar_wait);mix(a.probe_black);mix(a.probe_gap);
            mix(a.contact_set);mix(a.edge_shift_active);mix(a.angled_probe);mix(a.line.curve_follow);
            phases|=1ULL<<a.phase;steps++;
        }
        stops+=out.stop;completed+=a.completed;
    }
    printf("hash=%016llx steps=%llu stops=%llu completed=%llu phases=%llx sizeof=%zu\n",
        (unsigned long long)h,steps,stops,completed,phases,sizeof(obstacle_bypass_t));
    return 0;}
