/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

/**
 * @struct registers
 *
 * This defines what the stack looks like after an ISR was running.
 */
struct registers
{
    unsigned int gs, fs, es, ds;
    unsigned int edi, esi, ebp, esp, ebx, edx, ecx, eax;
    unsigned int interrupt_number, error_code;
    unsigned int eip, cs, eflags, user_esp, ss;
};
