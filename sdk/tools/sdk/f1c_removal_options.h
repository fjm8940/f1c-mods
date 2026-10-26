#ifndef F1C_REMOVAL_OPTIONS_H
#define F1C_REMOVAL_OPTIONS_H
/* Exact supported F1C executable only. Reviewed source: red_flag/src/gr44.c
   PLAYER_OPTIONS_BASE, VEHICLE_REMOVAL_ENTRY_OFF, OPTION_VALUE_A/B_OFF and
   VEHICLE_REMOVAL_RUNTIME. Vehicle options only; no debris/parts writes.
   Caller must already have passed exact-target and offline session guards. */
#include <windows.h>
#include <stdint.h>
static volatile LONG *f1c_removal_option(int i){
 static const uintptr_t addresses[3]={0x007CE498u+9696u+20u,0x007CE498u+9696u+24u,0x007D0A8Cu};
 return i>=0&&i<3?(volatile LONG*)addresses[i]:NULL;
}
static int f1c_removal_options_read(unsigned out[3]){
 for(int i=0;i<3;i++){
  volatile LONG *v=f1c_removal_option(i);
  if(IsBadReadPtr((const void*)v,4)||IsBadWritePtr((void*)v,4))return 0;
  out[i]=(unsigned)InterlockedCompareExchange(v,0,0);
 }
 return 1;
}
static int f1c_removal_options_write(unsigned slot,unsigned before,unsigned after,DWORD game_thread){
 if(!game_thread||GetCurrentThreadId()!=game_thread||slot>=3)return 0;
 return (unsigned)InterlockedCompareExchange(f1c_removal_option((int)slot),(LONG)after,(LONG)before)==before;
}
#endif
