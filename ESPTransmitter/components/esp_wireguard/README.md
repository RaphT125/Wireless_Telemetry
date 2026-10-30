# esp_wireguard

WireGuard implementation for ESP-IDF, copied from
[trombik/esp_wireguard](https://github.com/trombik/esp_wireguard) v0.9.0
(BSD-3-Clause, Copyright (c) 2021 Kenta Ida and Copyright (c) 2021 Daniel Hope, see `LICENSE`).
It is based on [wireguard-lwip](https://github.com/smartalock/wireguard-lwip) by Daniel Hope.
The x25519 code (Copyright (c) 2015-2016 Cryptography Research, Inc., MIT licence) is covered by `src/crypto/refc/x25519-license.txt`.

## Changes for ESP-IDF v6.1

- `src/wireguard-platform.c`: replaced the mbedTLS entropy/CTR-DRBG random generator
  (API removed in the mbedTLS version shipped with ESP-IDF v6) with `esp_fill_random`.
- `CMakeLists.txt`: disabled `-Wunterminated-string-initialization` for `src/wireguard.c`
  (the protocol constants are intentionally stored without a NUL terminator);
  dropped the unused NaCl x25519 sources; added `esp_hw_support`.
- `src/crypto.h`, `Kconfig`: removed the NaCl x25519 option.
