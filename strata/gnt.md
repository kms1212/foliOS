# GNT(Global Node Tree) (equivalent to VFS(Virtual File System) in other OS)

to get current process: open "/System/Processes/Current"

```
/
├── System/
│   ├── Kernel/
│   ├── Processes/
│   │   ├── Current -> ./0
│   │   ├── 1 -> ./0/1
│   │   └── 0/
│   │       ├── Io/
│   │       │   ├── Stdin -> /System/Devices/Vtty0
│   │       │   ├── Stdout -> /System/Devices/Vtty0
│   │       │   └── Stderr -> /System/Devices/Vtty0
│   │       ├── Memory/
│   │       │   └── ...
│   │       ├── Threads/
│   │       │   └── 0 -> /System/Threads/0
│   │       └── 1/
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
│   ├── Shms/
│   ├── Modules/
│   │   ├── HwDrivers/
│   │   ├── BusDrivers/
│   │   ├── FastServices/
│   │   └── Subsystems/
│   ├── Devices/
│   │   ├── Debug0 -> ./AcpiSystem/Debug0
│   │   ├── Keyboard0 -> ./AcpiSystem/HidCon0/Keyboard0
│   │   ├── Mouse0 -> ./AcpiSystem/HidCon0/Mouse0
│   │   ├── Rtc0 -> ./AcpiSystem/Rtc0
│   │   ├── Serial0 -> ./AcpiSystem/Serial0
│   │   ├── Serial1 -> ./AcpiSystem/Serial1
│   │   ├── Parallel0 -> ./AcpiSystem/Parallel0
│   │   ├── Drive0 -> ./AcpiSystem/Floppy0
│   │   ├── Drive1 -> ./AcpiSystem/Floppy1
│   │   ├── Drive2 -> ./AcpiSystem/Ata0
│   │   └── Drive3 -> ./AcpiSystem/Atapi0
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
│       ├── Current -> ./0.0.1
│       └── 0.0.1/
│           └── Foo
└── Configs/
    └── ...
```
