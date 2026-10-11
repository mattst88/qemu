syscall_nr_generators += {
  'ia64': generator(sh,
                    arguments: [ meson.current_source_dir() / 'syscallhdr.sh',
                                 '@INPUT@', '@OUTPUT@', '@EXTRA_ARGS@',
                                 '', 'TARGET_SYSCALL_OFFSET' ],
                    output: '@BASENAME@_nr.h')
}

vdso_inc = gen_vdso.process('vdso.so',
                            extra_args: ['-r', '__kernel_sigtramp'])

linux_user_ss.add(when: 'TARGET_IA64', if_true: vdso_inc)
