#ifndef STATION_SETTINGS_H
#define STATION_SETTINGS_H
/* Wi-Fi station settings for the dashboard builds (-DROBOCAR_WIFI_STATION=ON).
 *
 * 1. Copy this file to station_settings.local.h in the same folder.
 *    The .local.h file is git-ignored. NEVER commit it, and never publish or
 *    share a UF2 built from it: the Wi-Fi password is compiled into the image.
 * 2. Put the name and password of the network your computer is on. The Pico W
 *    supports 2.4 GHz only (not 5 GHz) and WPA2-PSK. WPA2-Enterprise ("login
 *    with username") networks are not supported. A phone hotspot set to 2.4 GHz
 *    works well.
 * 3. Rebuild. If the car cannot join, it falls back to its own AAI10-Calibrate
 *    access point, so the dashboard stays reachable. */
#define STATION_WIFI_SSID "your-network-name"
#define STATION_WIFI_PASSWORD "your-network-password"
/* Optional. Default is CYW43_AUTH_WPA2_AES_PSK. For a router in WPA2/WPA3 mixed
 * mode try CYW43_AUTH_WPA2_MIXED_PSK. WPA3-only networks are not supported. */
/* #define STATION_WIFI_AUTH CYW43_AUTH_WPA2_MIXED_PSK */
#endif
