    #include <stdio.h>
    #include <stdint.h>
    #include <string.h>
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
    #include "freertos/queue.h"
    #include "freertos/ringbuf.h"
    #include "esp_heap_caps.h"
    #include "esp_log.h"
    #include "nvs_flash.h"
    #include "esp_netif.h"
    #include "esp_event.h"
    #include "esp_twai.h"
    #include "esp_twai_onchip.h"
    #include <time.h>
    #include <sys/time.h>
    #include "esp_timer.h"
    #include "esp_netif_sntp.h"
    #include "esp_random.h"
    #include <esp_wifi.h>
    #include "wifi_config.h"
    #include "freertos/event_groups.h"
    #include "lwip/sockets.h"
    #include "esp_wireguard.h"
    #include "lwip/inet.h"
    #include <errno.h>
    #include "esp_netif_ppp.h"
    #include "esp_modem_api.h"
    #include "driver/gpio.h"
    #include "lwip/netdb.h"


    #define CAN_TX_GPIO GPIO_NUM_21
    #define CAN_RX_GPIO GPIO_NUM_22
    #define CAN_BITRATE 1000000
    #define MAX_FRAMES_PER_PACKET 90
    #define RING_BUFFER_SIZE (3 * 1024 * 1024)
    #define RING_BUFFER_FALLBACK_SIZE (32 * 1024)
    #define WIFI_CONNECTION BIT0
    #define SERVER_IP "10.8.0.1"
    #define SERVER_PORT 5005
    #define LTE_CONNECTION BIT1
    #define MODEM_TX_GPIO GPIO_NUM_27
    #define MODEM_RX_GPIO GPIO_NUM_26
    #define MODEM_PWRKEY_GPIO GPIO_NUM_4
    #define MODEM_FLIGHT_GPIO GPIO_NUM_25
    #define MODEM_STATUS_GPIO GPIO_NUM_34


    typedef struct {
        uint64_t time_offset_ms;
        uint32_t id;
        uint8_t  dlc;
        uint8_t  data[8];
    } can_frame_t;

    static const char *TAG = "main";
    static twai_node_handle_t can_node;
    static uint32_t bootid;
    static can_frame_t canFrameBuffer[MAX_FRAMES_PER_PACKET];
    static size_t canFrameCount;
    static RingbufHandle_t packetRingBuffer;
    static volatile uint32_t dropped_packet;
    static EventGroupHandle_t wifi_events;
    static esp_netif_t *wifi_netif;
    static esp_netif_t *lte_netif;
    static esp_netif_t *volatile uplink_netif;
    static volatile bool time_synced = false;
    static volatile bool wifi_enabled = false;
    static bool wifi_setup_done = false;
    static bool lte_setup_done = false;
    static bool sntp_started = false;
    static esp_modem_dce_t *dce;
    static int udp_socket = -1;
    static struct sockaddr_in server_address;
    static wireguard_ctx_t wgCTX = {0};
    static wireguard_config_t cfg = ESP_WIREGUARD_CONFIG_DEFAULT();
    static uint32_t last_wifi_check_ms = 0;


    typedef struct __attribute__((packed)) {
        uint8_t header;
        uint32_t bootId;
        uint64_t baseTimeMs;
        uint32_t sequence;
        uint8_t  frameCount;
        uint8_t  littleEndian;
    } headPacket;

    typedef struct __attribute__((packed)) {
        uint16_t timeOffsetMs;
        uint32_t id;
        uint8_t  dlc;
        uint8_t  data[8];
    } wireFrame;


    static QueueHandle_t frame_queue;
    static headPacket packet;

    static void init_storage(void) {
        esp_err_t err = nvs_flash_init();
        if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_ERROR_CHECK(nvs_flash_erase());
            err = nvs_flash_init();
        }
        ESP_ERROR_CHECK(err);
    }

    static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data) {
        if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
            if (wifi_enabled) {
                esp_wifi_connect();
            }
        } else if ( base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
            xEventGroupClearBits(wifi_events, WIFI_CONNECTION);
            wifi_event_sta_disconnected_t *d = (wifi_event_sta_disconnected_t *)data;
            ESP_LOGW(TAG, "lost wifi connection %d",  d->reason);
            if (wifi_enabled) {
                esp_wifi_connect();
            }
        }else if ( base == IP_EVENT && id == IP_EVENT_STA_GOT_IP ) {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
            ESP_LOGI(TAG,"Aquired ip: " IPSTR, IP2STR(&event->ip_info.ip));
            xEventGroupSetBits( wifi_events, WIFI_CONNECTION);
        }
    }
    static bool init_WiFi(void) {
        if (!wifi_setup_done) {
            wifi_netif = esp_netif_create_default_wifi_sta();
            wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
            ESP_ERROR_CHECK(esp_wifi_init(&wcfg));
            ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

            ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_event, NULL));
            ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi_event, NULL));

            wifi_config_t usercfg = {0};
            strncpy((char *)usercfg.sta.ssid, WIFI_SSID, sizeof(usercfg.sta.ssid));
            strncpy((char *)usercfg.sta.password, WIFI_PASSWORD, sizeof(usercfg.sta.password));
            usercfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &usercfg));
            wifi_setup_done = true;
        }

        wifi_enabled = true;
        if (esp_wifi_start() != ESP_OK) {
            wifi_enabled = false;
            return false;
        }
        EventBits_t bits = xEventGroupWaitBits(wifi_events, WIFI_CONNECTION, pdFALSE, pdTRUE, pdMS_TO_TICKS(10000));
        if (bits & WIFI_CONNECTION) {
            return true;
        }
        ESP_LOGW(TAG, "wifi connection not found");
        wifi_enabled = false;
        esp_wifi_stop();
        return false;
    }

    static void on_ip_event( void *arg, esp_event_base_t base, int32_t id, void *data) {
        if (id == IP_EVENT_PPP_GOT_IP) {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
            ESP_LOGI(TAG, "LTE IP:" IPSTR, IP2STR(&event->ip_info.ip));
            xEventGroupSetBits(wifi_events,LTE_CONNECTION);
        }else if ( id == IP_EVENT_PPP_LOST_IP ) {
            ESP_LOGI(TAG, "LTE IP LOST:");
            xEventGroupClearBits(wifi_events,LTE_CONNECTION);
        }
    }
    static void on_ppp_changed( void *arg, esp_event_base_t base, int32_t id, void *data) {
        ESP_LOGI(TAG, "PPP status changed %d", (int) id);
    }

    static void modem_power_on(void) {
            gpio_reset_pin(MODEM_FLIGHT_GPIO);
            gpio_set_direction(MODEM_FLIGHT_GPIO, GPIO_MODE_OUTPUT);
            gpio_set_level(MODEM_FLIGHT_GPIO, 1);
            gpio_reset_pin(MODEM_PWRKEY_GPIO);
            gpio_set_direction(MODEM_PWRKEY_GPIO, GPIO_MODE_OUTPUT);
            gpio_set_level(MODEM_PWRKEY_GPIO, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(MODEM_PWRKEY_GPIO, 1);
            vTaskDelay(pdMS_TO_TICKS(1000));
            gpio_set_level(MODEM_PWRKEY_GPIO, 0);
            vTaskDelay(pdMS_TO_TICKS(5000));
        }

    static bool init_LTE(void) {
        if (!lte_setup_done) {
            modem_power_on();
            ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &on_ip_event, NULL));
            ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, NULL));
            esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();
            lte_netif = esp_netif_new(&netif_ppp_config);
            esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
            dte_config.uart_config.tx_io_num = MODEM_TX_GPIO;
            dte_config.uart_config.rx_io_num = MODEM_RX_GPIO;
            dte_config.uart_config.rts_io_num = UART_PIN_NO_CHANGE;
            dte_config.uart_config.cts_io_num = UART_PIN_NO_CHANGE;
            dte_config.uart_config.flow_control = ESP_MODEM_FLOW_CONTROL_NONE;
            esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(LTE_APN);
            dce = esp_modem_new_dev(ESP_MODEM_DCE_SIM7600, &dte_config, &dce_config, lte_netif);
            if (dce == NULL) {
                ESP_LOGE(TAG, "modem create failed");
                return false;
            }
            lte_setup_done = true;
        }

        if (esp_modem_set_mode(dce, ESP_MODEM_MODE_DATA) != ESP_OK) {
            ESP_LOGW(TAG, "modem did not enter data mode");
            return false;
        }
        EventBits_t bits = xEventGroupWaitBits(wifi_events, LTE_CONNECTION, pdFALSE, pdTRUE, pdMS_TO_TICKS(60000));
        if (bits & LTE_CONNECTION) {
            return true;
        }
        ESP_LOGW(TAG, "LTE connection not found");
        return false;
    }

    static void init_network_stack(void) {
        wifi_events = xEventGroupCreate();
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
    }

    static bool network_connect(void) {
        if (init_WiFi()) {
            uplink_netif = wifi_netif;
            ESP_LOGW(TAG, "WIFI is connected");
        } else if (init_LTE()) {
            uplink_netif = lte_netif;
            ESP_LOGW(TAG, "LTE is connected");
        } else {
            uplink_netif = NULL;
            ESP_LOGW(TAG, "Could not establish any internet connection, caching data");
            return false;
        }
        esp_netif_set_default_netif(uplink_netif);
        return true;
    }
    static void udp_init() {
        udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (udp_socket < 0) {
            ESP_LOGE(TAG, "socket create failed: errno %d", errno);
            return;
        }
        memset(&server_address, 0, sizeof(server_address));
        server_address.sin_family = AF_INET;
        server_address.sin_port = htons(SERVER_PORT);
        if (inet_pton(AF_INET, SERVER_IP, &server_address.sin_addr) != 1) {
            ESP_LOGE(TAG, "invalid server ip %s", SERVER_IP);
            return;
        }
        ESP_LOGI(TAG, "udp socket ready, sending to %s:%d", SERVER_IP, SERVER_PORT);
    }
    static uint64_t runtime(void) {
            return (uint64_t)(esp_timer_get_time()/1000);
    }
    static void wireguard_setup(void) {
        cfg.private_key = PRIVATE_KEY;
        cfg.public_key = PUBLIC_KEY;
        cfg.allowed_ip = ALLOWED_IP;
        cfg.allowed_ip_mask = ALLOWED_IP_MASK;
        cfg.endpoint = ENDPOINT;
        cfg.persistent_keepalive = PERSISTENT_KEEPALIVE;
        ESP_ERROR_CHECK(esp_wireguard_init(&cfg, &wgCTX));
    }

    static bool wireguard_try_connect(void) {
        struct addrinfo hints = { .ai_family = AF_INET };
        struct addrinfo *res = NULL;
        if (getaddrinfo(ENDPOINT, NULL, &hints, &res) != 0 || res == NULL) {
            ESP_LOGW(TAG, "cannot resolve %s yet", ENDPOINT);
            return false;
        }
        freeaddrinfo(res);
        if (esp_wireguard_connect(&wgCTX) != ESP_OK) {
            ESP_LOGW(TAG, "wireguard connect failed");
            return false;
        }
        ESP_LOGI(TAG, "wireguard started, waiting for handshake");
        return true;
    }

    static bool frame_builder(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata) {
        uint8_t rx_data[8] = {0};
        twai_frame_t rx_frame = {
            .buffer = rx_data,
            .buffer_len = sizeof(rx_data),
        };
        if (twai_node_receive_from_isr(handle, &rx_frame) != ESP_OK) {
            return false;
        }
        can_frame_t process_frame;
        process_frame.id = rx_frame.header.id;
        process_frame.dlc = rx_frame.header.dlc > 8 ? 8 : rx_frame.header.dlc;
        memcpy(process_frame.data, rx_data, sizeof(process_frame.data));
        process_frame.time_offset_ms = runtime();

        BaseType_t woke_loop = pdFALSE;
        if (xQueueSendFromISR(frame_queue, &process_frame, &woke_loop) != pdTRUE) {
            dropped_packet++;
        }
        return woke_loop == pdTRUE;
    }

    static bool on_can_rx(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx) {
        return frame_builder(handle, edata);

    }

    static void init_can() {
        twai_onchip_node_config_t tcfg = {.io_cfg.tx = CAN_TX_GPIO,.io_cfg.rx = CAN_RX_GPIO,.bit_timing.bitrate = CAN_BITRATE, .tx_queue_depth = 5};

        ESP_ERROR_CHECK(twai_new_node_onchip(&tcfg, &can_node));

        twai_event_callbacks_t cbs = {
            .on_rx_done = on_can_rx,
        };
        ESP_ERROR_CHECK(twai_node_register_event_callbacks(can_node, &cbs, NULL));

        ESP_ERROR_CHECK(twai_node_enable(can_node));

        ESP_LOGI(TAG, "CAN started at %d bit/s on TX=%d RX=%d", CAN_BITRATE, CAN_TX_GPIO, CAN_RX_GPIO);
    }

    static bool world_clock_init(void) {
        if (!sntp_started) {
            esp_sntp_config_t ecfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
            if (esp_netif_sntp_init(&ecfg) != ESP_OK) {
                return false;
            }
            setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
            tzset();
            sntp_started = true;
        }
        if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000)) != ESP_OK) {
            ESP_LOGW(TAG, "time not synced yet: world clock still at 1970");
            return false;
        }
        ESP_LOGI(TAG, "world clock synced");
        return true;
    }
    static uint64_t utc_ms(void) {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
    }
    static void init_headPacket() {
        packet.header = 0x2;
        packet.bootId = bootid;
        packet.baseTimeMs = utc_ms();
        packet.sequence = 0;
        packet.frameCount = 0;
        packet.littleEndian = 1;
    }

    static void sendData() {
        if (!time_synced) {
            return;
        }
        static uint8_t dataPacket[19+(15*MAX_FRAMES_PER_PACKET)]; // too big os i am keeping it off the stack mem, hence static
        uint64_t firstArrival = canFrameBuffer[0].time_offset_ms;
        packet.baseTimeMs = utc_ms() - (runtime() - firstArrival);
        packet.frameCount = (uint8_t)canFrameCount;
        memcpy(dataPacket, &packet, sizeof(packet));

        for (size_t i = 0; i < canFrameCount; i++) {
            wireFrame body;
            uint64_t offset = canFrameBuffer[i].time_offset_ms - firstArrival;
            body.timeOffsetMs = offset > 0xFFFF ? 0xFFFF : (uint16_t)offset;
            body.id = canFrameBuffer[i].id;
            body.dlc = canFrameBuffer[i].dlc;
            memcpy(body.data, canFrameBuffer[i].data, sizeof(body.data));
            memcpy(dataPacket + sizeof(packet) + i * sizeof(body), &body, sizeof(body));
        }
        size_t packetLength = sizeof(packet) + canFrameCount * sizeof(wireFrame);
         if (udp_socket >= 0 &&  uplink_netif && esp_netif_is_netif_up(uplink_netif)
         && wgCTX.netif != NULL
         && esp_wireguardif_peer_is_up(&wgCTX) == ESP_OK
         && sendto(udp_socket, dataPacket, packetLength, 0, (struct sockaddr *)&server_address, sizeof(server_address)) >= 0) {
            size_t size;
            uint8_t *item = xRingbufferReceive(packetRingBuffer, &size, 0);
            if (item) {
                if (sendto(udp_socket, item, size, 0 , (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
                    dropped_packet++;
                }
                vRingbufferReturnItem(packetRingBuffer, item);
            }
        }else {
            if (xRingbufferSend(packetRingBuffer, dataPacket, packetLength, 0) != pdTRUE) {
                dropped_packet++;
            }
        }
        packet.sequence++;
    }
    static bool wifi_status(void) {
        wifi_scan_config_t scan = {.ssid = (uint8_t*) WIFI_SSID, .show_hidden = false};
        if (esp_wifi_start() != ESP_OK || esp_wifi_scan_start(&scan,true) != ESP_OK) {
            return false;
        }
        uint16_t count = 1;
        wifi_ap_record_t ap;
        if (esp_wifi_scan_get_ap_records(&count, &ap) != ESP_OK || count == 0) {
            return false;
        }
        return ap.rssi > -75;
    }

    static void network_task(void *arg) {
        while (true) {
            if (uplink_netif == NULL || !esp_netif_is_netif_up(uplink_netif)) {
                uplink_netif = NULL;
                network_connect();
            }
            if (uplink_netif == lte_netif && runtime() - last_wifi_check_ms > 60000) {
                last_wifi_check_ms = runtime();
                if (init_WiFi()&& wifi_status()) {
                    uplink_netif = wifi_netif;
                    esp_netif_set_default_netif(wifi_netif);
                }
            }
            if (uplink_netif != NULL && !time_synced) {
                time_synced = world_clock_init();
            }
            if (uplink_netif != NULL && time_synced && wgCTX.netif == NULL) {
                wireguard_try_connect();
            }
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }

    void app_main(void) {
        bootid = esp_random();
        init_storage();
        init_network_stack();
        wireguard_setup();
        init_headPacket();
        frame_queue = xQueueCreate(256, sizeof(can_frame_t));
        canFrameCount = 0;
        packetRingBuffer = xRingbufferCreateWithCaps(RING_BUFFER_SIZE, RINGBUF_TYPE_NOSPLIT, MALLOC_CAP_SPIRAM);
        if (packetRingBuffer == NULL) {
            packetRingBuffer = xRingbufferCreate(RING_BUFFER_FALLBACK_SIZE, RINGBUF_TYPE_NOSPLIT);
        }
        ESP_LOGI(TAG, "ring buffer %s", packetRingBuffer ? "created" : "FAILED");
        udp_init();
        init_can();
        xTaskCreate(network_task, "network", 6144, NULL, tskIDLE_PRIORITY + 1, NULL);
        uint64_t next_send = 0;
        ESP_LOGI(TAG, "init done, queue %s", frame_queue ? "created" : "FAILED");
        next_send = runtime()+20;

        while (true) {
            uint64_t now = runtime();
            TickType_t wait = (now >= next_send) ? 0 : pdMS_TO_TICKS(next_send - now);
            can_frame_t frame;
            if (xQueueReceive(frame_queue, &frame, wait) == pdTRUE) {
                if (canFrameCount == MAX_FRAMES_PER_PACKET) {
                    sendData();
                    canFrameCount = 0;
                }
                canFrameBuffer[canFrameCount++] = frame;
            }

            if (runtime() >= next_send) {
                if (canFrameCount > 0) {
                    sendData();
                    canFrameCount = 0;
                }
                next_send += 20;
            }
        }
    }