#include <lwip/init.h>
#include <lwip/dhcp.h>
#include <lwip/timeouts.h>
#include <netif/ethernet.h>
#include <lib/c/stdio.h>
#include <process/semaphore.h>
#include <process/thread.h>
#include <drivers/rtl8139.h>
#include <net/port/rtl8139_netif.h>

#include "net.h"

static struct netif rtl_netif;
static semaphore_t *net_sem;

void
net_wakeup(void)
{
	if (net_sem != NULL)
	{
		semaphore_up(net_sem);
	}
}

static void
status_changed(struct netif *netif)
{
	kprintf("IP address: %s\n", ip4addr_ntoa(netif_ip4_addr(netif)));
	kprintf(" mask %s\n", ip4addr_ntoa(netif_ip4_netmask(netif)));
	kprintf(" gw %s\n", ip4addr_ntoa(netif_ip4_gw(netif)));
}

static void
net_thread(uint32_t arg)
{
	(void)arg;

	for (;;)
	{
		semaphore_down(net_sem);
		rtl8139_netif_poll();
		sys_check_timeouts();
	}
}

void
net_setup(void)
{
	lwip_init();

	if (netif_add(&rtl_netif, NULL, NULL, NULL, NULL,
		rtl8139_netif_init, ethernet_input) == NULL)
	{
		return;
	}

	netif_set_status_callback(&rtl_netif, status_changed);
	netif_set_default(&rtl_netif);
	netif_set_up(&rtl_netif);
	dhcp_start(&rtl_netif);

	net_sem = semaphore_create(0);
	rtl8139_set_rx_notify(net_wakeup);
	thread_kernel_create("net", net_thread, NULL);
}
