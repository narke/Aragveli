#pragma once

#include <lib/types.h>

status_t ramdisk_create(const char *name, paddr_t start, paddr_t end);
