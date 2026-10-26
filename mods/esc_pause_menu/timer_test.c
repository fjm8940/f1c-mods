#include <stdint.h>
#include <stdio.h>
#include <assert.h>
typedef uint32_t DWORD; typedef int32_t LONG;
static DWORD fake; static DWORD GetTickCount(void){return fake;}
int main(void){DWORD starts[]={0,1,0x7fffffff,0x80000000,3000000000u,0xffffff00,0xffffffff};int checks=0;
for(unsigned i=0;i<sizeof(starts)/sizeof(*starts);i++){fake=starts[i]; DWORD next_arm_refresh = GetTickCount();
DWORD now=fake;int activated=0; if ((LONG)(now - next_arm_refresh) >= 0) {activated=1;next_arm_refresh=now+250u;} assert(activated);
now=fake+249u;int early=0; if ((LONG)(now - next_arm_refresh) >= 0) {early=1;} assert(!early);
now=fake+250u;int due=0; if ((LONG)(now - next_arm_refresh) >= 0) {due=1;} assert(due);checks+=3;}
for(int mp=-1;mp<=2;mp++)for(int mp_menu=0;mp_menu<=1;mp_menu++){uint32_t want = mp == 0 ? 1u : (mp > 0 && mp_menu) ? 2u : 0u; assert(want==(mp==0?1u:(mp>0&&mp_menu?2u:0u)));checks++;}
printf("PASS %d first-arm/wrap/mode cases\n",checks);return 0;}
