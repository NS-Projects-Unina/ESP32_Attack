#include "wifi_info.h"
#include "app_config.h"

const char *wifi_info_get_mode(void)
{
    return "AP";
}

const char *wifi_info_get_ssid(void)
{
    return AP_SSID;
}

const char *wifi_info_get_ip(void)
{
    return AP_IP;
}