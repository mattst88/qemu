/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * IA-64 emulation of Linux signals
 *
 * Copyright (c) 2026 René Rebe <rene@exactco.de>
 */

#include "qemu/osdep.h"
#include "qemu.h"
#include "user-internals.h"
#include "signal-common.h"
#include "linux-user/trace.h"
#include "target/ia64/fpreg.h"

/* arch/ia64/include/uapi/asm/sigcontext.h */
#define TARGET_IA64_SC_FLAG_ONSTACK     (1 << 0)
#define TARGET_IA64_SC_FLAG_IN_SYSCALL  (1 << 1)
#define TARGET_IA64_SC_FLAG_FPH_VALID   (1 << 2)

/* struct ia64_fpreg: the architected 128 bit spill format, 16 byte aligned. */
struct target_ia64_fpreg {
    abi_ulong bits[2];
} QEMU_ALIGNED(16);

struct target_sigcontext {
    abi_ulong sc_flags;
    abi_ulong sc_nat;
    target_stack_t sc_stack;
    abi_ulong sc_ip;
    abi_ulong sc_cfm;
    abi_ulong sc_um;
    abi_ulong sc_ar_rsc;
    abi_ulong sc_ar_bsp;
    abi_ulong sc_ar_rnat;
    abi_ulong sc_ar_ccv;
    abi_ulong sc_ar_unat;
    abi_ulong sc_ar_fpsr;
    abi_ulong sc_ar_pfs;
    abi_ulong sc_ar_lc;
    abi_ulong sc_pr;
    abi_ulong sc_br[8];
    /* sc_gr[0] doubles as the uc_link member of ucontext_t. */
    abi_ulong sc_gr[32];
    struct target_ia64_fpreg sc_fr[128];
    abi_ulong sc_rbs_base;
    abi_ulong sc_loadrs;
    abi_ulong sc_ar25;
    abi_ulong sc_ar26;
    abi_ulong sc_rsvd[12];
    target_sigset_t sc_mask;
};

QEMU_BUILD_BUG_ON(sizeof(struct target_sigcontext) != 2656);
QEMU_BUILD_BUG_ON(offsetof(struct target_sigcontext, sc_ip) != 40);
QEMU_BUILD_BUG_ON(offsetof(struct target_sigcontext, sc_pr) != 128);
QEMU_BUILD_BUG_ON(offsetof(struct target_sigcontext, sc_gr) != 200);
QEMU_BUILD_BUG_ON(offsetof(struct target_sigcontext, sc_fr) != 464);
QEMU_BUILD_BUG_ON(offsetof(struct target_sigcontext, sc_rbs_base) != 2512);
QEMU_BUILD_BUG_ON(offsetof(struct target_sigcontext, sc_mask) != 2640);

/*
 * arch/ia64/kernel/signal.c.  arg0..arg2 and the handler plabel are part of
 * the IA-64 Linux ABI: user level unwinders look for them at the bottom of
 * the frame, and the trampoline loads its call arguments from there.
 */
struct target_sigframe {
    abi_ulong arg0;             /* signum */
    abi_ulong arg1;             /* siginfo pointer */
    abi_ulong arg2;             /* sigcontext pointer */
    abi_ulong handler;          /* pointer to the handler's plabel */
    struct target_siginfo info;
    struct target_sigcontext sc;
};

QEMU_BUILD_BUG_ON(offsetof(struct target_sigframe, info) != 32);
QEMU_BUILD_BUG_ON(offsetof(struct target_sigframe, sc) != 160);

#define TARGET_IA64_STACK_ALIGN 16

static void setup_sigcontext(struct target_sigcontext *sc, CPUIA64State *env,
                             target_sigset_t *set, abi_ulong flags,
                             uint64_t cfm, uint64_t bsp, abi_ulong rbs_base)
{
    uint64_t low, high;
    int i;

    __put_user(flags | TARGET_IA64_SC_FLAG_FPH_VALID, &sc->sc_flags);
    /* Bit i of sc_nat is the NaT bit of gr[i]; gr[0] is never a NaT. */
    __put_user(env->nat[0] & 0xfffffffeULL, &sc->sc_nat);

    /* The kernel keeps the slot number in the low two bits of sc_ip. */
    __put_user(env->ip |
               ((env->psr & IA64_PSR_RI_MASK) >> IA64_PSR_RI_SHIFT),
               &sc->sc_ip);
    __put_user(cfm, &sc->sc_cfm);
    __put_user(env->psr & IA64_PSR_UM_MASK, &sc->sc_um);

    __put_user(env->ar_rsc, &sc->sc_ar_rsc);
    __put_user(bsp, &sc->sc_ar_bsp);
    __put_user(env->ar_rnat, &sc->sc_ar_rnat);
    __put_user(env->ar_ccv, &sc->sc_ar_ccv);
    __put_user(env->ar_unat, &sc->sc_ar_unat);
    __put_user(env->ar_fpsr, &sc->sc_ar_fpsr);
    __put_user(env->ar_pfs, &sc->sc_ar_pfs);
    __put_user(env->ar_lc, &sc->sc_ar_lc);
    __put_user(ia64_user_get_pr(env), &sc->sc_pr);

    for (i = 0; i < 8; i++) {
        __put_user(env->br[i], &sc->sc_br[i]);
    }

    /* sc_gr[0] is uc_link rather than gr[0], which is architecturally zero. */
    __put_user(0, &sc->sc_gr[0]);
    for (i = 1; i < 32; i++) {
        __put_user(env->gr[i], &sc->sc_gr[i]);
    }

    for (i = 0; i < 128; i++) {
        ia64_fpreg_to_spill(env, i, &low, &high);
        __put_user(low, &sc->sc_fr[i].bits[0]);
        __put_user(high, &sc->sc_fr[i].bits[1]);
    }

    /* The trampoline switches to rbs_base, if any, and sets sc_loadrs. */
    __put_user(rbs_base, &sc->sc_rbs_base);
    __put_user(0, &sc->sc_loadrs);
    __put_user(env->ar_csd, &sc->sc_ar25);
    __put_user(env->ar_ssd, &sc->sc_ar26);

    for (i = 0; i < 12; i++) {
        __put_user(0, &sc->sc_rsvd[i]);
    }
    for (i = 0; i < TARGET_NSIG_WORDS; i++) {
        __put_user(set->sig[i], &sc->sc_mask.sig[i]);
    }
}

static void restore_sigcontext(CPUIA64State *env, struct target_sigcontext *sc)
{
    uint64_t value, low, high, cfm;
    int i;

    __get_user(value, &sc->sc_ip);
    env->ip = value & ~3ULL;
    env->psr = (env->psr & ~IA64_PSR_RI_MASK) |
               ((value & 3ULL) << IA64_PSR_RI_SHIFT);

    __get_user(value, &sc->sc_um);
    env->psr = (env->psr & ~IA64_PSR_UM_MASK) | (value & IA64_PSR_UM_MASK);

    __get_user(env->ar_rsc, &sc->sc_ar_rsc);
    __get_user(env->ar_ccv, &sc->sc_ar_ccv);
    __get_user(env->ar_unat, &sc->sc_ar_unat);
    __get_user(env->ar_fpsr, &sc->sc_ar_fpsr);
    __get_user(env->ar_pfs, &sc->sc_ar_pfs);
    __get_user(env->ar_lc, &sc->sc_ar_lc);
    __get_user(env->ar_csd, &sc->sc_ar25);
    __get_user(env->ar_ssd, &sc->sc_ar26);

    __get_user(value, &sc->sc_pr);
    ia64_user_set_pr(env, value);

    for (i = 0; i < 8; i++) {
        __get_user(env->br[i], &sc->sc_br[i]);
    }

    for (i = 1; i < 32; i++) {
        __get_user(env->gr[i], &sc->sc_gr[i]);
    }
    __get_user(value, &sc->sc_nat);
    env->nat[0] = (env->nat[0] & ~0xffffffffULL) | (value & 0xfffffffeULL);

    for (i = 0; i < 128; i++) {
        __get_user(low, &sc->sc_fr[i].bits[0]);
        __get_user(high, &sc->sc_fr[i].bits[1]);
        ia64_fpreg_from_spill(env, i, low, high);
    }

    /*
     * Re-establish the interrupted register frame last: the mandatory RSE
     * loads it may trigger refill the stacked registers.  As in the kernel,
     * the frame is found below the current AR.BSP; sc_ar_bsp and sc_ar_rnat
     * are for the trampoline, which has acted on them by now.
     */
    __get_user(cfm, &sc->sc_cfm);
    ia64_user_signal_return(env, cfm);
}

void setup_rt_frame(int sig, struct target_sigaction *ka,
                    target_siginfo_t *info,
                    target_sigset_t *set, CPUArchState *env)
{
    struct target_sigframe *frame;
    abi_ulong frame_addr, sp, flags, rbs_base = 0;
    TaskState *ts = get_task_state(thread_cpu);
    uint64_t cfm, bsp;

    sp = target_sigsp(get_sp_from_cpustate(env), ka);
    /*
     * When moving to the alternate signal stack, the register stack goes
     * there as well, growing up from its base, unless it already is there.
     */
    if (sp != get_sp_from_cpustate(env) &&
        env->ar_bspstore - ts->sigaltstack_used.ss_sp >=
        ts->sigaltstack_used.ss_size) {
        rbs_base = QEMU_ALIGN_UP(ts->sigaltstack_used.ss_sp, 8);
    }
    frame_addr = (sp - sizeof(*frame)) & -TARGET_IA64_STACK_ALIGN;

    trace_user_setup_rt_frame(env, frame_addr);

    if (!lock_user_struct(VERIFY_WRITE, frame, frame_addr, 0)) {
        goto give_sigsegv;
    }

    /*
     * Spill the interrupted register frame and switch to an empty one before
     * recording AR.BSP; this is the kernel's cover plus CR.IFS = 1 << 63.
     */
    ia64_user_signal_enter(env, &cfm, &bsp);

    __put_user(sig, &frame->arg0);
    __put_user(frame_addr + offsetof(struct target_sigframe, info),
               &frame->arg1);
    __put_user(frame_addr + offsetof(struct target_sigframe, sc),
               &frame->arg2);
    __put_user(ka->_sa_handler, &frame->handler);

    frame->info = *info;

    /* The kernel keys IA64_SC_FLAG_ONSTACK off the sigcontext address. */
    flags = on_sig_stack(frame_addr + offsetof(struct target_sigframe, sc)) ?
            TARGET_IA64_SC_FLAG_ONSTACK : 0;
    target_save_altstack(&frame->sc.sc_stack, env);
    setup_sigcontext(&frame->sc, env, set, flags, cfm, bsp, rbs_base);

    unlock_user_struct(frame, frame_addr, 1);

    /* The kernel leaves a 16 byte scratch area below the frame. */
    env->gr[12] = frame_addr - 16;
    env->nat[0] &= ~(1ULL << 12);
    env->ar_fpsr = IA64_FPSR_DEFAULT;

    /* Start in the first slot of the trampoline, forced little endian. */
    env->ip = default_rt_sigreturn;
    env->psr &= ~(IA64_PSR_RI_MASK | IA64_PSR_BE);
    return;

 give_sigsegv:
    force_sigsegv(sig);
}

long do_rt_sigreturn(CPUArchState *env)
{
    /* sys_rt_sigreturn() finds its frame at sp + 16. */
    abi_ulong frame_addr = get_sp_from_cpustate(env) + 16;
    struct target_sigframe *frame;
    sigset_t set;

    trace_user_do_rt_sigreturn(env, frame_addr);

    if (frame_addr & (TARGET_IA64_STACK_ALIGN - 1)) {
        goto badframe;
    }
    if (!lock_user_struct(VERIFY_READ, frame, frame_addr, 1)) {
        goto badframe;
    }

    target_to_host_sigset(&set, &frame->sc.sc_mask);
    set_sigmask(&set);

    restore_sigcontext(env, &frame->sc);
    target_restore_altstack(&frame->sc.sc_stack, env);

    unlock_user_struct(frame, frame_addr, 0);
    return -QEMU_ESIGRETURN;

 badframe:
    force_sig(TARGET_SIGSEGV);
    return -QEMU_ESIGRETURN;
}
