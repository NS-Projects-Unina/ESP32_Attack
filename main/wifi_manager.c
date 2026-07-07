#include "wifi_manager.h"
#include "app_config.h"
#include "scanner.h"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include "lwip/ip4_addr.h"

static const char *TAG = "WIFI_MANAGER";
static const uint8_t DHCPS_OFFER_DNS = 0x02;

static uint16_t ap_client_count = 0;
static bool nat_routing_enabled = false;
static wifi_manager_ap_mode_t ap_mode = WIFI_MANAGER_AP_MANAGEMENT;

static esp_netif_t *ap_netif = NULL;
static esp_netif_t *sta_netif = NULL;

static esp_err_t configure_softap(const char *ssid,
                                  const char *password)
{
    wifi_config_t wifi_config = {
        .ap = {
            .channel = AP_CHANNEL,
            .max_connection = AP_MAX_CONNECTIONS,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = false
            }
        }
    };

    strncpy((char *)wifi_config.ap.ssid,
            ssid,
            sizeof(wifi_config.ap.ssid) - 1);
    wifi_config.ap.ssid[sizeof(wifi_config.ap.ssid) - 1] = '\0';

    wifi_config.ap.ssid_len =
        strlen((const char *)wifi_config.ap.ssid);

    strncpy((char *)wifi_config.ap.password,
            password,
            sizeof(wifi_config.ap.password) - 1);
    wifi_config.ap.password[sizeof(wifi_config.ap.password) - 1] = '\0';

    if (strlen(password) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    return esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
}

/*
 * WiFi Event Handler
 */

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base != WIFI_EVENT)
        return;

    switch (event_id)
    {
        case WIFI_EVENT_AP_START:
            ESP_LOGI(TAG, "Access Point Started");
            break;

        case WIFI_EVENT_AP_STACONNECTED:
        {
            wifi_event_ap_staconnected_t *event =
                (wifi_event_ap_staconnected_t *)event_data;

            ESP_LOGI(TAG,
                     "Client connected (%d) %02X:%02X:%02X:%02X:%02X:%02X",
                     event->aid,
                     event->mac[0],
                     event->mac[1],
                     event->mac[2],
                     event->mac[3],
                     event->mac[4],
                     event->mac[5]);

            ap_client_count++;

            break;
        }

        case WIFI_EVENT_AP_STADISCONNECTED:
        {
            wifi_event_ap_stadisconnected_t *event =
                (wifi_event_ap_stadisconnected_t *)event_data;

            ESP_LOGI(TAG,
                     "Client disconnected (%d) %02X:%02X:%02X:%02X:%02X:%02X",
                     event->aid,
                     event->mac[0],
                     event->mac[1],
                     event->mac[2],
                     event->mac[3],
                     event->mac[4],
                     event->mac[5]);

            if (ap_client_count > 0)
            {
                ap_client_count--;
            }

            break;
        }

        case WIFI_EVENT_SCAN_DONE:
            ESP_LOGI(TAG, "WiFi scan completed");
            scanner_scan_done();
            break;
        default:
            break;
    }
}

/*
 * WiFi Initialization
 */

void wifi_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing Network Stack...");
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ap_netif =
        esp_netif_create_default_wifi_ap();

    sta_netif =
        esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(
        esp_netif_set_default_netif(
            sta_netif));

    wifi_init_config_t cfg =
        WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(
        esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(
        esp_event_handler_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &wifi_event_handler,
            NULL));

    /*
     * Configure AP IP
     */

    ESP_ERROR_CHECK(
        esp_netif_dhcps_stop(ap_netif));

    esp_netif_ip_info_t ip_info;

    IP4_ADDR(&ip_info.ip, 192,168,4,1);
    IP4_ADDR(&ip_info.gw, 192,168,4,1);
    IP4_ADDR(&ip_info.netmask,255,255,255,0);

    ESP_ERROR_CHECK(
        esp_netif_set_ip_info(
            ap_netif,
            &ip_info));

    ESP_ERROR_CHECK(
        esp_netif_dhcps_start(ap_netif));

    ESP_ERROR_CHECK(
        esp_wifi_set_mode(
            WIFI_MODE_APSTA));

    ESP_ERROR_CHECK(
        configure_softap(MANAGEMENT_AP_SSID,
                         MANAGEMENT_AP_PASSWORD));

    ESP_ERROR_CHECK(
        esp_wifi_start());

    /*
     * Lo STA rimane inizializzato ma non connesso.
     */

    esp_wifi_disconnect();

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=========================================");
    ESP_LOGI(TAG, " ESP32 Wireless Analyzer");
    ESP_LOGI(TAG, "=========================================");
    ESP_LOGI(TAG, " SSID        : %s", MANAGEMENT_AP_SSID);
    ESP_LOGI(TAG, " PASSWORD    : %s", MANAGEMENT_AP_PASSWORD);
    ESP_LOGI(TAG, " IP          : %s", AP_IP);
    ESP_LOGI(TAG, " GATEWAY     : %s", AP_GATEWAY);
    ESP_LOGI(TAG, " NETMASK     : %s", AP_NETMASK);
    ESP_LOGI(TAG, " CHANNEL     : %d", AP_CHANNEL);
    ESP_LOGI(TAG, " MAX CLIENTS : %d", AP_MAX_CONNECTIONS);
    ESP_LOGI(TAG, "=========================================");
}

esp_err_t wifi_manager_enable_nat_routing(void)
{
    if (ap_netif == NULL || sta_netif == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    esp_netif_dns_info_t dns;

    if (esp_netif_get_dns_info(sta_netif,
                               ESP_NETIF_DNS_MAIN,
                               &dns) == ESP_OK)
    {
        ESP_ERROR_CHECK_WITHOUT_ABORT(
            esp_netif_dhcps_stop(ap_netif));

        ESP_ERROR_CHECK_WITHOUT_ABORT(
            esp_netif_dhcps_option(ap_netif,
                                   ESP_NETIF_OP_SET,
                                   ESP_NETIF_DOMAIN_NAME_SERVER,
                                   (void *)&DHCPS_OFFER_DNS,
                                   sizeof(DHCPS_OFFER_DNS)));

        ESP_ERROR_CHECK_WITHOUT_ABORT(
            esp_netif_set_dns_info(ap_netif,
                                   ESP_NETIF_DNS_MAIN,
                                   &dns));

        ESP_ERROR_CHECK_WITHOUT_ABORT(
            esp_netif_dhcps_start(ap_netif));
    }

    esp_err_t err =
        esp_netif_napt_enable(ap_netif);

    if (err == ESP_OK)
    {
        nat_routing_enabled = true;

        ESP_LOGI(TAG,
                 "NAT routing enabled for AP clients");
    }
    else
    {
        nat_routing_enabled = false;

        ESP_LOGE(TAG,
                 "Failed to enable NAT routing: %s",
                 esp_err_to_name(err));
    }

    return err;
}

esp_err_t wifi_manager_disable_nat_routing(void)
{
    if (ap_netif == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err =
        esp_netif_napt_disable(ap_netif);

    if (err == ESP_OK)
    {
        ESP_LOGI(TAG,
                 "NAT routing disabled");
    }
    else
    {
        ESP_LOGW(TAG,
                 "Failed to disable NAT routing: %s",
                 esp_err_to_name(err));
    }

    nat_routing_enabled = false;

    return err;
}

uint16_t wifi_manager_get_client_count(void)
{
    return ap_client_count;
}

const char *wifi_manager_get_ap_ssid(void)
{
    return ap_mode == WIFI_MANAGER_AP_FREEWIFI
        ? FREEWIFI_AP_SSID
        : MANAGEMENT_AP_SSID;
}

const char *wifi_manager_get_ap_ip(void)
{
    return AP_IP;
}

esp_err_t wifi_manager_get_ap_mac(uint8_t mac[6])
{
    if (mac == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    return esp_wifi_get_mac(WIFI_IF_AP, mac);
}

bool wifi_manager_is_nat_routing_enabled(void)
{
    return nat_routing_enabled;
}

wifi_manager_ap_mode_t wifi_manager_get_ap_mode(void)
{
    return ap_mode;
}

const char *wifi_manager_get_ap_mode_name(void)
{
    return ap_mode == WIFI_MANAGER_AP_FREEWIFI
        ? "freewifi"
        : "management";
}

esp_err_t wifi_manager_start_freewifi_ap(void)
{
    esp_err_t err =
        configure_softap(FREEWIFI_AP_SSID,
                         FREEWIFI_AP_PASSWORD);

    if (err != ESP_OK)
    {
        return err;
    }

    ap_mode = WIFI_MANAGER_AP_FREEWIFI;
    ap_client_count = 0;

    ESP_LOGI(TAG,
             "SoftAP switched to %s",
             FREEWIFI_AP_SSID);

    return ESP_OK;
}

esp_err_t wifi_manager_stop_freewifi_ap(void)
{
    wifi_manager_disable_nat_routing();

    esp_err_t err =
        configure_softap(MANAGEMENT_AP_SSID,
                         MANAGEMENT_AP_PASSWORD);

    if (err != ESP_OK)
    {
        return err;
    }

    ap_mode = WIFI_MANAGER_AP_MANAGEMENT;
    ap_client_count = 0;

    ESP_LOGI(TAG,
             "SoftAP restored to %s",
             MANAGEMENT_AP_SSID);

    return ESP_OK;
}
