/**
 * @file wifi_station_mode.c
 * @author Luigi Tagliaferri (luigitagliaferri792@gmail.com)
 * @date 2026-06-16
 * @copyright Copyright (c) 
 * 
 * @brief Implementation of Wi-Fi STA-AP mode
 */

 #include "wifi_station_mode.h"

 /**
 * @brief config STA-AP mode
 * 
 * 
 */
void config_sta_ap_mode(){
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(
        esp_wifi_set_mode(WIFI_MODE_APSTA)
    );
}

/**
 * @brief Station mode configuration
 * 
 * @param SSDI and PWD of the network
 */
void station_mode(char *SSID, char *PWD){
    wifi_config_t sta_cfg = {
        .sta = {
            .ssid = SSID,
            .password = PWD,
        }
    };

    ESP_ERROR_CHECK(
        esp_wifi_set_config(WIFI_IF_STA, &sta_cfg)
    );
}

/**
 * @brief AP mode configuration
 * 
 * 
 */
void ap_mode(){
    wifi_config_t ap_cfg = {
        .ap = {
            .ssid = "OpenWifi",
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
            .channel = 1
        }
    };

    ESP_ERROR_CHECK(
        esp_wifi_set_config(WIFI_IF_AP, &ap_cfg)
    );
}

/**
 * @brief ESP32 wifi initialization
 * 
 * 
 */
void wifi_init(){
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
}

/**
 * @brief NAPT enable
 * 
 * 
 */
void enable_napt(){
    ip_napt_enable(esp_ip4addr_aton("192.168.4.1"), 1);  
}