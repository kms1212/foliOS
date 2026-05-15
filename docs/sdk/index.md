# SDK and Runtime {#sdk}

SDK documents describe user-facing runtime and ABI contracts.

- @subpage sdk_c_runtime "C Runtime"
- @subpage sdk_libc_entry "libc Entry"
- @subpage sdk_syscall_abi "Syscall ABI"

## Boundary

The SDK boundary is where conventional C/POSIX-style program expectations are
translated into Strata contracts. The kernel stores and transports `StStatus`;
the C runtime decides how `main` return values, `_Exit`, `exit`, handles, and
libc initialization map onto that kernel-facing status and syscall model.

Keep compatibility policy here rather than in generic Strata kernel headers.
