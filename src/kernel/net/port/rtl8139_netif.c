#include <lwip/etharp.h>
#include <lib/c/string.h>
#include <drivers/rtl8139.h>

#include "rtl8139_netif.h"

static struct netif *rtl_netif;

static void
input(const uint8_t *frame, size_t length)
{
	struct pbuf *p = pbuf_alloc(PBUF_RAW, (u16_t)length, PBUF_POOL);

	if (p == NULL)
	{
		return;
	}

	pbuf_take(p, frame, (u16_t)length);

	if (rtl_netif->input(p, rtl_netif) != ERR_OK)
	{
		pbuf_free(p);
	}
}

static err_t
linkoutput(struct netif *netif, struct pbuf *p)
{
	static uint8_t frame[1514];
	u16_t length = pbuf_copy_partial(p, frame, sizeof(frame), 0);

	(void)netif;

	while (send_packet(frame, length) < 0)
	{
		asm volatile("pause");
	}

	return ERR_OK;
}

err_t
rtl8139_netif_init(struct netif *netif)
{
	if (rtl8139_device.tx_buffer == NULL)
	{
		return ERR_IF;
	}

	rtl_netif = netif;
	netif->name[0] = 'e';
	netif->name[1] = 'n';
	netif->hwaddr_len = sizeof(rtl8139_device.mac_addr);
	memcpy(netif->hwaddr, rtl8139_device.mac_addr, sizeof(rtl8139_device.mac_addr));
	netif->mtu = 1500;
	netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP |
		NETIF_FLAG_ETHERNET | NETIF_FLAG_LINK_UP;
	netif->output = etharp_output;
	netif->linkoutput = linkoutput;

	return ERR_OK;
}

void
rtl8139_netif_poll(void)
{
	rtl8139_receive(input);
}
