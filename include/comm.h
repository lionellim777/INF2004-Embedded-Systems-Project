#ifndef COMM_H
#define COMM_H

#include <stdint.h>

#include "telemetry.h"

#define COMM_EVENT_CAPACITY 16U
#define COMM_TEXT_SIZE 64U

typedef enum
{
    COMM_OK,
    COMM_INVALID,
    COMM_BUSY,
    COMM_FULL,
    COMM_ERROR
} comm_result_t;

typedef enum
{
    COMM_STATE_STARTING,
    COMM_STATE_WIFI_CONNECTING,
    COMM_STATE_MQTT_CONNECTING,
    COMM_STATE_SUBSCRIBING,
    COMM_STATE_ONLINE,
    COMM_STATE_RETRYING
} comm_state_t;

typedef struct
{
    char ssid[33];
    char wifi_password[64];
    char broker_ipv4[16];
    char broker_user[32];
    char broker_password[64];
    char team_id[16];
    char robot_id[16];
} comm_config_t;

typedef struct
{
    comm_state_t state;
    uint32_t boot_id;
    uint32_t reconnect_count;
    uint32_t dropped_count;
    uint32_t last_error;
    uint32_t telemetry_period_ms;
    uint32_t task_ticks;
    uint32_t queue_peak;
} comm_status_t;

/* Task-context API. Calls copy their input and never wait for the network. */
comm_result_t comm_init(comm_config_t const *p_config);
comm_result_t comm_update_telemetry(telemetry_sample_t const *p_sample);
comm_result_t comm_post_event(telemetry_event_t const *p_event);
comm_result_t comm_get_status(comm_status_t *p_status);
void comm_task(int start_code, void *p_context);

#endif /* COMM_H */
