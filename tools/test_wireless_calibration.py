#!/usr/bin/env python3
"""Host tests of real calibration logic/HTTP callbacks with mocked radio/lwIP.
No claims about RF, DHCP interoperability or actual motor timing are made.
"""
from pathlib import Path
import os, subprocess
root=Path(__file__).resolve().parents[1]
build=root/'build/wireless-host-tests';build.mkdir(parents=True,exist_ok=True)
stub=build/'stubs';stub.mkdir(exist_ok=True)
(stub/'mock.h').write_text(r'''
#ifndef WIRELESS_TEST_MOCK_H
#define WIRELESS_TEST_MOCK_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
typedef int err_t; typedef uint16_t u16_t;
#define ERR_OK 0
#define ERR_MEM -1
#define ERR_VAL -6
#define ERR_ABRT -13
#define TCP_WRITE_FLAG_COPY 1
#define IPADDR_TYPE_V4 0
#define IP_ANY_TYPE NULL
#define CYW43_COUNTRY_SINGAPORE 0
#define CYW43_AUTH_WPA2_AES_PSK 0
typedef struct {uint32_t addr;} ip_addr_t;
#define IP4_ADDR(p,a,b,c,d) ((p)->addr=(a)|((b)<<8)|((c)<<16)|((uint32_t)(d)<<24))
#define ip_2_ip4(p) (p)
#define ip4_addr_get_u32(p) ((p)->addr)
#define ip_addr_copy(a,b) ((a)=(b))
struct tcp_pcb {int id;}; struct udp_pcb {int id;}; struct netif {int id;};
struct pbuf {u16_t tot_len;void *payload;};
typedef struct {void (*out_chars)(const char*,int);} stdio_driver_t;
uint64_t time_us_64(void); uint32_t get_rand_32(void);
void stdio_set_driver_enabled(stdio_driver_t*,bool);
int cyw43_arch_init_with_country(int);void cyw43_arch_enable_ap_mode(const char*,const char*,int);
void cyw43_arch_disable_ap_mode(void);void cyw43_arch_deinit(void);void cyw43_arch_poll(void);
void tcp_arg(struct tcp_pcb*,void*);
void tcp_sent(struct tcp_pcb*,err_t(*)(void*,struct tcp_pcb*,u16_t));
void tcp_recv(struct tcp_pcb*,err_t(*)(void*,struct tcp_pcb*,struct pbuf*,err_t));
void tcp_err(struct tcp_pcb*,void(*)(void*,err_t));
void tcp_poll(struct tcp_pcb*,err_t(*)(void*,struct tcp_pcb*),int);
err_t tcp_close(struct tcp_pcb*);void tcp_abort(struct tcp_pcb*);
u16_t tcp_sndbuf(struct tcp_pcb*);err_t tcp_write(struct tcp_pcb*,const void*,u16_t,int);
err_t tcp_output(struct tcp_pcb*);void tcp_recved(struct tcp_pcb*,u16_t);
void tcp_nagle_disable(struct tcp_pcb*);
struct tcp_pcb *tcp_new_ip_type(int);err_t tcp_bind(struct tcp_pcb*,const ip_addr_t*,u16_t);
struct tcp_pcb *tcp_listen_with_backlog(struct tcp_pcb*,int);
void tcp_accept(struct tcp_pcb*,err_t(*)(void*,struct tcp_pcb*,err_t));
u16_t pbuf_copy_partial(const struct pbuf*,void*,u16_t,u16_t);void pbuf_free(struct pbuf*);
typedef void (*udp_recv_fn)(void*,struct udp_pcb*,struct pbuf*,const ip_addr_t*,u16_t);
struct udp_pcb *udp_new(void);void udp_recv(struct udp_pcb*,udp_recv_fn,void*);
void udp_remove(struct udp_pcb*);err_t udp_bind(struct udp_pcb*,const ip_addr_t*,u16_t);
#define PBUF_TRANSPORT 0
#define PBUF_RAM 0
struct pbuf *pbuf_alloc(int,u16_t,int);
err_t udp_sendto_if(struct udp_pcb*,struct pbuf*,const ip_addr_t*,u16_t,struct netif*);
err_t udp_sendto(struct udp_pcb*,struct pbuf*,const ip_addr_t*,u16_t);
struct netif *ip_current_input_netif(void);uint32_t cyw43_hal_ticks_ms(void);
#endif
''')
for name in ['pico/cyw43_arch.h','pico/stdlib.h','pico/stdio/driver.h','pico/rand.h','lwip/tcp.h','lwip/ip_addr.h','lwip/udp.h','cyw43_config.h']:
    p=stub/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('#include "mock.h"\n')
env=dict(os.environ,SDKROOT='/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk')
app=root/'firmware/app/robot_firmware'
flags=['clang','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-g','-I'+str(stub),'-I'+str(app),'-I'+str(app/'wireless')]
for name in ['core','transport','stationary_transport']:
    test_name='transport' if name=='stationary_transport' else name
    defines=['-DROBOCAR_STATIONARY_BARCODE=1'] if name=='stationary_transport' else []
    command=flags+defines+['-DROBOCAR_WIRELESS_CALIBRATION=1',str(app/'wireless/calibration_core.c'),str(root/f'firmware/tests/wireless_calibration_{test_name}_test.c'),'-o',str(build/name)]
    subprocess.run(command,check=True,env=env);subprocess.run([str(build/name)],check=True,env=env)
