#include <lib/c/stdio.h>
#include <process/semaphore.h>
#include <process/thread.h>
#include "tusb.h"
#include "usb.h"

static semaphore_t *usb_sem;

void
usb_wakeup(void)
{
	if (usb_sem != NULL)
		semaphore_up(usb_sem);
}

static void
usb_thread(uint32_t arg)
{
	(void)arg;

	// Runs here, not in extra_kernel(): hcd_init() busy-waits on g_pit_ticks
	tusb_rhport_init_t init = { .role = TUSB_ROLE_HOST, .speed = TUSB_SPEED_AUTO };

	if (!tusb_init(0, &init))
	{
		kprintf("USB: OHCI host controller init failed\n");
		thread_exit();
	}

	for (;;)
	{
		semaphore_down(usb_sem);
		usb_msc_service();
		tuh_task();
	}
}

void
usb_setup(void)
{
	if (!ohci_pci_probe())
	{
		return;
	}

	usb_sem = semaphore_create(0);
	thread_kernel_create("usb", usb_thread, NULL);
}

void
tuh_event_hook_cb(uint8_t rhport, uint32_t eventid, bool in_isr)
{
	(void)rhport;
	(void)eventid;
	(void)in_isr;
	usb_wakeup();
}

void
usb_tick(void)
{
	if (usb_sem != NULL && (tuh_task_event_ready() || usb_msc_pending()))
	{
		semaphore_up(usb_sem);
	}
}
