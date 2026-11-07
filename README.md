# Wireless_Telemetry

## Purpose
Providing near-real-time telemetry data for Formula UBC SAE race car using custom CAN protocol sent from AIM EVO5 dataloger. CAN data is sent to an ESP32 wired to a SIM7600 which is then encrpyted through wireguard and sent via UDP to a windows service where data will then be stored in a postgres database and displayed on Grafana. Users with wireguard connection to the server will also be able to view the wireguard dashboard live in order to better understand and coorelate car's in real time and eliminate the hassle of manually pulling data from the car. 

## Feature
**ESP32**
- Interrupt-driven CAN capture at 1Mbit/s
- CAN frame are batched into binary UDP packet format
- PSRAM ring buffer prevents dataloss during dropouts, data will resend after connection is restablished
- Wi-Fi and LTE connectivity are the primary method of transfering data
- Wireguard tunnel to ensure secured connection to the server 

**Desktop service** 
- Hosts a wireguard endpoint to ensure end to end security when exposing a port
- Custom validation to packets to record packet loss and ensure only valid data packets are recorded and decoded 
- Process and decode binary UDP packets back into CAN frames where a custom CAN MAP decodes and converts messages back into scaled signals
- Batching inserts into a timscaleDB hypertable in a postgreSQL database, buffering writes if database becomes unavailable 
- Live stream data to Grafana live and allows wireguard connected users to view on their devices 
- Rate-limited logging, recording dropped packet, databse and connection errors.
- Periodic pulling from google sheet/ locally cached csv for wireguard public key


## DataFormat From ESP32

| Header | bootId | baseTime | Sequence | Frame count | timeOffset | Byte order | Modified Can Frame |
| --- | --- | --- | --- | --- | --- | --- | --- |

##Tech stack 
- **Firmware** C, ESP-IDF, FREERTOS, esp_modem, lwIp, WireGuard
- **Server** C++20, Asio, libpqxx, cpr, wireguard-nt, CMake + vcpkg
- **Data** PostgreSQL + TimescaleDB, Grafana 
- **Hardware** LilyGo T-Sim76600G-H 

### Desktop service (Windows)
Requires vcpkg (`VCPKG_ROOT` set), MinGW, PostgreSQL with TimescaleDB, and `wireguard.dll` from [download.wireguard.com](https://download.wireguard.com/wireguard-nt/) placed next to the executable.

```
cmake -B build -G Ninja
cmake --build build
build\telemetry_service.exe [port]    # default port 5005, run as Administrator
```

### ESP32 firmware
Requires ESP-IDF v6.x.

1. Create `ESPTransmitter/main/wifi_config.h` (git-ignored) with your Wi-Fi credentials, the device's WireGuard private key, the server's public key, endpoint and the LTE APN.
2. Build and flash:
   ```
   cd ESPTransmitter
   idf.py build
   idf.py -p COMx flash monitor
   ```

### Credits
- [esp_wireguard](https://github.com/trombik/esp_wireguard) by Tomoyuki Sakurai (BSD-3-Clause), vendored with small patches for ESP-IDF v6. See `ESPTransmitter/components/esp_wireguard/README.md`.
