# libc Entry {#sdk_libc_entry}

This page describes the `stmain` and C `main` compatibility model, weak entry
points, and status translation.

## Two Entry Shapes

foliOS supports two application entry shapes:

```c
StStatus stmain(int argc, char **argv, char **envp);
int main(int argc, char **argv, char **envp);
```

`stmain` is the native Strata-aware entry point. It returns `StStatus` directly.
`main` is the C compatibility entry point. It returns an integer that libc
translates into a Strata process-exit status.

## Weak Default

The runtime provides a weak `stmain` implementation. The default implementation
calls the linked `main` function and converts the result:

```c
exit_code = posix_main(argc, argv, envp);
return MAKE_PROCESS_EXIT_STATUS(exit_code);
```

An application that defines its own `stmain` bypasses the integer conversion and
can return a Strata status directly.

## Why Translation Lives Here

The kernel should not know POSIX process-exit conventions. It only needs to
store and propagate a typed process exit status. The runtime owns the policy of
turning C integer return values into `STATUS_AREA_PROCESS_EXIT` values.

This keeps the kernel ABI cleaner and allows future runtimes to choose different
language-appropriate translations without changing Strata process internals.
