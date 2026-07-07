#ifndef SCANNER_H
#define SCANNER_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_wifi.h"

#define MAX_SCAN_RESULTS 50

typedef struct
{
    char ssid[33];
    int rssi;
    uint8_t channel;
    wifi_auth_mode_t authmode;
} wifi_scan_result_t;

/*
 * Initialization
 */

esp_err_t scanner_init(void);

/*
 * Scan control
 */

/*
 * Avvia una scansione WiFi.
 * Ritorna immediatamente.
 */
esp_err_t scanner_start(void);

/*
 * Callback richiamata quando lo scan è terminato.
 * Viene chiamata dal WiFi Event Handler.
 */
void scanner_scan_done(void);

/*
 * True se è in corso una scansione.
 */
bool scanner_is_busy(void);

/*
 * True se sono disponibili risultati validi.
 */
bool scanner_is_ready(void);

/*
 * Results
 */

uint16_t scanner_get_count(void);

const wifi_scan_result_t *scanner_get_result(uint16_t index);

/*
 * Statistics
 */

uint32_t scanner_get_scan_counter(void);

uint32_t scanner_get_last_scan_time(void);

/*
 * Helpers
 */

const char *scanner_auth_to_string(wifi_auth_mode_t auth);

#endif