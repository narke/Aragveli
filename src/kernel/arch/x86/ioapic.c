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

#include <lib/c/stdio.h>
#include "ioapic.h"
#include "mmio.h"

// ------------------------------------------------------------------------------------------------
// Globals
uint8_t *g_ioApicAddr;

// ------------------------------------------------------------------------------------------------
// Memory mapped registers for IO APIC register access
#define IOREGSEL                        0x00
#define IOWIN                           0x10

// ------------------------------------------------------------------------------------------------
// IO APIC Registers
#define IOAPICID                        0x00
#define IOAPICVER                       0x01
#define IOAPICARB                       0x02
#define IOREDTBL                        0x10

// ------------------------------------------------------------------------------------------------
static void IoApicOut(uint8_t *base, uint8_t reg, uint32_t val)
{
    MmioWrite32(base + IOREGSEL, reg);
    MmioWrite32(base + IOWIN, val);
}

// ------------------------------------------------------------------------------------------------
static uint32_t IoApicIn(uint8_t *base, uint8_t reg)
{
    MmioWrite32(base + IOREGSEL, reg);
    return MmioRead32(base + IOWIN);
}

// ------------------------------------------------------------------------------------------------
void IoApicSetEntry(uint8_t *base, uint8_t index, uint64_t data)
{
    IoApicOut(base, IOREDTBL + index * 2, (uint32_t)data);
    IoApicOut(base, IOREDTBL + index * 2 + 1, (uint32_t)(data >> 32));
}

// ------------------------------------------------------------------------------------------------
void IoApicInit()
{
    // Get number of entries supported by the IO APIC
    uint32_t x = IoApicIn(g_ioApicAddr, IOAPICVER);
    uint32_t count = ((x >> 16) & 0xff) + 1;    // maximum redirection entry

    printf("I/O APIC pins = %d\n", count);

    // Disable all entries
    for (uint32_t i = 0; i < count; ++i)
    {
        IoApicSetEntry(g_ioApicAddr, i, 1 << 16);
    }
}
