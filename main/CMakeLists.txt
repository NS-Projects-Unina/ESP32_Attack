idf_component_register(
    SRCS
        "main.c"
        "wifi_manager.c"
        "webserver.c"
        "filesystem.c"
        "api.c"
        "scanner.c"
        "station_manager.c"
        "capture.c"
        "wifi_info.c"

    INCLUDE_DIRS
        "."

    REQUIRES
        esp_http_server
        esp_wifi
        esp_netif
        esp_timer
        nvs_flash
        spiffs
        json
)
