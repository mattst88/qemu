/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2026 René Rebe <rene@exactco.de>
 */

#ifndef IA64_TARGET_SIGNAL_H
#define IA64_TARGET_SIGNAL_H

#include "../generic/signal.h"

/*
 * arch/ia64/include/uapi/asm/signal.h.  The kernel's MINSIGSTKSZ has a
 * long-standing typo (131027 rather than 131072) that has to be kept.
 */
#undef TARGET_MINSIGSTKSZ
#define TARGET_MINSIGSTKSZ      131027
#define TARGET_SIGSTKSZ         262144

/*
 * The real kernel puts the trampoline in the gate page; a page of our own
 * serves the same purpose here.
 */
#define TARGET_ARCH_HAS_SIGTRAMP_PAGE 1

#endif /* IA64_TARGET_SIGNAL_H */
