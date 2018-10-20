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
#define TX_OK 0x04
#define TX_ERR 0x08

#define RCR_AAP  (1 << 0) /* Accept All Packets */
#define RCR_APM  (1 << 1) /* Accept Physical Match Packets */
#define RCR_AM   (1 << 2) /* Accept Multicast Packets */
#define RCR_AB   (1 << 3) /* Accept Broadcast Packets */
#define RCR_WRAP (1 << 7) /* Wrap packets too long */

#define RX_BUF 0x30
#define RX_BUF_PTR 0x38
#define RX_BUF_ADDR 0x3a

#define RX_MISSED 0x4c

pci_device_t  pci_rtl8139_device;
rtl8139_dev_t rtl8139_device;

void
packet_handler(int number)
{
	(void)number;
	uint16_t status = inw(rtl8139_device.io_base + ISR);

	if (!status)
		return;

	// Acknowledge
	outw(rtl8139_device.io_base + ISR, status);

	if (status & TX_OK)
		printf("Packet sent.\n");
	if (status & RX_OK)
		printf("Packet received.\n");
}

void
read_mac_address(void)
{
	uint32_t mac_part1 = indw(rtl8139_device.io_base + 0x00);
	uint16_t mac_part2 = inw(rtl8139_device.io_base + 0x04);

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
	rtl8139_device.tx_cur = 0;

	// 2. Get the I/O base address
	uint32_t io_base = pci_config_read_dword(pci_rtl8139_device.bus,
			pci_rtl8139_device.slot,
			pci_rtl8139_device.func, 0x10);

	rtl8139_device.io_base = io_base & (~0x3);

	// 3. Power on the device
	outb(rtl8139_device.io_base + CONFIG_1, 0x0);

	// 4. Software reset
	outb(rtl8139_device.io_base + CMD, 0x10);

	while ((inb(rtl8139_device.io_base + CMD) & 0x10) != 0)
	{
		/* Wait for RST to be done */
	}

	// 5. Init the receive buffer
	rtl8139_device.rx_buffer = malloc(8192 + 16 + 1500);

	if (!rtl8139_device.rx_buffer)
	{
		return;
	}

	memset(rtl8139_device.rx_buffer, 0x0, 8192 + 16 + 1500);
	outdw(rtl8139_device.io_base + RX_BUF, (uintptr_t)rtl8139_device.rx_buffer);
	outdw(rtl8139_device.io_base + RX_BUF_PTR, 0);
	outdw(rtl8139_device.io_base + RX_BUF_ADDR, 0);

	// 6. Set IMR + ISR, enable some interrupts
	outw(rtl8139_device.io_base + IMR, RX_OK | TX_OK | TX_ERR);

	// 7. Set RCR (Receive Configuration Register)
	outdw(rtl8139_device.io_base + RCR, RCR_AAP | RCR_APM | RCR_AM | RCR_AB | RCR_WRAP);

	// 8. Enable RX and TX
	outdw(rtl8139_device.io_base + RX_MISSED, 0x0);
	outb(rtl8139_device.io_base + CMD, 0x0c); // Sets the RE and TE bits high

	// 9. Register IRQ handler
	uint32_t irq_number = pci_config_read_dword(pci_rtl8139_device.bus,
			pci_rtl8139_device.slot, pci_rtl8139_device.func, 0x3c) & 0xff;

	printf("RTL IRQ: %d\n", irq_number);

	x86_irq_set_routine(irq_number, packet_handler);


	read_mac_address();
}
