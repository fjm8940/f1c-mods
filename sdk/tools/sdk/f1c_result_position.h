#pragma once
#include <stdint.h>
#include <string.h>
/* Reviewed ConditionalFieldSetter_004bc2f0: this=scoring subobject, arg=rank.
 * Native writer updates rank and its AI notification together. Caller must
 * prove owned offline field on session-end callback, before Steward_Post.
 * Exact supported EXE entry bytes; consumers already enforce target hash. */
static int f1c_result_position_ready(void){
 static const unsigned char bytes[]={0x56,0x8b,0xf1,0x8b,0x46,0x2c,0x57,0x8b,0x7c,0x24,0x0c,0x3b,0xc7,0x74,0x12,0x57};
 return !memcmp((const void*)0x004BC2F0u,bytes,sizeof(bytes));
}
static void f1c_result_position_set(uintptr_t vehicle,int rank){
 typedef void (__attribute__((thiscall)) *Set)(void*,int);
 ((Set)0x004BC2F0u)((void*)(vehicle+16436u),rank);
}
