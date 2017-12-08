/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <acpi.h>
#include <memory/heap.h>
#include <arch/x86/io-ports.h>
#include <lib/types.h>

/*
 * Environmental and ACPI Tables
 */

ACPI_STATUS
AcpiOsInitialize(void)
{
	return AE_OK;
}

ACPI_STATUS
AcpiOsTerminate(void)
{
	return AE_OK;
}

ACPI_PHYSICAL_ADDRESS
AcpiOsGetRootPointer(void)
{
	ACPI_PHYSICAL_ADDRESS Ret;
	Ret = 0;
	AcpiFindRootPointer(&Ret);
	return Ret;
}

ACPI_STATUS
AcpiOsPredefinedOverride(
		const ACPI_PREDEFINED_NAMES *InitVal,
		ACPI_STRING                 *NewVal)
{
	if (!InitVal || !NewVal)
	{
		return AE_BAD_PARAMETER;
	}

	*NewVal = NULL;
	return AE_OK;
}

ACPI_STATUS
AcpiOsTableOverride(
		ACPI_TABLE_HEADER       *ExistingTable,
		ACPI_TABLE_HEADER       **NewTable)
{
	if (!ExistingTable || !NewTable)
	{
		return AE_BAD_PARAMETER;
	}

	*NewTable = NULL;
	return AE_OK;
}

/*
 * Memory Management
 */

void *
AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS Where, ACPI_SIZE Length)
{
	return Where;
}

void
AcpiOsUnmapMemory(void *LogicalAddress, ACPI_SIZE Size)
{
	// Do nothing.
}

void *
AcpiOsAllocate(ACPI_SIZE Size)
{
	return heap_alloc(Size);
}

void
AcpiOsFree(void *Memory)
{
	heap_free(Memory);
}

BOOLEAN
AcpiOsReadable(
		void                    *Pointer,
		ACPI_SIZE               Length)
{
	return TRUE;
}

BOOLEAN
AcpiOsWritable(
		void                    *Pointer,
		ACPI_SIZE               Length)
{
	return TRUE;
}

/*
 * Multithreading and Scheduling Services
 */

ACPI_THREAD_ID
AcpiOsGetThreadId(void)
{
	return (ACPI_THREAD_ID)0;
}

ACPI_STATUS
AcpiOsExecute(
		ACPI_EXECUTE_TYPE       Type,
		ACPI_OSD_EXEC_CALLBACK  Function,
		void                    *Context)
{
	return AE_ERROR;
}

void
AcpiOsSleep(UINT64 Milliseconds)
{
}

void
AcpiOsStall(UINT32 Microseconds)
{
}

/*
 * Mutual Exclusion and Synchronization
 */

ACPI_STATUS
AcpiOsCreateSemaphore(
		UINT32		MaxUnits,
		UINT32		InitialUnits,
		ACPI_HANDLE	*OutHandle)
{
	*OutHandle = (ACPI_HANDLE)1;
	return AE_OK;
}

ACPI_STATUS
AcpiOsDeleteSemaphore(ACPI_HANDLE Handle)
{
	return AE_OK;
}

ACPI_STATUS
AcpiOsWaitSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units, UINT16 Timeout)
{
	return 0;
}

ACPI_STATUS
AcpiOsSignalSemaphore(ACPI_HANDLE Handle, UINT32 Units)
{
	return AE_OK;
}

ACPI_STATUS
AcpiOsCreateLock(ACPI_SPINLOCK *OutHandle)
{
	return AcpiOsCreateSemaphore(1, 1, OutHandle);
}

void
AcpiOsDeleteLock(ACPI_SPINLOCK Handle)
{
	AcpiOsDeleteSemaphore(Handle);
}

ACPI_CPU_FLAGS
AcpiOsAcquireLock(ACPI_SPINLOCK Handle)
{
	AcpiOsWaitSemaphore(Handle, 1, 0);
	return 0;
}

void
AcpiOsReleaseLock(ACPI_SPINLOCK Handle, ACPI_CPU_FLAGS Flags)
{
	AcpiOsSignalSemaphore(Handle, 1);
}

/*
 * Interrupt Handling
 */

ACPI_STATUS
AcpiOsInstallInterruptHandler(
		UINT32                  InterruptNumber,
		ACPI_OSD_HANDLER        ServiceRoutine,
		void                    *Context)
{
	return AE_ERROR;
}

ACPI_STATUS
AcpiOsRemoveInterruptHandler(
		UINT32                  InterruptNumber,
		ACPI_OSD_HANDLER        ServiceRoutine)
{
	(void)InterruptNumber;
	(void)ServiceRoutine;
	return AE_OK;
}

/*
 * Misc
 */

void
AcpiOsWaitEventsComplete(void)
{
    // Do nothing
}

UINT64
AcpiOsGetTimer(void)
{
	return (UINT64)0;
}

ACPI_STATUS
AcpiOsSignal(
		UINT32                  Function,
		void                    *Info)
{
	return AE_ERROR;
}

ACPI_STATUS
AcpiOsEnterSleep(
		UINT8                   SleepState,
		UINT32                  RegaValue,
		UINT32                  RegbValue)
{
	return AE_OK;
}

ACPI_STATUS
AcpiOsCreateCache(
		char		*CacheName,
		UINT16		ObjectSize,
		UINT16		MaxDepth,
		ACPI_CACHE_T	**ReturnCache)
{
	return AE_OK;
}

ACPI_STATUS
AcpiOsDeleteCache (ACPI_CACHE_T *Cache)
{
	return AE_OK;
}

ACPI_STATUS
AcpiOsPurgeCache (ACPI_CACHE_T *Cache)
{
	return AE_OK;
}

void *
AcpiOsAcquireObject(ACPI_CACHE_T *Cache)
{
	return NULL;
}

ACPI_STATUS
AcpiOsReleaseObject(ACPI_CACHE_T *Cache, void *Object)
{
	return AE_OK;
}

void
AcpiOsVprintf(const char *Format, va_list Args)
{
}

void
AcpiOsPrintf(const char *Format, ...)
{
}

ACPI_STATUS
AcpiOsReadPort(
		ACPI_IO_ADDRESS         Address,
		UINT32                  *Value,
		UINT32                  Width)
{
	*Value = 0;

	switch(Width)
	{
		case 8:
			*((uint8_t *)Value) = inb(Address);
			break;

		case 16:
			*((uint16_t *)Value) = inb(Address);
			break;

		case 32:
			*((uint32_t *)Value) = inb(Address);
			break;

		default:
			return AE_BAD_PARAMETER;
	}

	return AE_OK;
}

ACPI_STATUS
AcpiOsWritePort(
		ACPI_IO_ADDRESS Address,
		UINT32 Value,
		UINT32 Width)
{
	switch (Width)
	{
		case 8:
			outb(Address, (uint8_t)Value);
			break;

		case 16:
			outb(Address, (uint16_t)Value);
			break;

		case 32:
			outb(Address, (uint32_t)Value);
			break;

		default:
			return AE_BAD_PARAMETER;
	}

	return AE_OK;
}

ACPI_STATUS
AcpiOsReadMemory(
		ACPI_PHYSICAL_ADDRESS	Address,
		UINT64			*Value,
		UINT32			Width)
{
	switch(Width)
	{
		case 8:
			*Value = *(volatile uint8_t *)Address;
			break;

		case 16:
			*Value = *(volatile uint16_t *)Address;
			break;

		case 32:
			*Value = *(volatile uint32_t *)Address;
			break;

		default:
			return AE_BAD_PARAMETER;
	}

	return AE_OK;
}

ACPI_STATUS
AcpiOsWriteMemory(
		ACPI_PHYSICAL_ADDRESS   Address,
		UINT64                  Value,
		UINT32                  Width)
{
	switch(Width)
	{
		case 8:
			*(volatile uint8_t *)Address = Value;
			break;

		case 16:
			*(volatile uint16_t *)Address = Value;
			break;

		case 32:
			*(volatile uint32_t *)Address = Value;
			break;

		default:
			return AE_BAD_PARAMETER;
	}

	return AE_OK;
}

ACPI_STATUS
AcpiOsReadPciConfiguration(
		ACPI_PCI_ID             *PciId,
		UINT32                  Reg,
		UINT64                  *Value,
		UINT32                  Width)
{
	return AE_OK;
}

ACPI_STATUS
AcpiOsWritePciConfiguration(
		ACPI_PCI_ID             *PciId,
		UINT32                  Reg,
		UINT64                  Value,
		UINT32                  Width)
{
	return AE_OK;
}

ACPI_STATUS
AcpiOsPhysicalTableOverride(
		ACPI_TABLE_HEADER       *ExistingTable,
		ACPI_PHYSICAL_ADDRESS   *NewAddress,
		UINT32                  *NewTableLength)
{
	return AE_NOT_IMPLEMENTED;
}

ACPI_STATUS
AcpiOsGetTableByName(
		char                    *Signature,
		UINT32                  Instance,
		ACPI_TABLE_HEADER       **Table,
		ACPI_PHYSICAL_ADDRESS   *Address)
{
	return AE_NOT_IMPLEMENTED;
}

ACPI_STATUS
AcpiOsGetTableByIndex(
		UINT32                  Index,
		ACPI_TABLE_HEADER       **Table,
		UINT32                  *Instance,
		ACPI_PHYSICAL_ADDRESS   *Address)
{
	return AE_NOT_IMPLEMENTED;
}

ACPI_STATUS
AcpiOsGetTableByAddress(
		ACPI_PHYSICAL_ADDRESS   Address,
		ACPI_TABLE_HEADER       **Table)
{
	return AE_NOT_IMPLEMENTED;
}

static unsigned long long
divandmod64(
		unsigned long long a,
		unsigned long long b,
		unsigned long long *remainder)
{
	unsigned long long result;
	int steps = sizeof(unsigned long long) * 8;

	*remainder = 0;
	result = 0;

	if (b == 0)
	{
		/* FIXME: division by zero */
		return 0;
	}

	if (a < b)
	{
		*remainder = a;
		return 0;
	}

	for (; steps > 0; steps--)
	{
		/* shift one bit to remainder */
		*remainder = ((*remainder) << 1) | ((a >> 63) & 0x1);
		result <<= 1;

		if (*remainder >= b)
		{
			*remainder -= b;
			result |= 0x1;
		}
		a <<= 1;
	}

	return result;
}

/* 64bit unsigned integer division */
unsigned long long
__udivdi3(unsigned long long a, unsigned long long b)
{
	unsigned long long rem;
	return divandmod64(a, b, &rem);
}

/* 64bit remainder of the unsigned division */
unsigned long long
__umoddi3(unsigned long long a, unsigned long long b)
{
	unsigned long long rem;
	divandmod64(a, b, &rem);
	return rem;
}
