#ifndef F1C_SC_FAULT_H
#define F1C_SC_FAULT_H
/* Optional, process-scoped fault notification. Never conveys control ownership:
   RF must still acquire the separate race-control lease after physical SC reset.
   Single SC publisher; RF consumes an exact sequence. Old RC ABI is untouched. */
#include <windows.h>
#include <stdint.h>
#define F1C_SCF_MAGIC 0x53434631L
#define F1C_SCF_DAMAGE 1
#define F1C_SCF_STUCK 2
#define F1C_SCF_IDENTITY 3
#define F1C_SCF_RESET 4
#define F1C_SCF_PROGRESS 5
typedef struct {
 volatile LONG magic,abi,size,rf_registered,state,sequence,generation,vehicle,reason;
} F1CSCFault;
typedef struct {LONG sequence,generation,vehicle,reason;} F1CSCFaultRequest;
static LONG f1c_scf_read(volatile LONG*p){return InterlockedCompareExchange(p,0,0);}
static F1CSCFault*f1c_scf_open(void){
 static F1CSCFault*v;static HANDLE mapping;FILETIME created,exit_time,kernel,user;char name[100];
 if(v)return v;
 if(!GetProcessTimes(GetCurrentProcess(),&created,&exit_time,&kernel,&user))return NULL;
 wsprintfA(name,"Local\\F1C_SC_FAULT_V1_%lu_%08lX%08lX",GetCurrentProcessId(),created.dwHighDateTime,created.dwLowDateTime);
 mapping=CreateFileMappingA(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,sizeof(*v),name);if(!mapping)return NULL;
 DWORD existing=GetLastError();F1CSCFault*c=(F1CSCFault*)MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(*v));
 if(!c){CloseHandle(mapping);mapping=NULL;return NULL;}
 if(existing!=ERROR_ALREADY_EXISTS){c->abi=1;c->size=sizeof(*c);InterlockedExchange(&c->magic,F1C_SCF_MAGIC);}
 if(f1c_scf_read(&c->magic)!=F1C_SCF_MAGIC||c->abi!=1||c->size!=(LONG)sizeof(*c)){
  UnmapViewOfFile(c);CloseHandle(mapping);mapping=NULL;return NULL;
 }v=c;return v;
}
static LONG f1c_scf_publish(F1CSCFault*s,LONG generation,LONG vehicle,int reason){
 if(!s||generation<=0||reason<F1C_SCF_DAMAGE||reason>F1C_SCF_PROGRESS)return 0;
 if(InterlockedCompareExchange(&s->state,-1,0)!=0)return 0;
 LONG sequence=f1c_scf_read(&s->sequence);
 if(sequence>=0x3fffffff){InterlockedExchange(&s->state,0);return 0;}
 sequence++;s->generation=generation;s->vehicle=vehicle;s->reason=reason;
 InterlockedExchange(&s->sequence,sequence);InterlockedExchange(&s->state,sequence);return sequence;
}
static int f1c_scf_peek(F1CSCFault*s,LONG generation,F1CSCFaultRequest*out){
 if(!s||!out)return 0;LONG seq=f1c_scf_read(&s->state);if(seq<=0)return 0;
 F1CSCFaultRequest r={seq,f1c_scf_read(&s->generation),f1c_scf_read(&s->vehicle),f1c_scf_read(&s->reason)};
 if(r.generation!=generation||r.reason<F1C_SCF_DAMAGE||r.reason>F1C_SCF_PROGRESS||f1c_scf_read(&s->state)!=seq)return 0;
 *out=r;return 1;
}
static int f1c_scf_ack(F1CSCFault*s,LONG seq){return s&&seq>0&&InterlockedCompareExchange(&s->state,0,seq)==seq;}
/* SC calls only at its authenticated session boundary; no owner is revoked. */
static void f1c_scf_reset(F1CSCFault*s){if(s){LONG seq=f1c_scf_read(&s->state);if(seq>0)f1c_scf_ack(s,seq);}}
#endif
