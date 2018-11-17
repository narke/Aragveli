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
#include "smp.h"
#include "acpi.h"
#include "lapic.h"

// ------------------------------------------------------------------------------------------------
void SmpInit()
{
    printf("Waking up all CPUs\n");

    g_activeCpuCount = 1;
    uint32_t localId = LocalApicGetId();

    // Send Init to all cpus except self
    for (uint32_t i = 0; i < g_acpiCpuCount; ++i)
    {
        uint32_t apicId = g_acpiCpuIds[i];
        if (apicId != localId)
        {
            LocalApicSendInit(apicId);
        }
    }

    // wait
    //PitWait(10);

    // Send Startup to all cpus except self
    for (uint32_t i = 0; i < g_acpiCpuCount; ++i)
    {
        uint32_t apicId = g_acpiCpuIds[i];
        if (apicId != localId)
        {
            LocalApicSendStartup(apicId, 0x8);
        }
    }

    // Wait for all cpus to be active
    //PitWait(1);
    while (g_activeCpuCount != g_acpiCpuCount)
    {
        printf("Waiting... %d\n", g_activeCpuCount);
        //PitWait(1);
    }

    printf("All CPUs activated\n");
}
