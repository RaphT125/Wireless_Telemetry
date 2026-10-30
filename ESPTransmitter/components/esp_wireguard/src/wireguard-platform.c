/*
 * From trombik/esp_wireguard v0.9.0 (BSD-3-Clause, see LICENSE).
 * Modified for ESP-IDF v6.1: random bytes come from esp_fill_random instead of mbedTLS.
 */
#include "wireguard-platform.h"

#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <inttypes.h>
#include <lwip/sys.h>
#include <esp_random.h>
#include <esp_err.h>

#include "crypto.h"

esp_err_t wireguard_platform_init() {
	return ESP_OK;
}

void wireguard_random_bytes(void *bytes, size_t size) {
	esp_fill_random(bytes, size);
}

uint32_t wireguard_sys_now() {
	return sys_now();
}

void wireguard_tai64n_now(uint8_t *output) {
	struct timeval tv;
	gettimeofday(&tv, NULL);

	uint64_t seconds = 0x400000000000000aULL + tv.tv_sec;
	uint32_t nanos = tv.tv_usec * 1000;
	U64TO8_BIG(output + 0, seconds);
	U32TO8_BIG(output + 8, nanos);
}

bool wireguard_is_under_load() {
	return false;
}
