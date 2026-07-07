#ifndef STATION_MANAGER_H
#define STATION_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

/*
 * Station Manager
 */

/**
 * Inizializza l'interfaccia STA.
 */
esp_err_t station_manager_init(void);

/**
 * Connette l'ESP32 ad un Access Point.
 */
esp_err_t station_manager_connect(const char *ssid,
                                  const char *password);

/**
 * Disconnette la Station.
 */
esp_err_t station_manager_disconnect(void);

/**
 * Restituisce true se la Station è connessa.
 */
bool station_manager_is_connected(void);

bool station_manager_is_connecting(void);

/**
 * Restituisce l'SSID corrente.
 */
const char *station_manager_get_ssid(void);

const char *station_manager_get_ip(void);

const char *station_manager_get_status(void);

#endif
