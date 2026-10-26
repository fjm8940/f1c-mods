#ifndef F1C_RESTART_PENALTIES_H
#define F1C_RESTART_PENALTIES_H
/* Reviewed static evidence, native acceptance pending. Restart-only snapshot;
 * never use this to clear penalties during ordinary racing. Caller owns exact
 * vehicle identity, writable extent, offline target, and broker game thread.
 * AccumulatorWithFixedArray_004bbed0: four inline {laps_left,seconds} pairs.
 * LapCompletion_004bc4e0: first word is a REMAINING count, not absolute lap.
 * ArrayShiftConditionalDispatch_004bbf70: service consumes one pair.
 * ArithmeticComparison_5660_004a5660: select native stop-go service mode.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "f1c_sdk.h"
enum { F1C_RESTART_PENALTY_CAPACITY=4,
       F1C_RESTART_PENALTY_BYTES=4+4*8,
       F1C_RESTART_PENALTY_MODE_ADDRESS=0x004A5660u,
       F1C_RESTART_PENALTY_PIT_OFFSET=0x32CCu };
typedef struct { int32_t count, pair[4][2]; } F1CRestartPenalties;
/* Exact native mode-selector body, including both relative calls and RET 4.
 * Executable memory alone does not establish an ABI-compatible function. */
static int f1c_restart_penalty_mode_code_valid(const void *code,size_t bytes) {
    static const uint8_t expected[]={
        0x8a,0x54,0x24,0x04,0x33,0xc0,0x3a,0xd0,0x74,0x22,
        0x8b,0x51,0x28,0xb8,0x02,0x00,0x00,0x00,0x3b,0xd0,0x74,0x23,
        0x89,0x41,0x28,0xb8,0x01,0x00,0x00,0x00,0x89,0x41,0x2c,
        0x89,0x41,0x30,0xe8,0x57,0xff,0xff,0xff,0xc2,0x04,0x00,
        0xc7,0x41,0x28,0x01,0x00,0x00,0x00,0x89,0x41,0x2c,
        0x89,0x41,0x30,0xe8,0x42,0xff,0xff,0xff,0xc2,0x04,0x00};
    return code && bytes>=sizeof expected && !memcmp(code,expected,sizeof expected);
}
static int f1c_restart_penalties_valid(const F1CRestartPenalties *s) {
    if(!s||s->count<0||s->count>4)return 0;
    for(int i=0;i<s->count;i++)
        if(s->pair[i][0]<0||s->pair[i][0]>3||s->pair[i][1]<0||s->pair[i][1]>3600)return 0;
    return 1;
}
static int f1c_restart_penalties_capture(const void *vehicle,size_t bytes,F1CRestartPenalties *out) {
    F1CRestartPenalties tmp;
    if(!vehicle||!out||bytes<F1C_OFF_noOfPenaltiesToProcess+sizeof(tmp))return 0;
    memcpy(&tmp,(const uint8_t*)vehicle+F1C_OFF_noOfPenaltiesToProcess,sizeof(tmp));
    if(!f1c_restart_penalties_valid(&tmp))return 0;
    *out=tmp;return 1;
}
/* No native call here: the broker adapter selects native service mode after
 * a complete field preflight. No object pointers or unrelated scoring copied. */
static int f1c_restart_penalties_restore(void *vehicle,size_t bytes,const F1CRestartPenalties *s) {
    if(!vehicle||bytes<F1C_OFF_noOfPenaltiesToProcess+sizeof(*s)||!f1c_restart_penalties_valid(s))return 0;
    memcpy((uint8_t*)vehicle+F1C_OFF_noOfPenaltiesToProcess,s,sizeof(*s));return 1;
}
#endif
