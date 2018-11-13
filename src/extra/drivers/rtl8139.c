/*
 * Copyright (c) 2018 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <arch/x86/io-ports.h>
#include <lib/c/stdlib.h>
#include <lib/c/string.h>
#include <lib/c/stdio.h>
#include <lib/c/stdbool.h>
#include <arch/x86/irq.h>
#include <drivers/vbe.h>
#include "rtl8139.h"
#include "pci.h"


// https://wiki.osdev.org/RTL8139

#define CMD 0x37
#define IMR 0x3c // Interrupt Mask Register
#define ISR 0x3e // Interrupt Status Register

#define RCR 0x44 // Receive Config Register

#define CONFIG_1 0x52

#define RX_OK 0x01
#define RX_ERR 0x02
#define TX_OK 0x04
#define TX_ERR 0x08
#define TX_STATUS 0x10
#define TX_ADDRESS 0x20
#define TX_BUFFER_SIZE 1536
#define TX_HOST_OWNS 0x2000
#define TX_FIFO_THRESHOLD 256

#define RCR_AAP  (1 << 0) /* Accept All Packets */
#define RCR_APM  (1 << 1) /* Accept Physical Match Packets */
#define RCR_AM   (1 << 2) /* Accept Multicast Packets */
#define RCR_AB   (1 << 3) /* Accept Broadcast Packets */
#define RCR_WRAP (1 << 7) /* Wrap packets too long */

#define RX_BUF 0x30
#define RX_BUF_PTR 0x38
#define RX_BUF_ADDR 0x3a

#define RX_MISSED 0x4c

#define CMD_NOT_EMPTY 0x01

#define RX_BUFFER_LENGTH 8192 + 16 + 1500

#define RX_STATUS_OK 0x1
#define RX_BAD_ALIGN 0x2
#define RX_CRC_ERR 0x4
#define RX_TOO_LONG 0x8
#define RX_RUNT 0x10
#define RX_BAD_SYMBOL 0x20
#define RX_BROADCAST 0x2000
#define RX_PHYSICAL 0x4000
#define RX_MULTICAST 0x8000

#define NB_TX_DESCRIPTORS 4

#define ETH_MIN_LENGTH 60
#define ETH_FRAME_LEGTH 1514

pci_device_t  pci_rtl8139_device;
rtl8139_dev_t rtl8139_device;

static void
handle_rx(void)
{
	while ((in8(rtl8139_device.io_base + CMD) & CMD_NOT_EMPTY) == 0)
	{
		uint32_t offset = rtl8139_device.rx_buffer_idx % RX_BUFFER_LENGTH;

		uint32_t rx_status = *(uint32_t *)(rtl8139_device.rx_buffer + offset);
		uint32_t rx_size = rx_status >> 16;
		rx_status &= 0xFFFF;

		if ((rx_status & (RX_BAD_SYMBOL | RX_RUNT | RX_TOO_LONG |
			RX_CRC_ERR | RX_BAD_ALIGN)) ||
			(rx_size < ETH_MIN_LENGTH) ||
			(rx_size > ETH_FRAME_LEGTH))
		{
			printf("RTL8139 packet error.\n");
			return;
		}

		rxpacket_t *packet = malloc(sizeof(rxpacket_t) - 1 + rx_size - 4);
		if (packet == NULL)
			return;

		// Discard FCS
		packet->length = rx_size - 4;
		if (packet->length <= 0)
			return;

		if (offset + 4 + rx_size - 4 > RX_BUFFER_LENGTH)
		{
			uint32_t semi_count = RX_BUFFER_LENGTH - offset - 4;
			memcpy(packet->data, rtl8139_device.rx_buffer + offset + 4, semi_count);
			memcpy(packet->data + semi_count, rtl8139_device.rx_buffer, rx_size - 4 - semi_count);
		}
		else
		{
			memcpy(packet->data, rtl8139_device.rx_buffer + offset + 4, packet->length);
		}

		// Align on 4 bytes
		rtl8139_device.rx_buffer_idx = (rtl8139_device.rx_buffer_idx + rx_size + 4 + 3) & ~ 3;
		out16(rtl8139_device.io_base + RX_BUF_PTR, rtl8139_device.rx_buffer_idx - 0x10);

		free(packet);
	}
}

static void
handle_tx(void)
{
	for (int i = 0; i < NB_TX_DESCRIPTORS; i++)
	{
		// Read the status of every descriptor when a Tx interrupt occurs
		in32(rtl8139_device.io_base + TX_STATUS + (i * 4));
	}
}

static void
packet_handler(int number)
{
	(void)number;
	uint16_t status = in16(rtl8139_device.io_base + ISR);

	if (!status)
		return;

	// Acknowledge
	out16(rtl8139_device.io_base + ISR, status);

	if (status & (RX_OK | RX_ERR))
	{
		printf("Packet received.\n");
		handle_rx();
	}
	else if (status & (TX_OK | TX_ERR))
	{
		printf("Packet sent.\n");
		handle_tx();
	}
}

static void
read_mac_address(void)
{
	uint32_t mac_part1 = in32(rtl8139_device.io_base + 0x00);
	uint16_t mac_part2 = in16(rtl8139_device.io_base + 0x04);

	rtl8139_device.mac_addr[0] = mac_part1 >> 0;
	rtl8139_device.mac_addr[1] = mac_part1 >> 8;
	rtl8139_device.mac_addr[2] = mac_part1 >> 16;
	rtl8139_device.mac_addr[3] = mac_part1 >> 24;

	rtl8139_device.mac_addr[4] = mac_part2 >> 0;
	rtl8139_device.mac_addr[5] = mac_part2 >> 8;

	vbe_set_color(NORMAL_YELLOW);
	printf("MAC address: %x:%x:%x:%x:%x:%x\n",
			rtl8139_device.mac_addr[0],
			rtl8139_device.mac_addr[1],
			rtl8139_device.mac_addr[2],
			rtl8139_device.mac_addr[3],
			rtl8139_device.mac_addr[4],
			rtl8139_device.mac_addr[5]);
}

size_t
send_packet(const void *data, size_t length)
{
	uint32_t flags;

	X86_IRQs_DISABLE(flags);

	if (in32(rtl8139_device.io_base + TX_STATUS + (rtl8139_device.tx_buffer_idx * 4)) & TX_HOST_OWNS)
	{
		// A free buffer was found.

		// Copy data to TX buffer.
		memcpy(rtl8139_device.tx_buffer + (TX_BUFFER_SIZE * rtl8139_device.tx_buffer_idx),
				data, length);

		// Padding
		while (length < ETH_MIN_LENGTH)
		{
			(rtl8139_device.tx_buffer +
			 (TX_BUFFER_SIZE * rtl8139_device.tx_buffer_idx))[length++] = '\0';
		}

		// Move TX buffer's content to the internal transmission FIFO
		// and then to PCI bus.
		out32(rtl8139_device.io_base + TX_ADDRESS + rtl8139_device.tx_buffer_idx * 4,
				(uint32_t)(rtl8139_device.tx_buffer +
					(TX_BUFFER_SIZE * rtl8139_device.tx_buffer_idx)));

		out32(rtl8139_device.io_base + TX_STATUS + rtl8139_device.tx_buffer_idx * 4,
				((TX_FIFO_THRESHOLD << 11) & 0x003F0000) | length);

		// Select a new descriptor for future transmission.
		rtl8139_device.tx_buffer_idx = (rtl8139_device.tx_buffer_idx + 1) % NB_TX_DESCRIPTORS;

		X86_IRQs_ENABLE(flags);

		return length;

	}

	X86_IRQs_ENABLE(flags);

	// No available buffer!
	return -1;
}

void
rtl8139_setup(void)
{
	// Get RTL8139 PCI device
	pci_rtl8139_device = pci_device_lookup(RTL8139_VENDOR_ID,
			RTL8139_DEVICE_ID);

	if (pci_rtl8139_device.vendor_id == 0 && pci_rtl8139_device.device_id == 0)
		return;

	// 1. Enable PCI Bus Mastering (allows DMA)
	uint32_t cmd_register = pci_config_read_dword(pci_rtl8139_device.bus,
			pci_rtl8139_device.slot, pci_rtl8139_device.func, 0x4);
	cmd_register |= (1 << 2);
	pci_config_write_dword(pci_rtl8139_device.bus, pci_rtl8139_device.slot,
			pci_rtl8139_device.func, 0x4, cmd_register);

	// Set current TSAD
	rtl8139_device.tx_buffer_idx = 0;

	// 2. Get the I/O base address
	uint32_t io_base = pci_config_read_dword(pci_rtl8139_device.bus,
			pci_rtl8139_device.slot,
			pci_rtl8139_device.func, 0x10);

	rtl8139_device.io_base = io_base & (~0x3);

	// 3. Power on the device
	out8(rtl8139_device.io_base + CONFIG_1, 0x0);

	// 4. Software reset
	out8(rtl8139_device.io_base + CMD, 0x10);

	while ((in8(rtl8139_device.io_base + CMD) & 0x10) != 0)
	{
		/* Wait for RST to be done */
	}

	// 5. Init the receive buffer
	rtl8139_device.rx_buffer = malloc(RX_BUFFER_LENGTH);

	if (!rtl8139_device.rx_buffer)
	{
		return;
	}

	memset(rtl8139_device.rx_buffer, 0x0, RX_BUFFER_LENGTH);
	out32(rtl8139_device.io_base + RX_BUF, (uintptr_t)rtl8139_device.rx_buffer);
	out32(rtl8139_device.io_base + RX_BUF_PTR, 0);
	out32(rtl8139_device.io_base + RX_BUF_ADDR, 0);

	rtl8139_device.rx_buffer_idx = 0;

	// and Tx buffer DMA addresses
	rtl8139_device.tx_buffer = malloc(TX_BUFFER_SIZE * NB_TX_DESCRIPTORS);

	if (!rtl8139_device.tx_buffer)
	{
		return;
	}

	for (int i = 0; i < NB_TX_DESCRIPTORS; i++)
	{
		out32(rtl8139_device.io_base + TX_ADDRESS + (i * 4),
				(uint32_t)(rtl8139_device.tx_buffer + (TX_BUFFER_SIZE * i)));
	}

	// 6. Set IMR + ISR, enable some interrupts
	out16(rtl8139_device.io_base + IMR, RX_OK | TX_OK | TX_ERR);

	// 7. Set RCR (Receive Configuration Register)
	out32(rtl8139_device.io_base + RCR, RCR_AAP | RCR_APM | RCR_AM | RCR_AB | RCR_WRAP);

	// 8. Enable RX and TX
	out32(rtl8139_device.io_base + RX_MISSED, 0x0);
	out8(rtl8139_device.io_base + CMD, 0x0c); // Sets the RE and TE bits high

	// 9. Register IRQ handler
	uint32_t irq_number = pci_config_read_dword(pci_rtl8139_device.bus,
			pci_rtl8139_device.slot, pci_rtl8139_device.func, 0x3c) & 0xff;

	x86_irq_set_routine(irq_number, packet_handler);

	read_mac_address();
}
