/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2026 René Rebe <rene@exactco.de>
 */

#ifndef IA64_TARGET_ELF_H
#define IA64_TARGET_ELF_H

#define ELF_CLASS               ELFCLASS64
#define ELF_MACHINE             EM_IA_64

#define VDSO_HEADER             "vdso.c.inc"

#define HAVE_ELF_CORE_DUMP      1

/* See linux kernel: arch/ia64/include/asm/elf.h. */
#define TARGET_ELF_NGREG        128
typedef struct target_elf_gregset_t {
    abi_ulong regs[TARGET_ELF_NGREG];
} target_elf_gregset_t;

#define HAVE_ELF_CORE_DUMP_FPREGSET 1

typedef struct target_elf_fpregset_t {
    abi_ulong fr[128][2];
} target_elf_fpregset_t;

/*
 * Like the kernel, point AT_SYSINFO at the syscall stub of the gate DSO.
 * glibc makes all of its system calls through it.
 */
#define DLINFO_ARCH_ITEMS (vdso_info != NULL)
#define ARCH_DLINFO                                     \
    do {                                                \
        if (vdso_info) {                                \
            NEW_AUX_ENT(AT_SYSINFO, vdso_info->entry);  \
        }                                               \
    } while (0)

#endif
