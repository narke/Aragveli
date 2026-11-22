#pragma once

#define CFG_TUSB_MCU          OPT_MCU_NONE
#define CFG_TUSB_OS           OPT_OS_CUSTOM
#define CFG_TUSB_DEBUG        0

/* No MCU is selected, so declare the controller IP here */
#define TUP_USBIP_OHCI
#define TUP_USBIP_OHCI_NXP              /* reuses ohci_nxp.h, see step 3 */
#define TUP_OHCI_RHPORTS      3         /* must match NDP of the controller (QEMU pci-ohci: 3) */

#define CFG_TUH_ENABLED       1
#define CFG_TUH_MAX_SPEED     OPT_MODE_FULL_SPEED
#define CFG_TUH_MEM_ALIGN     __attribute__((aligned(4)))
#define CFG_TUH_ENUMERATION_BUFSIZE 256

#define CFG_TUH_HUB           1
#define CFG_TUH_DEVICE_MAX    (3 * CFG_TUH_HUB + 1)
#define CFG_TUH_HID           4
#define CFG_TUH_HID_EPIN_BUFSIZE  64
#define CFG_TUH_HID_EPOUT_BUFSIZE 64
#define CFG_TUH_MSC           1
