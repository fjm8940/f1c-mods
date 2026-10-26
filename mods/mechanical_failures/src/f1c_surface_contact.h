#pragma once
#include <stdint.h>
#include <stddef.h>
/* Reviewed Red Flag v76 drivable_surface_contacts chain: vehicle+0x3674,
 * four current wheel hits at stride0xf8 -> hit+0x54 material -> tags+0x30.
 * Same exact-target layout, read-only. Road/wet-road/rumble tags do not prove
 * free passage or racing-line overlap. Caller supplies bounded stable-thread read. */
typedef int (*F1CSurfaceRead)(uintptr_t,void*,size_t);
typedef struct { int known,drivable; } F1CSurfaceContacts;
static F1CSurfaceContacts f1c_surface_contacts(uintptr_t vehicle,F1CSurfaceRead read_at){
 F1CSurfaceContacts out={0,0};
 if(!vehicle||!read_at||vehicle>UINTPTR_MAX-0x3960u)return out;
 for(int i=0;i<4;i++){
   uint32_t hit=0,material=0,tags[3];
   if(!read_at(vehicle+0x3674u+i*0xf8u,&hit,4)||!hit||hit>UINT32_MAX-0x58u)continue;
   if(!read_at((uintptr_t)hit+0x54u,&material,4)||!material||material>UINT32_MAX-0x3cu)continue;
   if(!read_at((uintptr_t)material+0x30u,tags,sizeof(tags)))continue;
   out.known++;
   if(tags[2]==27u && ((tags[0]==6u&&tags[1]==21u)||
      (tags[0]==7u&&tags[1]==22u)||(tags[0]==11u&&tags[1]==21u)))out.drivable++;
 }
 return out;
}
