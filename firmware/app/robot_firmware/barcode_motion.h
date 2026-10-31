#ifndef BARCODE_MOTION_H
#define BARCODE_MOTION_H
#include <stdbool.h>
#include <stdint.h>
/* Centre-of-axle odometry, NOT the offset sensor's exact path on a tight bend.
 * Both-wheel forward curves and stationary pauses are supported. A reverse
 * excursion >4 counts on EITHER wheel invalidates the current scan. Never
 * combine a pivot's recrossed stripes with a preceding forward fragment. */
typedef struct {
    uint32_t left_raw,right_raw,position,breaks;
    int64_t left,right,peak_left,peak_right;
    bool enabled;
} barcode_motion_t;
static inline int32_t barcode_count_delta(uint32_t a,uint32_t b)
{
    uint32_t d=a-b;
    return d<=INT32_MAX?(int32_t)d:(int32_t)((int64_t)d-INT64_C(4294967296));
}
static inline void barcode_motion_begin(barcode_motion_t *m,uint32_t l,uint32_t r)
{*m=(barcode_motion_t){.left_raw=l,.right_raw=r,.enabled=true};}
static inline void barcode_motion_step(barcode_motion_t *m,uint32_t l,uint32_t r)
{
    int64_t dl=-(int64_t)barcode_count_delta(l,m->left_raw);
    int64_t dr=barcode_count_delta(r,m->right_raw);
    m->left_raw=l;m->right_raw=r;
    /* More than64 counts per 1ms tick indicates reset/jump, not usable travel. */
    if(dl>64 || dl< -64 || dr>64 || dr< -64){
        m->breaks++;m->left=m->right=m->peak_left=m->peak_right=0;return;
    }
    m->left+=dl;m->right+=dr;
    if(m->peak_left-m->left>4 || m->peak_right-m->right>4){
        m->breaks++;m->peak_left=m->left;m->peak_right=m->right;return;
    }
    int64_t advance=0;
    if(m->left>m->peak_left){advance+=m->left-m->peak_left;m->peak_left=m->left;}
    if(m->right>m->peak_right){advance+=m->right-m->peak_right;m->peak_right=m->right;}
    /* 1000 units per summed wheel count; one unit step=half a mean wheel count.
     * Width cap750000 therefore permits375 mean wheel counts per bar/gap.
     * Unsigned wrap is intentional; individual intervals are bounded. */
    m->position+=(uint32_t)advance*1000u;
}
#endif
