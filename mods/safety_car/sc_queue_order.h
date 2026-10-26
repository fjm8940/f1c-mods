#ifndef SC_QUEUE_ORDER_H
#define SC_QUEUE_ORDER_H
#include <string.h>
#include <math.h>
#define SC_QUEUE_SLOTS 128
typedef struct { int known,participant,pit,position,roster; char name[32]; float distance; } SCQueueCar;

/* Cyclic distance to the physical pace car, independent of classification and
 * lap deficit. Called before initial order capture and insertion of pit rejoins.
 * Existing locked neighbours still keep their order in sc_queue_step. */
static int sc_queue_physical_positions(SCQueueCar *c,int n,float anchor,float lap) {
    float gap[SC_QUEUE_SLOTS];
    if(!c||n<0||n>SC_QUEUE_SLOTS||!isfinite(lap)||lap<100||!isfinite(anchor)||anchor<0||anchor>=lap)return 0;
    for(int i=0;i<n;i++)if(c[i].participant&&!c[i].pit){
        if(!c[i].known||!isfinite(c[i].distance)||c[i].distance<0||c[i].distance>=lap)return 0;
        gap[i]=fmodf(anchor-c[i].distance+lap,lap);
    }
    for(int i=0;i<n;i++)if(c[i].participant&&!c[i].pit){
        int rank=1;
        for(int j=0;j<n;j++)if(c[j].participant&&!c[j].pit &&
            (gap[j]<gap[i]||(gap[j]==gap[i]&&j<i)))rank++;
        c[i].position=rank;
    }
    return 1;
}
typedef struct { unsigned epoch; int rank[SC_QUEUE_SLOTS],roster[SC_QUEUE_SLOTS]; char name[SC_QUEUE_SLOTS][32]; } SCQueueOrder;
static void sc_queue_reset(SCQueueOrder *s){memset(s,0,sizeof(*s));}
/* Existing on-track members retain relative order. A known pit entry leaves
   the queue; a rejoin inserts at its current physical position among
   existing members. No wholesale live-position sort can legitimize a pass. */
static void sc_queue_step(SCQueueOrder *s,const SCQueueCar *c,int n,unsigned epoch){
    if(!s||!c||n<0||n>SC_QUEUE_SLOTS)return;
    if(s->epoch!=epoch){sc_queue_reset(s);s->epoch=epoch;}
    for(int i=0;i<n;i++){
        if(!c[i].known)continue; /* uncertainty is not removal */
        if(s->rank[i] && (!c[i].participant||c[i].pit||s->roster[i]!=c[i].roster||memcmp(s->name[i],c[i].name,32))){
            int old=s->rank[i];s->rank[i]=0;
            for(int j=0;j<n;j++)if(s->rank[j]>old)s->rank[j]--;
        }
    }
    for(int i=0;i<n;i++){
        int before=0;
        if(s->rank[i]||!c[i].known||!c[i].participant||c[i].pit||c[i].position<=0||c[i].position>SC_QUEUE_SLOTS)continue;
        for(int j=0;j<n;j++)if(s->rank[j] && c[j].known &&
            (c[j].position<c[i].position||(c[j].position==c[i].position&&j<i)))before++;
        for(int j=0;j<n;j++)if(s->rank[j]>before)s->rank[j]++;
        s->rank[i]=before+1;s->roster[i]=c[i].roster;memcpy(s->name[i],c[i].name,32);
    }
}
static int sc_queue_identity(const SCQueueOrder *s,int i,unsigned epoch,int roster,const char *name){
    return i>=0&&i<SC_QUEUE_SLOTS&&s->epoch==epoch&&s->rank[i]>0&&s->roster[i]==roster&&!memcmp(s->name[i],name,32);
}
#endif
