/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Linker script for linux ia64 replacement vdso.
 */

VERSION {
        LINUX_2.5 {
        global:
                __kernel_syscall_via_break;
                __kernel_syscall_via_epc;
                __kernel_sigtramp;

        local: *;
        };
}

/* glibc uses the entry point when there is no AT_SYSINFO. */
ENTRY(__kernel_syscall_via_epc)

PHDRS {
        phdr            PT_PHDR         FLAGS(4) PHDRS;
        load            PT_LOAD         FLAGS(7) FILEHDR PHDRS;
        dynamic         PT_DYNAMIC      FLAGS(4);
        note            PT_NOTE         FLAGS(4);
        /* PT_IA_64_UNWIND, which ld does not know by name. */
        unwind          0x70000001      FLAGS(4);
}

SECTIONS {
        /*
         * We can't prelink to any address without knowing something about
         * the virtual memory space of the host, since that leaks over into
         * the available memory space of the guest.
         */
        . = SIZEOF_HEADERS;

        /*
         * The following, including the FILEHDRS and PHDRS, are modified
         * when we relocate the binary.  We want them to be initially
         * writable for the relocation; we'll force them read-only after.
         */
        .note           : { *(.note*) }         :load :note
        .dynamic        : { *(.dynamic) }       :load :dynamic
        .dynsym         : { *(.dynsym) }        :load
        /*
         * There ought not be any real read-write data.
         * But since we manipulated the segment layout,
         * we have to put these sections somewhere.
         */
        .data           : {
                *(.data*)
                *(.sdata*)
                *(.got.plt) *(.got)
                *(.gnu.linkonce.d.*)
                *(.bss*)
                *(.sbss*)
                *(.dynbss*)
                *(.gnu.linkonce.b.*)
        }

        .rodata         : { *(.rodata*) }
        .hash           : { *(.hash) }
        .gnu.hash       : { *(.gnu.hash) }
        .dynstr         : { *(.dynstr) }
        .gnu.version    : { *(.gnu.version) }
        .gnu.version_d  : { *(.gnu.version_d) }
        .gnu.version_r  : { *(.gnu.version_r) }

        /*
         * The unwind table entries are segment relative, which with a
         * single segment loaded at 0 means relative to the load address.
         */
        .IA_64.unwind_info : { *(.IA_64.unwind_info*) }
        .IA_64.unwind   : { *(.IA_64.unwind*) } :load :unwind

        .text           : { *(.text*) }         :load   =0x00040000
}
