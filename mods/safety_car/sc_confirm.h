#ifndef SC_CONFIRM_H
#define SC_CONFIRM_H
#include <string.h>
/* A second released-and-pressed edge authorizes only the same action/state.
   Caller tracks held keys even outside focus; no input queue survives a gate. */
typedef struct { int action, state; unsigned epoch; double expires; } SCConfirm;
static int sc_confirm_step(SCConfirm *p,int authorized,int action,int state,
                           unsigned epoch,double now,double window) {
    if(!authorized || p->epoch!=epoch || p->state!=state || now>p->expires)
        memset(p,0,sizeof(*p));
    if(!authorized || !action)return 0;
    if(p->action==action){memset(p,0,sizeof(*p));return 2;}
    p->action=action;p->state=state;p->epoch=epoch;p->expires=now+window;
    return 1;
}
#endif
