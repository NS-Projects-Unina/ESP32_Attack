#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "esp_err.h"

#include <stdbool.h>

/*
 * HTTP Server
 */

/**
 * Avvia il web server.
 */
esp_err_t webserver_start(void);

/**
 * Arresta il web server.
 */
esp_err_t webserver_stop(void);

/**
 * Riavvia il web server.
 */
esp_err_t webserver_restart(void);

/**
 * Restituisce true se il server è attivo.
 */
bool webserver_is_running(void);

#endif