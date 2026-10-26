/* Exact executable identity required by the fixed-address SDK. */
#pragma once
#include <stdint.h>

#define F1C_TARGET_IMAGE_BASE       0x00400000u
#define F1C_TARGET_MACHINE          0x014Cu
#define F1C_TARGET_TIMESTAMP        0x3E9C24F6u
#define F1C_TARGET_ENTRY_RVA        0x004D8000u
#define F1C_TARGET_IMAGE_SIZE       0x005FA000u
#define F1C_TARGET_CHECKSUM         0x00306F7Bu
#define F1C_TARGET_FILE_SIZE        3141632u
#define F1C_TARGET_SHA256 \
    "1A70E5ED0C4A059F7F66741B1682D0AEDB14921AA6140CDB70B09549FDF3A003"

/* Optional in-memory code fingerprints. A hook installed before validation
 * may change these bytes; PE identity remains the authoritative compatibility
 * gate while fingerprints diagnose unexpected runtime patching. */
#define F1C_TARGET_PROBE_ADDRESS    0x0042BC80u
#define F1C_TARGET_PROBE_LENGTH     8u
static const uint8_t F1C_TARGET_PROBE_BYTES[F1C_TARGET_PROBE_LENGTH] = {
    0x53, 0x56, 0x33, 0xF6, 0x33, 0xDB, 0xB8, 0xD8
};
