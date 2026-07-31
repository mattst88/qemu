/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * Folding of a sparse guest virtual address space into the host's.
 *
 * Copyright (c) 2026 René Rebe <rene@exactco.de>
 */

#ifndef USER_GUEST_FOLD_H
#define USER_GUEST_FOLD_H

#include "exec/vaddr.h"

/*
 * A guest may select one of a few widely spaced virtual regions with the
 * top bits of an address.  IA-64 does so with bits 63:61, and Linux puts
 * the executable text, its data and the mmap arena in different regions,
 * which spans far more than the 47 bits of user address space a host such
 * as x86-64 offers.  No single guest_base offset can map that, so move the
 * region number down next to a per-region window instead: the guest space
 * then occupies guest_fold_span bytes of contiguous host address space
 * that guest_base points at.  Anything a guest places beyond the window of
 * its region is rejected rather than silently aliased.
 *
 * These live as variables rather than target #defines because the code
 * that folds is shared by every target (accel/tcg/user-exec.c).  The
 * defaults below are the identity mapping, at the cost of a few always
 * predictable ALU operations for targets that do not need folding.
 */
extern uint64_t guest_fold_off_mask;    /* offset bits kept within a region */
extern uint64_t guest_fold_span;        /* folded space size; 0 when unused */
extern uint8_t guest_fold_region_shift; /* first guest region-select bit */
extern uint8_t guest_fold_shift;        /* first folded region-select bit */

#define GUEST_FOLD_DISABLED_SHIFT 63

static inline bool guest_addr_is_folded(void)
{
    return guest_fold_span != 0;
}

/*
 * Idempotent: an address that already lies in the folded space is returned
 * unchanged.  Generated code folds addresses itself (tcg/tcg-op-ldst.c) and
 * its helper slow paths land back here, so folding twice must be harmless.
 * The identity on already folded values is exact because a guest address
 * below guest_fold_span can only be a region 0 address, which folds to
 * itself.
 */
static inline vaddr guest_addr_fold(vaddr x)
{
    if (x < guest_fold_span) {
        return x;
    }
    return ((x >> guest_fold_region_shift) << guest_fold_shift) |
           (x & guest_fold_off_mask);
}

static inline vaddr guest_addr_unfold(vaddr x)
{
    return ((x >> guest_fold_shift) << guest_fold_region_shift) |
           (x & guest_fold_off_mask);
}

/* False if @x would alias another address once folded. */
static inline bool guest_addr_foldable(vaddr x)
{
    uint64_t hole = ((1ull << guest_fold_region_shift) - 1) &
                    ~guest_fold_off_mask;

    return (x & hole) == 0;
}

/* Last address of the per-region window that @x lives in. */
static inline vaddr guest_addr_region_last(vaddr x)
{
    return (x & ~guest_fold_off_mask) | guest_fold_off_mask;
}

static inline void guest_addr_fold_init(unsigned region_shift, unsigned shift)
{
    guest_fold_region_shift = region_shift;
    guest_fold_shift = shift;
    guest_fold_off_mask = (1ull << shift) - 1;
    guest_fold_span = (1ull << (64 - region_shift)) << shift;
}

/*
 * Definition of the folding variables, for the per-target file that owns
 * guest_base.  Folding stays off until guest_addr_fold_init() turns it on.
 */
#define GUEST_FOLD_DEFINE_STATE()                                       \
    uint64_t guest_fold_off_mask = UINT64_MAX;                          \
    uint64_t guest_fold_span;                                           \
    uint8_t guest_fold_region_shift = GUEST_FOLD_DISABLED_SHIFT;        \
    uint8_t guest_fold_shift = GUEST_FOLD_DISABLED_SHIFT

#endif /* USER_GUEST_FOLD_H */
