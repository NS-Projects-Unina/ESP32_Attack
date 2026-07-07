#include "scanner.h"

#include <string.h>

#include "esp_log.h"
#include "esp_wifi.h"

static const char *TAG = "SCANNER";

/*
 * Internal data
 */

static wifi_scan_result_t results[MAX_SCAN_RESULTS];
static wifi_ap_record_t ap_records[MAX_SCAN_RESULTS];
static uint16_t result_count = 0;
static bool scan_busy = false;
static bool scan_ready = false;
static uint32_t scan_counter = 0;
static uint32_t last_scan_tick = 0;

/*
 * Initialization
 */

esp_err_t scanner_init(void)
{
    memset(results, 0, sizeof(results));
    memset(ap_records, 0, sizeof(ap_records));
    result_count = 0;
    scan_busy = false;
    scan_ready = false;
    scan_counter = 0;
    last_scan_tick = 0;
    ESP_LOGI(TAG, "Scanner initialized");
    return ESP_OK;
}

/*
 * Start Scan
 */

esp_err_t scanner_start(void)
{
    if (scan_busy)
    {
        ESP_LOGW(TAG, "Scan already running");
        return ESP_ERR_INVALID_STATE;
    }

    wifi_scan_config_t config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true

    };

    result_count = 0;
    scan_ready = false;
    scan_busy = true;
    ESP_LOGI(TAG, "Starting WiFi scan...");
    esp_err_t err = esp_wifi_scan_start(&config, false);

    if (err != ESP_OK)
    {
        scan_busy = false;
        ESP_LOGE(TAG,
                 "Cannot start scan (%s)",
                 esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

/*
 * Scan Completed
 */

void scanner_scan_done(void)
{
    uint16_t ap_count = MAX_SCAN_RESULTS;
    esp_err_t err =
        esp_wifi_scan_get_ap_records(
            &ap_count,
            ap_records);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG,
                 "Cannot read AP records (%s)",
                 esp_err_to_name(err));
        scan_busy = false;
        scan_ready = false;
        return;
    }

    result_count = ap_count;
    memset(results, 0, sizeof(results));

    for (uint16_t i = 0; i < result_count; i++)
    {
        strncpy(results[i].ssid,
                (const char *)ap_records[i].ssid,
                sizeof(results[i].ssid) - 1);
        results[i].ssid[32] = '\0';
        results[i].rssi = ap_records[i].rssi;
        results[i].channel = ap_records[i].primary;
        results[i].authmode = ap_records[i].authmode;
        ESP_LOGI(TAG,
                 "%2u | %-32s | RSSI=%4d | CH=%2u | %s",
                 i + 1,
                 results[i].ssid,
                 results[i].rssi,
                 results[i].channel,
                 scanner_auth_to_string(results[i].authmode));
    }

    scan_counter++;
    last_scan_tick = xTaskGetTickCount();
    scan_busy = false;
    scan_ready = true;
    ESP_LOGI(TAG,
             "Scan completed (%u APs)",
             result_count);
}

/*
 * Results
 */

uint16_t scanner_get_count(void)
{
    return result_count;
}

const wifi_scan_result_t *scanner_get_result(uint16_t index)
{
    if (index >= result_count)
    {
        return NULL;
    }

    return &results[index];
}

/*
 * Status
 */

bool scanner_is_busy(void)
{
    return scan_busy;
}

bool scanner_is_ready(void)
{
    return scan_ready;
}

uint32_t scanner_get_scan_counter(void)
{
    return scan_counter;
}

uint32_t scanner_get_last_scan_time(void)
{
    return last_scan_tick;
}

/*
 * Helpers
 */

const char *scanner_auth_to_string(wifi_auth_mode_t auth)
{
    switch (auth)
    {
        case WIFI_AUTH_OPEN:
            return "OPEN";

        case WIFI_AUTH_WEP:
            return "WEP";

        case WIFI_AUTH_WPA_PSK:
            return "WPA";

        case WIFI_AUTH_WPA2_PSK:
            return "WPA2";

        case WIFI_AUTH_WPA_WPA2_PSK:
            return "WPA/WPA2";

        case WIFI_AUTH_WPA3_PSK:
            return "WPA3";

#ifdef WIFI_AUTH_WPA2_WPA3_PSK
        case WIFI_AUTH_WPA2_WPA3_PSK:
            return "WPA2/WPA3";
#endif

        default:
            return "UNKNOWN";
    }
}