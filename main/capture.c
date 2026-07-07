#include "capture.h"

#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

#include "wifi_manager.h"

static const char *TAG = "CAPTURE";

typedef struct
{
    uint32_t timestamp_sec;
    uint32_t timestamp_usec;
    uint16_t captured_length;
    uint16_t original_length;
    uint8_t data[CAPTURE_MAX_FRAME_SIZE];
} capture_packet_t;

static capture_stats_t stats;
static capture_packet_t packets[CAPTURE_MAX_PACKETS];
static portMUX_TYPE stats_lock = portMUX_INITIALIZER_UNLOCKED;

static bool mac_equal(const uint8_t *left,
                      const uint8_t *right)
{
    return memcmp(left, right, 6) == 0;
}

static void copy_mac(uint8_t *destination,
                     const uint8_t *source)
{
    memcpy(destination, source, 6);
}

static bool frame_matches_ap_traffic(const uint8_t *payload,
                                     uint16_t length,
                                     const uint8_t *ap_mac,
                                     bool *uplink,
                                     bool *downlink)
{
    if (length < 24)
    {
        return false;
    }

    uint16_t frame_control =
        (uint16_t)payload[0] |
        ((uint16_t)payload[1] << 8);
    uint8_t frame_type =
        (uint8_t)((frame_control >> 2) & 0x03);

    if (frame_type != 2)
    {
        return false;
    }

    bool to_ds = (frame_control & 0x0100) != 0;
    bool from_ds = (frame_control & 0x0200) != 0;

    *uplink = false;
    *downlink = false;

    if (to_ds && !from_ds && mac_equal(&payload[4], ap_mac))
    {
        *uplink = true;
        return true;
    }

    if (!to_ds && from_ds && mac_equal(&payload[10], ap_mac))
    {
        *downlink = true;
        return true;
    }

    return false;
}

static void capture_packet_handler(void *buffer,
                                   wifi_promiscuous_pkt_type_t type)
{
    const wifi_promiscuous_pkt_t *packet =
        (const wifi_promiscuous_pkt_t *)buffer;

    int64_t timestamp_us = esp_timer_get_time();
    uint16_t original_length = packet->rx_ctrl.sig_len;
    uint16_t captured_length = original_length;
    bool ap_uplink = false;
    bool ap_downlink = false;
    bool accept_packet = true;
    capture_mode_t mode;
    uint8_t ap_mac[6];

    if (captured_length > CAPTURE_MAX_FRAME_SIZE)
    {
        captured_length = CAPTURE_MAX_FRAME_SIZE;
    }

    portENTER_CRITICAL(&stats_lock);
    mode = stats.mode;
    copy_mac(ap_mac, stats.ap_mac);
    portEXIT_CRITICAL(&stats_lock);

    if (mode == CAPTURE_MODE_AP_TRAFFIC)
    {
        accept_packet =
            frame_matches_ap_traffic(packet->payload,
                                     captured_length,
                                     ap_mac,
                                     &ap_uplink,
                                     &ap_downlink);
    }

    portENTER_CRITICAL(&stats_lock);

    stats.observed_packets++;

    if (!accept_packet)
    {
        portEXIT_CRITICAL(&stats_lock);

        return;
    }

    stats.total_packets++;
    stats.last_rssi = packet->rx_ctrl.rssi;
    stats.last_packet_ms = (uint32_t)(timestamp_us / 1000ULL);

    if (mode == CAPTURE_MODE_AP_TRAFFIC)
    {
        stats.ap_traffic_packets++;
        stats.ap_traffic_bytes += original_length;

        if (ap_uplink)
        {
            stats.ap_uplink_packets++;
        }

        if (ap_downlink)
        {
            stats.ap_downlink_packets++;
        }
    }

    if (stats.stored_packets < CAPTURE_MAX_PACKETS)
    {
        capture_packet_t *stored =
            &packets[stats.stored_packets];

        stored->timestamp_sec = (uint32_t)(timestamp_us / 1000000ULL);
        stored->timestamp_usec = (uint32_t)(timestamp_us % 1000000ULL);
        stored->captured_length = captured_length;
        stored->original_length = original_length;

        memcpy(stored->data,
               packet->payload,
               captured_length);

        stats.stored_packets++;
        stats.pcap_bytes += 16 + captured_length;
    }
    else
    {
        stats.dropped_packets++;
    }

    switch (type)
    {
        case WIFI_PKT_MGMT:
            stats.management_packets++;

            if (captured_length >= 24)
            {
                const uint8_t *payload = packet->payload;
                uint16_t frame_control =
                    (uint16_t)payload[0] |
                    ((uint16_t)payload[1] << 8);
                uint8_t frame_type =
                    (uint8_t)((frame_control >> 2) & 0x03);
                uint8_t frame_subtype =
                    (uint8_t)((frame_control >> 4) & 0x0f);

                if (frame_type == 0 &&
                    (frame_subtype == 10 || frame_subtype == 12))
                {
                    if (frame_subtype == 12)
                    {
                        stats.deauth_packets++;
                    }
                    else
                    {
                        stats.disassoc_packets++;
                    }

                    stats.last_deauth_rssi = packet->rx_ctrl.rssi;
                    stats.last_deauth_ms = (uint32_t)(timestamp_us / 1000ULL);
                    copy_mac(stats.last_deauth_receiver, &payload[4]);
                    copy_mac(stats.last_deauth_sender, &payload[10]);
                    copy_mac(stats.last_deauth_bssid, &payload[16]);
                }
            }
            break;

        case WIFI_PKT_CTRL:
            stats.control_packets++;
            break;

        case WIFI_PKT_DATA:
            stats.data_packets++;
            break;

        default:
            break;
    }

    portEXIT_CRITICAL(&stats_lock);
}

esp_err_t capture_init(void)
{
    memset(&stats, 0, sizeof(stats));

    wifi_promiscuous_filter_t filter = {
        .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT |
                       WIFI_PROMIS_FILTER_MASK_CTRL |
                       WIFI_PROMIS_FILTER_MASK_DATA
    };

    esp_err_t err = esp_wifi_set_promiscuous_filter(&filter);

    if (err != ESP_OK)
    {
        return err;
    }

    err = esp_wifi_set_promiscuous_rx_cb(capture_packet_handler);

    if (err != ESP_OK)
    {
        return err;
    }

    ESP_LOGI(TAG, "Packet capture initialized");

    return ESP_OK;
}

esp_err_t capture_start(uint8_t channel)
{
    if (channel < 1 || channel > 13)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = esp_wifi_set_promiscuous(false);

    if (err != ESP_OK)
    {
        return err;
    }

    uint8_t active_channel = channel;
    wifi_second_chan_t second_channel = WIFI_SECOND_CHAN_NONE;

    err = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);

    if (err != ESP_OK)
    {
        esp_err_t get_err =
            esp_wifi_get_channel(&active_channel,
                                 &second_channel);

        if (get_err != ESP_OK)
        {
            return err;
        }

        ESP_LOGW(TAG,
                 "Cannot switch to channel %u, using current channel %u (%s)",
                 channel,
                 active_channel,
                 esp_err_to_name(err));
    }

    portENTER_CRITICAL(&stats_lock);
    memset(&stats, 0, sizeof(stats));
    memset(packets, 0, sizeof(packets));
    stats.running = true;
    stats.mode = CAPTURE_MODE_ALL;
    stats.channel = active_channel;
    stats.pcap_bytes = 24;
    portEXIT_CRITICAL(&stats_lock);

    err = esp_wifi_set_promiscuous(true);

    if (err != ESP_OK)
    {
        portENTER_CRITICAL(&stats_lock);
        stats.running = false;
        portEXIT_CRITICAL(&stats_lock);

        return err;
    }

    ESP_LOGI(TAG,
             "Packet capture started on channel %u",
             active_channel);

    return ESP_OK;
}

esp_err_t capture_start_ap_traffic(void)
{
    if (wifi_manager_get_ap_mode() != WIFI_MANAGER_AP_FREEWIFI)
    {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = esp_wifi_set_promiscuous(false);

    if (err != ESP_OK)
    {
        return err;
    }

    uint8_t ap_mac[6] = {0};

    err = wifi_manager_get_ap_mac(ap_mac);

    if (err != ESP_OK)
    {
        return err;
    }

    uint8_t active_channel = 0;
    wifi_second_chan_t second_channel = WIFI_SECOND_CHAN_NONE;

    err = esp_wifi_get_channel(&active_channel,
                               &second_channel);

    if (err != ESP_OK)
    {
        return err;
    }

    portENTER_CRITICAL(&stats_lock);
    memset(&stats, 0, sizeof(stats));
    memset(packets, 0, sizeof(packets));
    stats.running = true;
    stats.mode = CAPTURE_MODE_AP_TRAFFIC;
    stats.channel = active_channel;
    stats.pcap_bytes = 24;
    copy_mac(stats.ap_mac, ap_mac);
    portEXIT_CRITICAL(&stats_lock);

    err = esp_wifi_set_promiscuous(true);

    if (err != ESP_OK)
    {
        portENTER_CRITICAL(&stats_lock);
        stats.running = false;
        portEXIT_CRITICAL(&stats_lock);

        return err;
    }

    ESP_LOGI(TAG,
             "AP traffic capture started on channel %u",
             active_channel);

    return ESP_OK;
}

esp_err_t capture_stop(void)
{
    esp_err_t err = esp_wifi_set_promiscuous(false);

    portENTER_CRITICAL(&stats_lock);
    stats.running = false;
    portEXIT_CRITICAL(&stats_lock);

    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "Packet capture stopped");
    }

    return err;
}

void capture_get_stats(capture_stats_t *out)
{
    if (out == NULL)
    {
        return;
    }

    portENTER_CRITICAL(&stats_lock);
    *out = stats;
    portEXIT_CRITICAL(&stats_lock);
}

bool capture_get_packet(uint16_t index,
                        capture_packet_view_t *packet)
{
    if (packet == NULL)
    {
        return false;
    }

    portENTER_CRITICAL(&stats_lock);

    if (index >= stats.stored_packets)
    {
        portEXIT_CRITICAL(&stats_lock);

        return false;
    }

    const capture_packet_t *stored = &packets[index];

    packet->timestamp_sec = stored->timestamp_sec;
    packet->timestamp_usec = stored->timestamp_usec;
    packet->captured_length = stored->captured_length;
    packet->original_length = stored->original_length;
    packet->data = stored->data;

    portEXIT_CRITICAL(&stats_lock);

    return true;
}

bool capture_is_running(void)
{
    bool running;

    portENTER_CRITICAL(&stats_lock);
    running = stats.running;
    portEXIT_CRITICAL(&stats_lock);

    return running;
}

const char *capture_mode_to_string(capture_mode_t mode)
{
    switch (mode)
    {
        case CAPTURE_MODE_AP_TRAFFIC:
            return "ap";

        case CAPTURE_MODE_ALL:
        default:
            return "all";
    }
}
