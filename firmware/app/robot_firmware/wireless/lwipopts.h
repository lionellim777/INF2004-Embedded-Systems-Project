#ifndef CALIBRATION_LWIPOPTS_H
#define CALIBRATION_LWIPOPTS_H
#define NO_SYS 1
#define LWIP_SOCKET 0
#define LWIP_NETCONN 0
#define MEM_ALIGNMENT 4
#define MEM_SIZE 16384
#define MEMP_NUM_TCP_PCB 8
#define MEMP_NUM_TCP_PCB_LISTEN 1
#define MEMP_NUM_TCP_SEG 32
#define PBUF_POOL_SIZE 16
#define LWIP_ARP 1
#define LWIP_ETHERNET 1
#define LWIP_ICMP 1
#define LWIP_RAW 0
#ifdef ROBOCAR_WIFI_STATION
/* Station builds join an existing network and need lwIP's DHCP client. */
#define LWIP_DHCP 1
#else
#define LWIP_DHCP 0
#endif
/* We run a DHCP SERVER, not lwIP's DHCP client. Without this hook, lwIP's
 * IPv4 input rejects source 0.0.0.0 as a broadcast source before our UDP/67
 * handler sees a new client's DISCOVER/REQUEST. Argument is network order. */
#define LWIP_IP_ACCEPT_UDP_PORT(port) ((port) == PP_NTOHS(67))
#define LWIP_DNS 0
#define LWIP_IPV4 1
#define LWIP_IPV6 0
#define LWIP_TCP 1
#define LWIP_UDP 1
#define TCP_MSS 1460
#define TCP_WND (4 * TCP_MSS)
#define TCP_SND_BUF (4 * TCP_MSS)
#define TCP_SND_QUEUELEN 32
/* Every page poll is a new connection, so a lost packet waits this long before
 * a resend. lwIP's 3000 ms default outlasts the page's poll timeout; 1000 ms
 * is the RFC 6298 initial value. */
#define LWIP_TCP_RTO_TIME 1000
#define LWIP_NETIF_STATUS_CALLBACK 1
#define LWIP_NETIF_LINK_CALLBACK 1
#define LWIP_NETIF_HOSTNAME 1
#define LWIP_NETIF_TX_SINGLE_PBUF 1
#define LWIP_CHKSUM_ALGORITHM 3
#define LWIP_STATS 0
#define LWIP_PROVIDE_ERRNO 1
#endif
