#include <lib/c/stdlib.h>
#include <lib/status.h>
#include <process/mutex.h>
#include <drivers/rtc.h>
#include "ff.h"

#if FF_USE_LFN == 3
void *
ff_memalloc(UINT msize)
{
	return malloc(msize);
}

void
ff_memfree(void *mblock)
{
	free(mblock);
}
#endif

#if FF_FS_REENTRANT
/* Index FF_VOLUMES is the FatFs system mutex. */
static mutex_t ff_mutexes[FF_VOLUMES + 1];

int
ff_mutex_create(int vol)
{
	mutex_init(&ff_mutexes[vol]);
	return 1;
}

void
ff_mutex_delete(int vol)
{
	(void)vol;
}

int
ff_mutex_take(int vol)
{
	return mutex_lock(&ff_mutexes[vol]) == KERNEL_OK;
}

void
ff_mutex_give(int vol)
{
	mutex_unlock(&ff_mutexes[vol]);
}
#endif

#if !FF_FS_READONLY && !FF_FS_NORTC
/* FAT timestamp: bits 31-25 year-1980, 24-21 month, 20-16 day,
 * 15-11 hour, 10-5 minute, 4-0 second/2. */
DWORD
get_fattime(void)
{
	struct datetime now;
	if (rtc_read(&now) != KERNEL_OK || now.year < 1980 || now.year > 2107)
		return (DWORD)(FF_NORTC_YEAR - 1980) << 25
			| (DWORD)FF_NORTC_MON << 21
			| (DWORD)FF_NORTC_MDAY << 16;
	return (DWORD)(now.year - 1980) << 25
		| (DWORD)now.month << 21
		| (DWORD)now.day << 16
		| (DWORD)now.hour << 11
		| (DWORD)now.minute << 5
		| (DWORD)(now.second / 2);
}
#endif
