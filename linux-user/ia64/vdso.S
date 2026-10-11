/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * IA-64 linux replacement vdso: the equivalent of the kernel's gate DSO
 * (arch/ia64/kernel/gate.S).
 *
 * It provides the syscall stubs that AT_SYSINFO and the ELF entry point
 * refer to, and the signal trampoline.  Having the trampoline in a DSO,
 * rather than in an anonymous page, is what gives it unwind info: unwinders
 * find the gate DSO with dl_iterate_phdr() or getunwind() and recognise a
 * signal frame by the ".unwabi 3, 's'" marker of the trampoline.
 */

#define __NR_rt_sigreturn       1181
#define __BREAK_SYSCALL         0x100000

/*
 * struct target_sigframe and struct target_sigcontext, relative to the sp
 * the trampoline runs with, which is 16 bytes below the sigframe.
 */
#define ARG0_OFF                (16 + 0)
#define ARG1_OFF                (16 + 8)
#define ARG2_OFF                (16 + 16)
#define SIGHANDLER_OFF          (16 + 24)
#define SIGCONTEXT_OFF          (16 + 160)

#define IP_OFF                  40
#define CFM_OFF                 48
#define BSP_OFF                 72
#define UNAT_OFF                96
#define FPSR_OFF                104
#define PR_OFF                  128
#define SP_OFF                  (200 + 12 * 8)

        .text

        /*
         * Called with "br.call.sptk.many b6 = b7", the syscall number in
         * r15 and the arguments in the caller's output registers.  For
         * syscall restart to work the break has to be the first instruction
         * of the bundle.
         */
        .align  32
        .global __kernel_syscall_via_break
        .proc   __kernel_syscall_via_break
__kernel_syscall_via_break:
        .prologue
        .altrp  b6
        .body
        break   __BREAK_SYSCALL
        nop.i   0
        br.ret.sptk.many b6
        .endp   __kernel_syscall_via_break

        /* There is no privilege to gain here, so this is the same thing. */
        .align  32
        .global __kernel_syscall_via_epc
        .proc   __kernel_syscall_via_epc
__kernel_syscall_via_epc:
        .prologue
        .altrp  b6
        .body
        break   __BREAK_SYSCALL
        nop.i   0
        br.ret.sptk.many b6
        .endp   __kernel_syscall_via_epc

        /*
         * The trampoline runs with an empty register frame.  setup_rt_frame()
         * has done the cover and filled in all of the sigcontext, including
         * sc_ar_bsp, so the interrupted state is described from the first
         * instruction on.  It loads signum, the siginfo pointer and the
         * sigcontext pointer from the sigframe, dereferences the handler's
         * plabel, calls the handler and issues rt_sigreturn.
         */
        .align  32
        .global __kernel_sigtramp
        .proc   __kernel_sigtramp
__kernel_sigtramp:
        .prologue
        .unwabi 3, 's'
        .unwabi @svr4, 's'
        .savesp ar.unat, UNAT_OFF + SIGCONTEXT_OFF
        .savesp ar.fpsr, FPSR_OFF + SIGCONTEXT_OFF
        .savesp pr, PR_OFF + SIGCONTEXT_OFF
        .savesp rp, IP_OFF + SIGCONTEXT_OFF
        .savesp ar.pfs, CFM_OFF + SIGCONTEXT_OFF
        .vframesp SP_OFF + SIGCONTEXT_OFF
        .spillsp ar.bsp, BSP_OFF + SIGCONTEXT_OFF
        .body
        alloc   r2 = ar.pfs, 0, 0, 3, 0
        adds    r16 = ARG0_OFF, r12
        ;;
        ld8     out0 = [r16], ARG1_OFF - ARG0_OFF
        ;;
        ld8     out1 = [r16], ARG2_OFF - ARG1_OFF
        ;;
        ld8     out2 = [r16], SIGHANDLER_OFF - ARG2_OFF
        ;;
        ld8     r17 = [r16]
        ;;
        ld8     r18 = [r17], 8
        ;;
        ld8     gp = [r17]
        mov     b6 = r18
        ;;
        br.call.sptk.many rp = b6
        ;;
        mov     r15 = __NR_rt_sigreturn
        ;;
        .restore sp
        break   __BREAK_SYSCALL
        ;;
        .endp   __kernel_sigtramp
