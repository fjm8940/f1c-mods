/* sha256.h — compact SHA-256 (FIPS 180-4) for the sidecar integrity report.
 * Used by /health to publish the workbench_ui.js hash beside the DLL. */
#ifndef F1C_SHA256_H
#define F1C_SHA256_H

#include <stddef.h>
#include <stdint.h>

void sha256_buf(const void *data, size_t len, unsigned char out[32]);
void sha256_hex(const unsigned char digest[32], char out[65]);

#endif /* F1C_SHA256_H */
