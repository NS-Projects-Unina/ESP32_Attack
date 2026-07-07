#include "api.h"

#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_check.h"
#include "esp_http_server.h"
#include "esp_wifi.h"

#include "capture.h"
#include "scanner.h"
#include "station_manager.h"
#include "wifi_manager.h"

#define API_VERSION "5.0"
#define API_POST_BUFFER_SIZE 192
#define PCAP_LINKTYPE_IEEE802_11 105

static const char *TAG = "API";

static void json_append_escaped(char *buffer,
                                size_t buffer_size,
                                int *len,
                                const char *value)
{
    if (value == NULL)
    {
        value = "";
    }

    for (const char *p = value;
         *p != '\0' && *len < (int)buffer_size - 1;
         p++)
    {
        if (*p == '"' || *p == '\\')
        {
            *len += snprintf(buffer + *len,
                             buffer_size - *len,
                             "\\%c",
                             *p);
        }
        else if ((unsigned char)*p >= 0x20)
        {
            buffer[(*len)++] = *p;
            buffer[*len] = '\0';
        }
    }
}

static esp_err_t api_send_json(httpd_req_t *req,
                               const char *json)
{
    httpd_resp_set_type(req, "application/json");

    return httpd_resp_send(req,
                           json,
                           HTTPD_RESP_USE_STRLEN);
}

static esp_err_t api_send_error(httpd_req_t *req,
                                int status,
                                const char *message)
{
    char json[128];

    snprintf(json,
             sizeof(json),
             "{\"status\":\"error\",\"message\":\"%s\"}",
             message);

    httpd_resp_set_status(req,
                          status == 400 ? "400 Bad Request" : "500 Internal Server Error");

    return api_send_json(req, json);
}

static esp_err_t api_read_body(httpd_req_t *req,
                               char *body,
                               size_t body_size)
{
    if (req->content_len == 0 || req->content_len >= body_size)
    {
        return ESP_ERR_INVALID_SIZE;
    }

    int received = httpd_req_recv(req,
                                  body,
                                  req->content_len);

    if (received <= 0)
    {
        return ESP_FAIL;
    }

    body[received] = '\0';

    return ESP_OK;
}

static void format_mac(char *buffer,
                       size_t buffer_size,
                       const uint8_t *mac)
{
    snprintf(buffer,
             buffer_size,
             "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0],
             mac[1],
             mac[2],
             mac[3],
             mac[4],
             mac[5]);
}

esp_err_t api_status_handler(httpd_req_t *req)
{
    char json[512];
    capture_stats_t capture_stats;

    capture_get_stats(&capture_stats);

    int len =
        snprintf(json,
                 sizeof(json),
                 "{"
                 "\"status\":\"ok\","
                 "\"version\":\"%s\","
                 "\"scanner_busy\":%s,"
                 "\"scanner_ready\":%s,"
                 "\"scan_counter\":%lu,"
                 "\"capture_running\":%s,"
                 "\"capture_mode\":\"%s\","
                 "\"ap_ssid\":\"%s\","
                 "\"ap_mode\":\"%s\","
                 "\"ap_ip\":\"%s\","
                 "\"ap_clients\":%u,"
                 "\"nat_routing\":%s,"
                 "\"station_status\":\"%s\","
                 "\"connected\":%s,"
                 "\"connecting\":%s,"
                 "\"connected_ssid\":\"",
                 API_VERSION,
                 scanner_is_busy() ? "true" : "false",
                 scanner_is_ready() ? "true" : "false",
                 (unsigned long)scanner_get_scan_counter(),
                 capture_stats.running ? "true" : "false",
                 capture_mode_to_string(capture_stats.mode),
                 wifi_manager_get_ap_ssid(),
                 wifi_manager_get_ap_mode_name(),
                 wifi_manager_get_ap_ip(),
                 wifi_manager_get_client_count(),
                 wifi_manager_is_nat_routing_enabled() ? "true" : "false",
                 station_manager_get_status(),
                 station_manager_is_connected() ? "true" : "false",
                 station_manager_is_connecting() ? "true" : "false");

    json_append_escaped(json,
                        sizeof(json),
                        &len,
                        station_manager_get_ssid());

    len += snprintf(json + len,
                    sizeof(json) - len,
                    "\",\"station_ip\":\"");

    json_append_escaped(json,
                        sizeof(json),
                        &len,
                        station_manager_get_ip());

    snprintf(json + len,
             sizeof(json) - len,
             "\"}");

    return api_send_json(req, json);
}

esp_err_t api_scan_handler(httpd_req_t *req)
{
    if (scanner_is_busy())
    {
        return api_send_json(req,
                             "{\"status\":\"busy\"}");
    }

    if (capture_is_running())
    {
        ESP_RETURN_ON_ERROR(capture_stop(), TAG, "stop capture before scan");
    }

    esp_err_t err = scanner_start();

    if (err != ESP_OK)
    {
        return api_send_error(req,
                              500,
                              esp_err_to_name(err));
    }

    return api_send_json(req,
                         "{\"status\":\"started\"}");
}

static esp_err_t api_results_handler(httpd_req_t *req)
{
    char json[4096];
    int len = 0;

    len += snprintf(json + len,
                    sizeof(json) - len,
                    "[");

    uint16_t count = scanner_get_count();

    for (uint16_t i = 0; i < count; i++)
    {
        const wifi_scan_result_t *ap =
            scanner_get_result(i);

        if (ap == NULL)
        {
            continue;
        }

        len += snprintf(json + len,
                        sizeof(json) - len,
                        "%s{\"ssid\":\"",
                        (i == 0) ? "" : ",");

        json_append_escaped(json,
                            sizeof(json),
                            &len,
                            ap->ssid);

        len += snprintf(json + len,
                        sizeof(json) - len,
                        "\",\"rssi\":%d,\"channel\":%u,\"auth\":\"%s\"}",
                        ap->rssi,
                        ap->channel,
                        scanner_auth_to_string(ap->authmode));
    }

    snprintf(json + len,
             sizeof(json) - len,
             "]");

    return api_send_json(req, json);
}

esp_err_t api_station_handler(httpd_req_t *req)
{
    char json[256];
    int len =
        snprintf(json,
                 sizeof(json),
                 "{"
                 "\"status\":\"%s\","
                 "\"connected\":%s,"
                 "\"connecting\":%s,"
                 "\"ssid\":\"",
                 station_manager_get_status(),
                 station_manager_is_connected() ? "true" : "false",
                 station_manager_is_connecting() ? "true" : "false");

    json_append_escaped(json,
                        sizeof(json),
                        &len,
                        station_manager_get_ssid());

    len += snprintf(json + len,
                    sizeof(json) - len,
                    "\",\"ip\":\"");

    json_append_escaped(json,
                        sizeof(json),
                        &len,
                        station_manager_get_ip());

    snprintf(json + len,
             sizeof(json) - len,
             "\"}");

    return api_send_json(req, json);
}

esp_err_t api_connect_handler(httpd_req_t *req)
{
    char body[API_POST_BUFFER_SIZE];

    if (api_read_body(req, body, sizeof(body)) != ESP_OK)
    {
        return api_send_error(req,
                              400,
                              "invalid request body");
    }

    cJSON *root = cJSON_Parse(body);

    if (root == NULL)
    {
        return api_send_error(req,
                              400,
                              "invalid json");
    }

    const cJSON *ssid = cJSON_GetObjectItemCaseSensitive(root, "ssid");
    const cJSON *password = cJSON_GetObjectItemCaseSensitive(root, "password");

    if (!cJSON_IsString(ssid) || ssid->valuestring[0] == '\0')
    {
        cJSON_Delete(root);

        return api_send_error(req,
                              400,
                              "ssid required");
    }

    const char *password_value =
        cJSON_IsString(password) ? password->valuestring : "";

    esp_err_t err =
        station_manager_connect(ssid->valuestring,
                                password_value);

    cJSON_Delete(root);

    if (err != ESP_OK)
    {
        return api_send_error(req,
                              500,
                              esp_err_to_name(err));
    }

    return api_send_json(req,
                         "{\"status\":\"connecting\"}");
}

esp_err_t api_disconnect_handler(httpd_req_t *req)
{
    esp_err_t err = station_manager_disconnect();

    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_CONNECT)
    {
        return api_send_error(req,
                              500,
                              esp_err_to_name(err));
    }

    return api_send_json(req,
                         "{\"status\":\"disconnected\"}");
}

static esp_err_t api_capture_handler(httpd_req_t *req)
{
    capture_stats_t stats;
    char json[1024];
    char receiver[18];
    char sender[18];
    char bssid[18];
    char ap_mac[18];

    capture_get_stats(&stats);
    format_mac(receiver, sizeof(receiver), stats.last_deauth_receiver);
    format_mac(sender, sizeof(sender), stats.last_deauth_sender);
    format_mac(bssid, sizeof(bssid), stats.last_deauth_bssid);
    format_mac(ap_mac, sizeof(ap_mac), stats.ap_mac);

    snprintf(json,
             sizeof(json),
             "{"
             "\"running\":%s,"
             "\"mode\":\"%s\","
             "\"channel\":%u,"
             "\"total\":%lu,"
             "\"observed\":%lu,"
             "\"stored\":%lu,"
             "\"dropped\":%lu,"
             "\"pcap_bytes\":%lu,"
             "\"management\":%lu,"
             "\"control\":%lu,"
             "\"data\":%lu,"
             "\"ap_traffic\":%lu,"
             "\"ap_uplink\":%lu,"
             "\"ap_downlink\":%lu,"
             "\"ap_traffic_bytes\":%lu,"
             "\"ap_mac\":\"%s\","
             "\"deauth\":%lu,"
             "\"disassoc\":%lu,"
             "\"last_deauth_rssi\":%d,"
             "\"last_deauth_ms\":%lu,"
             "\"last_deauth_receiver\":\"%s\","
             "\"last_deauth_sender\":\"%s\","
             "\"last_deauth_bssid\":\"%s\","
             "\"last_rssi\":%d,"
             "\"last_packet_ms\":%lu"
             "}",
             stats.running ? "true" : "false",
             capture_mode_to_string(stats.mode),
             stats.channel,
             (unsigned long)stats.total_packets,
             (unsigned long)stats.observed_packets,
             (unsigned long)stats.stored_packets,
             (unsigned long)stats.dropped_packets,
             (unsigned long)stats.pcap_bytes,
             (unsigned long)stats.management_packets,
             (unsigned long)stats.control_packets,
             (unsigned long)stats.data_packets,
             (unsigned long)stats.ap_traffic_packets,
             (unsigned long)stats.ap_uplink_packets,
             (unsigned long)stats.ap_downlink_packets,
             (unsigned long)stats.ap_traffic_bytes,
             ap_mac,
             (unsigned long)stats.deauth_packets,
             (unsigned long)stats.disassoc_packets,
             stats.last_deauth_rssi,
             (unsigned long)stats.last_deauth_ms,
             receiver,
             sender,
             bssid,
             stats.last_rssi,
             (unsigned long)stats.last_packet_ms);

    return api_send_json(req, json);
}

static void write_le16(uint8_t *buffer,
                       uint16_t value)
{
    buffer[0] = (uint8_t)(value & 0xff);
    buffer[1] = (uint8_t)((value >> 8) & 0xff);
}

static void write_le32(uint8_t *buffer,
                       uint32_t value)
{
    buffer[0] = (uint8_t)(value & 0xff);
    buffer[1] = (uint8_t)((value >> 8) & 0xff);
    buffer[2] = (uint8_t)((value >> 16) & 0xff);
    buffer[3] = (uint8_t)((value >> 24) & 0xff);
}

static esp_err_t api_capture_download_handler(httpd_req_t *req)
{
    capture_stats_t stats;
    uint8_t header[24];
    uint8_t packet_header[16];

    capture_get_stats(&stats);

    httpd_resp_set_type(req, "application/vnd.tcpdump.pcap");
    httpd_resp_set_hdr(req,
                       "Content-Disposition",
                       "attachment; filename=\"wifi_capture.pcap\"");

    write_le32(&header[0], 0xa1b2c3d4);
    write_le16(&header[4], 2);
    write_le16(&header[6], 4);
    write_le32(&header[8], 0);
    write_le32(&header[12], 0);
    write_le32(&header[16], CAPTURE_MAX_FRAME_SIZE);
    write_le32(&header[20], PCAP_LINKTYPE_IEEE802_11);

    ESP_RETURN_ON_ERROR(httpd_resp_send_chunk(req,
                                              (const char *)header,
                                              sizeof(header)),
                        TAG,
                        "send pcap header");

    for (uint16_t i = 0; i < stats.stored_packets; i++)
    {
        capture_packet_view_t packet;

        if (!capture_get_packet(i, &packet))
        {
            break;
        }

        write_le32(&packet_header[0], packet.timestamp_sec);
        write_le32(&packet_header[4], packet.timestamp_usec);
        write_le32(&packet_header[8], packet.captured_length);
        write_le32(&packet_header[12], packet.original_length);

        ESP_RETURN_ON_ERROR(httpd_resp_send_chunk(req,
                                                  (const char *)packet_header,
                                                  sizeof(packet_header)),
                            TAG,
                            "send pcap packet header");

        ESP_RETURN_ON_ERROR(httpd_resp_send_chunk(req,
                                                  (const char *)packet.data,
                                                  packet.captured_length),
                            TAG,
                            "send pcap packet data");
    }

    return httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t api_capture_start_handler(httpd_req_t *req)
{
    char body[API_POST_BUFFER_SIZE];
    uint8_t channel = 1;
    bool ap_mode = false;

    if (scanner_is_busy())
    {
        return api_send_error(req,
                              400,
                              "scanner busy");
    }

    if (req->content_len > 0)
    {
        if (api_read_body(req, body, sizeof(body)) != ESP_OK)
        {
            return api_send_error(req,
                                  400,
                                  "invalid request body");
        }

        cJSON *root = cJSON_Parse(body);

        if (root == NULL)
        {
            return api_send_error(req,
                                  400,
                                  "invalid json");
        }

        const cJSON *channel_item =
            cJSON_GetObjectItemCaseSensitive(root, "channel");

        if (cJSON_IsNumber(channel_item))
        {
            channel = (uint8_t)channel_item->valueint;
        }

        const cJSON *mode_item =
            cJSON_GetObjectItemCaseSensitive(root, "mode");

        if (cJSON_IsString(mode_item) &&
            (strcmp(mode_item->valuestring, "ap") == 0 ||
             strcmp(mode_item->valuestring, "freewifi") == 0))
        {
            ap_mode = true;
        }

        cJSON_Delete(root);
    }

    esp_err_t err =
        ap_mode ? capture_start_ap_traffic()
                : capture_start(channel);

    if (err != ESP_OK)
    {
        return api_send_error(req,
                              400,
                              esp_err_to_name(err));
    }

    return api_send_json(req,
                         "{\"status\":\"started\"}");
}

static esp_err_t api_freewifi_start_handler(httpd_req_t *req)
{
    if (!station_manager_is_connected())
    {
        return api_send_error(req,
                              400,
                              "connect STA before starting freeWiFi");
    }

    if (capture_is_running())
    {
        ESP_RETURN_ON_ERROR(capture_stop(), TAG, "stop capture before AP switch");
    }

    esp_err_t err = wifi_manager_start_freewifi_ap();

    if (err != ESP_OK)
    {
        return api_send_error(req,
                              500,
                              esp_err_to_name(err));
    }

    err = wifi_manager_enable_nat_routing();

    if (err != ESP_OK)
    {
        return api_send_error(req,
                              500,
                              esp_err_to_name(err));
    }

    return api_send_json(req,
                         "{\"status\":\"started\"}");
}

static esp_err_t api_freewifi_stop_handler(httpd_req_t *req)
{
    if (capture_is_running())
    {
        ESP_RETURN_ON_ERROR(capture_stop(), TAG, "stop capture before AP switch");
    }

    esp_err_t err = wifi_manager_stop_freewifi_ap();

    if (err != ESP_OK)
    {
        return api_send_error(req,
                              500,
                              esp_err_to_name(err));
    }

    return api_send_json(req,
                         "{\"status\":\"stopped\"}");
}

static esp_err_t api_capture_stop_handler(httpd_req_t *req)
{
    capture_stats_t stats;

    capture_get_stats(&stats);

    esp_err_t err = capture_stop();

    if (err != ESP_OK)
    {
        return api_send_error(req,
                              500,
                              esp_err_to_name(err));
    }

    if (stats.mode == CAPTURE_MODE_AP_TRAFFIC &&
        wifi_manager_get_ap_mode() == WIFI_MANAGER_AP_FREEWIFI)
    {
        err = wifi_manager_stop_freewifi_ap();

        if (err != ESP_OK)
        {
            return api_send_error(req,
                                  500,
                                  esp_err_to_name(err));
        }
    }

    return api_send_json(req,
                         "{\"status\":\"stopped\"}");
}

esp_err_t api_system_handler(httpd_req_t *req)
{
    char json[256];

    snprintf(json,
             sizeof(json),
             "{"
             "\"version\":\"%s\","
             "\"ap_ssid\":\"%s\","
             "\"ap_mode\":\"%s\","
             "\"ap_ip\":\"%s\","
             "\"ap_clients\":%u,"
             "\"nat_routing\":%s"
             "}",
             API_VERSION,
             wifi_manager_get_ap_ssid(),
             wifi_manager_get_ap_mode_name(),
             wifi_manager_get_ap_ip(),
             wifi_manager_get_client_count(),
             wifi_manager_is_nat_routing_enabled() ? "true" : "false");

    return api_send_json(req, json);
}

esp_err_t api_info_handler(httpd_req_t *req)
{
    return api_system_handler(req);
}

esp_err_t api_register(httpd_handle_t server)
{
    httpd_uri_t status = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = api_status_handler,
        .user_ctx = NULL
    };

    httpd_uri_t scan = {
        .uri = "/api/scan",
        .method = HTTP_GET,
        .handler = api_scan_handler,
        .user_ctx = NULL
    };

    httpd_uri_t results = {
        .uri = "/api/results",
        .method = HTTP_GET,
        .handler = api_results_handler,
        .user_ctx = NULL
    };

    httpd_uri_t station = {
        .uri = "/api/station",
        .method = HTTP_GET,
        .handler = api_station_handler,
        .user_ctx = NULL
    };

    httpd_uri_t connect = {
        .uri = "/api/connect",
        .method = HTTP_POST,
        .handler = api_connect_handler,
        .user_ctx = NULL
    };

    httpd_uri_t disconnect = {
        .uri = "/api/disconnect",
        .method = HTTP_POST,
        .handler = api_disconnect_handler,
        .user_ctx = NULL
    };

    httpd_uri_t system = {
        .uri = "/api/system",
        .method = HTTP_GET,
        .handler = api_system_handler,
        .user_ctx = NULL
    };

    httpd_uri_t capture = {
        .uri = "/api/capture",
        .method = HTTP_GET,
        .handler = api_capture_handler,
        .user_ctx = NULL
    };

    httpd_uri_t capture_start = {
        .uri = "/api/capture/start",
        .method = HTTP_POST,
        .handler = api_capture_start_handler,
        .user_ctx = NULL
    };

    httpd_uri_t capture_stop = {
        .uri = "/api/capture/stop",
        .method = HTTP_POST,
        .handler = api_capture_stop_handler,
        .user_ctx = NULL
    };

    httpd_uri_t capture_download = {
        .uri = "/api/capture/download",
        .method = HTTP_GET,
        .handler = api_capture_download_handler,
        .user_ctx = NULL
    };

    httpd_uri_t freewifi_start = {
        .uri = "/api/freewifi/start",
        .method = HTTP_POST,
        .handler = api_freewifi_start_handler,
        .user_ctx = NULL
    };

    httpd_uri_t freewifi_stop = {
        .uri = "/api/freewifi/stop",
        .method = HTTP_POST,
        .handler = api_freewifi_stop_handler,
        .user_ctx = NULL
    };

    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &status), TAG, "status route");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &scan), TAG, "scan route");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &results), TAG, "results route");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &station), TAG, "station route");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &connect), TAG, "connect route");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &disconnect), TAG, "disconnect route");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &system), TAG, "system route");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &capture), TAG, "capture route");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &capture_start), TAG, "capture start route");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &capture_stop), TAG, "capture stop route");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &capture_download), TAG, "capture download route");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &freewifi_start), TAG, "freewifi start route");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &freewifi_stop), TAG, "freewifi stop route");

    return ESP_OK;
}
