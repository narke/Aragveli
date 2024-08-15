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
#include <lib/c/stdbool.h>
#include <lib/c/string.h>
#include "io-ports.h"
#include "lapic.h"
#include "ioapic.h"
#include "acpi.h"

#define RSD_PTR_SIGNATURE 0x2052545020445352	// 'RSD PTR '
#define APIC_SIGNATURE 0x43495041
#define FACS_SIGNATURE 0x50434146
#define RSDT_SIGNATURE 0x54445352

uint32_t g_acpiCpuCount;
uint8_t g_acpiCpuIds[MAX_CPU_COUNT];

// ------------------------------------------------------------------------------------------------
typedef struct AcpiHeader
{
    uint32_t signature;
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    uint8_t oem[6];
    uint8_t oemTableId[8];
    uint32_t oemRevision;
    uint32_t creatorId;
    uint32_t creatorRevision;
} __attribute__((packed)) AcpiHeader;

// ------------------------------------------------------------------------------------------------
typedef struct AcpiFadt
{
    AcpiHeader header;
    uint32_t firmwareControl;
    uint32_t dsdt;
    uint8_t reserved;
    uint8_t preferredPMProfile;
    uint16_t sciInterrupt;
    uint32_t smiCommandPort;
    uint8_t acpiEnable;
    uint8_t acpiDisable;
    // TODO - fill in rest of data
} __attribute__((packed)) AcpiFadt;

// ------------------------------------------------------------------------------------------------
typedef struct AcpiMadt
{
    AcpiHeader header;
    uint32_t localApicAddr;
    uint32_t flags;
} __attribute__((packed)) AcpiMadt;

// ------------------------------------------------------------------------------------------------
typedef struct ApicHeader
{
    uint8_t type;
    uint8_t length;
} __attribute__((packed)) ApicHeader;

// APIC structure types
#define APIC_TYPE_LOCAL_APIC            0
#define APIC_TYPE_IO_APIC               1
#define APIC_TYPE_INTERRUPT_OVERRIDE    2

// ------------------------------------------------------------------------------------------------
typedef struct ApicLocalApic
{
    ApicHeader header;
    uint8_t acpiProcessorId;
    uint8_t apicId;
    uint32_t flags;
} __attribute__((packed)) ApicLocalApic;

// ------------------------------------------------------------------------------------------------
typedef struct ApicIoApic
{
    ApicHeader header;
    uint8_t ioApicId;
    uint8_t reserved;
    uint32_t ioApicAddress;
    uint32_t globalSystemInterruptBase;
} __attribute__((packed)) ApicIoApic;

// ------------------------------------------------------------------------------------------------
typedef struct ApicInterruptOverride
{
    ApicHeader header;
    uint8_t bus;
    uint8_t source;
    uint32_t interrupt;
    uint16_t flags;
} __attribute__((packed)) ApicInterruptOverride;

typedef struct RSDPDescriptor {
	char signature[8];
	uint8_t checksum;
	char oemid[6];
	uint8_t revision;
	uint32_t rsdt_address;
} __attribute__((packed)) AcpiRSDP;

static AcpiMadt *g_madt;

// ------------------------------------------------------------------------------------------------

static void
AcpiParseFacp(AcpiFadt *facp)
{
    if (facp->smiCommandPort)
    {
        printf("Enabling ACPI\n");
        out8(facp->smiCommandPort, facp->acpiEnable);

        // TODO - wait for SCI_EN bit
    }
    else
    {
        printf("ACPI already enabled\n");
    }
}

// ------------------------------------------------------------------------------------------------
static void
AcpiParseApic(AcpiMadt *madt)
{
	if (madt->header.signature != APIC_SIGNATURE)
	{
		printf("ACPI: APIC signature not found.\n");
		return;
	}

	g_madt = madt;

	printf("Local APIC Address: 0x%lx\n", madt->localApicAddr);


    g_localApicAddr = (uint8_t *)(uintptr_t)madt->localApicAddr;

    uint8_t *p = (uint8_t *)(madt + 1);
    uint8_t *end = (uint8_t *)madt + madt->header.length;

    while (p < end)
    {
        ApicHeader *header = (ApicHeader *)p;
        uint8_t type = header->type;
        uint8_t length = header->length;

        if (type == APIC_TYPE_LOCAL_APIC)
        {
            ApicLocalApic *s = (ApicLocalApic *)p;

            printf("Found CPU: %d %d %x\n", s->acpiProcessorId, s->apicId, s->flags);
            if (g_acpiCpuCount < MAX_CPU_COUNT)
            {
                g_acpiCpuIds[g_acpiCpuCount] = s->apicId;
                ++g_acpiCpuCount;
            }
        }
        else if (type == APIC_TYPE_IO_APIC)
        {
            ApicIoApic *s = (ApicIoApic *)p;

            printf("Found I/O APIC: %d 0x%x %d\n", s->ioApicId, s->ioApicAddress, s->globalSystemInterruptBase);
            g_ioApicAddr = (uint8_t *)(uintptr_t)s->ioApicAddress;
        }
        else if (type == APIC_TYPE_INTERRUPT_OVERRIDE)
        {
            ApicInterruptOverride *s = (ApicInterruptOverride *)p;

            printf("Found Interrupt Override: %d %d %d 0x%x\n", s->bus, s->source, s->interrupt, s->flags);
        }
        else
        {
            printf("Unknown APIC structure %d\n", type);
        }

        p += length;
    }
}

static bool
doChecksum(AcpiHeader *header)
{
	unsigned char sum = 0;

	for (uint32_t i = 0; i < header->length; i++)
	{
		sum += ((char *)header)[i];
	}

	return sum == 0;
}

static void
AcpiParseDT(AcpiHeader *header)
{
    char descriptor_name[5];
    memcpy(descriptor_name, &header->signature, 4);
    descriptor_name[4] = 0;
    printf("Descriptor Table: %s, signature: 0x%x\n", descriptor_name, header->signature);

    if (header->signature == FACS_SIGNATURE)
    {
        AcpiParseFacp((AcpiFadt *)header);
    }
    else if (header->signature == APIC_SIGNATURE)
    {
        AcpiParseApic((AcpiMadt *)header);
    }
}

static void
AcpiParseRsdt(AcpiHeader *rsdt)
{
	if (rsdt->signature != RSDT_SIGNATURE)
	{
		printf("ACPI: RSDT signature not found.\n");
		return;
	}

	if (!doChecksum(rsdt))
	{
		printf("ACPI RSDT checksum failed.\n");
		return;
	}

	// Additional descriptor entries
	uint8_t entries = (rsdt->length - sizeof(AcpiHeader)) / 4;
	uint32_t *otherDT = (uint32_t *)(rsdt + 1);

	for (uint8_t i = 0; i < entries; i++)
	{
		AcpiParseDT((AcpiHeader *)otherDT[i]);

	}
}

static bool
AcpiParseRsdp(uint8_t *p)
{
	// Parse Root System Description Pointer
	printf("ACPI: RSDP found\n");

	// RSDP checksum, AcpiRSDP's size is 20.
	uint8_t sum = 0;
	for (uint8_t i = 0; i < 20; i++)
	{
		sum += p[i];
	}

	if (sum)
	{
		printf("Checksum failed\n");
		return false;
	}

	AcpiRSDP *rsdp = (AcpiRSDP *)p;

	printf("ACPI OEM: %s\n", rsdp->oemid);

	if (rsdp->revision == 0)
	{
		printf("ACPI version: 1\n");
		AcpiParseRsdt((AcpiHeader *)rsdp->rsdt_address);
	}
	else if (rsdp->revision == 2)
	{
		printf("ACPI version: 2\n");
	}
	else
	{
		printf("Unsupported ACPI version %d\n", rsdp->revision);
	}

	return true;
}


void
AcpiInit(void)
{
	uint8_t *p = (uint8_t *)0x000e0000;
	uint8_t *end = (uint8_t *)0x000fffff;

	while (p < end)
	{
		uint64_t signature = *(uint64_t *)p;

		if (signature == RSD_PTR_SIGNATURE)
		{
			if (AcpiParseRsdp(p))
			{
				break;
			}
		}

		p += 16;
	}
}

uint16_t
AcpiRemapIrq(uint16_t irq)
{
    AcpiMadt *madt = g_madt;

    uint8_t *p = (uint8_t *)(madt + 1);
    uint8_t *end = (uint8_t *)madt + madt->header.length;

    while (p < end)
    {
        ApicHeader *header = (ApicHeader *)p;
        uint8_t type = header->type;
        uint8_t length = header->length;

        if (type == APIC_TYPE_INTERRUPT_OVERRIDE)
        {
            ApicInterruptOverride *s = (ApicInterruptOverride *)p;

            if (s->source == irq)
            {
                return s->interrupt;
            }
        }

        p += length;
    }

    return irq;
}
