#pragma once
#include <windows.h>
#include <stdint.h>
#include <string.h>
/* RF's reviewed AIW geometry, shared with SC; no controller authority here.
   Process-creation scoped, exact vehicle identity, bounded freshness, seqlock. */
#define F1C_GEOMETRY_SLOTS 104
typedef struct { LONG vehicle,stamp,known,near_road,blocked; } F1CIncidentGeometry;
typedef struct { volatile LONG sequence; F1CIncidentGeometry row; } F1CGeometrySlot;
typedef struct { LONG magic,size; F1CGeometrySlot cars[F1C_GEOMETRY_SLOTS]; F1CGeometrySlot decision[2][F1C_GEOMETRY_SLOTS]; } F1CGeometryMap;
static __inline F1CGeometryMap *f1c_geometry_open(void) {
    static F1CGeometryMap *view;static HANDLE mapping;
    FILETIME c,e,k,u;char name[100];
    if(view)return view;
    if(!GetProcessTimes(GetCurrentProcess(),&c,&e,&k,&u))return NULL;
    wsprintfA(name,"Local\\F1C_INCIDENT_GEOMETRY_V1_%lu_%08lX%08lX",GetCurrentProcessId(),c.dwHighDateTime,c.dwLowDateTime);
    mapping=CreateFileMappingA(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,sizeof(*view),name);
    if(!mapping)return NULL;
    int fresh=GetLastError()!=ERROR_ALREADY_EXISTS;
    F1CGeometryMap *v=(F1CGeometryMap*)MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(*v));
    if(!v){CloseHandle(mapping);return NULL;}
    if(fresh){v->size=sizeof(*v);InterlockedExchange(&v->magic,0x31474946);}
    if(v->magic!=0x31474946 || v->size!=sizeof(*v)){UnmapViewOfFile(v);CloseHandle(mapping);return NULL;}
    view=v;return view;
}
static __inline void f1c_geometry_publish(int slot,uintptr_t vehicle,int known,int near_road,int blocked) {
    F1CGeometryMap *m=f1c_geometry_open();if(!m||slot<0||slot>=F1C_GEOMETRY_SLOTS)return;
    F1CGeometrySlot *s=&m->cars[slot];InterlockedIncrement(&s->sequence);MemoryBarrier();
    s->row.vehicle=(LONG)vehicle;s->row.stamp=GetTickCount();s->row.known=known;
    s->row.near_road=near_road;s->row.blocked=blocked;MemoryBarrier();InterlockedIncrement(&s->sequence);
}
static __inline F1CIncidentGeometry f1c_geometry_read(int slot,uintptr_t vehicle) {
    F1CIncidentGeometry r={0};F1CGeometryMap *m=f1c_geometry_open();
    if(!m||slot<0||slot>=F1C_GEOMETRY_SLOTS)return r;
    F1CGeometrySlot *s=&m->cars[slot];LONG seq=InterlockedCompareExchange(&s->sequence,0,0);
    if(seq&1)return r;
    memcpy(&r,&s->row,sizeof(r));MemoryBarrier();
    if(seq!=InterlockedCompareExchange(&s->sequence,0,0)||r.vehicle!=(LONG)vehicle||
       (DWORD)(GetTickCount()-(DWORD)r.stamp)>500)memset(&r,0,sizeof(r));
    return r;
}

/* Inversion-path diagnostic reasons only: 0 unavailable,1 not inverted,
   2 pit,3 location unknown,4 away from road,5 confirming/other gates,
   6 local incident qualifies,7 RF priority,8 session/automatic gate.
   Never authorize actions from these diagnostic fields. */
static __inline void f1c_decision_publish(int producer,int slot,uintptr_t vehicle,int reason){
    F1CGeometryMap *m=f1c_geometry_open();if(!m||producer<0||producer>1||slot<0||slot>=104)return;
    F1CGeometrySlot *s=&m->decision[producer][slot];InterlockedIncrement(&s->sequence);MemoryBarrier();
    s->row.vehicle=(LONG)vehicle;s->row.known=reason;s->row.stamp=GetTickCount();
    MemoryBarrier();InterlockedIncrement(&s->sequence);
}
static __inline int f1c_decision_read(int producer,int slot,uintptr_t vehicle){
    F1CGeometryMap *m=f1c_geometry_open();if(!m||slot<0||slot>=104||producer<0||producer>1)return 0;
    F1CGeometrySlot *s=&m->decision[producer][slot];LONG seq=InterlockedCompareExchange(&s->sequence,0,0);
    if(seq&1)return 0;
    F1CIncidentGeometry v=s->row;MemoryBarrier();
    return seq==InterlockedCompareExchange(&s->sequence,0,0)&&v.vehicle==(LONG)vehicle&&
      (DWORD)(GetTickCount()-(DWORD)v.stamp)<=500?v.known:0;
}
