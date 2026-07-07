#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/*
 * Access Point Configuration
 */

#define MANAGEMENT_AP_SSID      "ESP32 - Management"
#define MANAGEMENT_AP_PASSWORD  ""

#define FREEWIFI_AP_SSID        "freeWiFi"
#define FREEWIFI_AP_PASSWORD    ""

#define AP_SSID                 MANAGEMENT_AP_SSID
#define AP_PASSWORD             MANAGEMENT_AP_PASSWORD

#define AP_CHANNEL              1
#define AP_MAX_CONNECTIONS      4

#define AP_IP                   "192.168.4.1"
#define AP_GATEWAY              "192.168.4.1"
#define AP_NETMASK              "255.255.255.0"

/*
 * Scanner Configuration
 */

#define MAX_SCAN_RESULTS        50

#define SCAN_TIMEOUT_MS         5000

/*
 * Web Server
 */

#define HTTP_SERVER_PORT        80
#define HTTP_MAX_URI_HANDLERS   20

/*
 * Planned Features
 */

#define ENABLE_NAT_ROUTING      1
#define ENABLE_AP_TRAFFIC_SNIFFER 1

#endif
