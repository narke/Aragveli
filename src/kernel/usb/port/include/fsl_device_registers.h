#pragma once
#include <lib/types.h>

extern volatile uint32_t g_ohci_mmio;          /* BAR0, set by PCI probe */
#define USBFSH_BASE        (g_ohci_mmio)
#define USB0_IRQn          0
void ohci_irq_unmask(void);
void ohci_irq_mask(void);
#define NVIC_EnableIRQ(n)  ohci_irq_unmask()
#define NVIC_DisableIRQ(n) ohci_irq_mask()
