/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2026 René Rebe <rene@exactco.de>
 */

#include "qemu/osdep.h"
#include "qemu.h"
#include "loader.h"
#include "target_elf.h"


const char *get_elf_cpu_model(uint32_t eflags)
{
    return "itanium2";
}
