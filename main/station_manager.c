#include "station_manager.h"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include "wifi_manager.h"

static const char *TAG = "STATION";
static bool connected = false;
static bool connecting = false;
static char current_ssid[33] = {0};
static char current_ip[16] = {0};

/*
 * Event Handler
 */

static void station_event_handler(void *arg,
                                  esp_event_base_t event_base,
                                  int32_t event_id,
                                  void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "Connected to AP");
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
                wifi_manager_disable_nat_routing();
                connected = false;
                connecting = false;
                current_ssid[0] = '\0';
                current_ip[0] = '\0';
                ESP_LOGW(TAG, "Disconnected");
                break;

            default:
                break;
        }
    }
    else if (event_base == IP_EVENT)
    {
        switch (event_id)
        {
            case IP_EVENT_STA_GOT_IP:
            {
                ip_event_got_ip_t *event =
                    (ip_event_got_ip_t *)event_data;

                connected = true;
                connecting = false;
                esp_ip4addr_ntoa(&event->ip_info.ip,
                                 current_ip,
                                 sizeof(current_ip));
                ESP_LOGI(TAG,
                         "IP address acquired: %s",
                         current_ip);
                if (wifi_manager_get_ap_mode() == WIFI_MANAGER_AP_FREEWIFI)
                {
                    wifi_manager_enable_nat_routing();
                }

                break;
            }

            default:
                break;
        }
    }
}

/*
 * Initialization
 */

esp_err_t station_manager_init(void)
{
    ESP_ERROR_CHECK(
        esp_event_handler_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            station_event_handler,
            NULL));

    ESP_ERROR_CHECK(
        esp_event_handler_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            station_event_handler,
            NULL));

    ESP_LOGI(TAG, "Station manager initialized");

    return ESP_OK;
}

/*
 * Connect
 */

esp_err_t station_manager_connect(const char *ssid,
                                  const char *password)
{
    wifi_config_t config = {0};

    if (ssid == NULL || ssid[0] == '\0')
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (password == NULL)
    {
        password = "";
    }

    strncpy((char *)config.sta.ssid,
            ssid,
            sizeof(config.sta.ssid) - 1);

    strncpy((char *)config.sta.password,
            password,
            sizeof(config.sta.password) - 1);

    config.sta.threshold.authmode = WIFI_AUTH_OPEN;

    esp_err_t err =
        esp_wifi_set_config(
            WIFI_IF_STA,
            &config);

    if (err != ESP_OK)
    {
        return err;
    }

    strncpy(current_ssid,
            ssid,
            sizeof(current_ssid) - 1);

    current_ssid[sizeof(current_ssid) - 1] = '\0';
    current_ip[0] = '\0';
    connected = false;
    connecting = true;
    ESP_LOGI(TAG,
             "Connecting to %s...",
             ssid);

    err = esp_wifi_connect();

    if (err != ESP_OK)
    {
        connecting = false;
    }

    return err;
}

/*
 * Disconnect
 */

esp_err_t station_manager_disconnect(void)
{
    wifi_manager_disable_nat_routing();
    connected = false;
    connecting = false;
    current_ssid[0] = '\0';
    current_ip[0] = '\0';
    ESP_LOGI(TAG, "Disconnecting");
    
    return esp_wifi_disconnect();
}

/*
 * Status
 */

bool station_manager_is_connected(void)
{
    return connected;
}

bool station_manager_is_connecting(void)
{
    return connecting;
}

const char *station_manager_get_ssid(void)
{
    return current_ssid;
}

const char *station_manager_get_ip(void)
{
    return current_ip;
}

const char *station_manager_get_status(void)
{
    if (connected)
    {
        return "connected";
    }

    if (connecting)
    {
        return "connecting";
    }

    return "disconnected";
}
