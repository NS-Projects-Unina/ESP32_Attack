#ifndef API_H
#define API_H

#include "esp_err.h"
#include "esp_http_server.h"

/*
 * REST API
 */

/*
 * Registra tutte le API REST sul webserver.
 */
esp_err_t api_register(httpd_handle_t server);

/*
 * API Endpoints
 */

/*
 * GET /api/status
 */
esp_err_t api_status_handler(httpd_req_t *req);

/*
 * GET /api/scan
 */
esp_err_t api_scan_handler(httpd_req_t *req);

/*
 * GET /api/station
 */
esp_err_t api_station_handler(httpd_req_t *req);

/*
 * POST /api/connect
 */
esp_err_t api_connect_handler(httpd_req_t *req);

/*
 * POST /api/disconnect
 */
esp_err_t api_disconnect_handler(httpd_req_t *req);

/*
 * GET /api/system
 */
esp_err_t api_system_handler(httpd_req_t *req);

/*
 * GET /api/info
 */
esp_err_t api_info_handler(httpd_req_t *req);

#endif
