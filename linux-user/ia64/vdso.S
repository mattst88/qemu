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
#define RNAT_OFF                80
#define UNAT_OFF                96
#define FPSR_OFF                104
#define PR_OFF                  128
#define SP_OFF                  (200 + 12 * 8)
#define RBS_BASE_OFF            2512
#define LOADRS_OFF              2520

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
         *
         * As in the kernel, the memory stack is switched to the alternate
         * signal stack by setup_rt_frame() but the register stack is switched
         * here: a non-zero sc_rbs_base is the address of the new backing
         * store.  The dirty partition is not flushed to the old backing
         * store, which may well be what overflowed; it moves to the new one,
         * and sc_loadrs records its size so that it can be loaded back.
         *
         * On the way out, AR.BSP differs from sc_ar_bsp if the backing store
         * was switched, or if the handler changed sc_ar_bsp in order to
         * resume elsewhere.  Either way sc_ar_bsp is where to go back to.
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
        adds    r14 = RBS_BASE_OFF + SIGCONTEXT_OFF, r12
        ;;
        ld8     r15 = [r14]
        ;;
        cmp.ne  p1, p0 = r15, r0
        .spillsp.p p1, ar.rnat, RNAT_OFF + SIGCONTEXT_OFF
(p1)    br.cond.spnt.few .Lsetup_rbs
        ;;
.Lback_from_setup_rbs:
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
        adds    r14 = BSP_OFF + SIGCONTEXT_OFF, r12
        ;;
        ld8     r15 = [r14]
        mov     r14 = ar.bsp
        ;;
        cmp.ne  p1, p0 = r14, r15
(p1)    br.cond.spnt.few .Lrestore_rbs
        ;;
.Lback_from_restore_rbs:
        mov     r15 = __NR_rt_sigreturn
        ;;
        .restore sp
        break   __BREAK_SYSCALL
        ;;

        /*
         * Switch to the backing store at r15.  AR.BSPSTORE can only be
         * written in enforced lazy mode, and writing it leaves AR.RNAT
         * undefined, so that is saved first: the interrupted context's
         * sc_ar_rnat is the one of the old backing store.  If the old
         * AR.BSPSTORE is at a NaT collection slot, the collection is
         * complete and belongs in memory.
         */
.Lsetup_rbs:
        mov     ar.rsc = r0
        ;;
        mov     r19 = ar.rnat
        mov     r18 = ar.bspstore
        ;;
        mov     ar.bspstore = r15
        ;;
        adds    r14 = RNAT_OFF + SIGCONTEXT_OFF, r12
        mov     r16 = ar.bsp
        extr.u  r20 = r18, 3, 6
        ;;
        st8     [r14] = r19
        sub     r16 = r16, r15
        cmp.eq  p8, p0 = 63, r20
        ;;
        adds    r14 = LOADRS_OFF + SIGCONTEXT_OFF, r12
        shl     r16 = r16, 16
        ;;
        st8     [r14] = r16
(p8)    st8     [r18] = r19
        mov     r20 = 0xf
        ;;
        invala
        mov     ar.rsc = r20
        br.cond.sptk.few .Lback_from_setup_rbs
        ;;

        /*
         * Go back to the backing store of the interrupted context, with
         * r14 = AR.BSP and r15 = sc_ar_bsp.  The sc_loadrs >> 16 bytes below
         * AR.BSP are the interrupted context's dirty partition.  loadrs
         * brings its registers back, and AR.BSPSTORE then has to be as far
         * below sc_ar_bsp as it takes to hold the same number of registers,
         * which need not be the same number of bytes: the NaT collection
         * slots fall in different places in the two backing stores.
         */
.Lrestore_rbs:
        adds    r16 = LOADRS_OFF + SIGCONTEXT_OFF, r12
        adds    r17 = RNAT_OFF + SIGCONTEXT_OFF, r12
        ;;
        ld8     r16 = [r16]
        ld8     r17 = [r17]
        ;;
        shr.u   r18 = r16, 16           /* size of the dirty partition */
        ;;
        sub     r19 = r14, r18          /* where it starts */
        shr.u   r18 = r18, 3            /* slots in it */
        ;;
        extr.u  r19 = r19, 3, 6
        ;;
        add     r19 = r19, r18
        ;;
        shr.u   r19 = r19, 6            /* of which NaT collections */
        ;;
        sub     r18 = r18, r19          /* registers in it */
        extr.u  r19 = r15, 3, 6
        ;;
        sub     r19 = r18, r19
        ;;
        adds    r19 = 62, r19
        ;;
        shr.u   r19 = r19, 6            /* NaT collections below sc_ar_bsp */
        ;;
        add     r18 = r18, r19
        ;;
        shl     r18 = r18, 3
        ;;
        sub     r15 = r15, r18
        alloc   r2 = ar.pfs, 0, 0, 0, 0
        ;;
        mov     ar.rsc = r16
        ;;
        loadrs
        ;;
        mov     ar.bspstore = r15
        ;;
        mov     ar.rnat = r17
        mov     r18 = 0xf
        ;;
        invala
        mov     ar.rsc = r18
        br.cond.sptk.few .Lback_from_restore_rbs
        ;;
        .endp   __kernel_sigtramp
