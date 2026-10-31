#include "line_check.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    int32_t levels[3]={1000,0,0};ir_roles_t r=ir_roles(levels);
    assert(r.barcode && !r.left && !r.right);
    line_check_output_t a=line_check_step(r,0,0,0,false);
    levels[0]=0;r=ir_roles(levels);
    line_check_output_t b=line_check_step(r,0,0,0,false);
    assert(a.left==b.left && a.right==b.right && !a.stop);
    levels[1]=250;r=ir_roles(levels);a=line_check_step(r,0,0,0,false);
    assert(r.left && !r.right && a.left<a.right && a.left>0);
    levels[1]=249;levels[2]=250;r=ir_roles(levels);a=line_check_step(r,0,0,0,false);
    assert(!r.left && r.right && a.left>a.right && a.right>0);
    levels[1]=250;r=ir_roles(levels);assert(line_check_step(r,0,0,0,false).stop);
    r=(ir_roles_t){0};
    assert(!line_check_step(r,1999999,3536,3536,false).stop);
    assert(line_check_step(r,2000000,0,0,false).stop);
    assert(line_check_step(r,1,3537,0,false).stop);
    assert(line_check_step(r,1,0,3537,false).stop);
    assert(line_check_step(r,1,-65,0,false).stop);
    assert(line_check_step(r,1,0,-65,false).stop);
    assert(line_check_step(r,1,0,0,true).stop);
    for(int mask=0;mask<8;mask++) {
        r=(ir_roles_t){mask&1,mask&2,mask&4};
        a=line_check_step(r,2000000,0,0,false);
        assert(a.stop && a.left==0 && a.right==0);
    }
    puts("PASS dedicated barcode isolation, steering polarity, ambiguity, limits and encoder fault stops.");
}
