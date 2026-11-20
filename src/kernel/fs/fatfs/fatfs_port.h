#pragma once

#include <drivers/blockdev.h>
#include "ff.h"

status_t diskio_bind(BYTE pdrv, struct block_device *dev);
