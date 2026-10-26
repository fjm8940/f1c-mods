#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "sc_layout_fingerprints.h"
static uint16_t rd16(const unsigned char*p){return (uint16_t)(p[0]|p[1]<<8);}
static uint32_t rd32(const unsigned char*p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static const uint32_t required[][3]={{4096u,1689062u,1610612736u},{1695744u,127137u,1073741824u},{1826816u,3242608u,3221225472u},{5070848u,2272u,3221225472u},{5074944u,1024u,3221225472u},{5079040u,745472u,1610612736u},{5824512u,65536u,3221225472u}};
static int layout_headers(const unsigned char*b,size_t size,int file){
 uint32_t pe,t,n,i,j,image; if(size<64||rd16(b)!=0x5a4d)return 0;
 pe=rd32(b+60);if(pe>size-24||rd32(b+pe)!=0x4550||rd16(b+pe+4)!=0x14c)return 0;
 n=rd16(b+pe+6);t=rd16(b+pe+20);if(t<224||t>size||pe+24>size-t)return 0;
 if(rd16(b+pe+24)!=0x10b||rd32(b+pe+52)!=0x400000||(rd16(b+pe+94)&0x40))return 0;
 image=rd32(b+pe+80);t+=pe+24;if(n<7||n>96||t>size||n>(size-t)/40)return 0;
 for(i=0;i<7;i++){int found=0;for(j=0;j<n;j++){
  const unsigned char*s=b+t+j*40;uint32_t rva=rd32(s+12),vs=rd32(s+8),raw=rd32(s+20),len=rd32(s+16);
  if(rva==required[i][0]){if(found++||vs!=required[i][1]||(rd32(s+36)&0xe0000000)!=required[i][2]||rva>image||vs>image-rva)return 0;
   if(file&&(raw>size||len>size-raw))return 0;}
 }if(found!=1)return 0;}return 1;
}
static int layout_file(const unsigned char*b,size_t size){
 uint32_t pe,t,n,i,j;if(!layout_headers(b,size,1))return 0;
 pe=rd32(b+60);n=rd16(b+pe+6);t=pe+24+rd16(b+pe+20);
 for(i=0;i<sizeof(sc_probes)/sizeof(sc_probes[0]);i++){int found=0;uint32_t rva=sc_probes[i].offset;
  for(j=0;j<n;j++){const unsigned char*s=b+t+j*40;uint32_t va=rd32(s+12),len=rd32(s+16),raw=rd32(s+20);
   if(rva>=va&&rva-va<=len&&len-(rva-va)>=32){uint32_t delta=rva-va;
    if(found++||raw>size||delta>size-raw||size-raw-delta<32||memcmp(b+raw+delta,sc_probes[i].bytes,32))return 0;
   }
  }if(found!=1)return 0;
 }return 1;
}

