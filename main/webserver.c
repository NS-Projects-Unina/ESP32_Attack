#include "webserver.h"

#include "api.h"
#include "app_config.h"
#include "filesystem.h"

#include "esp_http_server.h"
#include "esp_log.h"

static const char *TAG = "WEBSERVER";

static httpd_handle_t server = NULL;

/*
 * Static Files
 */

static esp_err_t root_handler(httpd_req_t *req)
{
    return filesystem_send_file(req,
                                "/spiffs/index.html",
                                "text/html");
}

static esp_err_t css_handler(httpd_req_t *req)
{
    return filesystem_send_file(req,
                                "/spiffs/style.css",
                                "text/css");
}

static esp_err_t js_handler(httpd_req_t *req)
{
    return filesystem_send_file(req,
                                "/spiffs/app.js",
                                "application/javascript");
}

static esp_err_t favicon_handler(httpd_req_t *req)
{
    return filesystem_send_file(req,
                                "/spiffs/favicon.ico",
                                "image/x-icon");
}

/*
 * HTTP Server
 */

esp_err_t webserver_start(void)
{
    if (server != NULL)
    {
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = HTTP_SERVER_PORT;
    config.stack_size = 10240;
    config.max_uri_handlers = HTTP_MAX_URI_HANDLERS;
    ESP_LOGI(TAG, "Starting HTTP server...");

    if (httpd_start(&server, &config) != ESP_OK)
    {
        ESP_LOGE(TAG, "Cannot start HTTP server");
        return ESP_FAIL;
    }

    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
        .user_ctx = NULL
    };

    httpd_uri_t css = {
        .uri = "/style.css",
        .method = HTTP_GET,
        .handler = css_handler,
        .user_ctx = NULL
    };

    httpd_uri_t js = {
        .uri = "/app.js",
        .method = HTTP_GET,
        .handler = js_handler,
        .user_ctx = NULL
    };

    httpd_uri_t favicon = {
        .uri = "/favicon.ico",
        .method = HTTP_GET,
        .handler = favicon_handler,
        .user_ctx = NULL
    };

    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &root));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &css));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &js));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &favicon));
    ESP_ERROR_CHECK(api_register(server));

    ESP_LOGI(TAG, "HTTP server started");

    return ESP_OK;
}

/*
 * Stop Server
 */

esp_err_t webserver_stop(void)
{
    if (server == NULL)
    {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping HTTP server...");
    ESP_ERROR_CHECK(httpd_stop(server));
    server = NULL;

    return ESP_OK;
}

/*
 * Restart Server
 */

esp_err_t webserver_restart(void)
{
    ESP_ERROR_CHECK(webserver_stop());
    return webserver_start();
}

/*
 * Status
 */

bool webserver_is_running(void)
{
    return (server != NULL);
}
