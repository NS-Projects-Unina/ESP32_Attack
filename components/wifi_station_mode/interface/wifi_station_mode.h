/**
 * @file wifi_station_mode.h
 * @author Luigi Tagliaferri (luigitagliaferri792@gmail.com)
 * @date 2026-06-16
 * @copyright Copyright (c) 
 * 
 * @brief Provides interface for Wi-Fi Station mode and AP mode
 * 
 * This component make the board to work like a Wi-Fi Station and create an AP in orther to creare a "freeWiFi" network
 */

 #include <stdint.h>
 #include <unistd.h>
 #include <string.h>

 #include "lwip/lwip_napt.h"
 #include "esp_wifi_types.h"

/**
 * @brief config STA-AP mode
 * 
 * 
 */
void config_sta_ap_mode();

/**
 * @brief Station mode configuration
 * 
 * @param SSDI of the network
 * @param PWD of the network
 */
void station_mode(const char *SSID, const char *PWD);

/**
 * @brief AP mode configuration
 * 
 * 
 */
void ap_mode();

/**
 * @brief ESP32 wifi initialization
 * 
 * 
 */
void wifi_init();

/**
 * @brief NAPT enable
 * 
 * 
 */
void enable_napt();