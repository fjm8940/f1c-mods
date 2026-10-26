#pragma once
#include <stdint.h>
#include <stddef.h>
/* sub_424080 parses five (condition,type) pairs at vehicle+16708.
 * Type5, condition0 is start StallDNS class. Descriptor is not current
 * engine state; caller must corroborate a present stationary road hazard. */
typedef int (*F1CStartStallRead)(uintptr_t,void*,size_t);
static int f1c_start_stall_dns(F1CStartStallRead rd,uintptr_t vehicle){
 int32_t pairs[10];
 if(!rd||!vehicle||vehicle>UINT32_MAX-0x416cu||
    !rd(vehicle+0x4144u,pairs,sizeof(pairs)))return 0;
 for(int i=0;i<5;i++)if(pairs[2*i]==0&&pairs[2*i+1]==5)return 1;
 return 0;
}
