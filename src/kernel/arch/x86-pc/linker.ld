ENTRY (multiboot_entry)
KERNEL_VIRTUAL_BASE = 0xC0000000;

SECTIONS
{
	/* Load the kernel at 0x00100000.
	 * A multi-boot compatible kernel cannot be loaded before the 1MB mark,
	 * it must be loaded at or above 1MB. */
	. = 0x00100000;

	.multiboot ALIGN(0x1000) :
	{
		/* The multiboot header must come within the first 8K of
		 * our kernel */
		*(.multiboot);


		/* 32 bit aligned as required by the multiboot specs */
		LONG(0);
	}

	/* Early boot code + boot page directory: identity-mapped, paging OFF */
	.boot ALIGN(0x1000) :
	{
		*(.boot)
	}

	. += KERNEL_VIRTUAL_BASE;	/* After that everything is linked high */

	/* Start of the kernel */
	__kernel_start = .;

	.text ALIGN (0x1000) : AT(ADDR(.text) - KERNEL_VIRTUAL_BASE)
	{
		*(.text)
	}

	.rodata ALIGN (0x1000) : AT(ADDR(.rodata) - KERNEL_VIRTUAL_BASE)
	{
		*(.rodata*)
	}

	.data ALIGN (0x1000) : AT(ADDR(.data) - KERNEL_VIRTUAL_BASE)
	{
		*(.data)
	}

	.bss ALIGN(0x1000) : AT(ADDR(.bss) - KERNEL_VIRTUAL_BASE)
	{
		sbss = .;
		*(COMMON)
		*(.bss)
		ebss = .;
	}

	/* End of the kernel */
	__kernel_end = .;
}
