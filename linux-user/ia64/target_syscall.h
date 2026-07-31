/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2026 René Rebe <rene@exactco.de>
 */

#ifndef IA64_TARGET_SYSCALL_H
#define IA64_TARGET_SYSCALL_H

#define UNAME_MACHINE "ia64"
#define UNAME_MINIMUM_RELEASE "2.6.32"

/* arch/ia64/include/uapi/asm/unistd.h: __NR_Linux */
#define TARGET_SYSCALL_OFFSET 1024

/* arch/ia64/include/uapi/asm/break.h: __IA64_BREAK_SYSCALL */
#define TARGET_BREAK_SYSCALL 0x100000

#define TARGET_MCL_CURRENT 1
#define TARGET_MCL_FUTURE  2
#define TARGET_MCL_ONFAULT 4

#endif
