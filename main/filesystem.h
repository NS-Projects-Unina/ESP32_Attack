#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "esp_err.h"
#include "esp_http_server.h"

/*
 * Filesystem
 */

/**
 * Inizializza SPIFFS.
 */
esp_err_t filesystem_init(void);

/**
 * Smonta SPIFFS.
 */
esp_err_t filesystem_deinit(void);

/**
 * Invia un file al browser.
 */
esp_err_t filesystem_send_file(httpd_req_t *req,
                               const char *path,
                               const char *content_type);

/*
 * Helpers
 */

/*
 * Verifica se un file esiste.
 */
bool filesystem_exists(const char *path);

/**
 * Restituisce la dimensione di un file.
 */
size_t filesystem_file_size(const char *path);

#endif