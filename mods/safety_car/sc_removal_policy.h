#ifndef SC_REMOVAL_POLICY_H
#define SC_REMOVAL_POLICY_H
#include <math.h>
#include <string.h>
typedef struct {int owned; unsigned saved[3],written[3];} SCRemovalLoan;
static int sc_removal_seconds(unsigned bits,float *seconds){memcpy(seconds,&bits,4);return isfinite(*seconds)&&*seconds>=0&&*seconds<=86400;}
/* RF state: 0 absent/inactive, 1 active compatible, -1 uncertain/legacy.
   Returns bitmask of changed slots for the authenticated native adapter. */
static unsigned sc_removal_step(SCRemovalLoan *s,unsigned values[3],int rf,int active,float desired){
 unsigned changed=0;
 if(rf!=0){s->owned=0;return 0;} /* never restore over an RF cleanup write */
 if(!active){
  if(s->owned)for(int i=0;i<3;i++)if(values[i]==s->written[i]){values[i]=s->saved[i];changed|=1u<<i;}
  s->owned=0;return changed;
 }
 if(!isfinite(desired)||desired<1||desired>600)return 0;
 if(s->owned){
  /* User/other-plugin edits end the loan; do not repeatedly fight them. */
  for(int i=0;i<3;i++)if(values[i]!=s->written[i])return 0;
  return 0;
 }
 for(int i=0;i<3;i++){float seconds;if(!sc_removal_seconds(values[i],&seconds))return 0;}
 unsigned bits;memcpy(&bits,&desired,4);
 for(int i=0;i<3;i++){s->saved[i]=values[i];values[i]=s->written[i]=bits;changed|=1u<<i;}
 s->owned=1;return changed;
}
#endif
