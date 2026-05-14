# FoliOS Strata Kernel Naming Convention

**Version:** 1.0 (Draft)
**Date:** 2026-01-12
**Scope:** Strata Kernel (`strata`) and FoliOS SDK

## 1. Philosophy

The naming convention of FoliOS is designed to reflect the **superposition of architecture layers** and the **polymorphism** of hardware abstractions. The primary goal is **Scope Visibility**: A developer should immediately recognize the architectural layer and the affected subsystem of a function solely by its name.

## 2. Function Naming Rules

All kernel/SDK functions follow the **`Prefix[Region][Scope]_[Action]`** pattern.
The casing style is **PascalCase** for the segments, separated by an underscore after the prefix block.

### 2.1. The Syntax

```text
St[Region][Scope]_[Action]()
```

* **`St` (Strata):** The global namespace for the kernel.
* **`Region` (Optional):** The logical subsystem (e.g., `Vmm`, `Sched`, `Fs`).
* **`Scope` (Optional):** Hardware dependency level (`A` for Arch, `P` for Platform).
* **`Action`:** The operation being performed (e.g., `Map`, `Init`, `Reset`).

### 2.2. Categories & Prefixes

| Category | Prefix Structure | Description | Examples |
| --- | --- | --- | --- |
| **Generic API** | `St[Region]_` | High-level, architecture-agnostic logic. <br/><br/> *Call Direction: Downward* | `StMm_MapGlobal()`<br/><br/>`StThread_Yield()` |
| **Architecture** | `StA_` | **CPU/ISA-specific** routines (ASM/Intrinsics).<br/><br/>Pure hardware manipulation. | `StA_Hlt()`<br/><br/>`StA_SaveInterrupt()` |
| **Platform** | `StP_` | **Board/Firmware-specific** routines.<br/><br/>Peripheral & Bus control. | `StP_InitGdt()`<br/><br/>`StP_Panic()` |
| **Hybrid Impl** | `St[Region]A_` <br/><br/> `St[Region]P_` | **Architecture/Platform-specific implementation** of a generic region. | `StApicA_EnableLocal()`<br/><br/>`StThreadP_Switch()` |
| **Global Util** | `St_` | Kernel-wide utilities. | `St_Panic()`<br/><br/>`St_SwapEndian32()` |

---

## 3. Detailed Guidelines

### 3.1. General Subsystems (Regions)

Use standard abbreviations for Regions to keep names concise.

* `StPmm`: Physical Memory Manager
* `StVmm`: Virtual Memory Manager
* `StSched`: Scheduler / Threading
* `StInt`: Interrupt handling / IDT
* `StIo`: Input/Output (Port/MMIO)
* `StFs`: File System abstraction

First-class object families that own a lifecycle or public API may use the
object name as the Region even when their implementation lives under a broader
subsystem directory.

* `StPool`: Pool allocator
* `StAddressSpace`: Address space lifecycle
* `StAllocationOwner`: Allocation ownership and accounting

**Example:**

```c
// Good: current codebase examples with clear hierarchy
StVmm_InitGlobalDomain(domain, base_vpn, limit_vpn);
StAddressSpace_Create(&asp, process);

// Avoid: snake case, ambiguous scope, or hiding a first-class object under MM
vmm_init_global_domain(domain, base_vpn, limit_vpn);
StMm_CreateAddressSpace(&asp, process);

```

### 3.2. Architecture Abstraction (`A`)

Use `StA_` or `St[Region]A_` for code residing in `strata/arch/*`. These functions often wrap assembly instructions or CPU-specific registers.

* **`StA_`**: Pure architectural actions (Context switch, Cache flush).
* *Example:* `StA_SaveInterrupt()`

* **`St[Region]A_`**: A generic region's architectural backend.
* *Example:* `StApicA_SendEoi()`

### 3.3. Platform Abstraction (`P`)

Use `StP_` or `St[Region]P_` for code residing in `strata/arch/[arch]/*`. These functions handle board variations (ACPI, BIOS, UEFI, Device Tree).

* **`StP_`**: System-level platform actions.
* *Example:* `StP_InitGdt()`

* **`St[Region]P_`**: A generic region's platform backend.
* *Example:* `StIntP_Init()` (initializes the platform interrupt backend)

---

## 4. Variable & Type Naming

To contrast with the PascalCase function names, variables and types use specific casings.

### 4.1. Variables

* **Variables/Parameters:** `snake_case`
* `size_t pageSize;`

### 4.2. Data Types

Types should be **PascalCase**. Structs used across the kernel may carry the `St` prefix. Same as function naming rules, but there's no verb in the name.

* **Structs/Unions:** `struct St_SomeStruct` `union St_SomeUnion`
* **Typedefs:** `St_SomeType`
* **Enums:** `enum St_SomeEnum`

### 4.3. Macros & Constants

All caps with underscores (**UPPER_SNAKE_CASE**). Do not attach prefix `ST_` or `VL_`.

* `STATUS_SUCCESS`
* `STATUS_ERROR_UNCLASSIFIED`
* `STATUS_CRITICAL_UNCLASSIFIED`
* `PAGE_SIZE`
