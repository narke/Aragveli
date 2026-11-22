#include <lib/types.h>
#include <lib/c/stdio.h>
#include <arch/x86/paging.h>
#include <arch/x86/irq.h>
#include <arch/x86/idt.h>
#include <arch/x86/ioapic.h>
#include <arch/x86/acpi.h>
#include <drivers/pci.h>
#include <arch/x86/mmio.h>
#include <usb/usb.h>
#include "tusb.h"

#define OHCI_HC_RH_DESCRIPTOR_A  0x48
#define OHCI_RH_DESC_A_NDP_MASK  0xff

#define PCI_CLASS_SERIAL_BUS 0x0C
#define PCI_SUBCLASS_USB     0x03
#define PCI_PROGIF_OHCI      0x10
#define PCI_CMD_MEMORY       (1 << 1)
#define PCI_CMD_BUS_MASTER   (1 << 2)
#define IOAPIC_LEVEL         (1 << 15)
#define IOAPIC_MASKED        (1 << 16)
volatile uint32_t g_ohci_mmio;
static uint8_t ohci_irq;
static x86_irq_handler_t ohci_chained_isr;

extern volatile uint32_t g_pit_ticks;

uint32_t
tusb_time_millis_api(void)
{
	// PIT = 100 Hz, as in sys_arch.c
	return g_pit_ticks * 10;
}

void *
tusb_app_virt_to_phys(void *virt_addr)
{
	if (virt_addr == NULL)
		return NULL;

	return (void *)VA2PA(virt_addr);
}

void *
tusb_app_phys_to_virt(void *phys_addr)
{
	if (phys_addr == NULL)
		return NULL;

	return PA2VA(phys_addr);
}

static void
ohci_ioapic_write(uint64_t flags)
{
	IoApicSetEntry(g_ioApicAddr, AcpiRemapIrq(ohci_irq),
		(X86_IRQ_BASE + ohci_irq) | IOAPIC_LEVEL | flags);
}
void
ohci_irq_mask(void)
{
	ohci_ioapic_write(IOAPIC_MASKED);
}
void
ohci_irq_unmask(void)
{
	ohci_ioapic_write(0);
}
static void
ohci_isr(int irq)
{
	tuh_int_handler(0, true);

	// PCI INTx lines are often shared (e.g. with the RTL8139 in QEMU)
	if (ohci_chained_isr != NULL)
	{
		ohci_chained_isr(irq);
	}
}

bool
ohci_pci_probe(void)
{
	pci_device_t devices[16];
	uint8_t count;

	pci_devices_copy(devices, &count);

	for (uint8_t i = 0; i < count; i++)
	{
		pci_device_t *dev = &devices[i];

		// Offset 0x08: class[31:24] subclass[23:16] prog-if[15:8] revision[7:0]
		uint32_t class_reg = pci_config_read_dword(dev->bus, dev->slot,
				dev->func, 0x08);
		
		if ((class_reg >> 24) != PCI_CLASS_SERIAL_BUS ||
		    ((class_reg >> 16) & 0xff) != PCI_SUBCLASS_USB ||
		    ((class_reg >> 8) & 0xff) != PCI_PROGIF_OHCI)
		{
			continue;
		}

		uint32_t bar0 = pci_config_read_dword(dev->bus, dev->slot,
				dev->func, 0x10);
		if (bar0 & 0x1)
		{
			return false;	// I/O BAR, OHCI must be MMIO
		}

		g_ohci_mmio = bar0 & ~0xFUL;

		// PDEs 768..831 remap this window to RAM in every page directory
		if (g_ohci_mmio >= KERNEL_VIRTUAL_BASE && g_ohci_mmio < 0xD0000000u)
		{
			kprintf("OHCI BAR %lx collides with kernel mapping\n",
					g_ohci_mmio);
			return false;
		}

		uint32_t cmd = pci_config_read_dword(dev->bus, dev->slot,
				dev->func, 0x04);

		cmd |= PCI_CMD_MEMORY | PCI_CMD_BUS_MASTER;

		pci_config_write_dword(dev->bus, dev->slot, dev->func, 0x04, cmd);

		uint32_t ndp = MmioRead32((void *)(g_ohci_mmio + OHCI_HC_RH_DESCRIPTOR_A))
		& OHCI_RH_DESC_A_NDP_MASK;

		if (ndp != TUP_OHCI_RHPORTS)
		{
			kprintf("OHCI has %lu root ports, TUP_OHCI_RHPORTS is %d\n",
					ndp, TUP_OHCI_RHPORTS);
		}
		
		ohci_irq = (uint8_t)(pci_config_read_dword(dev->bus, dev->slot,
				dev->func, 0x3c) & 0xff);
		ohci_chained_isr = x86_irq_get_routine(ohci_irq);
		x86_irq_set_routine(ohci_irq, ohci_isr);
		ohci_irq_mask();	// TinyUSB unmasks it via hcd_int_enable()

		kprintf("OHCI at %lx, IRQ %u\n", g_ohci_mmio, ohci_irq);

		return true;
	}
	return false;
}
