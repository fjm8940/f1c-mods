#pragma once
#include <math.h>
#include <stdint.h>
#include <string.h>
/* Exact target review ORIENTATION_REVIEW_2026-09-14.md:
   WheelContactTransform receives vehicle+12. Physics +0x714 = vehicle+0x720.
   Validate the complete local-up vector so an uninitialized transform fails closed. */
#define F1C_VEHICLE_UP_VECTOR_OFFSET 0x71cu
typedef struct { int valid, state; float up_dot; } F1COrientation;
/* state: 0 unknown, 1 upright, 2 side/steep tilt, 3 overturned. */
static F1COrientation f1c_orientation(float x,float y,float z) {
    F1COrientation o={0,0,0};float norm=x*x+y*y+z*z;
    if(!isfinite(norm)||!isfinite(y)||norm<0.90f||norm>1.10f||fabsf(y)>1.05f)return o;
    o.valid=1;o.up_dot=y;o.state=y<=-0.25f?3:(y<0.5f?2:1);return o;
}
static F1COrientation f1c_orientation_vehicle(const void *vehicle) {
    float v[3];memcpy(v,(const char*)vehicle+F1C_VEHICLE_UP_VECTOR_OFFSET,sizeof(v));
    return f1c_orientation(v[0],v[1],v[2]);
}
typedef struct { unsigned identity; int initialized, previous_inverted; double last,wall,held; float x,z; } F1CInversionTrack;
/* Observed simulation seconds, never paused wall time; stale gaps reset proof.
   Low speed and bounded displacement establish suspected incapacitation only. */
static float f1c_inversion_step(F1CInversionTrack *t,unsigned identity,double now,
    double wall,int running,F1COrientation o,float speed,float x,float z) {
    int good=running&&o.valid&&o.state==3&&isfinite(speed)&&speed>=0&&speed<2 &&isfinite(x)&&isfinite(z);
    double dt=now-t->last;
    if(!isfinite(now)||!isfinite(wall)||!t->initialized||identity!=t->identity||dt<0||wall<t->wall||wall-t->wall>2){
        memset(t,0,sizeof(*t));t->identity=identity;t->initialized=1;dt=0;
    }
    if(!running){t->last=now;t->wall=wall;return (float)t->held;}
    if(!good)t->held=0;
    else if(dt>0){
        float dx=x-t->x,dz=z-t->z;
        if(t->previous_inverted && dx*dx+dz*dz<=4)t->held+=fmin(dt,0.25);
        else t->held=0;
    }
    t->last=now;t->wall=wall;t->x=x;t->z=z;t->previous_inverted=good;
    return (float)t->held;
}
