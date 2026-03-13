#include <vellum/pci/class.h>

#include <stddef.h>
#include <stdio.h>

#include <vellum/macros.h>

struct container;

struct entry {
    uint8_t key;
    const char *str;
    const struct container *child;
};

struct container {
    int entry_count;
    struct entry entries[];
};

static const struct container interface_names_vendor_specific = {
    1,
    {
        {
            0x00,
            "Vendor-specific interface",
        },
    }
};

static const struct container subclass_names_00 = {
    2,
    {
        {0x00, "Non-VGA unclassified device", &interface_names_vendor_specific},
        {0x01, "VGA compatible unclassified device", &interface_names_vendor_specific},
    }
};

static const struct container interface_names_01_00 = {
    5,
    {
        {
            0x00,
            "Vendor-specific interface",
        },
        {
            0x11,
            "SCSI storage device - SOP target port using PQI",
        },
        {
            0x12,
            "SCSI controller - SOP target port using PQI",
        },
        {
            0x13,
            "SCSI storage device and controller - SOP target port using PQI",
        },
        {
            0x21,
            "SCSI storage device - SOP target port using NVMe queueing interface",
        },
    }
};

static const struct container interface_names_01_05 = {
    2,
    {
        {
            0x20,
            "Single stepping ADMA interface",
        },
        {
            0x30,
            "Continuous operation ADMA interface",
        },
    }
};

static const struct container interface_names_01_06 = {
    3,
    {
        {
            0x00,
            "Vendor-specific interface",
        },
        {
            0x01,
            "AHCI interface",
        },
        {
            0x02,
            "Serial Storage Bus interface",
        },
    }
};

static const struct container interface_names_01_07 = {
    2,
    {
        {
            0x00,
            "Vendor-specific interface",
        },
        {
            0x01,
            "Obsolete",
        },
    }
};

static const struct container interface_names_01_08 = {
    4,
    {
        {
            0x00,
            "Vendor-specific interface",
        },
        {
            0x01,
            "NVMHCI interface",
        },
        {
            0x02,
            "NVMe I/O controller",
        },
        {
            0x03,
            "NVMe administrative controller",
        },
    }
};

static const struct container interface_names_01_09 = {
    2,
    {
        {
            0x00,
            "Vendor-specific interface",
        },
        {
            0x01,
            "UFSHCI interface",
        },
    }
};

static const struct container subclass_names_01 = {
    11,
    {
        {0x00, "SCSI storage controller", &interface_names_01_00},
        {0x01, "IDE interface"},
        {0x02, "Floppy disk controller", &interface_names_vendor_specific},
        {0x03, "IPI bus controller", &interface_names_vendor_specific},
        {0x04, "RAID bus controller", &interface_names_vendor_specific},
        {0x05, "ATA controller", &interface_names_01_05},
        {0x06, "SATA controller", &interface_names_01_06},
        {0x07, "Serial Attached SCSI controller", &interface_names_01_07},
        {0x08, "Non-Volatile memory controller", &interface_names_01_08},
        {0x09, "Universal Flash Storage controller", &interface_names_01_09},
        {0x80, "Mass storage controller", &interface_names_vendor_specific},
    }
};

static const struct container subclass_names_02 = {
    10,
    {
        {0x00, "Ethernet controller", &interface_names_vendor_specific},
        {0x01, "Token Ring controller", &interface_names_vendor_specific},
        {0x02, "FDDI controller", &interface_names_vendor_specific},
        {0x03, "ATM controller", &interface_names_vendor_specific},
        {0x04, "ISDN controller", &interface_names_vendor_specific},
        {0x05, "WorldFip controller", &interface_names_vendor_specific},
        {0x06, "PICMG 2.14 multi computing"},
        {0x07, "InfiniBand controller", &interface_names_vendor_specific},
        {0x08, "Host fabric controller", &interface_names_vendor_specific},
        {0x80, "Other network controller", &interface_names_vendor_specific},
    }
};

static const struct container interface_names_03_00 = {
    2,
    {
        {0x00, "VGA-compatible interface"},
        {0x01, "8514-compatible interface"},
    }
};

static const struct container subclass_names_03 = {
    4,
    {
        {0x00, "Display controller", &interface_names_03_00},
        {0x01, "XGA controller", &interface_names_vendor_specific},
        {0x02, "3D controller", &interface_names_vendor_specific},
        {0x80, "Other display controller", &interface_names_vendor_specific},
    }
};

static const struct container interface_names_04_03 = {0, {}};

static const struct container subclass_name_04 = {
    5,
    {
        {0x00, "Video device", &interface_names_vendor_specific},
        {0x01, "Audio device", &interface_names_vendor_specific},
        {0x02, "Computer telephony device", &interface_names_vendor_specific},
        {0x03, "Intel HD Audio device", &interface_names_04_03},
        {0x80, "Other multimedia device", &interface_names_vendor_specific},
    }
};

static const struct container subclass_name_05 = {
    3,
    {
        {0x00, "RAM", &interface_names_vendor_specific},
        {0x01, "Flash", &interface_names_vendor_specific},
        {0x80, "Other memory controller", &interface_names_vendor_specific},
    }
};

static const struct container interface_names_06_04 = {0, {}};

static const struct container interface_names_06_09 = {0, {}};

static const struct container interface_names_06_0B = {0, {}};

static const struct container subclass_name_06 = {
    13,
    {
        {0x00, "Host bridge", &interface_names_vendor_specific},
        {0x01, "ISA bridge", &interface_names_vendor_specific},
        {0x02, "EISA bridge", &interface_names_vendor_specific},
        {0x03, "MCA bridge", &interface_names_vendor_specific},
        {0x04, "PCI-to-PCI bridge", &interface_names_06_04},
        {0x05, "PCMCIA bridge", &interface_names_vendor_specific},
        {0x06, "NuBus bridge", &interface_names_vendor_specific},
        {0x07, "CardBus bridge", &interface_names_vendor_specific},
        {0x08, "RACEway bridge", &interface_names_vendor_specific},
        {0x09, "Semi-transparent PCI-to-PCI bridge", &interface_names_06_09},
        {0x0A, "InfiniBand-to-PCI host bridge", &interface_names_vendor_specific},
        {0x0B, "Advanced Switching to PCI host bridge", &interface_names_06_0B},
        {0x80, "Other bridge device", &interface_names_vendor_specific},
    }
};

static const struct container interface_names_07_00 = {0, {}};

static const struct container interface_names_07_01 = {0, {}};

static const struct container interface_names_07_03 = {0, {}};

static const struct container subclass_name_07 = {
    7,
    {
        {0x00, "Serial port controller", &interface_names_07_00},
        {0x01, "Parallel port controller", &interface_names_07_01},
        {0x02, "Multiport serial port controller", &interface_names_vendor_specific},
        {0x03, "Modem controller", &interface_names_07_03},
        {0x04, "GPIB controller", &interface_names_vendor_specific},
        {0x05, "Smart Card", &interface_names_vendor_specific},
        {0x80, "Other communications device", &interface_names_vendor_specific},
    }
};

static const struct container interface_names_08_00 = {0, {}};

static const struct container interface_names_08_01 = {0, {}};

static const struct container interface_names_08_02 = {0, {}};

static const struct container interface_names_08_03 = {0, {}};

static const struct container subclass_name_08 = {
    9,
    {
        {0x00, "Interrupt controller", &interface_names_08_00},
        {0x01, "DMA controller", &interface_names_08_01},
        {0x02, "System timer", &interface_names_08_02},
        {0x03, "RTC controller", &interface_names_08_03},
        {0x04, "Generic PCI hot-plug controller", &interface_names_vendor_specific},
        {0x05, "SD host controller", &interface_names_vendor_specific},
        {0x06, "IOMMU", &interface_names_vendor_specific},
        {0x07, "Root Complex Event Collector", &interface_names_vendor_specific},
        {0x80, "Other system peripheral", &interface_names_vendor_specific},
    }
};

static const struct container interface_names_09_04 = {0, {}};

static const struct container subclass_name_09 = {
    6,
    {
        {0x00, "Keyboard controller", &interface_names_vendor_specific},
        {0x01, "Digitizer", &interface_names_vendor_specific},
        {0x02, "Mouse controller", &interface_names_vendor_specific},
        {0x03, "Scanner controller", &interface_names_vendor_specific},
        {0x04, "Gameport controller", &interface_names_09_04},
        {0x80, "Other input controller", &interface_names_vendor_specific},
    }
};

static const struct container subclass_name_0A = {
    2,
    {
        {0x00, "Generic docking station", &interface_names_vendor_specific},
        {0x80, "Other docking station", &interface_names_vendor_specific},
    }
};

static const struct container subclass_name_0B = {
    8,
    {
        {0x00, "386", &interface_names_vendor_specific},
        {0x01, "486", &interface_names_vendor_specific},
        {0x02, "Pentium", &interface_names_vendor_specific},
        {0x10, "Alpha", &interface_names_vendor_specific},
        {0x20, "PowerPC", &interface_names_vendor_specific},
        {0x30, "MIPS", &interface_names_vendor_specific},
        {0x40, "Co-processor", &interface_names_vendor_specific},
        {0x80, "Other processors", &interface_names_vendor_specific},
    }
};

static const struct container interface_names_0C_00 = {0, {}};

static const struct container interface_names_0C_03 = {0, {}};

static const struct container interface_names_0C_07 = {0, {}};

static const struct container subclass_name_0C = {
    12,
    {
        {0x00, "IEEE1394", &interface_names_0C_00},
        {0x01, "ACCESS.bus", &interface_names_vendor_specific},
        {0x02, "SSA", &interface_names_vendor_specific},
        {0x03, "USB", &interface_names_0C_03},
        {0x04, "Fibre Channel", &interface_names_vendor_specific},
        {0x05, "SMBus", &interface_names_vendor_specific},
        {0x06, "InfiniBand (deprecated)", &interface_names_vendor_specific},
        {0x07, "IPMI", &interface_names_0C_07},
        {0x08, "SERCOS", &interface_names_vendor_specific},
        {0x09, "CANbus", &interface_names_vendor_specific},
        {0x0A, "MIPI I3C host controller", &interface_names_vendor_specific},
        {0x80, "Other serial bus controllers", &interface_names_vendor_specific},
    }
};

static const struct container interface_names_0D_01 = {0, {}};

static const struct container subclass_name_0D = {
    10,
    {
        {0x00, "iRDA compatible controller", &interface_names_vendor_specific},
        {0x00, "CIR or UWB Radio controller", &interface_names_0D_01},
        {0x00, "RF controller", &interface_names_vendor_specific},
        {0x00, "Bluetooth", &interface_names_vendor_specific},
        {0x00, "Broadband", &interface_names_vendor_specific},
        {0x00, "Ethernet (5 GHz)", &interface_names_vendor_specific},
        {0x00, "Ethernet (2.4 GHz)", &interface_names_vendor_specific},
        {0x00, "Cellular controller/modem", &interface_names_vendor_specific},
        {0x00, "Cellular controller/modem plus Ethernet", &interface_names_vendor_specific},
        {0x00, "Other wireless controllers", &interface_names_vendor_specific},
    }
};

static const struct container interface_names_0E_00 = {0, {}};

static const struct container subclass_name_0E = {
    1,
    {
        {0x00, "Intelligent I/O controller", &interface_names_0E_00},
    }
};

static const struct container subclass_name_0F = {
    5,
    {
        {0x01, "TV", &interface_names_vendor_specific},
        {0x02, "Audio", &interface_names_vendor_specific},
        {0x03, "Voice", &interface_names_vendor_specific},
        {0x04, "Data", &interface_names_vendor_specific},
        {0x80, "Other satellite communications controller", &interface_names_vendor_specific},
    }
};

static const struct container subclass_name_10 = {
    3,
    {
        {0x00,
         "Network and computing encryption and decryption controller",
         &interface_names_vendor_specific},
        {0x10,
         "Entertainment encryption and decryption controller",
         &interface_names_vendor_specific},
        {0x80, "Other encryption and decryption controller", &interface_names_vendor_specific},
    }
};

static const struct container subclass_name_11 = {
    5,
    {
        {0x00, "DPIO Modules", &interface_names_vendor_specific},
        {0x01, "Performance counters", &interface_names_vendor_specific},
        {0x10,
         "Communications synchronization plus time and frequency test/measurement",
         &interface_names_vendor_specific},
        {0x20, "Management card", &interface_names_vendor_specific},
        {0x80,
         "Other data acquisition/signal processing controllers",
         &interface_names_vendor_specific},
    }
};

static const struct container subclass_name_12 = {
    1,
    {
        {0x00, "Processing accelerator", &interface_names_vendor_specific},
    }
};

static const struct container subclass_name_13 = {
    1,
    {
        {0x00, "Non-essential instrumentation", &interface_names_vendor_specific},
    }
};

static const struct container class_names = {
    21,
    {
        {0x00, "Unclassified device", &subclass_names_00},
        {0x01, "Mass storage controller", &subclass_names_01},
        {0x02, "Network controller", &subclass_names_02},
        {0x03, "Display controller", &subclass_names_03},
        {0x04, "Multimedia controller", &subclass_name_04},
        {0x05, "Memory controller", &subclass_name_05},
        {0x06, "Bridge", &subclass_name_06},
        {0x07, "Communication controller", &subclass_name_07},
        {0x08, "Generic system peripheral", &subclass_name_08},
        {0x09, "Input device controller", &subclass_name_09},
        {0x0a, "Docking station", &subclass_name_0A},
        {0x0b, "Processor", &subclass_name_0B},
        {0x0c, "Serial bus controller", &subclass_name_0C},
        {0x0d, "Wireless controller", &subclass_name_0D},
        {0x0e, "Intelligent controller", &subclass_name_0E},
        {0x0f, "Satellite communications controller", &subclass_name_0F},
        {0x10, "Encryption controller", &subclass_name_10},
        {0x11, "Signal processing controller", &subclass_name_11},
        {0x12, "Processing accelerators", &subclass_name_12},
        {0x13, "Non-essential instrumentation", &subclass_name_13},
        {
            0xFF,
            "Unassigneed class",
        },
    }
};

static const struct entry *binary_search(const struct container *cont, uint8_t key)
{
    const struct entry *ret = NULL;
    int start_idx = 0, end_idx = cont->entry_count - 1;

    if (cont->entry_count < 1) {
        return NULL;
    }

    while (!ret) {
        int half_idx = (start_idx + end_idx) / 2;

        if (start_idx == end_idx && cont->entries[half_idx].key != key) {
            return NULL;
        } else if (start_idx == half_idx) {
            if (cont->entries[start_idx].key == key) {
                return &cont->entries[start_idx];
            } else if (cont->entries[end_idx].key == key) {
                return &cont->entries[end_idx];
            } else {
                return NULL;
            }
        } else if (cont->entries[half_idx].key > key) {
            end_idx = half_idx;
        } else if (cont->entries[half_idx].key < key) {
            start_idx = half_idx;
        } else {
            ret = &cont->entries[half_idx];
        }
    }

    return ret;
}

const char *VlPci_GetDeviceClassName(uint8_t class)
{
    const struct entry *class_entry = binary_search(&class_names, class);
    if (!class_entry) return NULL;

    return class_entry->str;
}

const char *VlPci_GetDeviceSubclassName(uint8_t class, uint8_t subclass)
{
    const struct entry *class_entry = binary_search(&class_names, class);
    if (!class_entry) return NULL;

    const struct entry *subclass_entry = binary_search(class_entry->child, subclass);
    if (!subclass_entry) return NULL;

    return subclass_entry->str;
}

const char *VlPci_GetDeviceInterfaceName(uint8_t class, uint8_t subclass, uint8_t interface)
{
    const struct entry *class_entry = binary_search(&class_names, class);
    if (!class_entry) return NULL;

    const struct entry *subclass_entry = binary_search(class_entry->child, subclass);
    if (!subclass_entry) return NULL;

    const struct entry *interface_entry = binary_search(subclass_entry->child, interface);
    if (!interface_entry) return NULL;

    return interface_entry->str;
}
