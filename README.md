# foliOS: Next-Generation OS Built on Strata

**foliOS** is an experimental operating system built on **Strata**, a kernel that explores the **Ambikernel** architecture. The project studies whether selected driver and service logic can run in explicit module domains with lower overhead than process-style IPC while still preserving auditable kernel boundaries and operational fault containment.

## tl;dr

foliOS is an experimental operating system that explores an **Ambikernel** architecture: explicit user, module, and kernel memory regions combined with architecture-local module protection primitives.
One focus is moving selected driver and service logic into fault-contained Ring 3 modules to evaluate whether kernel transition overhead can be reduced without giving up operational containment.
The system is intentionally designed as a research platform, making explicit performance, fault-containment, and security trade-offs while mapping architecture-specific hardware features onto generic kernel concepts.
It does not claim to provide universal safety or general-purpose robustness.

## 🧠 What is the Ambikernel?

Traditional architectures either place most functionality in a single privilege layer(**monolithic**), or push functionality to external servers(**Microkernel**). The **Ambikernel** philosophy, as explored in **Strata**, takes a third path: it brings hardware-assisted primitives *into* the kernel's core to create an explicit module stratum where selected services can execute with reduced overhead.

Unlike an **Exokernel** that merely exports hardware to user space, an **Ambikernel** internalizes hardware features so the kernel can coordinate conventional process isolation, module memory regions, and runtime-managed protection state. In **Strata**, protection is not achieved only by simple address space separation, but by architecture-local permission domains that can be changed by the runtime. This allows for:

* **Descriptor-Mediated Fast Data Paths:** Module interfaces can use handles, generated bindings, validated descriptors, or explicit page remapping to avoid unnecessary IPC copies where the module ABI permits it. Raw cross-domain pointers are not the ABI.
* **Hardware-Assisted Module Containment:** Assigning module memory to protection shards so trusted-but-buggy modules can be contained without forcing every interaction through a separate address space.
* **Architecture-Specific Backends:** On amd64, Strata plans to implement protection shards with MPK/PKRU and PCID-aware page-table views. Other architectures may use different primitives or a page-table-only fallback.

> Note: Many of the performance and isolation properties described here are design goals validated through targeted benchmarks and controlled configurations, rather than universal guarantees across all workloads and hardware.

### The Strata Philosophy: "Legacy as Heritage"

foliOS views traditional memory views not as a burden to be removed, but as a **Heritage** that enables safety. By maintaining a traditional memory model view for user processes while providing a high-speed "fast-path" for modules, Strata achieves a high degree of POSIX compatibility alongside next-generation I/O throughput.

* **Linux Driver Porting Profile:** A future compatibility profile may emulate selected Linux driver helper APIs on top of the native module runtime.
  * **User Buffer Access:** `copy_from_user`-style helpers must translate to validated module-runtime operations such as bounded copies, temporary mapping windows, page loans, or descriptor-mediated access. Module code must not assume it can freely dereference arbitrary user pointers.
  * **Sync Mapping:** Traditional `cli/sti` or spinlock-shaped helpers may map to **VIF (Virtual Interrupt Flag)** and late-masking operations where the module runtime defines equivalent semantics.
  * **MMIO/IOPB Directness:** High-performance drivers may receive carefully scoped MMIO or IOPB authority, but that authority is granted by the loader/runtime contract rather than by ambient Ring 3 access.

### Trust Model: Operational vs. Security

Strata separates trust into security authorization and operational reliability. Module protection is primarily a fault-containment mechanism for signed, trusted code that may still contain bugs. It is not presented as a complete sandbox for hostile code:

* **Security-Authorized Modules:** Drivers and system modules are cryptographically signed and verified at load-time. To keep fast paths low-overhead, foliOS prioritizes hardware-assisted mitigations where available, and applies software barriers selectively at trust boundaries:
  * **Control Flow Integrity (Spectre v2):** Instead of costly software trampolines (Retpoline), foliOS utilizes **Intel CET (Control-flow Enforcement Technology)** and **eIBRS**. This enforces strict Indirect Branch Tracking (IBT) at the hardware level with reduced overhead.
  * **Data Sanctity (Spectre v1):** Mitigation is applied strategically rather than indiscriminately. The SDK provides **`GUARD` macros** (selective `LFENCE` or arithmetic masking). Developers are required to apply these barriers only at critical boundaries where untrusted data (e.g., from User Space or Network) is ingested, preserving the pipeline performance for internal logic.
* **Operational Fault Containment:** The Micro-Core remains the ultimate guardian. While modules are trusted not to *attack*, they are not trusted to be *crash-free*. The kernel protects system integrity from module failures through **Ring 3 execution**, module protection shards, and strict loader/runtime contracts that prevent faulty modules from directly corrupting core kernel regions or unrelated module state.

## 🚀 Key Features

### 🛡️ Hardware-Assisted Domain Isolation

Strata's architecture direction is to use hardware primitives to support a **3-Layer Memory Model (User-Module-Kernel)** while keeping conventional process isolation as the baseline:

* **User Area:** Adheres to the traditional **Page-Table-Based Isolation**. Each user process possesses a private Virtual Memory Area (VMA), ensuring standard POSIX isolation guarantees. Context switching between user processes involves a standard page table transition.
* **Module Area:** A global module virtual-memory region where module images and module-owned memory are placed into ABI-defined slots and assigned to module protection shards. It is not ordinary user memory, and cross-domain data exchange must go through the module runtime contract.
  * **Scoped I/O Authority:** For supported port-mapped devices, the module runtime may eventually use mechanisms such as the **I/O Permission Bitmap (IOPB)** to grant narrowly scoped port access. This remains a loader/runtime contract, not ambient hardware authority.
* **Kernel Area:** The privileged core remains responsible for page-table updates, frame ownership, process/thread lifetime, handle tables, scheduling, and module-runtime authority.
  * **View Separation:** The intended protection model keeps ordinary user execution from treating kernel or module memory as directly usable address space. Exact mapping and protection behavior is backend-specific and must be expressed through the module ABI.
  * **Context Transition:** Transitions into kernel or module execution install ABI-defined runtime state such as the target module context, stack, protection binding, and shard generation.

### 🔑 Module Protection Shards: Scalable Logical Isolation Domains

Many fast in-address-space protection primitives have small hardware limits. Strata treats those limits as backend details by using **module protection shards**, enabling scalable logical isolation domains without making one architecture's key numbers part of the kernel ABI:

* **Shard Multiplexing:** Modules are grouped into logical shards, each with its own page-table view and architecture-local protection binding table. On amd64, the intended backend uses shard-local pkey assignments backed by PCID contexts.
* **Fast Same-Shard Transitions:** Calls between modules in the same shard can change only the architecture-local protection state. On amd64, the intended KRT path would use PKRU updates for this case.
* **Cross-Shard Transitions:** Calls between shards switch to the target shard's page-table and architecture context. This is more expensive than a same-shard transition but allows protection bindings to be reused across shards.
* **Scalability:** The kernel should be able to host more module protection domains than a single hardware binding table would allow, while a shard planner uses module metadata to improve locality over time.

### 🔌 KRT (Kernel RunTime): The Living Interface

foliOS uses the **KRT (Kernel RunTime)** as the user-visible runtime entry table for Strata services. KRT provides stable stubs and metadata that user runtimes such as `libstrata` can call before entering the kernel or, in future module paths, before switching module protection state:

* **Runtime Entry Table:** Exposes versioned entry points for node open, query, and call operations. The current amd64 implementation uses KRT stubs that enter the kernel through the syscall path.
* **Module Transition Coordinator:** Future module calls should pass through KRT-defined gates so stack switching, protection binding, call-frame lifetime, and fault attribution remain explicit.
* **Runtime Caches:** User runtimes may maintain handle or interface caches around KRT calls, but cached state is an optimization and not a replacement for kernel authority.

### 🌐 User-Level Networking: Shared Code, Private Data

The long-term networking direction is a module-runtime stack that can share verified code while keeping packet ownership and DMA authority explicit:

* **Code Sharing:** Processes may use common verified TCP/IP logic through protected module code or runtime libraries.
* **Buffer Ownership:** Packet buffers should be represented by descriptors, loans, or mapped windows whose ownership and lifetime are visible to the kernel and module runtime.
* **Safety:** Any copy reduction must preserve DMA isolation, buffer lifetime, and module fault attribution. Direct NIC DMA into arbitrary user heap memory is not a baseline guarantee.

### 🎲 Large Module Address Space

Strata reserves a large module virtual-memory region so module placement can be decoupled from the kernel core and, later, used for sparse placement and layout randomization:

* **Liberation from the 2GB Limit:** Unlike traditional kernels that must cluster modules near the core code to satisfy x86 `RIP-relative` addressing constraints, Strata structurally decouples the Micro-Core from the Module Area.
* **Large Slot Space:** Modules reside in a vast, contiguous region (Module Area) separate from the kernel core. The module ABI should define slot size, guard placement, sparse mapping, and loader relocation rules.
* **Future Layout Randomization:** Sparse module placement and function-level layout randomization are design options, not current security guarantees.

### ⚡ VIF (Virtual Interrupt Flag) & Late-Masking

The module runtime is expected to define a **Virtual Interrupt Flag (VIF)** and late-masking model for direct interrupt entry. The goal is to avoid unnecessary synchronization syscalls on fast paths without letting modules own hardware interrupt state directly:

* **Shared Masking State:** Modules may update ABI-defined runtime state to defer direct interrupt entry.
* **Late-Masking Logic:** When a hardware IRQ occurs, the kernel can consult the runtime state before dispatching and perform host-side masking or replay as needed.
* **Performance Goal:** Synchronization latency can be reduced when the common case avoids a privilege transition.

### 📦 Heterogeneous Module Loading

foliOS modules are packaged in **Strata Module Archives (SMA)** that bridge the Ring 3/Ring 0 divide.

* **Split Images:** A module archive may contain a primary Ring 3 user image and an optional, narrow kernel-side interrupt capsule. The user image owns the main driver or service policy.
* **Interface Binding:** Module ABI evolution is expressed through archive metadata, SIDL/SIF interface records, KRT entry tables, and generated bindings. Module code should not rely on load-time syscall ID hot-patching as the primary ABI mechanism.

To ensure modules follow the runtime and protection-domain contract, the intended loader path should enforce strict prerequisites alongside cryptographic verification:

* **Code Signing & Static Verification:** Normal module loading should require trusted module-domain authorization. Before loading, the kernel should perform conservative validation of the archive, executable images, and approved runtime code:
  * **Instruction Policy:** Privilege-altering or context-sensitive instructions such as `WRPKRU`/`RDPKRU`, `XRSTOR`/`XSAVE`, or `SYSCALL`/`SYSENTER` should be rejected outside approved runtime code.
  * **Immutable Executable Mapping:** Module code should be mapped with strict **W^X (Write XOR Execute)** policy. Module code sections should be `RX`, data sections should be `RW`, and module code should not be allowed to allocate or remap executable pages unless a future ABI profile explicitly permits it.

### 🌲 Global Node Tree (GNT) & Namespace Polymorphism

All system resources—files, device interfaces, IPC endpoints, and kernel services—are represented within a unified hierarchical structure called the **Global Node Tree (GNT)**.  
The GNT provides a common namespace abstraction that allows heterogeneous system resources to be accessed through a uniform path-based interface.

#### Hybrid Path Resolution

Strata implements a hybrid path resolution mechanism designed to balance performance with extensibility.

* **Kernel-Resolved Paths:**  
  Static or frequently accessed namespaces (e.g., `/System`, `/Devices`) are resolved directly by the kernel using in-memory directory structures.  
  This avoids user-space mediation for common operations and reduces the overhead typically associated with microkernel-style IPC.

* **Delegated Namespace Resolution:**  
  When a path refers to a dynamically defined namespace (e.g., semantic version resolution such as `/Packages/FooPackage/^1.2.5`), the kernel delegates resolution of the remaining path component to the responsible module through a `PathResolver` interface.  
  This allows modules to implement custom naming semantics without requiring kernel modification.

This mechanism effectively enables **namespace polymorphism**, where different segments of the global namespace may implement distinct resolution semantics while preserving a consistent path-based interface for applications.

#### Capability-Oriented Resource Access

Instead of relying on implicit global state (such as a process-wide root directory or current working directory), processes receive explicit **capability handles** during initialization.  
These handles represent authority over specific nodes in the GNT.

Applications operate relative to these handles, enabling straightforward construction of restricted execution environments.  
For example, sandboxing can be implemented by providing a process with a capability referencing a restricted subtree of the namespace.

#### Relative Resolution Optimization

To reduce repeated namespace delegation for dynamic namespaces, applications may obtain a **base handle** corresponding to a previously resolved node.  
Subsequent operations relative to this handle can often be resolved entirely within the kernel's fast-path directory traversal.

This mechanism reduces the overhead associated with repeated dynamic namespace interpretation.

#### Planned Reverse Reference Tracking

GNT nodes should eventually maintain metadata about active references to the resource they represent.
When a node is removed or invalidated, the kernel should be able to propagate invalidation events to dependent objects, links, or handles.

This reference tracking mechanism helps mitigate classes of errors related to stale or invalid resource references in shared environments.

### 📂 Package-Based Executable Management

foliOS plans to move away from the cluttered `/bin` and `/lib` hierarchy in favor of a strictly versioned **Package System**:

* **Atomic Updates:** Applications and their dependencies are managed as atomic units.
* **ABI Projection:** The OS "projects" the required ABI version into the process's view at load-time, allowing multiple versions of the same library to coexist without conflict.

## 🛠 Build & Run

### Prerequisites

* Python 3 and the host build tools needed by foliSDK.
* `i686-elf-gcc` for the IA-32 BIOS bootloader build.
* QEMU (for testing)

### Steps

```sh
# Clone the repository
git clone https://github.com/kms1212/foliOS --recursive
cd foliOS

# Build the canonical SDK/toolchain layout used by foliOS
(cd folisdk && ./build.py --arch x86_64 --builddir-layout --jobs 18)
export PATH="$(pwd)/folisdk/build/folisdk-host/bin:$(pwd)/folisdk/build/folisdk-x86_64/bin:$PATH"

# Configure and build for BIOS-based amd64 machine using foliSDK CMake
folisdk/build/folisdk-host/bin/cmake \
    -S . \
    -B build \
    -DCMAKE_BUILD_TYPE=Debug \
    -DTARGET=amd64-pc-bios \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
folisdk/build/folisdk-host/bin/cmake --build build --parallel=18

# Generate disk image and run 
# Note: option "-a ia32" is correct because the bootloader is still built for IA-32
scripts/mkdisk.sh -a ia32 disk.img
scripts/run.sh --disk disk.img pc-amd64
```

## ⚖️ License

foliOS, Vellum, and Strata are licensed under Apache License 2.0. See the `LICENSE` file for details.
