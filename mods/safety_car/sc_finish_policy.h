#ifndef SC_FINISH_POLICY_H
#define SC_FINISH_POLICY_H
/* Per identity: preserve unknown observations; no passing a stopped predecessor
   requirement and no finish/rank/scoring writes. */
static int sc_finish_loan_action(int owned,int identity,int observed,int finish,int pit) {
    if(!owned)return 0;
    if(!identity)return 3; /* drop stale ownership, never touch replacement */
    if(!observed)return 0;
    return finish!=0 || pit ? 1 : 2; /* release or retain own existing limit */
}
static int sc_finish_can_return(int unfinished,int unknown) {
    return unfinished==0 && unknown==0;
}
#endif
