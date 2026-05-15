idf_component_register(
    SRCS "main.c" "wifi_manager.c" "moonraker_client.c" "display_manager.c"
    INCLUDE_DIRS "."
    REQUIRES esp_event esp_netif esp_wifi nvs_flash esp_http_client esp_driver_i2c
)
