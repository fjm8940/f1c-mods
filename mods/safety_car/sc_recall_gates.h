#ifndef SC_RECALL_GATES_H
#define SC_RECALL_GATES_H
#include <math.h>
/* The caller always supplies the owned SC's native lap, never queue leader. */
static int sc_caution_lap_step(int *reference, int lap) {
    if(lap<0)return 0;
    if(*reference<0 || lap<*reference){*reference=lap;return 0;}
    if(lap>*reference){*reference=lap;return 1;}
    return 0;
}
/* Keep the ordered pack anchored to its first governed car. Its separate
   SC gap must still be close; an isolated compact pack is not formed. */
static int sc_formation_head_ready(float head, float target) {
    return isfinite(head)&&isfinite(target)&&target>0 && head>=0 && head<=3*target;
}
static float sc_formation_error(float behind,int index,float head,float target) {
    if(!isfinite(behind)||!isfinite(head)||target<=0||index<1)return INFINITY;
    return fabsf(behind-(head+(index-1)*target));
}
#endif
