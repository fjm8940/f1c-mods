#pragma once
/* The x86 game can leave x87 in 24-bit precision. Policy time arithmetic
 * requires double precision even for two calls with the identical timestamp.
 * Change only policy math, never the environment of native game calls. */
static inline unsigned short pit_fp_control() {
    unsigned short word;
    __asm__ __volatile__("fnstcw %0" : "=m"(word) : : "memory");
    return word;
}
static inline void pit_fp_restore(unsigned short word) {
    __asm__ __volatile__("fldcw %0" : : "m"(word) : "memory");
}
class PitPolicyPrecision {
    unsigned short saved;
public:
    PitPolicyPrecision():saved(pit_fp_control()) {
        pit_fp_restore((saved & ~0x0f00u) | 0x0200u); // 53-bit, nearest
    }
    ~PitPolicyPrecision() {pit_fp_restore(saved);}
    PitPolicyPrecision(const PitPolicyPrecision&)=delete;
    PitPolicyPrecision& operator=(const PitPolicyPrecision&)=delete;
};
template<class F> static auto pit_policy_math(F call) -> decltype(call()) {
    PitPolicyPrecision scope;
    return call();
}
