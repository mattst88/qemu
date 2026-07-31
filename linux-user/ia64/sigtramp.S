/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Reference source of the IA-64 signal trampoline that setup_sigtramp()
 * writes into the trampoline page.  The real kernel keeps the equivalent
 * code in the gate page as __kernel_sigtramp (arch/ia64/kernel/gate.S).
 *
 * The trampoline runs with an empty register frame, r12 pointing 16 bytes
 * below the sigframe.  It reads arg0/arg1/arg2 (signum, siginfo pointer,
 * sigcontext pointer) and the handler plabel from the sigframe, dereferences
 * the plabel into (entry point, gp), calls the handler and finally issues
 * rt_sigreturn.  Because the trampoline does the alloc and the plabel
 * dereference itself, setup_rt_frame() does not have to build an argument
 * frame or set gp.
 *
 * 1181 == TARGET_SYSCALL_OFFSET (1024) + 157 == __NR_rt_sigreturn.
 * 0x100000 == __IA64_BREAK_SYSCALL.
 *
 * This file is not built.  To regenerate the byte sequence in signal.c use
 * the IA-64 cross assembler from
 *   /srv/t2/build/ia64-desktop-26.8-desktop-ia64-itanium2-linux/TOOLCHAIN/cross/bin
 *
 *   ia64-t2-linux-gnu-as -o sigtramp.o sigtramp.S
 *   ia64-t2-linux-gnu-objdump -s -j .text sigtramp.o
 *
 * Copyright (c) 2026 René Rebe <rene@exactco.de>
 */

	.text
	.global __qemu_sigtramp
__qemu_sigtramp:
	alloc r2 = ar.pfs, 0, 0, 3, 0
	adds r16 = 16, r12
	;;
	ld8 out0 = [r16], 8
	;;
	ld8 out1 = [r16], 8
	;;
	ld8 out2 = [r16], 8
	;;
	ld8 r17 = [r16]
	;;
	ld8 r18 = [r17], 8
	;;
	ld8 gp = [r17]
	mov b6 = r18
	;;
	br.call.sptk.many b0 = b6
	;;
	mov r15 = 1181
	;;
	break.i 0x100000
	;;
