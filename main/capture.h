#ifndef CAPTURE_H
#define CAPTURE_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define CAPTURE_MAX_PACKETS     160
#define CAPTURE_MAX_FRAME_SIZE  256

typedef enum
{
    CAPTURE_MODE_ALL = 0,
    CAPTURE_MODE_AP_TRAFFIC
} capture_mode_t;

typedef struct
{
    bool running;
    capture_mode_t mode;
    uint8_t channel;
    uint8_t ap_mac[6];
    uint32_t total_packets;
    uint32_t observed_packets;
    uint32_t stored_packets;
    uint32_t dropped_packets;
    uint32_t pcap_bytes;
    uint32_t management_packets;
    uint32_t control_packets;
    uint32_t data_packets;
    uint32_t ap_traffic_packets;
    uint32_t ap_uplink_packets;
    uint32_t ap_downlink_packets;
    uint32_t ap_traffic_bytes;
    uint32_t deauth_packets;
    uint32_t disassoc_packets;
    int8_t last_rssi;
    int8_t last_deauth_rssi;
    uint32_t last_packet_ms;
    uint32_t last_deauth_ms;
    uint8_t last_deauth_receiver[6];
    uint8_t last_deauth_sender[6];
    uint8_t last_deauth_bssid[6];
} capture_stats_t;

typedef struct
{
    uint32_t timestamp_sec;
    uint32_t timestamp_usec;
    uint16_t captured_length;
    uint16_t original_length;
    const uint8_t *data;
} capture_packet_view_t;

esp_err_t capture_init(void);

esp_err_t capture_start(uint8_t channel);

esp_err_t capture_start_ap_traffic(void);

esp_err_t capture_stop(void);

const char *capture_mode_to_string(capture_mode_t mode);

void capture_get_stats(capture_stats_t *stats);

bool capture_get_packet(uint16_t index,
                        capture_packet_view_t *packet);

bool capture_is_running(void);

#endif
