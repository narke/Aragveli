#include <lib/c/string.h>

#include "ff.h"
#include "diskio.h"
#include "fatfs_port.h"

static struct block_device *drives[FF_VOLUMES];

status_t
diskio_bind(BYTE pdrv, struct block_device *dev)
{
	if (pdrv >= FF_VOLUMES)
		return -KERNEL_INVALID_VALUE;

	drives[pdrv] = dev;
	return KERNEL_OK;
}

static struct block_device *
drive_get(BYTE pdrv)
{
	return pdrv < FF_VOLUMES ? drives[pdrv] : NULL;
}

DSTATUS
disk_status(BYTE pdrv)
{
	return drive_get(pdrv) ? 0 : STA_NOINIT;
}

DSTATUS
disk_initialize(BYTE pdrv)
{
	return disk_status(pdrv);
}

DRESULT
disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
	struct block_device *dev = drive_get(pdrv);

	if (!dev)
		return RES_NOTRDY;

	return dev->read(dev, sector, buff, count) == KERNEL_OK
		? RES_OK : RES_ERROR;
}

DRESULT
disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
	struct block_device *dev = drive_get(pdrv);

	if (!dev)
		return RES_NOTRDY;

	return dev->write(dev, sector, buff, count) == KERNEL_OK
		? RES_OK : RES_ERROR;
}

DRESULT
disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
	struct block_device *dev = drive_get(pdrv);

	if (!dev)
		return RES_NOTRDY;

	switch (cmd)
	{
		case CTRL_SYNC:
			return RES_OK;
		case GET_SECTOR_COUNT:
			*(LBA_t *)buff = dev->sector_count;
			return RES_OK;
		case GET_SECTOR_SIZE:
			*(WORD *)buff = (WORD)dev->sector_size;
			return RES_OK;
		case GET_BLOCK_SIZE:
			*(DWORD *)buff = 1;
			return RES_OK;
		default:
			return RES_PARERR;
	}
}
