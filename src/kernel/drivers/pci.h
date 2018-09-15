/*
 * Copyright (c) 2018 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <lib/types.h>

#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC

typedef struct pci_device
{
	uint32_t bus;
	uint32_t slot;
	uint32_t func;
	uint32_t vendor_id;
	uint32_t device_id;
	uint32_t class_id;
} pci_device_t;

uint16_t pci_config_read_word(uint16_t bus, uint16_t slot, uint16_t func, uint8_t offset);
void pci_config_write_dword (uint16_t bus, uint16_t device, uint16_t function, uint8_t offset, uint32_t value);
void pci_scan(void);
pci_device_t pci_device_lookup(uint16_t vendor_id, uint16_t device_id);
void pci_devices_print(void);

