# Global Node Tree {#strata_gnt}

This page describes the Global Node Tree model, node ownership, path
resolution, handles, and node-backed process resources.

## Model

The Global Node Tree is a hierarchical namespace for kernel-visible resources.
Nodes may represent directories, leaves, or links. A directory can also have a
handler module, which lets static kernel children and delegated module-provided
entries coexist under one path.

Important root nodes include:

- `g_gnt_root_local`: `/`;
- `g_gnt_root_network`: `//`.

## Path Resolution

`StGnt_ResolvePath` handles:

- absolute local paths beginning with `/`;
- network-root paths beginning with `//`;
- relative paths from a base node;
- `.` and `..`;
- link resolution with loop-depth protection;
- delegated module resolution when a node has a resolver.

The resolver contract returns the next node and the remaining path. That keeps
dynamic namespace semantics localized to the module that owns them.

## Iteration

`StGnt_Iterate` writes `struct StGnt_DirectoryEntry` records into a caller
buffer and returns a continuation cookie. Static child cookies and module
cookies are separate so iteration can move from kernel-owned children into a
module-provided listing without losing position.

## Interfaces And Handles

Nodes can register interfaces identified by UUID and ABI version. Querying a
node returns a function-id base and negotiated ABI version. Handles retain node
objects for userspace-facing access; handle-table close/clear paths release the
underlying object according to handle type.

Use `StUuid_IsEqual` for UUID comparison. Do not add local UUID equality helpers.

## Namespace Shape

The following tree is an architectural sketch of the intended namespace shape,
not a promise that every path is implemented today. Code should document the
current implementation boundary when a node family is still being migrated.

```text
/
|-- System/
|   |-- Kernel/
|   |-- Processes/
|   |   |-- Current -> ./0
|   |   |-- 1 -> ./0/1
|   |   `-- 0/
|   |       |-- Io/
|   |       |   |-- Stdin -> /System/Devices/Vtty0
|   |       |   |-- Stdout -> /System/Devices/Vtty0
|   |       |   `-- Stderr -> /System/Devices/Vtty0
|   |       |-- Memory/
|   |       |-- Threads/
|   |       |   |-- Current -> ./0
|   |       |   `-- 0 -> /System/Threads/0
|   |       `-- 1/
|   |           |-- Executable -> /Packages/FooPackage/0.0.1/Foo
|   |           `-- Threads/
|   |               |-- 0 -> /System/Threads/1
|   |               `-- 1 -> /System/Threads/2
|   |-- Threads/
|   |   |-- 0/
|   |   |   `-- Process -> /System/Processes/0
|   |   |-- 1/
|   |   |   `-- Process -> /System/Processes/1
|   |   `-- 2/
|   |       `-- Process -> /System/Processes/1
|   |-- Pipes/
|   |-- Sockets/
|   |-- SharedMemories/
|   |-- Modules/
|   |   |-- HwDrivers/
|   |   |-- BusDrivers/
|   |   |-- FastServices/
|   |   `-- Subsystems/
|   |-- Devices/
|   |   |-- Debug0 -> ../Hardware/AcpiSystem/Debug0
|   |   |-- Keyboard0 -> ../Hardware/AcpiSystem/HidCon0/Keyboard0
|   |   |-- Mouse0 -> ../Hardware/AcpiSystem/HidCon0/Mouse0
|   |   |-- Rtc0 -> ../Hardware/AcpiSystem/Rtc0
|   |   |-- Serial0 -> ../Hardware/AcpiSystem/Serial0
|   |   |-- Serial1 -> ../Hardware/AcpiSystem/Serial1
|   |   |-- Parallel0 -> ../Hardware/AcpiSystem/Parallel0
|   |   |-- Drive0 -> ../Hardware/AcpiSystem/Floppy0
|   |   |-- Drive1 -> ../Hardware/AcpiSystem/Floppy1
|   |   |-- Drive2 -> ../Hardware/AcpiSystem/Ata0
|   |   `-- Drive3 -> ../Hardware/AcpiSystem/Atapi0
|   |-- Hardware/
|   |   `-- AcpiSystem/
|   |       |-- Cpu0/
|   |       |-- Memory0/
|   |       |-- Memory1/
|   |       |-- DebugOut0/
|   |       |-- HidCon0/
|   |       |   |-- Keyboard0/
|   |       |   `-- Mouse0/
|   |       |-- Rtc0/
|   |       |-- Serial0/
|   |       |-- Serial1/
|   |       |-- Parallel0/
|   |       |-- PciBridge0/
|   |       |   `-- 00/
|   |       |       |-- 00/
|   |       |       |   `-- 00/
|   |       |       `-- 01/
|   |       |           |-- 00/
|   |       |           |-- 01/
|   |       |           `-- 03/
|   |       |-- FloppyCon0/
|   |       |   |-- 0 -> ./Floppy0
|   |       |   |-- 1 -> ./Floppy1
|   |       |   |-- Floppy0/
|   |       |   |   `-- FLPDSK0/
|   |       |   `-- Floppy1/
|   |       |       `-- FLPDSK1/
|   |       `-- IdeCon0/
|   |           |-- 0 -> ./Ata0
|   |           |-- 1 -> ./Atapi0
|   |           |-- Ata0/
|   |           |   |-- 0 -> ./Boot
|   |           |   |-- 1 -> ./System
|   |           |   |-- Boot/
|   |           |   `-- System/
|   |           `-- Atapi0/
|   |               |-- 0 -> ./CDROM
|   |               `-- CDROM/
|   `-- Volumes/
|       |-- BootVolume -> /System/Hardware/AcpiSystem/IdeCon0/0/0
|       |-- SystemVolume -> /System/Hardware/AcpiSystem/IdeCon0/0/1
|       |-- FLPDSK0 -> /System/Hardware/AcpiSystem/FloppyCon0/0/FLPDSK0
|       |-- FLPDSK1 -> /System/Hardware/AcpiSystem/FloppyCon0/0/FLPDSK1
|       `-- CDROM -> /System/Hardware/AcpiSystem/IdeCon0/1/0
|-- Temp/
|-- Users/
|   |-- User0/
|   `-- User1/
|-- Packages/
|   `-- FooPackage/
|       |-- <libversion-compatible version filter> -> dynamically resolved
|       |-- Latest -> ./0.0.1
|       `-- 0.0.1/
|           `-- Foo
`-- Configs/
```
