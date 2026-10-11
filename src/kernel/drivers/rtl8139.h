/*
 * Copyright (c) 2018, 2026 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <lib/c/stdbool.h>
#include <lib/types.h>

#define RTL8139_VENDOR_ID 0x10EC
#define RTL8139_DEVICE_ID 0x8139

typedef struct rtl8139_dev {
	uint8_t  bar_type;
	uint16_t io_base;
	uint32_t mem_base;
	bool     eeprom_exist;
	uint8_t  mac_addr[6];
	uint8_t  *rx_buffer;
	uint32_t rx_buffer_idx;
	uint8_t  *tx_buffer;
	volatile uint32_t tx_buffer_idx;
} rtl8139_dev_t;

typedef void (*rtl8139_rx_handler_t)(const uint8_t *frame, size_t length);
extern rtl8139_dev_t rtl8139_device;

void rtl8139_setup(void);
ssize_t send_packet(const void *data, size_t length);
void rtl8139_receive(rtl8139_rx_handler_t deliver);
void rtl8139_set_rx_notify(void (*notify)(void));
