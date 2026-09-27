/*
 * Copyright (c) 2026 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define SYS_PCI		9
#define PCI_LIST	0

typedef struct pci_device {
	uint8_t  bus;
	uint8_t  slot;
	uint8_t  func;
	uint16_t vendor_id;
	uint16_t device_id;
	uint16_t class_id;
} pci_device_t;

const char* PCI_CLASS_IDS[18] =
{
    "no class specification",
    "Mass Storage Controller",
    "Network Controller",
    "Display Controller",
    "Multimedia Device",
    "Memory Controller",
    "Bridge Device",
    "Simple Communication Controller",
    "Base System Peripheral",
    "Input Device",
    "Docking Station",
    "Processor",
    "Serial Bus Controller",
    "Wireless Controller",
    "Intelligent I/O Controller",
    "Satellite Communication Controller",
    "Encryption/Decryption Controller",
    "Data Acquisition and Signal Processing Controller"
};

int main(int argc, char **argv)
{
	pci_device_t pci_devices[16];
	int n;

	(void)argc;
	(void)argv;

	asm volatile("int $0x80"
	: "=a"(n)
	: "a"(SYS_PCI), "b"(PCI_LIST), "c"(pci_devices), "d"(16)
	: "memory");

	if (n < 0)
	{
		printf("lspci: syscall failed\n");
		exit(1);
	}

	for (int i = 0; i < n; i++)
	{
		printf("PCI Vendor ID:%x Device ID:%x Class:%s\n",
			pci_devices[i].vendor_id,
			pci_devices[i].device_id,
			PCI_CLASS_IDS[pci_devices[i].class_id]);
	}

	exit(0);
}
