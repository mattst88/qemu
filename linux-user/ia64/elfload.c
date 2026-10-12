/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2026 René Rebe <rene@exactco.de>
 */

#include "qemu/osdep.h"
#include "qemu.h"
#include "loader.h"
#include "target_elf.h"
#include "target/ia64/arch/arch.h"
#include "target/ia64/fpreg.h"


const char *get_elf_cpu_model(uint32_t eflags)
{
    return "itanium2";
}

/*
 * See linux kernel: arch/ia64/kernel/process.c, do_copy_task_regs().
 * As there, the whole register stack is written to the backing store, and
 * ar.bsp and ar.rnat are those of its end, past the current frame.
 */
void elf_core_copy_regs(target_elf_gregset_t *r, const CPUIA64State *cenv)
{
    /* The task is stopped; flushing changes nothing it can observe. */
    CPUIA64State *env = (CPUIA64State *)cenv;
    uint64_t rnat = ia64_gdb_flush_rse(env);
    abi_ulong *regs = r->regs;

    memset(r, 0, sizeof(*r));

    for (int i = 1; i < 32; i++) {
        regs[i] = tswapal(env->gr[i]);
    }
    regs[32] = tswapal(env->nat[0] & 0xfffffffeULL);
    regs[33] = tswapal(ia64_user_get_pr(env));
    for (int i = 0; i < 8; i++) {
        regs[34 + i] = tswapal(env->br[i]);
    }
    regs[42] = tswapal(env->ip |
                       ((env->psr & IA64_PSR_RI_MASK) >> IA64_PSR_RI_SHIFT));
    regs[43] = tswapal(ia64_rse_current_cfm(env));
    regs[44] = tswapal(env->psr & IA64_PSR_UM_MASK);
    regs[45] = tswapal(env->ar_rsc);
    regs[46] = tswapal(ia64_gdb_read_bsp(env));
    regs[47] = tswapal(env->ar_bspstore);
    regs[48] = tswapal(rnat);
    regs[49] = tswapal(env->ar_ccv);
    regs[50] = tswapal(env->ar_unat);
    regs[51] = tswapal(env->ar_fpsr);
    regs[52] = tswapal(env->ar_pfs);
    regs[53] = tswapal(env->ar_lc);
    regs[54] = tswapal(env->ar_ec);
    regs[55] = tswapal(env->ar_csd);
    regs[56] = tswapal(env->ar_ssd);
}

void elf_core_copy_fpregs(target_elf_fpregset_t *r, const CPUIA64State *env)
{
    for (int i = 0; i < 128; i++) {
        uint64_t low, high;

        ia64_fpreg_to_spill(env, i, &low, &high);
        r->fr[i][0] = tswapal(low);
        r->fr[i][1] = tswapal(high);
    }
}
