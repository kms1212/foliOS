# 0004: Package-Managed Execution {#decision_0004_package_managed_execution}

Status: accepted

Date: 2026-05-17

## Context

foliOS needs a layered trust model for ordinary applications, module archives,
drivers, runtimes, and future interpreted environments. If executable trust is
handled separately by each binary format without a shared policy, the system
will have multiple places where signatures, update policy, rollback policy, and
execution permissions can disagree.

Module archives also need rich metadata for ABI and MPK planning, but they
request stronger authority than ordinary application binaries. Package-level
signing can authorize distribution and installation, but it should not by itself
authorize code to attach to module memory, direct interrupt entry, or
kernel-adjacent fast paths.

## Decision

Executable objects must be distributed and executed through packages. This
includes ordinary application binaries, module archives, driver components,
interpreters, bytecode runtimes, and other native executable images.

Package signatures authorize distribution, installation, update, rollback, and
ordinary application execution. The loader trusts package verification state as
the base proof that an executable object came through the package system, then
validates the object's format, ABI, capabilities, and runtime requirements.

Module archives require an additional module-domain signature in the normal
loading path. That signature authorizes the archive to request module memory,
direct user IRQ entry, kernel-side interrupt capsules, MPK protection-domain
constraints, and other module-specific authorities. This is not duplicate trust:
the package signature says the package may be distributed; the module signature
says this archive may be attached to module or kernel-adjacent execution
domains.

The kernel should not treat a raw executable file as trusted merely because its
format is valid. Execution requires package verification state or an explicit
development-mode exception.

Interpreters are executable objects and must therefore come from verified
packages. Scripts, bytecode files, and other interpreted inputs are data by
default. Running untrusted data through an interpreter does not grant authority
beyond the interpreter process and the capabilities it already has.

Scripts or bytecode may be treated as trusted executable resources only when a
verified package manifest declares them as such and binds them to an approved
interpreter or runtime. Direct kernel execution of arbitrary script files is not
part of the base execution model.

Module interrupt fast paths and direct module IRQ entries must use native
module ABI images. Interpreted code may participate in higher-level user module
policy only if it runs behind the module runtime and outside urgent interrupt
entry paths.

## Consequences

Signing, origin, update, rollback, and executable permission policy are
rooted in the package system, while module-domain authority is represented by
module archive signatures. This keeps ordinary application execution and module
attachment from sharing a single overly broad permission.

Module archives remain focused on payload separation, declarative module
metadata, ABI requirements, resource hints, interface dependencies, and MPK
planning constraints. Their signatures must cover the authority claims and the
payload digests the kernel uses to validate those claims.

The exec path needs a way to carry package verification state to the loader.
The loader must reject executable objects that are not associated with verified
package state unless the system is explicitly in a development or recovery mode
that permits unsigned execution.

The module loading path needs a way to verify module archive signatures against
keys or policies that are distinct from ordinary package distribution keys.
Development and recovery modes may allow test keys or unsigned modules, but
those exceptions must be explicit because they grant authority beyond ordinary
application execution.

Interpreter support should be built through manifest-driven policy instead of
kernel-level script parsing. A package may expose a trusted script or bytecode
entry, but the policy decision belongs to package metadata and the exec service,
not to ad-hoc file extension or shebang handling in the kernel.

## Related Docs

- [Module Runtime and Interrupt Entry](0003-module-runtime-and-interrupt-entry.md)
- [Ambikernel](../architecture/ambikernel.md)
- [SDK and Runtime](../sdk/index.md)
