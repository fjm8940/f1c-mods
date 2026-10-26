/* sc_usage.h - Safety Car usage hooks. The public build compiles no usage
 * summary; these stubs keep the call sites in safety_car_guardrails.c. */
#ifndef SC_USAGE_H
#define SC_USAGE_H
static void scu_tap(const char *s) { (void)s; }
static void scu_request(void) { }
static void scu_pump(void) { }
static void scu_flush(const char *why) { (void)why; }
#endif /* SC_USAGE_H */
