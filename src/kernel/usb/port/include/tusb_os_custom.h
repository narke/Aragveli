#pragma once

#include <lib/types.h>
#include <lib/status.h>
#include <lib/c/stdbool.h>
#include <arch/x86/spinlock.h>
#include <process/mutex.h>
#include <process/thread.h>

extern volatile uint32_t g_pit_ticks;

/* Time */

static inline uint32_t
osal_time_millis(void)
{
	return g_pit_ticks * 10;	/* PIT = 100 Hz */
}

/* Task */

typedef thread_t *osal_task_handle_t;

static inline osal_task_handle_t
osal_task_get_current_handle(void)
{
	return thread_get_current();
}

/* The scheduler has no timed sleep: spin on the PIT, IRQs must be enabled */
static inline void
osal_task_delay(uint32_t msec)
{
	uint32_t start = osal_time_millis();

	while (osal_time_millis() - start < msec)
		asm volatile("pause" ::: "memory");
}

/* Spinlock (shared between the USB ISR and threads, on any CPU) */

typedef struct
{
	spinlock_t lock;
	uint32_t   flags;
} osal_spinlock_t;

#define OSAL_SPINLOCK_DEF(_name, _int_set) \
	osal_spinlock_t _name = { .lock = SPINLOCK_INIT, .flags = 0 }

static inline void
osal_spin_init(osal_spinlock_t *ctx)
{
	spinlock_init(&ctx->lock);
}

static inline void
osal_spin_deinit(osal_spinlock_t *ctx)
{
	(void)ctx;
}

static inline void
osal_spin_lock(osal_spinlock_t *ctx, bool in_isr)
{
	(void)in_isr;
	uint32_t flags = spinlock_lock_irqsave(&ctx->lock);
	ctx->flags = flags;
}

static inline void
osal_spin_unlock(osal_spinlock_t *ctx, bool in_isr)
{
	(void)in_isr;
	spinlock_unlock_irqrestore(&ctx->lock, ctx->flags);
}

/* Mutex (thread context only) */

typedef mutex_t  osal_mutex_def_t;
typedef mutex_t *osal_mutex_t;

static inline osal_mutex_t
osal_mutex_create(osal_mutex_def_t *mdef)
{
	mutex_init(mdef);
	return mdef;
}

static inline bool
osal_mutex_delete(osal_mutex_t mutex)
{
	(void)mutex;
	return true;
}

static inline bool
osal_mutex_lock(osal_mutex_t mutex, uint32_t msec)
{
	(void)msec;	/* TinyUSB only uses OSAL_TIMEOUT_WAIT_FOREVER */
	return mutex_lock(mutex) == KERNEL_OK;
}

static inline bool
osal_mutex_unlock(osal_mutex_t mutex)
{
	return mutex_unlock(mutex) == KERNEL_OK;
}

/* Queue */

#include "common/tusb_fifo.h"

typedef struct
{
	osal_spinlock_t lock;
	uint16_t        item_size;
	tu_fifo_t       ff;
} osal_queue_def_t;

typedef osal_queue_def_t *osal_queue_t;

#define OSAL_QUEUE_DEF(_int_set, _name, _depth, _type)				\
	uint8_t _name##_buf[_depth * sizeof(_type)];				\
	osal_queue_def_t _name = {						\
		.lock      = { .lock = SPINLOCK_INIT, .flags = 0 },		\
		.item_size = sizeof(_type),					\
		.ff        = TU_FIFO_INIT(_name##_buf, _depth * sizeof(_type), false) \
	}

static inline osal_queue_t
osal_queue_create(osal_queue_def_t *qdef)
{
	osal_spin_init(&qdef->lock);
	tu_fifo_clear(&qdef->ff);
	return qdef;
}

static inline bool
osal_queue_delete(osal_queue_t queue)
{
	(void)queue;
	return true;
}

/* Never blocks: the USB thread sleeps on usb_sem instead */
static inline bool
osal_queue_receive(osal_queue_t queue, void *data, uint32_t msec)
{
	(void)msec;

	osal_spin_lock(&queue->lock, false);
	bool success = tu_fifo_read_n(&queue->ff, data, queue->item_size) > 0;
	osal_spin_unlock(&queue->lock, false);

	return success;
}

static inline bool
osal_queue_send(osal_queue_t queue, void const *data, bool in_isr)
{
	osal_spin_lock(&queue->lock, in_isr);
	bool success = tu_fifo_write_n(&queue->ff, data, queue->item_size) > 0;
	osal_spin_unlock(&queue->lock, in_isr);

	return success;
}

static inline bool
osal_queue_empty(osal_queue_t queue)
{
	return tu_fifo_empty(&queue->ff);
}
