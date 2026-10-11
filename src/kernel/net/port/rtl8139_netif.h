#pragma once

#include <lwip/netif.h>

err_t rtl8139_netif_init(struct netif *netif);
void rtl8139_netif_poll(void);
