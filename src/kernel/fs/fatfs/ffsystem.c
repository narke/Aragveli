#include <lib/c/stdlib.h>
#include <lib/status.h>
#include <process/mutex.h>
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
