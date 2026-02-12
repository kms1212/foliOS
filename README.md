# foliOS: Next-Generation OS Built on Strata

**foliOS** is a high-performance, secure operating system built on **Strata**, a kernel that implements the **Ambikernel** architecture. By internalizing modern hardware primitives, it bridges the gap between the isolation of Microkernels and the raw performance of monolithic kernels, creating a hybrid environment for the next generation of computing.

## tl;dr

foliOS is an experimental operating system that explores an **Ambikernel** architecture: a unified address space combined with hardware-enforced isolation using MPK and PCID.
One focus is moving selected driver and service logic into MPK-protected modules running in Ring 3 to evaluate whether kernel transition overhead can be reduced without giving up fault containment.
The system is intentionally designed as a research platform, making explicit performance–security trade-offs and relying on modern x86 hardware features.
It does not claim to provide universal safety or general-purpose robustness.

## 🧠 What is the Ambikernel?

Traditional architectures either place most functionality in a single privilege layer(**monolithic**), or push functionality to external servers(**Microkernel**). The **Ambikernel** philosophy, as realized in **Strata**, takes a third path: it brings hardware-assisted primitives *into* the kernel's core to create a hardware-enforced "stratum" where modules execute with reduced overhead.

Unlike an **Exokernel** that merely exports hardware to user space, an **Ambikernel** internalizes hardware features to enforce a **Unified Address Space**. In **Strata**, protection is not achieved by simple address space separation, but by hardware-level permission domains. This allows for:

* **Copy-free Data Access:** Seamless data sharing between domains (User ↔ Module ↔ Kernel) without the need for heavy IPC or context switching.
* **Hardware-Accelerated Isolation:** Leveraging **Intel MPK** to partition a single address space into multiple secure privilege domains.
* **Context-Persistent Execution:** Utilizing **PCID** to maintain TLB entries across domain transitions, significantly reducing the traditional performance cost of isolation.

> Note: Many of the performance and isolation properties described here are design goals validated through targeted benchmarks and controlled configurations, rather than universal guarantees across all workloads and hardware.

### The Strata Philosophy: "Legacy as Heritage"

foliOS views traditional memory views not as a burden to be removed, but as a **Heritage** that enables safety. By maintaining a traditional memory model view for user processes while providing a high-speed "fast-path" for modules, Strata achieve a high degree of POSIX compatibility alongside next-generation I/O throughput.

* **Linux Driver Parity:** By emulating Linux kernel headers, foliOS allows for a **seamless porting path** for existing drivers.
  * **Unified Memory:** `copy_from_user` can be reduced to a direct pointer access under controlled MPK-enforced conditions.
  * **Sync Mapping:** Traditional `cli/sti` or spinlocks are transparently mapped to **VIF (Virtual Interrupt Flag)** operations.
  * **MMIO/IOPB Directness:** High-performance drivers (NVMe, NIC) interact with hardware registers without leaving Ring 3.

### Trust Model: Operational vs. Security

The Strata separates trust into two distinct domains to maximize performance without compromising stability. It shifts away from heavy software-based mitigations, leveraging modern hardware features to enforce security with minimal overhead:

* **Security Trusted Domain (Modules):** Drivers and system modules are cryptographically signed and verified at load-time. To achieve raw performance, foliOS prioritizes hardware-assisted mitigations where available, and applies software barriers selectively at trust boundaries:
  * **Control Flow Integrity (Spectre v2):** Instead of costly software trampolines (Retpoline), foliOS utilizes **Intel CET (Control-flow Enforcement Technology)** and **eIBRS**. This enforces strict Indirect Branch Tracking (IBT) at the hardware level with reduced overhead.
  * **Data Sanctity (Spectre v1):** Mitigation is applied strategically rather than indiscriminately. The SDK provides **`GUARD` macros** (selective `LFENCE` or arithmetic masking). Developers are required to apply these barriers only at critical boundaries where untrusted data (e.g., from User Space or Network) is ingested, preserving the pipeline performance for internal logic.
* **Operational Trusted Domain (Kernel Core):** The Micro-Core remains the ultimate guardian. While modules are trusted not to *attack*, they are not trusted to be *crash-free*. The kernel protects system integrity from module failures via **MPK isolation** and **Ring 3 execution**, preventing direct memory corruption of core kernel regions by faulty modules.

## 🚀 Key Features

### 🛡️ Hardware-Accelerated Domain Isolation

Strata moves away from software-heavy isolation, relying instead on hardware primitives to define the **3-Layer Memory Model (User-Module-Kernel)**:

* **User Area:** Adheres to the traditional **Page-Table-Based Isolation**. Each user process possesses a private Virtual Memory Area (VMA), ensuring standard POSIX isolation guarantees. Context switching between user processes involves a standard page table transition.
* **Module Area:** A shared global address space where modules are isolated via **Intel MPK (Memory Protection Keys)**. This allows domain switching without TLB flushes.
  * **Direct I/O Access (IOPB):** Unlike traditional Microkernels that require a syscall for every port I/O, Strata leverages the **I/O Permission Bitmap (IOPB)** to grant specific modules direct access to hardware ports. This allows Ring 3 drivers to provide I/O performance closer to bare-metal execution for supported port-mapped devices.
* **Kernel Area:** The privileged core (Ring 0) utilizes a **Converged Isolation Model**. Instead of viewing KPTI solely as a mitigation, Strata integrates strict page table separation into the **MPK Sharding** mechanism.
  * **Dynamic Mapping:** In User Mode, Kernel and Module areas are strictly **unmapped**, protecting against speculative attacks (Meltdown) and granting the user process full utilization of MPK keys.
  * **Atomic Context Transition:** The transition to Kernel/Module mode involves a strategic CR3 switch that simultaneously maps the privileged areas and loads the target MPK Shard. This amalgamates the cost of Meltdown mitigation with the necessity of domain expansion.

### 🔑 MPK Sharding: Scalable Logical Isolation Domains

While Intel MPK is natively limited to 16 protection keys, Strata overcomes this hardware constraint through **MPK Sharding**, enabling scalable logical isolation domains beyond native MPK key limits:

* **Shard Multiplexing:** Modules are grouped into logical "Shards", each assigned a unique **PCID (Process-Context Identifier)**.
* **TLB-Preserving Switch:** When calling a module in a different shard, the kernel performs a lightweight PCID switch. Unlike a full context switch, this retains the TLB entries for the shared kernel/user mappings, flushing only the module-specific entries if necessary.
* **Scalability:** This architecture allows foliOS to host thousands of isolated drivers and services within a single address space, breaking the 16-domain barrier of traditional MPK implementations.

### 🔌 KRT (Kernel RunTime): The Living Interface

foliOS replaces static system call wrappers with the **KRT (Kernel RunTime)**, a dynamic kernel component mapped directly into every user process's address space:

* **Virtual Kernel Adapter:** Acts as a user-space proxy for the kernel, handling MPK key switching (`WRPKRU`) and module dispatching transparently.
* **State Arena:** Manages stateful resources (e.g., file descriptors, socket states) within a protected user-memory region, accessible only via KRT functions.
* **A low-latency kernel entry mechanism:** Replaces traditional `syscall` instructions with direct function calls (C FFI), avoiding full privilege-level context switches for supported operations.

### 🌐 User-Level Networking: Shared Code, Private Data

The network stack is implemented as a shared, read-only library mapped into user space, enabling copy-free data paths in supported DMA configurations:

* **Code Sharing:** All processes use the same verified TCP/IP logic (MPK-protected module code).
* **Data Privacy:** Packet buffers reside in the user's private heap. The NIC DMAs directly to user memory, and the shared code processes packet headers in-place.
* **Safety:** While data is local, the logic is mediated by the kernel-provided KRT, enforcing protocol constraints through a shared, verified network stack implementation.

### 🎲 Hyper-Entropy KASLR via Structural Decoupling

foliOS leverages the full 48-bit canonical address space to implement a KASLR strategy that is structurally more flexible to monolithic counterparts:

* **Liberation from the 2GB Limit:** Unlike traditional kernels that must cluster modules near the core code to satisfy x86 `RIP-relative` addressing constraints, Strata structurally decouples the Micro-Core from the Module Area.
* **Massive Entropy Pool:** Modules reside in a vast, contiguous region (Module Area) separate from the kernel core. This allows the loader to scatter drivers and services across terabytes of virtual space without requiring performance-heavy trampolines or PLTs.
* **Statistical Immunity:** By combining this spatial freedom with fine-grained function reordering, foliOS significantly increases the entropy and unpredictability of kernel and module layout, because the layout of system components is non-deterministic and sparse.

### ⚡ VIF (Virtual Interrupt Flag) & Late-Masking

foliOS eliminates the "Syscall-for-Sync" bottleneck. Instead of requesting the kernel to `cli/sti` (mask interrupts), modules use a **Virtual Interrupt Flag (VIF)** in shared memory:

* **Shared Masking:** Modules set a software flag in a dedicated memory slot.
* **Late-Masking Logic:** When a hardware IRQ occurs, the kernel checks the VIF before dispatching. If the flag is set, the kernel performs a **Late-mask** at the hardware level.
* **Performance:** Synchronization latency is reduced by avoiding privilege transitions, as no privilege transition is required to "mask" interrupts.

### 📦 Heterogeneous Module Loading

foliOS modules are packaged in **Unified Containers** that bridge the Ring 3/Ring 0 divide.

* **Split Loading:** The loader places the **Control Logic** in the Ring 3 Module Area (MPK-protected) and the **Fast-path ISR** in the Ring 0 Kernel Area.
* **Dynamic Binding:** System calls are not static constants; they are dynamically assigned IDs at load-time and "hot-patched" into the module code or resolved via a high-speed Dispatcher, allowing for modular ABI evolution.

To ensure the integrity of the Security Trusted Domain, the loader enforces strict prerequisites alongside cryptographic verification:

* **Mandatory Code Signing & Static Verification:** All modules must be signed by a trusted authority. Before loading, the kernel performs a conservative static verification pass enforcing an instruction allowlist:
  * **Instruction Blacklist:** The use of privilege-altering or context-sensitive instructions is strictly prohibited. If a binary contains `WRPKRU`/`RDPKRU` (Key manipulation), `XRSTOR`/`XSAVE` (CPU state manipulation), or `SYSCALL`/`SYSENTER` (Privilege transition), loading is immediately rejected.
  * **Immutable Executable Mapping:** Modules are subjected to strict **W^X (Write XOR Execute)** enforcement. The loader maps module code sections as Read-Only/Executable (`RX`) and data sections as Read-Write (`RW`). Crucially, modules are **stripped of the capability** to re-map their own memory as executable or allocate new executable pages at runtime, effectively neutralizing JIT-based attacks or self-modifying code.

### 🌲 GNT (Global Node Tree) & Polymorphism

All system resources—from files and drivers to network sockets—are organized into a **Global Node Tree (GNT)**:

* **Path-based Discovery:** Resources are accessed via a unified naming scheme (e.g., `/Devices/Net/Eth0`).
* **Polymorphism:** The GNT supports polymorphic interfaces identified by **UUIDv5** (deterministic hashes of names), ensuring binary compatibility across different versions of the OS.
* **Reverse Symlinks (Resource Integrity):** To solve the "Dangling Pointer" problem in shared memory, GNT nodes maintain a list of their observers. When a node is destroyed, the kernel uses these reverse links to **immediately invalidate** all remote pointers, reducing common classes of temporal memory safety errors.

### 📂 Package-Based Executable Management

foliOS abandons the cluttered `/bin` and `/lib` hierarchy in favor of a strictly versioned **Package System**:

* **Atomic Updates:** Applications and their dependencies are managed as atomic units.
* **ABI Projection:** The OS "projects" the required ABI version into the process's view at load-time, allowing multiple versions of the same library to coexist without conflict.

## 🛠 Build & Run

### Prerequisites

* CMake 3.13+
* LLVM/Clang or GCC (cross-compiler for amd64/i686, e.g. x86_64-elf-gcc)
* QEMU (for testing)

### Steps

```sh
# Clone the repository
git clone https://github.com/kms1212/foliOS
cd foliOS

# Configure and build for BIOS-based amd64 machine
cmake -S. -Bbuild -DTARGET=amd64-pc-bios
cmake --build build

# Generate disk image and run 
# Note: option "-a ia32" is correct because the bootloader is still built for IA-32
scripts/mkdisk.sh -a ia32 disk.img
scripts/run.sh pc-amd64
```

## 📁 Directory Structure

| Directory | Description |
| --- | --- |
| `docs` | Technical specifications, API documentation, and architecture whitepapers. |
| `scripts` | Automation tools for disk imaging, debugging, and QEMU orchestration. |
| `cmake` | Modular CMake build scripts and toolchain configurations. |
| `config` | Target-specific presets (e.g., amd64-pc-bios, i686-pc-bios). |
| `vellum` | The **Vellum Bootloader**: Performs generic bootloader functions. |
| `strata` | The **Strata Ambikernel**: Core PMM, VMM, and hardware-accelerated domain manager. |

### 📦 System Packages

* **`foligui`**: The foliOS Graphical User Interface stack.
  * `foligui`: High-performance drawing engine module with window compositor.
  * `libfoligui`: System call wrappers for foliGUI module

* **`folisdk`**: General Application SDK.
  * `libfoliimm`: Input Method Module (IMM) for multi-language support.
  * `libfoliutil`: OS-independent utility and data structure library.

* **`stratasdk`**: The Kernel Module Development Kit.
  * `libstmod`: Provides more privileged system calls for modules.
  * `libsidl`: The Strata Interface Definition Language (SIDL) runtime library.

## ⚖️ License

foliOS, Vellum, and Strata are licensed under Apache License 2.0. See the `LICENSE` file for details.
