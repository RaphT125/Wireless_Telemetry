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
    #include "lwip/inet.h"
    #include <errno.h>


    #define CAN_TX_GPIO GPIO_NUM_21
    #define CAN_RX_GPIO GPIO_NUM_22
    #define CAN_BITRATE 1000000
    #define MAX_FRAMES_PER_PACKET 90
    #define RING_BUFFER_SIZE (4 * 1024 * 1024)
    #define RING_BUFFER_FALLBACK_SIZE (32 * 1024)
    #define WIFI_CONNECTION BIT0
    #define SERVER_IP "10.0.0.27"
    #define SERVER_PORT 5005


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
    static int udp_socket = -1;
    static struct sockaddr_in server_address;


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
            esp_wifi_connect();
        } else if ( base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
            xEventGroupClearBits(wifi_events, WIFI_CONNECTION);
            wifi_event_sta_disconnected_t *d = (wifi_event_sta_disconnected_t *)data;
            ESP_LOGW(TAG, "lost wifi connection %d",  d->reason);
            esp_wifi_connect();
        }else if ( base == IP_EVENT && id == IP_EVENT_STA_GOT_IP ) {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
            ESP_LOGI(TAG,"Aquired ip: " IPSTR, IP2STR(&event->ip_info.ip));
            xEventGroupSetBits( wifi_events, WIFI_CONNECTION);
        }
    }

    static void init_network_stack(void) {
        wifi_events = xEventGroupCreate();
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        wifi_netif = esp_netif_create_default_wifi_sta();
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_event, NULL));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi_event, NULL));

        wifi_config_t usercfg = {0};
        strncpy((char *)usercfg.sta.ssid, WIFI_SSID, sizeof(usercfg.sta.ssid));
        strncpy((char *)usercfg.sta.password, WIFI_PASSWORD, sizeof(usercfg.sta.password));
        usercfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &usercfg));
        ESP_ERROR_CHECK(esp_wifi_start());
        xEventGroupWaitBits(wifi_events, WIFI_CONNECTION,pdFALSE, pdTRUE,portMAX_DELAY);
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
        twai_onchip_node_config_t cfg = {.io_cfg.tx = CAN_TX_GPIO,.io_cfg.rx = CAN_RX_GPIO,.bit_timing.bitrate = CAN_BITRATE, .tx_queue_depth = 5};

        ESP_ERROR_CHECK(twai_new_node_onchip(&cfg, &can_node));

        twai_event_callbacks_t cbs = {
            .on_rx_done = on_can_rx,
        };
        ESP_ERROR_CHECK(twai_node_register_event_callbacks(can_node, &cbs, NULL));

        ESP_ERROR_CHECK(twai_node_enable(can_node));

        ESP_LOGI(TAG, "CAN started at %d bit/s on TX=%d RX=%d", CAN_BITRATE, CAN_TX_GPIO, CAN_RX_GPIO);
    }

    static void world_clock_init(void) {
        esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
        ESP_ERROR_CHECK(esp_netif_sntp_init(&cfg));

        if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000)) != ESP_OK) {
            ESP_LOGW(TAG, "time not synced yet: world clock still at 1970");
        }

        setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
        tzset();
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
        (void)packetLength;
        if (udp_socket >=0 && esp_netif_is_netif_up(wifi_netif) && sendto(udp_socket, dataPacket, packetLength, 0 , (struct sockaddr  *)&server_address, sizeof(server_address)) >=0) {
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



    // ---- MOCK CAN (delete this block + the xTaskCreate line in app_main) ----
    static void mock_can_task(void *arg) {
        uint32_t tick = 0;
        while (true) {
            can_frame_t f = {0};
            f.dlc = 8;
            f.time_offset_ms = runtime();
            if (tick % 5 == 0) {
                uint16_t rpm = 3000 + (tick % 600) * 10;
                f.id = 0x1;
                f.data[0] = rpm >> 8;
                f.data[1] = rpm & 0xFF;
                xQueueSend(frame_queue, &f, 0);
            }
            int32_t lon = -1231200000 + (int32_t)(tick % 1000) * 100;
            int32_t lat = 492600000 + (int32_t)(tick % 1000) * 100;
            memset(f.data, 0, sizeof(f.data));
            f.id = 0x3;
            for (int i = 0; i < 4; i++) {
                f.data[i] = (uint8_t)(lon >> (24 - 8 * i));
                f.data[4 + i] = (uint8_t)(lat >> (24 - 8 * i));
            }
            xQueueSend(frame_queue, &f, 0);
            tick++;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    // ---- END MOCK CAN ----

    void app_main(void) {
        bootid = esp_random();
        init_storage();
        init_network_stack();
        world_clock_init();
        udp_init();
        init_headPacket();
        frame_queue = xQueueCreate(256, sizeof(can_frame_t));
        canFrameCount = 0;
        packetRingBuffer = xRingbufferCreateWithCaps(RING_BUFFER_SIZE, RINGBUF_TYPE_NOSPLIT, MALLOC_CAP_SPIRAM);
        if (packetRingBuffer == NULL) {
            packetRingBuffer = xRingbufferCreate(RING_BUFFER_FALLBACK_SIZE, RINGBUF_TYPE_NOSPLIT);
        }
        ESP_LOGI(TAG, "ring buffer %s", packetRingBuffer ? "created" : "FAILED");
        init_can();
        xTaskCreate(mock_can_task, "mock_can", 3072, NULL, 5, NULL); // MOCK CAN: delete this line
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