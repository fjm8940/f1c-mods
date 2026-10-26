#pragma once
/* A changing field is not evidence of losing the owned car. Only a stable
 * identity mismatch may hold a cycle. Recovery never clears another fault. */
typedef struct { int holding, owns_fault; unsigned epoch; } SCIdentityGuard;
static int sc_identity_guard_step(SCIdentityGuard *s,unsigned epoch,
 int stable,int owned,int engaged,int *fault) {
 if(s->epoch!=epoch){s->holding=s->owns_fault=0;s->epoch=epoch;}
 if(!stable)return 0;
 if(!engaged){s->holding=s->owns_fault=0;return owned;}
 if(!owned){
  if(!s->holding){s->holding=1;s->owns_fault=!*fault;}
  *fault=1;return 0;
 }
 if(s->holding){if(s->owns_fault)*fault=0;s->holding=s->owns_fault=0;}
 return 1;
}
