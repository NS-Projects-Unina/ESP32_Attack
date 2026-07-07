#include "nvs_flash.h"

#include "wifi_manager.h"
#include "station_manager.h"
#include "scanner.h"
#include "capture.h"
#include "filesystem.h"
#include "webserver.h"

#include "esp_log.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, " ESP32 Wireless Analyzer");
    ESP_LOGI(TAG, "========================================");

    /*
     * WiFi
     */

    wifi_manager_init();

    ESP_ERROR_CHECK(
        station_manager_init());

    /*
     * Scanner
     */

    ESP_ERROR_CHECK(
        scanner_init());

    /*
     * Packet Capture
     */

    ESP_ERROR_CHECK(
        capture_init());

    /*
     * SPIFFS
     */

    ESP_ERROR_CHECK(
        filesystem_init());

    /*
     * HTTP Server
     */

    ESP_ERROR_CHECK(
        webserver_start());

    ESP_LOGI(TAG, "System ready");

    /*
     * Main loop
     */

    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
