#include <stdio.h>
#include <string.h>
#include "f1c_usage.h"
static F1cUsageEvents E; static char buf[40000];
int main(void){ int i,f=0; f1c_usage_events_reset(&E);
 F1C_USAGE_EV2(&E,"grid_built",24,0.5); F1C_USAGE_EV1(&E,"Matias Driver",1); F1C_USAGE_EV1(&E,"c:/x",1);
 for(i=0;i<500;i++) F1C_USAGE_EV1(&E,"tick",i);
 size_t n=f1c_usage_events_json(&E,buf,sizeof buf);
 if(!n){puts("empty");f=1;} if(E.dropped!=103){printf("dropped %lu\n",E.dropped);f=1;}
 if(strstr(buf,"Matias")||strstr(buf,"c:")){puts("leak");f=1;}
 if(!strstr(buf,"\"tick\",499]")){puts("last");f=1;}
 char r[64]="{\"schema\":1,\"mod\":\"x\"}"; f1c_usage_mark_v2(r); if(strncmp(r,"{\"schema\":2,",12)){puts("mark");f=1;}
 E.count=0; E.pos=0; F1C_USAGE_EV2(&E,"a",1.25,3); f1c_usage_events_json(&E,buf,sizeof buf); printf("%s\n",buf);
 puts(f?"FAIL":"PASS"); return f; }
