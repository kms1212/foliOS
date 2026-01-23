# foliOS: Next-Generation OS Powered by Strata

**foliOS** is a high-performance, secure operating system built upon **Strata**, a kernel that implements the **Endokernel** architecture. By internalizing modern hardware primitives (MPK, PCID, FRED), foliOS bridges the gap between the isolation of Microkernels and the raw performance of Monolithic kernels, creating a "Fusional" environment for the next generation of computing.

## 🧠 What is the Endokernel?

Traditional architectures either include everything in a single privilege layer (**Monolithic**) or push functionality to external servers (**Microkernel**). The **Endokernel** philosophy, as realized in **Strata**, takes a third path: it brings hardware-specific acceleration features *into* the kernel's core to create a hardware-enforced "stratum" where modules execute with near-zero overhead.

Unlike an **Exokernel** that merely exports hardware to the outside, an **Endokernel** internalizes hardware features to enforce a **Unified Address Space**. In **Strata**, protection is not achieved by simple address space separation, but by hardware-level permission domains. This allows for:

* **Zero-Copy Direct Pointer Dereferencing:** Seamless data sharing between domains (User ↔ Module ↔ Kernel) without the need for heavy IPC or context switching.
* **Hardware-Accelerated Isolation:** Leveraging **Intel MPK** to partition a single address space into multiple secure privilege domains.
* **Context-Persistent Execution:** Utilizing **PCID** to maintain TLB entries across domain transitions, ensuring that "isolation" no longer comes at the cost of "performance."

### The Strata Philosophy: "Legacy as a Heritage"

foliOS views traditional memory boundaries not as a burden to be removed, but as a **Heritage** that enables safety. By maintaining a legacy view for user processes while providing a high-speed "fast-path" for modules, Strata achieves 100% POSIX compatibility alongside next-generation I/O throughput.

## 🚀 Key Features

### 🛡️ Hardware-Accelerated Domain Isolation

Strata moves away from software-heavy isolation, relying instead on hardware primitives to define the **3-Layer Memory Model (User-Module-Kernel)**:

* **User Area:** Traditional process-isolated VMA. Fully compatible with `fork()` and ASLR.
* **Module Area:** A shared global address space where modules are isolated via **Intel MPK (Memory Protection Keys)**. This allows  domain switching without TLB flushes.
* **Kernel Area:** The privileged core (Ring 0), protected by standard hardware rings and **Shadow Paging** to mitigate speculative execution attacks (Spectre/Meltdown) without the performance hit of KPTI.

### ⚡ VIF (Virtual Interrupt Flag) & Late-Masking

foliOS eliminates the "Syscall-for-Sync" bottleneck. Instead of requesting the kernel to `cli/sti` (mask interrupts), modules use a **Virtual Interrupt Flag (VIF)** in shared memory:

* **Shared Masking:** Modules set a software flag in a dedicated memory slot.
* **Late-Masking Logic:** When a hardware IRQ occurs, the kernel checks the VIF before dispatching. If the flag is set, the kernel performs a **Late-mask** at the hardware level.
* **Performance:** Synchronization latency is reduced to near-hardware speeds, as no privilege transition is required to "mask" interrupts.

### 📦 Heterogeneous Module Loading

foliOS modules are packaged in **Unified Containers** that bridge the Ring 3/Ring 0 divide:

* **Dual-Identity Binaries:** A single container holds two distinct binary images (typically ELF/PE) with identical section names (`.text`, `.data`) but different roles.
* **Split Loading:** The loader places the **Control Logic** in the Ring 3 Module Area (MPK-protected) and the **Fast-path ISR** in the Ring 0 Kernel Area.
* **Dynamic Binding:** System calls are not static constants; they are dynamically assigned IDs at load-time and "hot-patched" into the module code or resolved via a high-speed Dispatcher, allowing for modular ABI evolution.

### 🌲 GNT (Global Node Tree) & Polymorphism

All system resources—from files and drivers to network sockets—are organized into a **Global Node Tree (GNT)**:

* **Path-based Discovery:** Resources are accessed via a unified naming scheme (e.g., `/Devices/Net/Eth0`).
* **Polymorphism:** The GNT supports polymorphic interfaces identified by **UUIDv5** (deterministic hashes of names), ensuring binary compatibility across different versions of the OS.
* **Reverse Symlinks (Resource Integrity):** To solve the "Dangling Pointer" problem in shared memory, GNT nodes maintain a list of their observers. When a node is destroyed, the kernel uses these reverse links to **immediately invalidate** all remote pointers, ensuring temporal memory safety.

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

# Configure and build for BIOS-based x86-64
cmake -S. -Bbuild -DTARGET=amd64-pc-bios
cmake --build build

# Generate disk image and run
scripts/mkdisk.sh -a i686 disk.img
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
| `strata` | The **Strata Endokernel**: Core PMM, VMM, and hardware-accelerated domain manager. |
| `packages` | User-space and module-space software stack. |

### 📦 System Packages

* **`foligui`**: The foliOS Graphical User Interface stack.
  * `foligui`: High-performance drawing engine module with window compositor.
  * `libfoligui`: System call wrappers for foliGUI module

* **`folicrt`**: The fundamental C/C++ Runtime environment.
  * `libfolistdc`: C Standard Library optimized for foliOS system calls.
  * `libfolistdcxx`: C++ Standard Library support.

### 📦 System Packages

* **`foliposix`**: The POSIX Ecosystem Compatibility Suite.
  * `foliposix`: A specialized module dedicated to emulating POSIX-compliant file system semantics.
  * `libfoliposix`: The POSIX.1-2017 compliant API surface.

* **`folisdk`**: General Application SDK.
  * `libfoliimm`: Input Method Module (IMM) for multi-language support.
  * `libfoliutil`: OS-independent utility and data structure library.

* **`stratasdk`**: Low-level Kernel Interface.
  * `libstrata`: Standard system call wrappers for User-mode applications.

* **`stratamodsdk`**: Specialized SDK for **Endokernel Modules**.
  * `libstratamod`: Provides more privileged system calls for modules.

## ⚖️ License

foliOS and Strata are licensed under Apache License 2.0. See the `LICENSE` file for details.
