/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * QEMU IA-64 user cpu_loop.
 *
 * Copyright (c) 2026 René Rebe <rene@exactco.de>
 */

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qemu.h"
#include "user-internals.h"
#include "user/cpu_loop.h"
#include "user-mmap.h"
#include "signal-common.h"
#include "target_syscall.h"

/*
 * Size of the register backing store handed to a new process.  The real
 * kernel shares a 2GiB region between the memory stack and the RSE
 * backing store; the backing store only ever needs a fraction of that,
 * and the pages are faulted in on demand.
 */
#define IA64_RBS_SIZE (64 * MiB)

__thread abi_ulong ia64_clone_rbs_base;

/* The reservation for the main thread's register backing store. */
static abi_ulong ia64_rbs_start, ia64_rbs_end;

/*
 * The kernel's backing store is a VM_GROWSUP mapping that is only as big as
 * what has been used, so that a program can map something right above it,
 * typically a guard page, by passing the address as a hint.  Ours is mapped
 * in full from the start.  Treat what the calling thread has above its
 * AR.BSP as free for whoever asks for it by address.
 */
bool ia64_mmap_hint_is_free(abi_ulong start, abi_ulong len)
{
    CPUIA64State *env = cpu_env(thread_cpu);
    abi_ulong top = MAX(env->ar_bsp, env->ar_bspstore);

    if (top < ia64_rbs_start || top >= ia64_rbs_end) {
        return false;
    }
    top = TARGET_PAGE_ALIGN(top);
    return start >= top && start < ia64_rbs_end &&
           len <= ia64_rbs_end - start;
}

/* Resume at the instruction following the one that faulted. */
static void ia64_advance_slot(CPUIA64State *env)
{
    uint64_t slot = env->exception_state.fault_slot;

    if (slot >= 2) {
        env->ip = env->exception_state.fault_ip + 16;
        slot = 0;
    } else {
        env->ip = env->exception_state.fault_ip;
        slot++;
    }
    env->psr = (env->psr & ~IA64_PSR_RI_MASK) | (slot << IA64_PSR_RI_SHIFT);
}

/* Re-execute the faulting instruction, e.g. for a restarted system call. */
static void ia64_repeat_slot(CPUIA64State *env)
{
    env->ip = env->exception_state.fault_ip;
    env->psr = (env->psr & ~IA64_PSR_RI_MASK) |
               ((uint64_t)env->exception_state.fault_slot << IA64_PSR_RI_SHIFT);
}

static uint64_t ia64_syscall_arg(CPUIA64State *env, unsigned n)
{
    unsigned reg = IA64_STACKED_GR_BASE + env->cfm_sol + n;

    return reg < IA64_GR_COUNT ? env->gr[reg] : 0;
}

static void ia64_set_syscall_result(CPUIA64State *env, abi_long ret)
{
    if (is_error(ret)) {
        env->gr[8] = -ret;
        env->gr[10] = -1;
    } else {
        env->gr[8] = ret;
        env->gr[10] = 0;
    }
    /* Neither result register is ever a NaT. */
    env->nat[0] &= ~((1ULL << 8) | (1ULL << 10));
}

/*
 * IA-64 specific si_code values (asm-generic/siginfo.h) that linux-user has
 * no generic name for.
 */
#define TARGET_ILL_BREAK        10
#define TARGET_ILL_BNDMOD       11
#define TARGET_FPE_DECOVF        9
#define TARGET_FPE_DECDIV       10
#define TARGET_FPE_DECERR       11
#define TARGET_FPE_INVASC       12
#define TARGET_FPE_INVDEC       13
#define TARGET_SEGV_PSTKOVF      4

/*
 * arch/ia64/kernel/traps.c:ia64_bad_break().  A break is a fault, so the
 * instruction pointer stays on the faulting bundle and the reported address
 * carries the slot number in its low bits - which is also how a debugger
 * addresses an individual instruction.
 */
static void ia64_break_signal(CPUIA64State *env)
{
    uint64_t imm = env->exception_state.fault_imm;
    int sig = TARGET_SIGILL;
    int code = TARGET_ILL_ILLOPC;
    target_siginfo_t info = {};

    switch (imm) {
    case 0:                             /* __builtin_abort() */
        break;
    case 1:
        sig = TARGET_SIGFPE, code = TARGET_FPE_INTDIV;
        break;
    case 2:
        sig = TARGET_SIGFPE, code = TARGET_FPE_INTOVF;
        break;
    case 3:
        sig = TARGET_SIGFPE, code = TARGET_FPE_FLTSUB;
        break;
    case 4:
        sig = TARGET_SIGSEGV, code = TARGET_SEGV_MAPERR;
        break;
    case 5:
        /* The kernel really does pair SIGSEGV with BUS_ADRALN here. */
        sig = TARGET_SIGSEGV, code = TARGET_BUS_ADRALN;
        break;
    case 6:
        sig = TARGET_SIGFPE, code = TARGET_FPE_DECOVF;
        break;
    case 7:
        sig = TARGET_SIGFPE, code = TARGET_FPE_DECDIV;
        break;
    case 8:
        sig = TARGET_SIGFPE, code = TARGET_FPE_DECERR;
        break;
    case 9:
        sig = TARGET_SIGFPE, code = TARGET_FPE_INVASC;
        break;
    case 10:
        sig = TARGET_SIGFPE, code = TARGET_FPE_INVDEC;
        break;
    case 11:
        sig = TARGET_SIGSEGV, code = TARGET_SEGV_PSTKOVF;
        break;
    case 0x3f000 ... 0x3ffff:           /* bundle update in progress */
        code = TARGET_ILL_BNDMOD;
        break;
    default:
        if (imm < 0x80000) {
            code = TARGET_ILL_BREAK;
        } else {
            sig = TARGET_SIGTRAP, code = TARGET_TRAP_BRKPT;
        }
        break;
    }

    /* A break reports its immediate and no interruption status. */
    info.si_signo = sig;
    info.si_errno = 0;
    info.si_code = code;
    info._sifields._sigfault._addr =
        env->exception_state.fault_ip | env->exception_state.fault_slot;
    info._sifields._sigfault._imm = imm;
    queue_signal(env, sig, QEMU_SI_FAULT, &info);
}

static void ia64_do_syscall(CPUIA64State *env)
{
    abi_long ret;

    ia64_advance_slot(env);

    ret = do_syscall(env, env->gr[15],
                     ia64_syscall_arg(env, 0), ia64_syscall_arg(env, 1),
                     ia64_syscall_arg(env, 2), ia64_syscall_arg(env, 3),
                     ia64_syscall_arg(env, 4), ia64_syscall_arg(env, 5),
                     ia64_syscall_arg(env, 6), ia64_syscall_arg(env, 7));

    if (ret == -QEMU_ERESTARTSYS) {
        ia64_repeat_slot(env);
        return;
    }
    if (ret == -QEMU_ESIGRETURN || ret == -QEMU_ESETPC) {
        /* State has already been set by sigreturn or by a plugin. */
        return;
    }
    ia64_set_syscall_result(env, ret);
}

void cpu_loop(CPUIA64State *env)
{
    CPUState *cs = env_cpu(env);
    int trapnr;

    for (;;) {
        cpu_exec_start(cs);
        trapnr = cpu_exec(cs);
        cpu_exec_end(cs);
        qemu_process_cpu_events(cs);

        switch (trapnr) {
        case EXCP_INTERRUPT:
            /* just indicate that signals should be handled asap */
            break;

        case IA64_EXCP_BREAK:
            if (env->exception_state.fault_imm == TARGET_BREAK_SYSCALL) {
                ia64_do_syscall(env);
            } else {
                ia64_repeat_slot(env);
                ia64_break_signal(env);
            }
            break;

        case IA64_EXCP_ILLEGAL:
        case IA64_EXCP_RESERVED_TEMPLATE:
        case IA64_EXCP_DISABLED_ISA_TRANSITION:
            force_sig_fault(TARGET_SIGILL, TARGET_ILL_ILLOPC, env->exception_state.fault_ip);
            break;

        case IA64_EXCP_PRIVILEGED_OP:
        case IA64_EXCP_PRIVILEGED_REG:
            force_sig_fault(TARGET_SIGILL, TARGET_ILL_PRVOPC, env->exception_state.fault_ip);
            break;

        case IA64_EXCP_RESERVED_REG_FIELD:
        case IA64_EXCP_NAT_CONSUMPTION:
        case IA64_EXCP_UNSUPPORTED_DATA_REFERENCE:
            force_sig_fault(TARGET_SIGILL, TARGET_ILL_ILLOPN, env->exception_state.fault_ip);
            break;

        case IA64_EXCP_DISABLED_FP:
            force_sig_fault(TARGET_SIGFPE, TARGET_FPE_FLTUNK, env->exception_state.fault_ip);
            break;

        case IA64_EXCP_FP_FAULT:
        case IA64_EXCP_FP_TRAP:
            force_sig_fault(TARGET_SIGFPE, TARGET_FPE_FLTINV, env->exception_state.fault_ip);
            break;

        case IA64_EXCP_UNALIGNED:
            force_sig_fault(TARGET_SIGBUS, TARGET_BUS_ADRALN, env->cr_ifa);
            break;

        case IA64_EXCP_UNIMPL_DATA_ADDR:
            force_sig_fault(TARGET_SIGBUS, TARGET_BUS_ADRERR, env->cr_ifa);
            break;

        case IA64_EXCP_UNIMPL_INST_ADDR:
            force_sig_fault(TARGET_SIGBUS, TARGET_BUS_ADRERR, env->ip);
            break;

        case IA64_EXCP_KEY_PERMISSION:
        case IA64_EXCP_INST_ACCESS:
        case IA64_EXCP_DATA_ACCESS:
            force_sig_fault(TARGET_SIGSEGV, TARGET_SEGV_ACCERR, env->cr_ifa);
            break;

        case IA64_EXCP_PAGE_NOT_PRESENT:
        case IA64_EXCP_ITLB_FAULT:
        case IA64_EXCP_DTLB_FAULT:
        case IA64_EXCP_ALT_ITLB:
        case IA64_EXCP_ALT_DTLB:
            force_sig_fault(TARGET_SIGSEGV, TARGET_SEGV_MAPERR, env->cr_ifa);
            break;

        case EXCP_DEBUG:
            force_sig_fault(TARGET_SIGTRAP, TARGET_TRAP_BRKPT,
                            env->ip |
                            ((env->psr & IA64_PSR_RI_MASK) >>
                             IA64_PSR_RI_SHIFT));
            break;

        case EXCP_ATOMIC:
            cpu_exec_step_atomic(cs);
            break;

        default:
            EXCP_DUMP(env, "qemu: unhandled CPU exception 0x%x - aborting\n",
                      trapnr);
            exit(EXIT_FAILURE);
        }

        env->exception_state.exception = IA64_EXCP_NONE;
        process_pending_signals(env);
    }
}

void init_main_thread(CPUState *cs, struct image_info *info)
{
    CPUIA64State *env = cpu_env(cs);
    abi_ulong rbs;

    /*
     * arch/ia64/include/asm/processor.h:start_thread().  Run at the least
     * privileged level with instruction, data and RSE translation on.
     */
    env->psr = IA64_PSR_IT | IA64_PSR_DT | IA64_PSR_RT | IA64_PSR_IC |
               IA64_PSR_I | IA64_PSR_BN |
               ((uint64_t)3 << IA64_PSR_CPL_SHIFT);
    env->ar_fpsr = IA64_FPSR_DEFAULT;

    env->ip = info->entry;
    /* The kernel leaves a 16 byte scratch area below the argument block. */
    env->gr[12] = info->start_stack - 16;

    /* Eager RSE mode at privilege level 3. */
    env->ar_rsc = IA64_RSC_MODE | (3ULL << IA64_RSC_PL_SHIFT);

    rbs = target_mmap(0, IA64_RBS_SIZE, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (rbs == -1) {
        fprintf(stderr, "Unable to reserve the register backing store\n");
        exit(EXIT_FAILURE);
    }
    ia64_rbs_start = rbs;
    ia64_rbs_end = rbs + IA64_RBS_SIZE;
    env->ar_bsp = rbs;
    env->ar_bspstore = rbs;
}
