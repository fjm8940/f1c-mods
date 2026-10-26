#include <stdio.h>
#include "f1c_usage.h"
static F1cUsageEvents E; static char buf[40000];
static const char *L[]={
"race grid: source=qualifying cars=24 moved=24 GRID 0:24 32:2",
"qualifying keep: archive %USERPROFILE%\\Documents\\x.tsv",
"time warp: ON x10 (host Ctrl+T, session_type=5; clients not synced)",
"MF sync peer 203.0.113.1:65078",
"COOP-E13 refused content request: SeasonData\\x.veh",
"released bot: slot=37 name=Tester A (2002) (a human took this car) roster_after=-1 total=24",
"frames: avg 164.8 fps, worst frame 16ms, cars=24, session_type=1",
"Matias Driver: r10 teleport detected; pit-entry call suppressed",
"pit exit blocked: Stephane Sarrazin (2002) (session over, session_type=5)",
0};
int main(void){int i,f=0;for(i=0;L[i];i++)f1c_usage_ev_line(&E,L[i]);f1c_usage_events_json(&E,buf,sizeof buf);puts(buf);
 if(strstr(buf,"Matias")||strstr(buf,"Tester A")||strstr(buf,"85.156")||strstr(buf,"Users")||strstr(buf,"Sarrazin")){puts("LEAK");f=1;}
 puts(f?"FAIL":"PASS");return f;}
