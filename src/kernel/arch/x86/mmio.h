/*
 * Copyright (c) 2018 Konstantin Tcholokachvili.
 * Copyright (c) 2012 Patrick Doane and others.  See AUTHORS file for list.
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from the
 * use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it freely,
 * subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not claim
 * that you wrote the original software. If you use this software in a product,
 * an acknowledgment in the product documentation would be appreciated but is
 * not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 */

#pragma once

#include <lib/types.h>


// ------------------------------------------------------------------------------------------------
// memory mapped i/o functions

static inline void MmioWrite8(void *p, uint8_t data)
{
    *(volatile uint8_t *)(p) = data;
}

static inline uint8_t MmioRead8(void *p)
{
    return *(volatile uint8_t *)(p);
}

static inline void MmioWrite16(void *p, uint16_t data)
{
    *(volatile uint16_t *)(p) = data;
}

static inline uint16_t MmioRead16(void *p)
{
    return *(volatile uint16_t *)(p);
}

static inline void MmioWrite32(void *p, uint32_t data)
{
    *(volatile uint32_t *)(p) = data;
}

static inline uint32_t MmioRead32(void *p)
{
    return *(volatile uint32_t *)(p);
}

static inline void MmioWrite64(void *p, uint64_t data)
{
    *(volatile uint64_t *)(p) = data;
}

static inline uint64_t MmioRead64(void *p)
{
    return *(volatile uint64_t *)(p);
}

static inline void MmioReadN(void *dst, const volatile void *src, size_t bytes)
{
    volatile uint8_t *s = (volatile uint8_t *)src;
    uint8_t *d = (uint8_t *)dst;
    while (bytes > 0)
    {
        *d =  *s;
        ++s;
        ++d;
        --bytes;
    }
}
