/*
 * site_patch_test.c - Extended Damage Commentary COMP-04b site-patch test
 * (2026-10-08 batch E).
 *
 * Includes the real extended_damage_commentary.c and drives its
 * edc_patch_sites() core through a fake site arena plus an injectable
 * memory-protection shim. It forces each of the six patch sites to fail in
 * turn - once at the make-writable step and once at the restore-protection
 * step - and asserts that a refusal leaves NO partial patch: every site is
 * back to the expected original bytes.
 *
 * Build/run: run_tests.ps1 (output to %TEMP%).
 */
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "extended_damage_commentary.c"

#define WIDTH 4u

static unsigned char g_arena[SITE_COUNT][8];
static int g_fail_write_at = -1;    /* make-writable fails at this row     */
static int g_fail_restore_at = -1;  /* restore-protect fails at this row   */
static int g_unreadable_at = -1;    /* this row is unreadable              */

static int fake_index(const void *a)
{
    return (int)(((const unsigned char *)a - &g_arena[0][0]) / 8);
}

static int fake_readable(const void *a)
{
    return fake_index(a) != g_unreadable_at;
}

static int fake_vprotect(void *a, unsigned long np, unsigned long *op)
{
    int idx = fake_index(a);
    if (op) *op = PAGE_EXECUTE_READ;
    if (np == PAGE_EXECUTE_READWRITE) {
        if (idx == g_fail_write_at) return 0;
    } else {
        if (idx == g_fail_restore_at) return 0;
    }
    return 1;
}

static const EdcSiteOps g_fake = { fake_readable, fake_vprotect };

static void reset_arena(const unsigned char *expected)
{
    int i;
    for (i = 0; i < SITE_COUNT; i++) {
        memset(g_arena[i], 0xEE, sizeof g_arena[i]);
        memcpy(g_arena[i], expected, WIDTH);
    }
    g_fail_write_at = g_fail_restore_at = g_unreadable_at = -1;
}

static int count_patched(const unsigned char *patched)
{
    int i, n = 0;
    for (i = 0; i < SITE_COUNT; i++)
        if (memcmp(g_arena[i], patched, WIDTH) == 0) n++;
    return n;
}

static int all_expected(const unsigned char *expected)
{
    int i;
    for (i = 0; i < SITE_COUNT; i++)
        if (memcmp(g_arena[i], expected, WIDTH) != 0) return 0;
    return 1;
}

int main(void)
{
    const unsigned long exp_val = decode_site_value(0xDC03E61AUL);
    const unsigned long pat_val = decode_site_value(0xE20C19E5UL);
    unsigned char expected[WIDTH], patched[WIDTH];
    unsigned char *sites[SITE_COUNT];
    int i, fails = 0;

    memcpy(expected, &exp_val, WIDTH);
    memcpy(patched, &pat_val, WIDTH);
    for (i = 0; i < SITE_COUNT; i++) sites[i] = g_arena[i];

    reset_arena(expected);
    if (edc_patch_sites(sites, expected, patched, WIDTH, &g_fake) != SITE_COUNT
        || count_patched(patched) != SITE_COUNT) {
        puts("FAIL success case"); ++fails;
    }

    reset_arena(expected);
    memcpy(g_arena[2], patched, WIDTH);
    if (edc_patch_sites(sites, expected, patched, WIDTH, &g_fake) != SITE_COUNT
        || count_patched(patched) != SITE_COUNT) {
        puts("FAIL already-patched case"); ++fails;
    }

    reset_arena(expected);
    memcpy(g_arena[3], expected, WIDTH);
    g_arena[3][WIDTH - 1] ^= 0xFF;   /* unexpected byte */
    if (edc_patch_sites(sites, expected, patched, WIDTH, &g_fake) != -1
        || count_patched(patched) != 0) {
        puts("FAIL unexpected-bytes refusal"); ++fails;
    }

    reset_arena(expected);
    g_unreadable_at = 4;
    if (edc_patch_sites(sites, expected, patched, WIDTH, &g_fake) != -1
        || count_patched(patched) != 0) {
        puts("FAIL unreadable refusal"); ++fails;
    }

    for (i = 0; i < SITE_COUNT; i++) {
        reset_arena(expected);
        g_fail_write_at = i;
        if (edc_patch_sites(sites, expected, patched, WIDTH, &g_fake) != -1
            || count_patched(patched) != 0 || !all_expected(expected)) {
            printf("FAIL make-writable failure at site %d left a partial patch\n", i);
            ++fails;
        }
    }

    for (i = 0; i < SITE_COUNT; i++) {
        reset_arena(expected);
        g_fail_restore_at = i;
        if (edc_patch_sites(sites, expected, patched, WIDTH, &g_fake) != -1
            || count_patched(patched) != 0 || !all_expected(expected)) {
            printf("FAIL restore-protect failure at site %d left a partial patch\n", i);
            ++fails;
        }
    }

    if (fails) { printf("SITE PATCH TEST: %d FAILURES\n", fails); return 1; }
    puts("SITE PATCH TEST: ALL PASS (success, 2 refusals, 6+6 forced failures, all reverted)");
    return 0;
}
