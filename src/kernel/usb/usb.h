#pragma once

#include <lib/c/stdbool.h>

bool ohci_pci_probe(void);
void usb_wakeup(void);
void usb_setup(void);
void usb_tick(void);
void usb_msc_service(void);
bool usb_msc_pending(void);
