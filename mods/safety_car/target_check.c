#include <windows.h>
#include <stdlib.h>
#include "layout_check.h"
static const char *sc_reason="not checked";
const char *sc_target_reason(void){return sc_reason;}
static int sc_file(const WCHAR *path){
 HANDLE f;DWORD size,got;unsigned char*b;int ok=0;
 sc_reason="executable cannot be read";
 f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
 if(f==INVALID_HANDLE_VALUE)return 0;
 size=GetFileSize(f,NULL);if(size<64||size>64*1024*1024){CloseHandle(f);return 0;}
 b=(unsigned char*)malloc(size);
 if(b&&ReadFile(f,b,size,&got,NULL)&&got==size){ok=layout_file(b,size);sc_reason=ok?"required layout and 19 code fingerprints accepted":"required memory layout or code fingerprint differs";}
 free(b);CloseHandle(f);return ok;
}
int sc_target_compatible(void){
 HMODULE module=GetModuleHandleW(NULL);MEMORY_BASIC_INFORMATION mbi;WCHAR path[32768];DWORD n;
 sc_reason="unexpected loaded image layout";
 if((ULONG_PTR)module!=0x400000u||!VirtualQuery(module,&mbi,sizeof(mbi))||mbi.RegionSize<4096)return 0;
 if(!layout_headers((const unsigned char*)module,4096,0))return 0;
 n=GetModuleFileNameW(NULL,path,32768);if(!n||n>=32768)return 0;
 return sc_file(path);
}
