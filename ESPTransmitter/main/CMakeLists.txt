idf_component_register(SRCS "esp_transmitter_main.c"
                       PRIV_REQUIRES nvs_flash esp_netif esp_event esp_driver_twai esp_timer lwip esp_hw_support esp_ringbuf esp_wifi esp_wireguard
                       INCLUDE_DIRS "")
