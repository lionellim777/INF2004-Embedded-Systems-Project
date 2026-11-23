#ifndef RTOS_NETWORK_SETTINGS_H
#define RTOS_NETWORK_SETTINGS_H
/* Copy to ignored network_settings.local.h, then rebuild for a radio test.
 * No credentials are needed for the first USB motor/stop check. */
#define RTOS_NETWORK_CONFIGURED 0
#define RTOS_WIFI_SSID ""
#define RTOS_WIFI_PASSWORD ""
#define RTOS_BROKER_IPV4 "192.168.1.10"
#define RTOS_BROKER_PORT 1883
#define RTOS_BROKER_USER ""
#define RTOS_BROKER_PASSWORD ""
#define RTOS_TOPIC_PREFIX "inf2004/aai10/robot1/v1/"
#endif
