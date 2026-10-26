#pragma once
/* Read-only exact-target vehicle lifecycle and garage-position observation.
   IndexedStructDataRead_00403fc0 + RenderCallWithGuard_004d1140 provide
   table + 28*(team + bay*width); no calls into native code and no writes. */
#include <stdint.h>
#include <string.h>
#include <math.h>
typedef int (*F1CLifecycleRead)(uintptr_t,void*,size_t);
typedef struct {
    int valid,role,finish,retired,ai_control,participant;
    int garage_valid,garage_clear,team,bay;
    float body[3],garage[3],garage_distance;
} F1CVehicleLifecycle;
static int f1c_lifecycle_terminal(int retired,int finish) {
    return retired!=0 || finish==2 || finish==3;
}
static int f1c_lifecycle_participant(int role,int retired,int finish) {
    return (role==0 || role==1) && !retired && finish==0;
}
static int f1c_lifecycle_read_at(F1CLifecycleRead rd,uintptr_t a,void*out,size_t n) {
    return rd && a && a<=UINT32_MAX && n<=UINT32_MAX-a && rd(a,out,n);
}
static F1CVehicleLifecycle f1c_vehicle_lifecycle_read_impl(F1CLifecycleRead rd,uintptr_t vehicle,int dormant) {
    /* RenderCallWithGuard machine code: mov ecx,0x5EA2C0 (direct object). */
    F1CVehicleLifecycle v={0};uint32_t manager=0x005EA2C0u,table=0,width=0;float d2=0;
    if(!vehicle || vehicle>UINT32_MAX-0x48C0u)return v;
    if(!f1c_lifecycle_read_at(rd,vehicle+0x32C8,&v.role,4) ||
       !f1c_lifecycle_read_at(rd,vehicle+0x4064,&v.finish,4) ||
       !f1c_lifecycle_read_at(rd,vehicle+0x4410,&v.retired,1) ||
       !f1c_lifecycle_read_at(rd,vehicle+0x45F4,&v.ai_control,1))return v;
    if(v.role < -1 || v.role>2 || v.retired>1 || v.ai_control>1 || v.finish<0 || v.finish>3)return v;
    v.valid=1;v.participant=f1c_lifecycle_participant(v.role,v.retired,v.finish);
    /* Only the explicit dormant observer admits a nonterminal inactive slot.
       Ownership must be established by its caller; geometry remains required. */
    if((!f1c_lifecycle_terminal(v.retired,v.finish) &&
        !(dormant && v.role==-1 && v.finish==0 && !v.retired)) || v.ai_control)return v;
    if(!f1c_lifecycle_read_at(rd,vehicle+0x48A4,&v.team,4) ||
       !f1c_lifecycle_read_at(rd,vehicle+0x48A8,&v.bay,4) ||
       !f1c_lifecycle_read_at(rd,vehicle+0x34E0,v.body,12) ||
       !f1c_lifecycle_read_at(rd,manager+0x158,&table,4) ||
       !f1c_lifecycle_read_at(rd,manager+0x164,&width,4))return v;
    /* Native allocation search bounds team0..51 and bay0..2. */
    if(!table || width<1 || width>52 || v.team<0 || v.team>=(int)width || v.bay<0 || v.bay>2)return v;
    uint32_t offset=28u*((uint32_t)v.team+(uint32_t)v.bay*width);
    if(table>UINT32_MAX-offset-12u || !f1c_lifecycle_read_at(rd,table+offset,v.garage,12))return v;
    for(int i=0;i<3;i++){
        if(!isfinite(v.body[i]) || !isfinite(v.garage[i]) || fabsf(v.body[i])>1e7f || fabsf(v.garage[i])>1e7f)return v;
        float delta=v.body[i]-v.garage[i];d2+=delta*delta;
    }
    /* Empty garage entries are not evidence. Native allocator uses nonzero poses. */
    if(v.garage[0]==0 && v.garage[1]==0 && v.garage[2]==0)return v;
    v.garage_valid=1;v.garage_distance=sqrtf(d2);v.garage_clear=d2<=25.0f;
    return v;
}

/* Race incident clearance keeps its original terminal-only semantics. */
static F1CVehicleLifecycle f1c_vehicle_lifecycle_read(F1CLifecycleRead rd,uintptr_t vehicle) {
    return f1c_vehicle_lifecycle_read_impl(rd,vehicle,0);
}
/* Geometry observation for a caller-owned dormant extra vehicle, not evidence
 * of ownership, incident clearance, or authorization to deploy by itself. */
static F1CVehicleLifecycle f1c_vehicle_dormant_garage_read(F1CLifecycleRead rd,uintptr_t vehicle) {
    return f1c_vehicle_lifecycle_read_impl(rd,vehicle,1);
}
