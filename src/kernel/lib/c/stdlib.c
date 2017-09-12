/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <memory/heap.h>
#include "stdlib.h"

void *malloc(size_t size)
{
	return heap_alloc(size);
}

void free(void *address)
{
	heap_free(address);
}
