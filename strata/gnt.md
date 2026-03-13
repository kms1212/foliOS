# GNT(Global Node Tree) (similar to VFS(Virtual File System) in general)

```
/
├── System/
│   ├── Kernel/
│   ├── Processes/
│   │   ├── Current -> ./0  # Current process
│   │   ├── 1 -> ./0/1
│   │   └── 0/
│   │       ├── Io/
│   │       │   ├── Stdin -> /System/Devices/Vtty0
│   │       │   ├── Stdout -> /System/Devices/Vtty0
│   │       │   └── Stderr -> /System/Devices/Vtty0
│   │       ├── Memory/
│   │       │   └── ...
│   │       ├── Threads/
│   │       │   ├── Current -> ./0  # Current thread
│   │       │   └── 0 -> /System/Threads/0
│   │       └── 1/  # Child process
│   │           ├── Executable -> /Packages/FooPackage/0.0.1/Foo
│   │           └── Threads/
│   │               ├── 0 -> /System/Threads/1
│   │               └── 1 -> /System/Threads/2
│   ├── Threads/
│   │   ├── 0/
│   │   │   └── Process -> /System/Processes/0
│   │   ├── 1/
│   │   │   └── Process -> /System/Processes/1
│   │   └── 2/
│   │       └── Process -> /System/Processes/1
│   ├── Pipes/
│   ├── Sockets/
│   ├── SharedMemories/
│   ├── Modules/
│   │   ├── HwDrivers/
│   │   ├── BusDrivers/
│   │   ├── FastServices/
│   │   └── Subsystems/
│   ├── Devices/
│   │   ├── Debug0 -> ../Hardwares/AcpiSystem/Debug0
│   │   ├── Keyboard0 -> ../Hardwares/AcpiSystem/HidCon0/Keyboard0
│   │   ├── Mouse0 -> ../Hardwares/AcpiSystem/HidCon0/Mouse0
│   │   ├── Rtc0 -> ../Hardwares/AcpiSystem/Rtc0
│   │   ├── Serial0 -> ../Hardwares/AcpiSystem/Serial0
│   │   ├── Serial1 -> ../Hardwares/AcpiSystem/Serial1
│   │   ├── Parallel0 -> ../Hardwares/AcpiSystem/Parallel0
│   │   ├── Drive0 -> ../Hardwares/AcpiSystem/Floppy0
│   │   ├── Drive1 -> ../Hardwares/AcpiSystem/Floppy1
│   │   ├── Drive2 -> ../Hardwares/AcpiSystem/Ata0
│   │   └── Drive3 -> ../Hardwares/AcpiSystem/Atapi0
│   ├── Hardwares/
│   │   └── AcpiSystem/
│   │       ├── Cpu0/
│   │       ├── Memory0/
│   │       ├── Memory1/
│   │       ├── DebugOut0/
│   │       ├── HidCon0/
│   │       │   ├── Keyboard0/
│   │       │   └── Mouse0/
│   │       ├── Rtc0/
│   │       ├── Serial0/
│   │       ├── Serial1/
│   │       ├── Parallel0/
│   │       ├── PciBridge0/
│   │       ├── FloppyCon0/
│   │       │   ├── 0 -> ./Floppy0
│   │       │   ├── 1 -> ./Floppy1
│   │       │   ├── Floppy0/
│   │       │   │   └── FLPDSK0/  # Volume name specified in filesystem
│   │       │   │       └── ...  # Volume contents
│   │       │   └── Floppy1/
│   │       │       └── FLPDSK1/  # Volume name specified in filesystem
│   │       │           └── ...  # Volume contents
│   │       └── IdeCon0/
│   │           ├── 0 -> ./Ata0
│   │           ├── 1 -> ./Atapi0
│   │           ├── Ata0/
│   │           │   ├── 0 -> ./Boot
│   │           │   ├── 1 -> ./System
│   │           │   ├── Boot/  # Volume name specified in filesystem
│   │           │   │   └── ...  # Volume contents
│   │           │   └── System/  # Volume name specified in filesystem
│   │           │       └── ...  # Volume contents
│   │           └── Atapi0/
│   │               ├── 0 -> ./CDROM
│   │               └── CDROM/  # Volume name specified in filesystem
│   │                   └── ...  # Volume contents
│   └── Volumes/
│       ├── BootVolume -> /System/Hardwares/AcpiSystem/IdeCon0/0/0
│       ├── SystemVolume -> /System/Hardwares/AcpiSystem/IdeCon0/0/1
│       ├── FLPDSK0 -> /System/Hardwares/AcpiSystem/FloppyCon0/0/FLPDSK0
│       ├── FLPDSK1 -> /System/Hardwares/AcpiSystem/FloppyCon0/0/FLPDSK1
│       └── CDROM -> /System/Hardwares/AcpiSystem/IdeCon0/1/0
├── Temp/
├── Users/
│   ├── User0/
│   │   └── ...
│   └── User1/
│       └── ...
├── Packages/
│   └── FooPackage/
│       ├── <libversion-compatible version filter> -> (dynamically resolved)
│       ├── Latest -> ./0.0.1
│       └── 0.0.1/
│           └── Foo
└── Configs/
    └── ...
```
