/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * IA-64 specific CPU ABI and functions for linux-user
 *
 * Copyright (c) 2026 René Rebe <rene@exactco.de>
 */

#ifndef IA64_TARGET_CPU_H
#define IA64_TARGET_CPU_H

/*
 * clone2() hands the child a region for both its memory stack and its
 * register backing store.  do_fork() only forwards the stack pointer, so
 * the backing store base is stashed here in between.
 */
extern __thread abi_ulong ia64_clone_rbs_base;

static inline void cpu_clone_set_rbs(CPUIA64State *env, abi_ulong base)
{
    ia64_clone_rbs_base = base;
}

static inline void cpu_clone_regs_child(CPUIA64State *env, target_ulong newsp,
                                        unsigned flags)
{
    if (newsp) {
        env->gr[12] = newsp;
        if (ia64_clone_rbs_base) {
            ia64_user_set_rbs(env, ia64_clone_rbs_base);
        }
    }
    /* Success, with a zero return value. */
    env->gr[8] = 0;
    env->gr[10] = 0;
    env->nat[0] &= ~((1ULL << 8) | (1ULL << 10) | (1ULL << 12));
}

static inline void cpu_clone_regs_parent(CPUIA64State *env, unsigned flags)
{
}

static inline void cpu_set_tls(CPUIA64State *env, target_ulong newtls)
{
    /* r13 is the thread pointer on IA-64. */
    env->gr[13] = newtls;
    env->nat[0] &= ~(1ULL << 13);
}

static inline abi_ulong get_sp_from_cpustate(CPUIA64State *state)
{
    return state->gr[12];
}

#endif
