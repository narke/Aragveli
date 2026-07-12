ENTRY(_start)

SECTIONS
{
    . = 0x08048000;

    .text : {
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        *(.bss*)
        *(COMMON)
    }

    . = ALIGN(0x1000);
    _stack_bottom = .;
    . += 0x1000;
    _stack_top = .;
}
