#pragma once

#define NO_SYS                     1
#define SYS_LIGHTWEIGHT_PROT       0
#define LWIP_SOCKET                0
#define LWIP_NETCONN               0

#define MEM_ALIGNMENT              4
#define MEM_SIZE                   (64 * 1024)
#define PBUF_POOL_SIZE             32

#define LWIP_IPV4                  1
#define LWIP_IPV6                  0
#define LWIP_ARP                   1
#define LWIP_ETHERNET              1
#define LWIP_ICMP                  1
#define LWIP_UDP                   1
#define LWIP_TCP                   1
#define LWIP_DHCP                  1
#define LWIP_DHCP_DOES_ACD_CHECK   0
#define LWIP_NETIF_STATUS_CALLBACK 1
#define LWIP_STATS                 0
