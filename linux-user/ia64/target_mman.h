/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2026 René Rebe <rene@exactco.de>
 */

#ifndef IA64_TARGET_MMAN_H
#define IA64_TARGET_MMAN_H

/*
 * The eight IA-64 regions selected by bits 63:61 are 2EiB apart, far more
 * than a host can map behind a single guest_base.  Fold each region down
 * to a 1TiB window; see include/user/guest-fold.h.
 */
#define TARGET_GUEST_FOLD_REGION_SHIFT 61
#define TARGET_GUEST_FOLD_SHIFT        40

/* arch/ia64/include/asm/processor.h: DEFAULT_MAP_BASE */
#define TASK_UNMAPPED_BASE      0x2000000000000000ull

/* arch/ia64/include/asm/elf.h: TASK_UNMAPPED_BASE + 0x800000000 */
#define ELF_ET_DYN_BASE         (TASK_UNMAPPED_BASE + 0x800000000ull)

/* arch/ia64/include/uapi/asm/mman.h: register stack-like segment */
#define TARGET_MAP_GROWSUP      0x0200

#include "../generic/target_mman.h"

#endif
