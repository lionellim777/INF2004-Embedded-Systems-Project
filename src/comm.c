#include "comm.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "command.h"
#include "lwip/apps/mqtt.h"
#include "lwip/ip_addr.h"
#include "mtk_bridge.h"
#include "pico/cyw43_arch.h"
#include "pico/rand.h"
#include "pico/stdlib.h"

/* T-Kernel adds one 5 ms tick to a task delay. */
#define COMM_POLL_MS 5U
#define COMM_WIFI_DEADLINE_MS 15000U
#define COMM_MQTT_DEADLINE_MS 10000U
#define COMM_HEARTBEAT_MS 1000U
#define COMM_EVENT_MAX_AGE_MS 5000U
#define COMM_KEEPALIVE_SECONDS 15U
#define COMM_MQTT_PORT 1883U
#define COMM_TOPIC_SIZE 80U
#define COMM_CLIENT_ID_SIZE 48U

typedef enum
{
    COMM_PUBLISH_NONE,
    COMM_PUBLISH_STATUS,
    COMM_PUBLISH_EVENT,
    COMM_PUBLISH_RESULT
} comm_publish_kind_t;

typedef struct
{
    comm_config_t config;
    comm_status_t status;
    telemetry_snapshot_t snapshot;
    telemetry_event_t events[COMM_EVENT_CAPACITY];
    int32_t mutex_id;
    uint32_t event_head;
    uint32_t event_count;
    uint32_t sequence;
    uint32_t deadline_ms;
    uint32_t next_retry_ms;
    uint32_t retry_delay_ms;
    uint32_t online_since_ms;
    uint32_t last_status_ms;
    uint32_t last_telemetry_ms;
    uint32_t pending_period_ms;
    uint32_t subscribe_count;
    bool b_is_started;
    bool b_is_network_ready;
    bool b_is_mqtt_ready;
    bool b_has_failed;
    bool b_is_inbound_valid;
    bool b_has_command;
    bool b_has_result;
    bool b_is_publish_busy;
    bool b_is_publish_done;
    bool b_is_publish_success;
    bool b_is_command_accepted;
    comm_state_t state;
    comm_publish_kind_t publish_kind;
    mqtt_client_t *p_mqtt_client;
    struct mqtt_connect_client_info_t mqtt_info;
    ip_addr_t broker_address;
    char prefix[COMM_TOPIC_SIZE];
    char topic_status[COMM_TOPIC_SIZE];
    char topic_telemetry[COMM_TOPIC_SIZE];
    char topic_event[COMM_TOPIC_SIZE];
    char topic_get_status[COMM_TOPIC_SIZE];
    char topic_set_period[COMM_TOPIC_SIZE];
    char topic_result[COMM_TOPIC_SIZE];
    char client_id[COMM_CLIENT_ID_SIZE];
    char inbound_topic[COMMAND_PAYLOAD_SIZE];
    char pending_command[COMMAND_PAYLOAD_SIZE];
    command_frame_t inbound_frame;
    command_request_t command;
    char output[TELEMETRY_JSON_SIZE];
} comm_runtime_t;

static comm_runtime_t g_comm;

static bool comm_has_text(char const *p_text, size_t capacity);
static bool comm_has_id(char const *p_text, size_t capacity);
static bool comm_make_topics(void);
static bool comm_make_topic(char *p_output, size_t capacity,
                            char const *p_suffix);
static void comm_set_state(comm_state_t state);
static void comm_record_drop(void);
static void comm_expire_events(uint32_t now_ms);
static void comm_fail(uint32_t now_ms, uint32_t error_code);
static void comm_on_connection(mqtt_client_t *p_client, void *p_arg,
                               mqtt_connection_status_t status);
static void comm_on_subscription(void *p_arg, err_t error);
static void comm_on_publish(void *p_arg, err_t error);
static void comm_on_incoming_publish(void *p_arg, char const *p_topic,
                                     u32_t length);
static void comm_on_incoming_data(void *p_arg, u8_t const *p_data,
                                  u16_t length, u8_t flags);
static bool comm_publish(char const *p_topic, char const *p_payload,
                         uint8_t qos, uint8_t retain,
                         comm_publish_kind_t kind);
static void comm_publish_complete(uint32_t now_ms);
static void comm_start_wifi(uint32_t now_ms);
static void comm_start_mqtt(uint32_t now_ms);
static void comm_start_subscriptions(uint32_t now_ms);
static void comm_process_command(void);
static void comm_publish_status(uint32_t now_ms);
static void comm_publish_result(void);
static void comm_publish_event(void);
static void comm_publish_telemetry(uint32_t now_ms);
static void comm_step(uint32_t now_ms);

comm_result_t
comm_init(comm_config_t const *p_config)
{
    comm_result_t result = COMM_INVALID;

    if ((NULL != p_config) &&
        (true == comm_has_text(p_config->ssid,
                               sizeof(p_config->ssid))) &&
        (true == comm_has_text(p_config->wifi_password,
                               sizeof(p_config->wifi_password))) &&
        (true == comm_has_text(p_config->broker_ipv4,
                               sizeof(p_config->broker_ipv4))) &&
        (true == comm_has_text(p_config->broker_user,
                               sizeof(p_config->broker_user))) &&
        (true == comm_has_text(p_config->broker_password,
                               sizeof(p_config->broker_password))) &&
        (true == comm_has_id(p_config->team_id,
                             sizeof(p_config->team_id))) &&
        (true == comm_has_id(p_config->robot_id,
                             sizeof(p_config->robot_id))) &&
        (false == g_comm.b_is_started))
    {
        memset(&g_comm, 0, sizeof(g_comm));
        g_comm.config = *p_config;
        g_comm.retry_delay_ms = 1000U;
        g_comm.status.telemetry_period_ms = COMMAND_PERIOD_DEFAULT_MS;
        g_comm.status.boot_id = get_rand_32();
        g_comm.state = COMM_STATE_STARTING;
        g_comm.status.state = COMM_STATE_STARTING;
        g_comm.mutex_id = mtk_bridge_create_mutex();
        if (0 < g_comm.mutex_id)
        {
            if (true == comm_make_topics())
            {
                g_comm.b_is_started = true;
                result = COMM_OK;
            }
            else
            {
                mtk_bridge_delete_mutex(g_comm.mutex_id);
                result = COMM_ERROR;
            }
        }
        else
        {
            result = COMM_ERROR;
        }
    }

    return (result);
}

comm_result_t
comm_update_telemetry(telemetry_sample_t const *p_sample)
{
    comm_result_t result = COMM_INVALID;

    if ((NULL != p_sample) && (true == g_comm.b_is_started))
    {
        result = COMM_BUSY;
        if (true == mtk_bridge_lock_mutex(g_comm.mutex_id, false))
        {
            if (true == telemetry_apply_sample(&g_comm.snapshot, p_sample))
            {
                result = COMM_OK;
            }
            else
            {
                result = COMM_INVALID;
            }
            mtk_bridge_unlock_mutex(g_comm.mutex_id);
        }
    }

    return (result);
}

comm_result_t
comm_post_event(telemetry_event_t const *p_event)
{
    comm_result_t result = COMM_INVALID;
    uint32_t index = 0U;

    if ((NULL != p_event) && (TELEMETRY_EVENT_OBSTACLE >= p_event->kind) &&
        (true == g_comm.b_is_started))
    {
        result = COMM_BUSY;
        if (true == mtk_bridge_lock_mutex(g_comm.mutex_id, false))
        {
            if (COMM_EVENT_CAPACITY == g_comm.event_count)
            {
                g_comm.status.dropped_count++;
                result = COMM_FULL;
            }
            else
            {
                index = (g_comm.event_head + g_comm.event_count) %
                        COMM_EVENT_CAPACITY;
                g_comm.events[index] = *p_event;
                g_comm.event_count++;
                if (g_comm.event_count > g_comm.status.queue_peak)
                {
                    g_comm.status.queue_peak = g_comm.event_count;
                }
                result = COMM_OK;
            }
            mtk_bridge_unlock_mutex(g_comm.mutex_id);
        }
    }

    return (result);
}

comm_result_t
comm_get_status(comm_status_t *p_status)
{
    comm_result_t result = COMM_INVALID;

    if ((NULL != p_status) && (true == g_comm.b_is_started))
    {
        result = COMM_BUSY;
        if (true == mtk_bridge_lock_mutex(g_comm.mutex_id, false))
        {
            *p_status = g_comm.status;
            result = COMM_OK;
            mtk_bridge_unlock_mutex(g_comm.mutex_id);
        }
    }

    return (result);
}

void
comm_task(int start_code, void *p_context)
{
    uint32_t now_ms = 0U;

    (void)start_code;
    (void)p_context;

    if (true == g_comm.b_is_started)
    {
        if (0 == cyw43_arch_init())
        {
            g_comm.b_is_network_ready = true;
            cyw43_arch_enable_sta_mode();
            comm_start_wifi(to_ms_since_boot(get_absolute_time()));
        }
        else
        {
            comm_fail(to_ms_since_boot(get_absolute_time()), 1U);
        }
    }

    for (;;)
    {
        now_ms = to_ms_since_boot(get_absolute_time());
        comm_expire_events(now_ms);
        if (true == g_comm.b_is_network_ready)
        {
            cyw43_arch_poll();
            comm_step(now_ms);
        }
        else if (now_ms >= g_comm.next_retry_ms)
        {
            if (0 == cyw43_arch_init())
            {
                g_comm.b_is_network_ready = true;
                cyw43_arch_enable_sta_mode();
                comm_start_wifi(now_ms);
            }
            else
            {
                comm_fail(now_ms, 1U);
            }
        }
        if (true == mtk_bridge_lock_mutex(g_comm.mutex_id, false))
        {
            g_comm.status.task_ticks++;
            mtk_bridge_unlock_mutex(g_comm.mutex_id);
        }
        mtk_bridge_delay_ms(COMM_POLL_MS);
    }
}

static bool
comm_has_text(char const *p_text, size_t capacity)
{
    bool b_is_valid = false;

    if ((NULL != p_text) && ('\0' != p_text[0]) &&
        (NULL != memchr(p_text, '\0', capacity)))
    {
        b_is_valid = true;
    }

    return (b_is_valid);
}

static bool
comm_has_id(char const *p_text, size_t capacity)
{
    bool b_is_valid = comm_has_text(p_text, capacity);
    size_t index = 0U;

    if (true == b_is_valid)
    {
        for (index = 0U; '\0' != p_text[index]; index++)
        {
            if ((('a' > p_text[index]) || ('z' < p_text[index])) &&
                (('0' > p_text[index]) || ('9' < p_text[index])) &&
                ('_' != p_text[index]) && ('-' != p_text[index]))
            {
                b_is_valid = false;
                break;
            }
        }
    }

    return (b_is_valid);
}

static bool
comm_make_topics(void)
{
    bool b_is_valid = false;
    int length = 0;

    length = snprintf(g_comm.prefix, sizeof(g_comm.prefix),
                      "inf2004/%s/%s/", g_comm.config.team_id,
                      g_comm.config.robot_id);
    if ((0 < length) && ((size_t)length < sizeof(g_comm.prefix)))
    {
        length = snprintf(g_comm.client_id, sizeof(g_comm.client_id),
                          "inf2004-%s-%s", g_comm.config.team_id,
                          g_comm.config.robot_id);
        b_is_valid = (0 < length) &&
                     ((size_t)length < sizeof(g_comm.client_id));
    }

    if (true == b_is_valid)
    {
        b_is_valid = comm_make_topic(g_comm.topic_status,
                                     sizeof(g_comm.topic_status),
                                     "status") &&
                     comm_make_topic(g_comm.topic_telemetry,
                                     sizeof(g_comm.topic_telemetry),
                                     "telemetry") &&
                     comm_make_topic(g_comm.topic_event,
                                     sizeof(g_comm.topic_event),
                                     "event") &&
                     comm_make_topic(g_comm.topic_get_status,
                                     sizeof(g_comm.topic_get_status),
                                     "command/get_status") &&
                     comm_make_topic(g_comm.topic_set_period,
                                     sizeof(g_comm.topic_set_period),
                                     "command/set_period_ms") &&
                     comm_make_topic(g_comm.topic_result,
                                     sizeof(g_comm.topic_result),
                                     "command_result");
    }

    return (b_is_valid);
}

static bool
comm_make_topic(char *p_output, size_t capacity, char const *p_suffix)
{
    int length = snprintf(p_output, capacity, "%s%s",
                          g_comm.prefix, p_suffix);

    return ((0 < length) && ((size_t)length < capacity));
}

static void
comm_set_state(comm_state_t state)
{
    g_comm.state = state;
    if (true == mtk_bridge_lock_mutex(g_comm.mutex_id, true))
    {
        g_comm.status.state = state;
        mtk_bridge_unlock_mutex(g_comm.mutex_id);
    }
}

static void
comm_record_drop(void)
{
    if (true == mtk_bridge_lock_mutex(g_comm.mutex_id, true))
    {
        g_comm.status.dropped_count++;
        mtk_bridge_unlock_mutex(g_comm.mutex_id);
    }
}

static void
comm_expire_events(uint32_t now_ms)
{
    if (true == mtk_bridge_lock_mutex(g_comm.mutex_id, false))
    {
        while ((0U < g_comm.event_count) &&
               (COMM_EVENT_MAX_AGE_MS <
                (now_ms - g_comm.events[g_comm.event_head].captured_ms)))
        {
            g_comm.event_head =
                (g_comm.event_head + 1U) % COMM_EVENT_CAPACITY;
            g_comm.event_count--;
            g_comm.status.dropped_count++;
        }
        mtk_bridge_unlock_mutex(g_comm.mutex_id);
    }
}

static void
comm_fail(uint32_t now_ms, uint32_t error_code)
{
    if (NULL != g_comm.p_mqtt_client)
    {
        mqtt_disconnect(g_comm.p_mqtt_client);
    }
    g_comm.b_is_mqtt_ready = false;
    g_comm.b_has_failed = false;
    g_comm.b_is_publish_busy = false;
    g_comm.b_is_publish_done = false;
    g_comm.subscribe_count = 0U;
    g_comm.next_retry_ms = now_ms + g_comm.retry_delay_ms;
    if (30000U > g_comm.retry_delay_ms)
    {
        g_comm.retry_delay_ms *= 2U;
        if (30000U < g_comm.retry_delay_ms)
        {
            g_comm.retry_delay_ms = 30000U;
        }
    }
    if (true == mtk_bridge_lock_mutex(g_comm.mutex_id, true))
    {
        g_comm.status.reconnect_count++;
        g_comm.status.last_error = error_code;
        mtk_bridge_unlock_mutex(g_comm.mutex_id);
    }
    comm_set_state(COMM_STATE_RETRYING);
}

static void
comm_on_connection(mqtt_client_t *p_client, void *p_arg,
                   mqtt_connection_status_t status)
{
    (void)p_client;
    (void)p_arg;

    if (MQTT_CONNECT_ACCEPTED == status)
    {
        g_comm.b_is_mqtt_ready = true;
    }
    else
    {
        g_comm.b_has_failed = true;
    }
}

static void
comm_on_subscription(void *p_arg, err_t error)
{
    (void)p_arg;

    if (ERR_OK == error)
    {
        g_comm.subscribe_count++;
    }
    else
    {
        g_comm.b_has_failed = true;
    }
}

static void
comm_on_publish(void *p_arg, err_t error)
{
    (void)p_arg;
    g_comm.b_is_publish_success = (ERR_OK == error);
    g_comm.b_is_publish_done = true;
}

static void
comm_on_incoming_publish(void *p_arg, char const *p_topic,
                         u32_t length)
{
    char const *p_name = NULL;

    (void)p_arg;
    g_comm.b_is_inbound_valid = false;

    if ((false == g_comm.b_has_command) &&
        (false == g_comm.b_has_result))
    {
        g_comm.command.kind = COMMAND_INVALID;
        g_comm.command.period_ms = 0U;
        g_comm.inbound_topic[0] = '\0';

        if ((NULL != p_topic) &&
            (0 == strcmp(p_topic, g_comm.topic_get_status)))
        {
            p_name = "get_status";
        }
        else if ((NULL != p_topic) &&
                 (0 == strcmp(p_topic, g_comm.topic_set_period)))
        {
            p_name = "set_period_ms";
        }
        else
        {
            /* Other topics are not command inputs. */
        }

        if (NULL != p_name)
        {
            (void)snprintf(g_comm.inbound_topic,
                           sizeof(g_comm.inbound_topic), "%s", p_name);
            g_comm.b_is_inbound_valid = command_frame_begin(
                &g_comm.inbound_frame, (size_t)length);
        }
    }
}

static void
comm_on_incoming_data(void *p_arg, u8_t const *p_data,
                      u16_t length, u8_t flags)
{
    (void)p_arg;

    if (true == g_comm.b_is_inbound_valid)
    {
        g_comm.b_is_inbound_valid = command_frame_append(
            &g_comm.inbound_frame, p_data, (size_t)length,
            (0U != (flags & MQTT_DATA_FLAG_LAST)));
    }

    if (0U != (flags & MQTT_DATA_FLAG_LAST))
    {
        if (true == g_comm.b_is_inbound_valid)
        {
            g_comm.b_is_command_accepted = command_parse(
                g_comm.inbound_topic, g_comm.inbound_frame.payload,
                g_comm.inbound_frame.length, &g_comm.command);
            g_comm.b_has_command = true;
        }
        else
        {
            if ((false == g_comm.b_has_command) &&
                (false == g_comm.b_has_result))
            {
                g_comm.b_is_command_accepted = false;
                g_comm.b_has_command = true;
            }
            comm_record_drop();
        }
        g_comm.b_is_inbound_valid = false;
    }
}

static bool
comm_publish(char const *p_topic, char const *p_payload,
             uint8_t qos, uint8_t retain, comm_publish_kind_t kind)
{
    bool b_is_valid = false;
    err_t error = ERR_ARG;

    if ((false == g_comm.b_is_publish_busy) &&
        (NULL != g_comm.p_mqtt_client))
    {
        error = mqtt_publish(g_comm.p_mqtt_client, p_topic, p_payload,
                             (u16_t)strlen(p_payload), qos, retain,
                             (0U == qos) ? NULL : comm_on_publish, NULL);
        if (ERR_OK == error)
        {
            b_is_valid = true;
            if (0U != qos)
            {
                g_comm.b_is_publish_busy = true;
                g_comm.b_is_publish_done = false;
                g_comm.publish_kind = kind;
            }
        }
    }

    return (b_is_valid);
}

static void
comm_publish_complete(uint32_t now_ms)
{
    if ((true == g_comm.b_is_publish_busy) &&
        (true == g_comm.b_is_publish_done))
    {
        if (true == g_comm.b_is_publish_success)
        {
            if (COMM_PUBLISH_EVENT == g_comm.publish_kind)
            {
                if (true == mtk_bridge_lock_mutex(g_comm.mutex_id, true))
                {
                    g_comm.event_head =
                        (g_comm.event_head + 1U) % COMM_EVENT_CAPACITY;
                    g_comm.event_count--;
                    mtk_bridge_unlock_mutex(g_comm.mutex_id);
                }
            }
            else if (COMM_PUBLISH_RESULT == g_comm.publish_kind)
            {
                g_comm.b_has_result = false;
            }
            else if (COMM_PUBLISH_STATUS == g_comm.publish_kind)
            {
                g_comm.last_status_ms = now_ms;
            }
            else
            {
                /* No persistent state for this publication. */
            }
        }
        else
        {
            comm_fail(now_ms, 4U);
        }
        g_comm.b_is_publish_busy = false;
        g_comm.b_is_publish_done = false;
        g_comm.publish_kind = COMM_PUBLISH_NONE;
    }
}

static void
comm_start_wifi(uint32_t now_ms)
{
    if (0 == cyw43_arch_wifi_connect_async(
                 g_comm.config.ssid, g_comm.config.wifi_password,
                 CYW43_AUTH_WPA2_AES_PSK))
    {
        g_comm.deadline_ms = now_ms + COMM_WIFI_DEADLINE_MS;
        comm_set_state(COMM_STATE_WIFI_CONNECTING);
    }
    else
    {
        comm_fail(now_ms, 2U);
    }
}

static void
comm_start_mqtt(uint32_t now_ms)
{
    err_t error = ERR_ARG;

    if (NULL == g_comm.p_mqtt_client)
    {
        g_comm.p_mqtt_client = mqtt_client_new();
    }
    if ((NULL != g_comm.p_mqtt_client) &&
        (0 != ipaddr_aton(g_comm.config.broker_ipv4,
                          &g_comm.broker_address)))
    {
        g_comm.mqtt_info.client_id = g_comm.client_id;
        g_comm.mqtt_info.client_user = g_comm.config.broker_user;
        g_comm.mqtt_info.client_pass = g_comm.config.broker_password;
        g_comm.mqtt_info.keep_alive = COMM_KEEPALIVE_SECONDS;
        g_comm.mqtt_info.will_topic = g_comm.topic_status;
        g_comm.mqtt_info.will_msg = "offline";
        g_comm.mqtt_info.will_msg_len = 0U;
        g_comm.mqtt_info.will_qos = 1U;
        g_comm.mqtt_info.will_retain = 1U;
        mqtt_set_inpub_callback(g_comm.p_mqtt_client,
                                comm_on_incoming_publish,
                                comm_on_incoming_data, NULL);
        error = mqtt_client_connect(g_comm.p_mqtt_client,
                                    &g_comm.broker_address,
                                    COMM_MQTT_PORT,
                                    comm_on_connection, NULL,
                                    &g_comm.mqtt_info);
    }

    if (ERR_OK == error)
    {
        g_comm.deadline_ms = now_ms + COMM_MQTT_DEADLINE_MS;
        comm_set_state(COMM_STATE_MQTT_CONNECTING);
    }
    else
    {
        comm_fail(now_ms, 3U);
    }
}

static void
comm_start_subscriptions(uint32_t now_ms)
{
    err_t first_error = ERR_ARG;
    err_t second_error = ERR_ARG;

    g_comm.subscribe_count = 0U;
    first_error = mqtt_subscribe(g_comm.p_mqtt_client,
                                 g_comm.topic_get_status, 1U,
                                 comm_on_subscription, NULL);
    if (ERR_OK == first_error)
    {
        second_error = mqtt_subscribe(g_comm.p_mqtt_client,
                                      g_comm.topic_set_period, 1U,
                                      comm_on_subscription, NULL);
    }

    if (ERR_OK == second_error)
    {
        g_comm.deadline_ms = now_ms + COMM_MQTT_DEADLINE_MS;
        comm_set_state(COMM_STATE_SUBSCRIBING);
    }
    else
    {
        comm_fail(now_ms, 5U);
    }
}

static void
comm_process_command(void)
{
    if (true == g_comm.b_has_command)
    {
        if (true == g_comm.b_is_command_accepted)
        {
            if (COMMAND_SET_PERIOD == g_comm.command.kind)
            {
                if (true == mtk_bridge_lock_mutex(g_comm.mutex_id, true))
                {
                    g_comm.status.telemetry_period_ms =
                        g_comm.command.period_ms;
                    mtk_bridge_unlock_mutex(g_comm.mutex_id);
                }
            }
            else if (COMMAND_GET_STATUS == g_comm.command.kind)
            {
                g_comm.last_status_ms = 0U;
            }
            else
            {
                /* The parser has already rejected other commands. */
            }
        }
        g_comm.pending_period_ms = g_comm.command.period_ms;
        (void)snprintf(g_comm.pending_command,
                       sizeof(g_comm.pending_command), "%s",
                       g_comm.inbound_topic);
        g_comm.b_has_result = true;
        g_comm.b_has_command = false;
    }
}

static void
comm_publish_status(uint32_t now_ms)
{
    comm_status_t status = {0};
    int length = 0;

    if ((COMM_OK == comm_get_status(&status)) &&
        ((0U == g_comm.last_status_ms) ||
         (COMM_HEARTBEAT_MS <= (now_ms - g_comm.last_status_ms))))
    {
        length = snprintf(
            g_comm.output, sizeof(g_comm.output),
            "{\"schema\":1,\"state\":\"online\",\"boot_id\":%lu,"
            "\"uptime_ms\":%lu,\"reconnects\":%lu,\"dropped\":%lu,"
            "\"last_error\":%lu,\"period_ms\":%lu,"
            "\"queue_peak\":%lu,\"task_ticks\":%lu}",
            (unsigned long)status.boot_id, (unsigned long)now_ms,
            (unsigned long)status.reconnect_count,
            (unsigned long)status.dropped_count,
            (unsigned long)status.last_error,
            (unsigned long)status.telemetry_period_ms,
            (unsigned long)status.queue_peak,
            (unsigned long)status.task_ticks);
        if ((0 < length) && ((size_t)length < sizeof(g_comm.output)))
        {
            (void)comm_publish(g_comm.topic_status, g_comm.output,
                               1U, 1U, COMM_PUBLISH_STATUS);
        }
    }
}

static void
comm_publish_result(void)
{
    int length = 0;

    if (true == g_comm.b_has_result)
    {
        length = snprintf(
            g_comm.output, sizeof(g_comm.output),
            "{\"accepted\":%s,\"command\":\"%s\","
            "\"period_ms\":%lu}",
            (true == g_comm.b_is_command_accepted) ? "true" : "false",
            g_comm.pending_command,
            (unsigned long)g_comm.pending_period_ms);
        if ((0 < length) && ((size_t)length < sizeof(g_comm.output)))
        {
            (void)comm_publish(g_comm.topic_result, g_comm.output,
                               1U, 0U, COMM_PUBLISH_RESULT);
        }
    }
}

static void
comm_publish_event(void)
{
    telemetry_event_t event = {0};
    bool b_has_event = false;

    if (true == mtk_bridge_lock_mutex(g_comm.mutex_id, true))
    {
        b_has_event = (0U < g_comm.event_count);
        if (true == b_has_event)
        {
            event = g_comm.events[g_comm.event_head];
        }
        mtk_bridge_unlock_mutex(g_comm.mutex_id);
    }

    if ((true == b_has_event) &&
        (true == telemetry_format_event(
                     &event, g_comm.status.boot_id,
                     ++g_comm.sequence, g_comm.output,
                     sizeof(g_comm.output))))
    {
        (void)comm_publish(g_comm.topic_event, g_comm.output,
                           1U, 0U, COMM_PUBLISH_EVENT);
    }
}

static void
comm_publish_telemetry(uint32_t now_ms)
{
    telemetry_snapshot_t snapshot = {0};
    uint32_t period_ms = COMMAND_PERIOD_DEFAULT_MS;
    bool b_has_snapshot = false;

    if (true == mtk_bridge_lock_mutex(g_comm.mutex_id, false))
    {
        snapshot = g_comm.snapshot;
        period_ms = g_comm.status.telemetry_period_ms;
        b_has_snapshot = true;
        mtk_bridge_unlock_mutex(g_comm.mutex_id);
    }

    if ((true == b_has_snapshot) &&
        (period_ms <= (now_ms - g_comm.last_telemetry_ms)))
    {
        if (true == telemetry_format_snapshot(
                        &snapshot, g_comm.status.boot_id,
                        ++g_comm.sequence, now_ms,
                        g_comm.output, sizeof(g_comm.output)))
        {
            if (true == comm_publish(g_comm.topic_telemetry,
                                     g_comm.output, 0U, 0U,
                                     COMM_PUBLISH_NONE))
            {
                g_comm.last_telemetry_ms = now_ms;
            }
        }
    }
}

static void
comm_step(uint32_t now_ms)
{
    int link_status = cyw43_tcpip_link_status(&cyw43_state,
                                               CYW43_ITF_STA);

    if ((COMM_STATE_RETRYING != g_comm.state) &&
        (COMM_STATE_WIFI_CONNECTING != g_comm.state) &&
        (CYW43_LINK_UP != link_status))
    {
        comm_fail(now_ms, 6U);
    }
    else if (true == g_comm.b_has_failed)
    {
        comm_fail(now_ms, 7U);
    }
    else
    {
        switch (g_comm.state)
        {
        case COMM_STATE_WIFI_CONNECTING:
            if (CYW43_LINK_UP == link_status)
            {
                comm_start_mqtt(now_ms);
            }
            else if (now_ms >= g_comm.deadline_ms)
            {
                comm_fail(now_ms, 2U);
            }
            else
            {
                /* WiFi association and DHCP are still in progress. */
            }
            break;
        case COMM_STATE_MQTT_CONNECTING:
            if (true == g_comm.b_is_mqtt_ready)
            {
                comm_start_subscriptions(now_ms);
            }
            else if (now_ms >= g_comm.deadline_ms)
            {
                comm_fail(now_ms, 3U);
            }
            else
            {
                /* Wait for the MQTT connection callback. */
            }
            break;
        case COMM_STATE_SUBSCRIBING:
            if (2U == g_comm.subscribe_count)
            {
                g_comm.online_since_ms = now_ms;
                g_comm.last_status_ms = 0U;
                g_comm.last_telemetry_ms = now_ms;
                comm_set_state(COMM_STATE_ONLINE);
            }
            else if (now_ms >= g_comm.deadline_ms)
            {
                comm_fail(now_ms, 5U);
            }
            else
            {
                /* Wait for both subscription acknowledgements. */
            }
            break;
        case COMM_STATE_ONLINE:
            if (0U == mqtt_client_is_connected(g_comm.p_mqtt_client))
            {
                comm_fail(now_ms, 8U);
            }
            else
            {
                comm_publish_complete(now_ms);
                comm_process_command();
                if (false == g_comm.b_is_publish_busy)
                {
                    comm_publish_status(now_ms);
                }
                if ((false == g_comm.b_is_publish_busy) &&
                    (true == g_comm.b_has_result))
                {
                    comm_publish_result();
                }
                if (false == g_comm.b_is_publish_busy)
                {
                    comm_publish_event();
                }
                comm_publish_telemetry(now_ms);
                if (30000U <= (now_ms - g_comm.online_since_ms))
                {
                    g_comm.retry_delay_ms = 1000U;
                }
            }
            break;
        case COMM_STATE_RETRYING:
            if (now_ms >= g_comm.next_retry_ms)
            {
                if (CYW43_LINK_UP == link_status)
                {
                    comm_start_mqtt(now_ms);
                }
                else
                {
                    comm_start_wifi(now_ms);
                }
            }
            break;
        default:
            comm_fail(now_ms, 9U);
            break;
        }
    }
}
