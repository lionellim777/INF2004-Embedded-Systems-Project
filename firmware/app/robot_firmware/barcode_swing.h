#ifndef BARCODE_SWING_H
#define BARCODE_SWING_H
#include <stdbool.h>
#include <stdint.h>
/* GP26 black/white from the barcode's own rise and drop, not one fixed level.
 * The floor reads 230-340 and narrow gaps on the track read 1800-2500, so no
 * single level separates both from bars at 3000-4000. A bar starts when the
 * reading rises 700 above the lowest point of the gap before it, at least
 * half way back to the previous bar's highest point, and to white+850. A gap
 * starts when it drops 700 below the bar's highest point, or to white+650.
 * The half-way rule stops a narrow bar after a wide gap reading long; the
 * white+850 floor ignores the slow grey rise before the first bar.
 * White is the median GP26 reading while stopped before the run, lowered
 * whenever a lower reading is seen, so starting over a bar or tape only
 * delays reading until the sensor first passes real white. */
#define BARCODE_SWING_AMOUNT 700u
#define BARCODE_SWING_BLACK_MARGIN 850u
#define BARCODE_SWING_WHITE_MARGIN 650u
/* The sampler sees only black (4095) or white (0); these levels split them. */
#define BARCODE_SWING_SAMPLER_ON 2048u
#define BARCODE_SWING_SAMPLER_OFF 2047u
#define BARCODE_WHITE_SAMPLES 31u
#define BARCODE_WHITE_SPACING_US 50000u  /* 31 readings span the last 1.5 s. */
#define BARCODE_WHITE_SPREAD_MAX 100u    /* Still paper/floor reads within ~20. */

typedef struct {
    uint16_t samples[BARCODE_WHITE_SAMPLES];
    unsigned count,next;
    uint64_t last_us;
} barcode_white_t;

static inline void barcode_white_add(barcode_white_t *w,uint16_t raw,uint64_t now)
{
    if(w->count && now>=w->last_us && now-w->last_us<BARCODE_WHITE_SPACING_US)return;
    w->samples[w->next]=raw;w->next=(w->next+1u)%BARCODE_WHITE_SAMPLES;
    if(w->count<BARCODE_WHITE_SAMPLES)w->count++;
    w->last_us=now;
}

/* Median resists a passing spike; the middle-half spread rejects movement. */
static inline bool barcode_white_value(const barcode_white_t *w,uint64_t now,
                                       uint16_t *median,uint16_t *spread)
{
    *median=*spread=0;
    if(w->count<BARCODE_WHITE_SAMPLES || now<w->last_us ||
       now-w->last_us>2u*BARCODE_WHITE_SPACING_US)return false;
    uint16_t sorted[BARCODE_WHITE_SAMPLES];
    for(unsigned i=0;i<BARCODE_WHITE_SAMPLES;i++){
        unsigned j=i;uint16_t v=w->samples[i];
        while(j && sorted[j-1]>v){sorted[j]=sorted[j-1];j--;}
        sorted[j]=v;
    }
    *median=sorted[BARCODE_WHITE_SAMPLES/2u];
    *spread=(uint16_t)(sorted[(3u*BARCODE_WHITE_SAMPLES)/4u]-sorted[BARCODE_WHITE_SAMPLES/4u]);
    return *spread<=BARCODE_WHITE_SPREAD_MAX &&
           (uint32_t)*median+BARCODE_SWING_BLACK_MARGIN<4095u;
}

typedef struct {
    bool enabled,started,black;
    uint16_t extreme;       /* Lowest point of this gap, or highest of this bar. */
    uint16_t last_bar_max;  /* Highest point of the previous bar, 0 if none. */
    uint16_t white,black_level,white_level;
} barcode_swing_t;

static inline void barcode_swing_begin(barcode_swing_t *s,uint16_t white)
{
    *s=(barcode_swing_t){0};
    s->enabled=true;
    s->white=white;
    s->black_level=(uint16_t)(white+BARCODE_SWING_BLACK_MARGIN);
    s->white_level=(uint16_t)(white+BARCODE_SWING_WHITE_MARGIN);
}

/* Call once per filtered sample; pass the result to the sampler. */
static inline uint16_t barcode_swing_step(barcode_swing_t *s,uint16_t level)
{
    if(level<s->white){
        s->white=level;
        s->black_level=(uint16_t)(level+BARCODE_SWING_BLACK_MARGIN);
        s->white_level=(uint16_t)(level+BARCODE_SWING_WHITE_MARGIN);
    }
    if(!s->started){
        s->started=true;s->black=false;s->extreme=level;s->last_bar_max=0;
    }else if(s->black){
        if(level>s->extreme)s->extreme=level;
        else if((uint32_t)level+BARCODE_SWING_AMOUNT<=s->extreme || level<=s->white_level){
            s->black=false;s->last_bar_max=s->extreme;s->extreme=level;
        }
    }else{
        uint32_t half=s->last_bar_max>s->extreme ? ((uint32_t)s->last_bar_max-s->extreme)/2u : 0u;
        if(level<s->extreme)s->extreme=level;
        else if((uint32_t)level>=(uint32_t)s->extreme+BARCODE_SWING_AMOUNT &&
                (uint32_t)level>=(uint32_t)s->extreme+half && level>=s->black_level){
            s->black=true;s->extreme=level;
        }
    }
    return s->black ? 4095u : 0u;
}
#endif
