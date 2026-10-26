#pragma once
#include <stdint.h>
#include <stddef.h>
#include <math.h>
/* Read-only exact-layout evidence: Steward_Init stores Xpitin at +0x194;
   SceneNodePositionGetter reads node+0x204 -> render+0xe8/0xec/0xf0.
   Bit0x20000000 at render+0x14 means world position is already cached.
   If uncached, mirror pure affine point transform0055EEAF over local center
   render+0xdc and node matrix+0x1a0 without modifying the game cache.
   Caller is stable game thread and supplies bounded readable-memory copy. */
typedef int (*F1CPitReadFn)(uintptr_t,void*,size_t);
static int f1c_pit_entry_marker(uintptr_t steward,F1CPitReadFn read,float out[3]){
    uint32_t node=0,render=0,flags=0;float xyz[3];
    if(!steward||!read||!out||!read(steward+0x194,&node,4)||!node)return 0;
    if(node>UINT32_MAX-0x208u || !read((uintptr_t)node+0x204,&render,4)||!render||render>UINT32_MAX-0xf4u)return 0;
    if(!read((uintptr_t)render+0x14,&flags,4))return 0;
    if(flags&0x20000000u){
        if(!read((uintptr_t)render+0xe8,xyz,sizeof(xyz)))return 0;
    } else {
        float local[3],matrix[16];
        if(!read((uintptr_t)render+0xdc,local,sizeof(local)) ||
           !read((uintptr_t)node+0x1a0,matrix,sizeof(matrix)))return 0;
        for(int k=0;k<3;k++)if(!isfinite(local[k]))return 0;
        for(int k=0;k<16;k++)if(!isfinite(matrix[k]))return 0;
        if(fabsf(matrix[3])>0.001f||fabsf(matrix[7])>0.001f||
           fabsf(matrix[11])>0.001f||fabsf(matrix[15]-1.0f)>0.001f)return 0;
        for(int k=0;k<3;k++)xyz[k]=(float)((double)local[0]*matrix[k]+
            (double)local[1]*matrix[4+k]+(double)local[2]*matrix[8+k]+matrix[12+k]);
    }
    for(int k=0;k<3;k++)if(!isfinite(xyz[k])||fabsf(xyz[k])>1000000.0f)return 0;
    for(int k=0;k<3;k++)out[k]=xyz[k];
    return 1;
}
static int f1c_pit_within_100(const float car[3],const float marker[3]){
    if(!car||!marker)return 0;
    for(int k=0;k<3;k++)if(!isfinite(car[k])||!isfinite(marker[k]))return 0;
    float x=car[0]-marker[0],z=car[2]-marker[2];
    return fabsf(car[1]-marker[1])<=20.0f && x*x+z*z<=10000.0f;
}
/* Read-only pit-box position for a pit-team index. The AI database (AIW)
   object is directly based at 0x005EA2C0 (docs/52 section 3, OBSERVED live;
   RenderCallWithGuard uses mov ecx,0x5EA2C0). +0x154 is the pit-slot array
   of 28-byte position/orientation records, +0x164 its allocation count
   (raised to >= 52). Authored Monza pit records matched the AIW file;
   unauthored capacity is all-zero and is refused here. The vehicle's own
   index is VEHF_PitTeamIndex (veh+0x48A4, CONFIRMED). */
static int f1c_pit_box_position(F1CPitReadFn read,int team,float out[3]){
    const uintptr_t aiw=0x005EA2C0u;
    uint32_t array=0,count=0;float xyz[3];
    if(!read||!out||team<0||team>51)return 0;
    if(!read(aiw+0x154,&array,4)||!array||!read(aiw+0x164,&count,4)||
       count<52||count>1024||(uint32_t)team>=count||array>UINT32_MAX-28u*count)return 0;
    if(!read((uintptr_t)array+28u*(uint32_t)team,xyz,sizeof(xyz)))return 0;
    for(int k=0;k<3;k++)if(!isfinite(xyz[k])||fabsf(xyz[k])>1000000.0f)return 0;
    if(xyz[0]==0.0f&&xyz[1]==0.0f&&xyz[2]==0.0f)return 0;
    for(int k=0;k<3;k++)out[k]=xyz[k];
    return 1;
}
/* Read-only AIW waypoints near a point (docs/52 section 4: array pointer at
   AIW+0x134, count at +0x130, 316-byte records, active position +0x94..+0x9C,
   wp_branchID +0x12C; 0 = main line, 1 = pit road on observed assets).
   Copies up to max entries within radius (horizontal) of centre. */
static int f1c_aiw_waypoints_near(F1CPitReadFn read,const float centre[3],float radius,
                                  float out_xyz[][3],int out_branch[],int max){
    const uintptr_t aiw=0x005EA2C0u;
    uint32_t array=0,count=0;int n=0;
    if(!read||!centre||!out_xyz||!out_branch||max<=0)return 0;
    if(!read(aiw+0x134,&array,4)||!array||!read(aiw+0x130,&count,4)||!count||count>20000||
       array>UINT32_MAX-316u*count)return 0;
    for(uint32_t i=0;i<count&&n<max;i++){
        float p[3];int branch=0;
        if(!read((uintptr_t)array+316u*i+0x94,p,sizeof(p))||
           !read((uintptr_t)array+316u*i+0x12C,&branch,4))continue;
        if(!isfinite(p[0])||!isfinite(p[1])||!isfinite(p[2]))continue;
        float dx=p[0]-centre[0],dz=p[2]-centre[2];
        if(dx*dx+dz*dz>radius*radius)continue;
        out_xyz[n][0]=p[0];out_xyz[n][1]=p[1];out_xyz[n][2]=p[2];out_branch[n]=branch;n++;
    }
    return n;
}
