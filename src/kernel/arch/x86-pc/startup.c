/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <arch/x86-pc/bootstrap/multiboot.h>

/**
 * The kernel entry point. All starts from here!
 */
void aragveli_main(unsigned int magic, /*uint32_t*/unsigned int address)
{
    multiboot_info_t *mbi;
    mbi = (multiboot_info_t *)address;

    (void)magic;
    (void)mbi;

    *((unsigned char *)0xB8000) = 'A';
    *((unsigned char *)0xB8001) = 0x1A;
}
