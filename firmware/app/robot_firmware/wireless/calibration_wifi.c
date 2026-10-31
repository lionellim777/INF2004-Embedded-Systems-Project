/* Diagnostic HTTP transport; all callbacks execute in foreground poll mode.
 * No network callback can touch motors, start a move, or wait for a client.
 */
#include "calibration_wifi.h"
#include "calibration_core.h"
#ifdef ROBOCAR_STATIONARY_BARCODE
#include "stationary_page.h"
#define CAL_RESULT_TITLE "v5.7 stationary barcode / motors OFF / black>250 white<=220"
#else
#include "calibration_page.h"
#define CAL_RESULT_TITLE "v5.34 wireless / independent optical course capture / course test available / diagnostics retained / filtered black>=226 white<=220 mean12 2kHz / GP28=center GP27=right GP26=barcode / follow600cps bounded-search"
#endif
#include "dhcpserver.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/stdio/driver.h"
#include "pico/rand.h"
#include "lwip/tcp.h"
#include <stdio.h>
#include <string.h>

#define AP_NAME "AAI10-Calibrate"
#define AP_PASSWORD "AAI10-drive"
#define CLIENT_COUNT 3u
#define REQUEST_CAPACITY 1536u
#define BODY_CAPACITY 7168u
#define CLIENT_TIMEOUT_US 5000000u

typedef struct {
    struct tcp_pcb *pcb;
    uint64_t opened_us;
    size_t request_length;
    char request[REQUEST_CAPACITY];
    char header[512];
    size_t header_length;
    char data[BODY_CAPACITY];
    const char *body;
    size_t body_length;
    size_t queued;
    size_t acknowledged;
    bool responding;
} client_t;
static client_t clients[CLIENT_COUNT];
static calibration_core_t core;
static dhcp_server_t dhcp;
static bool ready;
static uint32_t boot_id;
static uint32_t max_poll_us;

static void capture(const char *data, int length)
{
    if (length > 0) { calibration_capture(&core, data, (size_t)length); }
}
static stdio_driver_t log_driver = {.out_chars = capture};

void calibration_wifi_capture_init(void)
{
    stdio_set_driver_enabled(&log_driver, true);
}
void calibration_wifi_help(void)
{
    printf("[WIFI] %s | network=%s password=%s | http://192.168.4.1\n",
           ready ? "READY" : "OFFLINE (USB still available)", AP_NAME, AP_PASSWORD);
    printf("[WIFI] Browser selects only; GP20 starts/aborts. Network loss does not restart motion.\n");
}

/* After tcp_abort, callbacks must return ERR_ABRT. Error callbacks get a freed
 * PCB and only release our static slot. Never close that PCB again. */
static err_t close_client(client_t *c)
{
    struct tcp_pcb *pcb = c->pcb;
    tcp_arg(pcb, NULL);
    tcp_sent(pcb, NULL);
    tcp_recv(pcb, NULL);
    tcp_err(pcb, NULL);
    tcp_poll(pcb, NULL, 0);
    c->pcb = NULL;
    if (tcp_close(pcb) != ERR_OK) { tcp_abort(pcb); return ERR_ABRT; }
    return ERR_OK;
}
static void client_error(void *arg, err_t err)
{
    (void)err;
    ((client_t *)arg)->pcb = NULL;
}
static err_t send_chunk(client_t *c)
{
    if (!c->responding) { return ERR_OK; }
    size_t total = c->header_length + c->body_length;
    if (c->acknowledged == total) { return close_client(c); }
    if (c->queued == total) { return ERR_OK; }
    const char *source;
    size_t n;
    if (c->queued < c->header_length) {
        source = c->header + c->queued;
        n = c->header_length - c->queued;
    } else {
        source = c->body + c->queued - c->header_length;
        n = total - c->queued;
    }
    if (n > 1024u) { n = 1024u; }
    if (n > tcp_sndbuf(c->pcb)) { n = tcp_sndbuf(c->pcb); }
    if (n != 0u) {
        err_t err = tcp_write(c->pcb, source, (u16_t)n, TCP_WRITE_FLAG_COPY);
        if (err == ERR_OK) { c->queued += n; (void)tcp_output(c->pcb); }
        else if (err != ERR_MEM) { return close_client(c); }
    }
    return ERR_OK;
}
static err_t client_sent(void *arg, struct tcp_pcb *pcb, u16_t length)
{
    (void)pcb;
    client_t *c = arg;
    c->acknowledged += length;
    return send_chunk(c);
}
static err_t client_poll(void *arg, struct tcp_pcb *pcb)
{
    (void)pcb;
    client_t *c = arg;
    if (time_us_64() - c->opened_us > CLIENT_TIMEOUT_US) { return close_client(c); }
    return send_chunk(c);
}

static void response(client_t *c, const char *status, const char *type,
                     const char *body, size_t length, uint32_t next, bool gap)
{
    c->body = body;
    c->body_length = length;
    int n = snprintf(c->header, sizeof(c->header),
        "HTTP/1.1 %s\r\nContent-Type: %s; charset=utf-8\r\nContent-Length: %u\r\n"
        "Connection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n"
        "X-Boot: %08lx\r\nX-Next: %lu\r\nX-Gap: %u\r\nX-Idle: %u\r\n\r\n",
        status, type, (unsigned)length, (unsigned long)boot_id, (unsigned long)next,
        gap ? 1u : 0u, core.selection_allowed ? 1u : 0u);
    c->header_length = (size_t)n;
    c->responding = true;
}
static void plain(client_t *c, const char *status, const char *body)
{
    response(c, status, "text/plain", body, strlen(body), 0u, false);
}
static bool control_header(const char *request)
{
    /* Match a complete header, case-insensitively. Custom header prevents a
     * foreign web page from issuing a simple cross-origin POST. No CORS. */
    const char *line = strstr(request, "\r\n");
    while (line != NULL) {
        line += 2;
        const char *end = strstr(line, "\r\n");
        if (end == NULL || end == line) { break; }
        const char expected[] = "x-calibrate: 1";
        if ((size_t)(end - line) == sizeof(expected) - 1u) {
            bool equal = true;
            for (size_t i = 0; i < sizeof(expected) - 1u; ++i) {
                char ch = line[i];
                if (ch >= 'A' && ch <= 'Z') { ch += 'a' - 'A'; }
                if (ch != expected[i]) { equal = false; }
            }
            if (equal) { return true; }
        }
        line = end;
    }
    return false;
}
static void handle_request(client_t *c)
{
    char method[8], path[96], protocol[16], extra;
    char *end = strstr(c->request, "\r\n");
    if (end == NULL) { plain(c, "400 Bad Request", "Malformed request.\n"); return; }
    *end = '\0';
    int fields = sscanf(c->request, "%7s %95s %15s %c", method, path, protocol, &extra);
    *end = '\r';
    if (fields != 3 || (strcmp(protocol, "HTTP/1.1") != 0 && strcmp(protocol, "HTTP/1.0") != 0)) {
        plain(c, "400 Bad Request", "Malformed request.\n"); return;
    }
    if (strcmp(method, "GET") == 0 && strcmp(path, "/") == 0) {
        response(c, "200 OK", "text/html", calibration_page, sizeof(calibration_page) - 1u, 0u, false);
    } else if (strcmp(method, "GET") == 0 && strncmp(path, "/logs?since=", 12u) == 0) {
        uint32_t cursor, next;
        bool gap;
        if (!calibration_parse_cursor(path + 12u, &cursor)) {
            plain(c, "400 Bad Request", "Invalid cursor.\n"); return;
        }
        size_t n = calibration_read(&core, cursor, c->data, sizeof(c->data), &next, &gap);
        response(c, "200 OK", "text/plain", c->data, n, next, gap);
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/result") == 0) {
        int n = snprintf(c->data, sizeof(c->data), CAL_RESULT_TITLE " / boot=%08lx\n%s%s%s%s%s%s%s%s%s",
                         (unsigned long)boot_id, core.stop, core.settled, core.summary, core.timing, core.barcode, core.trace[0], core.trace[1], core.course_result, core.optical_result);
        if (core.stop[0] == '\0' && core.settled[0] == '\0' && core.optical_result[0] == '\0') {
            n += snprintf(c->data + n, sizeof(c->data) - (size_t)n,
                          "No completed/aborted test result retained in this boot yet.\n");
        }
        response(c, "200 OK", "text/plain", c->data, (size_t)n, 0u, false);
    } else if (strcmp(method,"POST")==0 && strncmp(path,"/mission/",9u)==0) {
        if(!control_header(c->request)){plain(c,"403 Forbidden","Use the calibration page.\n");return;}
#ifdef ROBOCAR_STATIONARY_BARCODE
        plain(c,"409 Conflict","Stationary build: course driving unavailable.\n");
#else
        if(calibration_course_command(&core,path))plain(c,"202 Accepted","Barcode mission queued. Wait for [SELECT Y/Z], then GP20.\n");
        else plain(c,"409 Conflict","Course selection rejected: stop first; check settings and pending request.\n");
#endif
    } else if (strcmp(method, "POST") == 0 && strncmp(path, "/command/", 9u) == 0 && strlen(path) == 10u) {
        if (!control_header(c->request)) { plain(c, "403 Forbidden", "Use the calibration page.\n"); return; }
        if (calibration_command(&core, path[9])) {
            plain(c, "202 Accepted", path[9] == 'x' ? "Stop queued; check [STOP] in the log. GP20 aborts locally.\n" :
                  path[9]=='v' ? "Optical capture queued: motors stay off. Wait for [OPTICAL START], then slide paper for 5 seconds.\n" :
                  "Selection queued. Wait for [SELECT] in the log, then press GP20 on the car.\n");
        } else {
            plain(c, "409 Conflict", "Selection rejected: running, settling, another request pending, or invalid command.\n");
        }
    } else {
        plain(c, "404 Not Found", "Open http://192.168.4.1/\n");
    }
}
static err_t client_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    client_t *c = arg;
    if (p == NULL) { return close_client(c); }
    if (err != ERR_OK) { pbuf_free(p); return close_client(c); }
    size_t n = p->tot_len;
    tcp_recved(pcb, p->tot_len);
    if (!c->responding) {
        if (n >= sizeof(c->request) - c->request_length) {
            plain(c, "431 Request Header Fields Too Large", "Request too large.\n");
        } else {
            pbuf_copy_partial(p, c->request + c->request_length, (u16_t)n, 0u);
            c->request_length += n;
            c->request[c->request_length] = '\0';
            if (memchr(c->request, '\0', c->request_length) != NULL) {
                plain(c, "400 Bad Request", "Invalid request.\n");
            } else if (strstr(c->request, "\r\n\r\n") != NULL) { handle_request(c); }
        }
    }
    pbuf_free(p);
    return send_chunk(c);
}
static err_t accept_client(void *arg, struct tcp_pcb *pcb, err_t err)
{
    (void)arg;
    if (err != ERR_OK || pcb == NULL) { return ERR_VAL; }
    for (size_t i = 0; i < CLIENT_COUNT; ++i) {
        if (clients[i].pcb == NULL) {
            client_t *c = &clients[i];
            memset(c, 0, sizeof(*c));
            c->pcb = pcb;
            c->opened_us = time_us_64();
            tcp_arg(pcb, c);
            tcp_recv(pcb, client_recv);
            tcp_sent(pcb, client_sent);
            tcp_err(pcb, client_error);
            tcp_poll(pcb, client_poll, 1u);
            tcp_nagle_disable(pcb);
            return ERR_OK;
        }
    }
    tcp_abort(pcb);
    return ERR_ABRT;
}
bool calibration_wifi_init(void)
{
    boot_id = get_rand_32();
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_SINGAPORE) != 0) { return false; }
    cyw43_arch_enable_ap_mode(AP_NAME, AP_PASSWORD, CYW43_AUTH_WPA2_AES_PSK);
    ip_addr_t gateway, mask;
    IP4_ADDR(&gateway, 192, 168, 4, 1);
    IP4_ADDR(&mask, 255, 255, 255, 0);
    dhcp_server_init(&dhcp, &gateway, &mask);
    struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_V4);
    if (pcb != NULL && dhcp.udp != NULL && tcp_bind(pcb, IP_ANY_TYPE, 80u) == ERR_OK) {
        struct tcp_pcb *listener = tcp_listen_with_backlog(pcb, CLIENT_COUNT);
        if (listener != NULL) {
            tcp_accept(listener, accept_client);
            core.selection_allowed = true;
            ready = true;
            return true;
        }
    }
    if (pcb != NULL) { tcp_abort(pcb); }
    dhcp_server_deinit(&dhcp);
    cyw43_arch_disable_ap_mode();
    cyw43_arch_deinit();
    return false;
}
void calibration_wifi_poll(bool selection_allowed)
{
    if (!ready) { return; }
    core.selection_allowed = selection_allowed;
    uint64_t start = time_us_64();
    cyw43_arch_poll();
    /* One bounded chunk per connection per application iteration. */
    for (size_t i = 0; i < CLIENT_COUNT; ++i) {
        if (clients[i].pcb != NULL) {
            if (time_us_64() - clients[i].opened_us > CLIENT_TIMEOUT_US) { (void)close_client(&clients[i]); }
            else { (void)send_chunk(&clients[i]); }
        }
    }
    uint32_t elapsed = (uint32_t)(time_us_64() - start);
    if (elapsed > max_poll_us) { max_poll_us = elapsed; }
}
int calibration_wifi_getchar(void) { return calibration_take_command(&core); }
uint32_t calibration_wifi_max_poll_us(void) { return max_poll_us; }

void calibration_wifi_reset_timing(void) { max_poll_us = 0u; }

course_config_t calibration_wifi_course_config(void) { return core.course; }
