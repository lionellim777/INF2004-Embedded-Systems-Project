#ifndef COMM_SECRETS_H
#define COMM_SECRETS_H

/* Copy to comm_secrets.h and replace every placeholder before flashing. */
#define COMM_CONFIG_INITIALIZER { \
    .ssid = "REPLACE_WIFI_SSID", \
    .wifi_password = "REPLACE_WIFI_PASSWORD", \
    .broker_ipv4 = "192.168.1.10", \
    .broker_user = "REPLACE_BROKER_USER", \
    .broker_password = "REPLACE_BROKER_PASSWORD", \
    .team_id = "team1", \
    .robot_id = "robot1" \
}

#endif /* COMM_SECRETS_H */
