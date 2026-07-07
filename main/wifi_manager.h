#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"

#include <stdbool.h>
#include <stdint.h>

/*
 * WiFi Manager
 */

typedef enum
{
    WIFI_MANAGER_AP_MANAGEMENT = 0,
    WIFI_MANAGER_AP_FREEWIFI
} wifi_manager_ap_mode_t;

/**
 * Inizializza WiFi AP + STA.
 */
void wifi_manager_init(void);

/**
 * Restituisce il numero di client collegati all'Access Point.
 */
uint16_t wifi_manager_get_client_count(void);

/**
 * Restituisce l'SSID dell'Access Point.
 */
const char *wifi_manager_get_ap_ssid(void);

/**
 * Restituisce l'indirizzo IP dell'Access Point.
 */
const char *wifi_manager_get_ap_ip(void);

/**
 * Restituisce la modalita' corrente del SoftAP.
 */
wifi_manager_ap_mode_t wifi_manager_get_ap_mode(void);

/**
 * Restituisce la modalita' corrente come stringa.
 */
const char *wifi_manager_get_ap_mode_name(void);

/**
 * Passa il SoftAP alla rete freeWiFi.
 */
esp_err_t wifi_manager_start_freewifi_ap(void);

/**
 * Ripristina il SoftAP di gestione.
 */
esp_err_t wifi_manager_stop_freewifi_ap(void);

/**
 * Copia il MAC del SoftAP nel buffer passato.
 */
esp_err_t wifi_manager_get_ap_mac(uint8_t mac[6]);

/**
 * Abilita il NAT tra client AP e uplink STA.
 */
esp_err_t wifi_manager_enable_nat_routing(void);

/**
 * Disabilita il NAT del SoftAP.
 */
esp_err_t wifi_manager_disable_nat_routing(void);

/**
 * Restituisce true se il NAT sul SoftAP e' attivo.
 */
bool wifi_manager_is_nat_routing_enabled(void);

#endif
