/* IA-64 TCG helper ABI adapters for exception return. */

#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/helper-proto.h"
#include "arch/arch.h"

void helper_raise_exception(CPUIA64State *env, uint32_t exception,
                            uint64_t fault_ip, uint64_t fault_imm,
                            uint32_t fault_slot)
{
    ia64_raise_exception(env, exception, fault_ip, fault_imm, fault_slot);
}

/*
 * GDB addresses an IA-64 instruction as bundle | slot, so a breakpoint
 * inside a bundle cannot be matched by the generic per-TB check in
 * cpu_exec().  That check does put a CF_BP_PAGE translation in place for
 * the whole page, and this runs at the start of every slot of one.
 */
void helper_check_breakpoint(CPUIA64State *env, uint64_t bundle_ip,
                             uint32_t slot)
{
    CPUState *cs = env_cpu(env);

    if (!cpu_breakpoint_test(cs, bundle_ip | slot, BP_GDB)) {
        return;
    }
    env->ip = bundle_ip;
    env->psr = (env->psr & ~IA64_PSR_RI_MASK) |
               ((uint64_t)slot << IA64_PSR_RI_SHIFT);
    cs->exception_index = EXCP_DEBUG;
    cpu_loop_exit(cs);
}

void helper_check_native_traps(CPUIA64State *env, uint64_t target_ip,
                               uint64_t source_ip, uint32_t slots,
                               uint32_t conditions, uint64_t base_isr,
                               uint64_t trap_psr)
{
    ia64_check_native_traps(env, target_ip, source_ip, slots, conditions,
                            base_isr, trap_psr);
}

void helper_ia32_unsupported(CPUIA64State *env)
{
    ia64_ia32_unsupported(env);
}

void helper_raise_unaligned(CPUIA64State *env, uint64_t addr,
                            uint64_t isr_access, uint64_t fault_info)
{
    ia64_raise_unaligned(env, addr, isr_access, fault_info);
}

void helper_raise_nat_consumption(CPUIA64State *env, uint64_t isr_access,
                                  uint64_t fault_info)
{
    ia64_raise_nat_consumption(env, isr_access, fault_info);
}

void helper_rfi(CPUIA64State *env, uint64_t fault_ip, uint32_t fault_slot)
{
    ia64_rfi(env, fault_ip, fault_slot);
}
