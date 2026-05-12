#include <stdint.h>
#include <stdio.h>

#include <strata/gnt.h>
#include <strata/log.h>
#include <strata/macros.h>
#include <strata/status.h>
#include <strata/utf.h>

#include <plat/pci/cfgspace.h>

#include <pci/cfgspace.h>
#include <pci/scan.h>

#define MODULE_NAME "pci"

struct pci_iter_data {
    StStatus status;
    struct StGnt_Node *pci_root_node;
    struct StGnt_Node *current_bus_node;
    struct StGnt_Node *current_device_node;
    struct StGnt_Node *current_function_node;
};

enum StPci_ScanIterationDecision iterate_bus(void *data, uint8_t bus)
{
    StStatus status;
    struct pci_iter_data *iter_data = (struct pci_iter_data *)data;
    char node_name[3];
    St_Utf32Char node_name_utf32[3] = {
        0,
    };

    snprintf(node_name, sizeof(node_name), "%02X", bus);

    status = StUtf_Utf8ToUtf32(
        (const St_Utf8Char *)node_name,
        sizeof(node_name),
        node_name_utf32,
        ARRAY_SIZE(node_name_utf32),
        NULL
    );
    if (!CHECK_SUCCESS(status)) {
        iter_data->status = status;
        return ITERATION_ABORT;
    }

    status = StGnt_AddNode(iter_data->pci_root_node, node_name_utf32, &iter_data->current_bus_node);
    if (!CHECK_SUCCESS(status)) {
        iter_data->status = status;
        return ITERATION_ABORT;
    }
    iter_data->current_bus_node->type = GNT_NODETYPE_DIRECTORY;

    return ITERATION_CONTINUE;
}

enum StPci_ScanIterationDecision iterate_device(void *data, uint8_t bus, uint8_t device)
{
    StStatus status;
    struct pci_iter_data *iter_data = (struct pci_iter_data *)data;
    char node_name[3];
    St_Utf32Char node_name_utf32[3] = {
        0,
    };

    snprintf(node_name, sizeof(node_name), "%02X", device);

    status = StUtf_Utf8ToUtf32(
        (const St_Utf8Char *)node_name,
        sizeof(node_name),
        node_name_utf32,
        ARRAY_SIZE(node_name_utf32),
        NULL
    );
    if (!CHECK_SUCCESS(status)) {
        iter_data->status = status;
        return ITERATION_ABORT;
    }

    status = StGnt_AddNode(
        iter_data->current_bus_node,
        node_name_utf32,
        &iter_data->current_device_node
    );
    if (!CHECK_SUCCESS(status)) {
        iter_data->status = status;
        return ITERATION_ABORT;
    }
    iter_data->current_device_node->type = GNT_NODETYPE_DIRECTORY;

    return ITERATION_CONTINUE;
}

#define PCI_MAX_BARS 6

struct StPci_DeviceInfo {
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t base_class;
    uint8_t sub_class;
    uint8_t prog_if;
    uint8_t revision_id;
    uint8_t interrupt_line;  // (옵션) Legacy IRQ
    uint8_t interrupt_pin;
};

// BAR (Base Address Register) 정보 DTO
struct StPci_BarInfo {
    uint64_t base_address;  // 64-bit BAR를 위해 항상 64비트로 제공
    uint64_t size;          // 바이트 단위 크기
    uint8_t type;           // 0: Memory, 1: I/O
    uint8_t is_prefetchable;
    uint8_t is_64bit;
};

// 커널 스페이스에 상주하는 PCI 장치 컨텍스트
struct pci_device_node {
    // 위치 정보 (동적 Config Space 접근 시 사용)
    uint8_t bus;
    uint8_t device;
    uint8_t function;

    // 캐싱된 정적 정보
    struct StPci_DeviceInfo info;
    struct StPci_BarInfo bars[PCI_MAX_BARS];
    int bar_valid[PCI_MAX_BARS];  // 해당 BAR가 유효한지 여부
};

static void pci_scan_bars(struct pci_device_node *node)
{
    uint8_t bus = node->bus;
    uint8_t device = node->device;
    uint8_t function = node->function;
    uint8_t header_type;
    uint16_t orig_cmd;

    StPciP_ReadCfg16(bus, device, function, PCI_CFGHDR_COMMAND, &orig_cmd);
    StPciP_WriteCfg16(
        bus,
        device,
        function,
        PCI_CFGHDR_COMMAND,
        orig_cmd & ~(PCI_COMMAND_MEMORY_SPACE | PCI_COMMAND_IO_SPACE)
    );

    StPciP_ReadCfg8(bus, device, function, PCI_CFGHDR_HEADER_TYPE, &header_type);
    header_type &= 0x7F;
    int max_bars;
    if (header_type == 0x00) {
        max_bars = 6;
    } else {
        max_bars = (header_type == 0x01) ? 2 : 0;
    }

    for (int i = 0; i < max_bars; i++) {
        node->bar_valid[i] = 0;

        uint16_t offset = PCI_CFGHDR0_BAR0 + (i * 4);
        uint32_t orig_val;
        uint32_t size_mask;

        StPciP_ReadCfg32(bus, device, function, offset, &orig_val);
        StPciP_WriteCfg32(bus, device, function, offset, 0xFFFFFFFF);
        StPciP_ReadCfg32(bus, device, function, offset, &size_mask);
        StPciP_WriteCfg32(bus, device, function, offset, orig_val);

        uint32_t base_mask =
            (orig_val & PCI_BAR_IOSPACE_SEL) ? PCI_BAR_IO_BASE_MASK : PCI_BAR_MEM_BASE_MASK;
        if ((size_mask & base_mask) == 0) continue;

        struct StPci_BarInfo *bar = &node->bars[i];
        node->bar_valid[i] = 1;

        if (orig_val & PCI_BAR_IOSPACE_SEL) {
            bar->type = 1;
            bar->is_64bit = 0;
            bar->is_prefetchable = 0;
            bar->base_address = orig_val & PCI_BAR_IO_BASE_MASK;
            bar->size = ~(size_mask & PCI_BAR_IO_BASE_MASK) + 1;
        } else {
            bar->type = 0;
            bar->is_prefetchable = (orig_val & PCI_BAR_MEM_PREFETCHABLE) ? 1 : 0;
            bar->base_address = orig_val & PCI_BAR_MEM_BASE_MASK;
            bar->size = ~(size_mask & PCI_BAR_MEM_BASE_MASK) + 1;

            if ((orig_val & PCI_BAR_MEM_TYPE_MASK) == PCI_BAR_MEM_TYPE_64BIT) {
                bar->is_64bit = 1;

                if (i + 1 < max_bars) {
                    uint16_t offset_high = offset + 4;
                    uint32_t orig_high;
                    uint32_t size_mask_high;

                    StPciP_ReadCfg32(bus, device, function, offset_high, &orig_high);
                    StPciP_WriteCfg32(bus, device, function, offset_high, 0xFFFFFFFF);
                    StPciP_ReadCfg32(bus, device, function, offset_high, &size_mask_high);
                    StPciP_WriteCfg32(bus, device, function, offset_high, orig_high);

                    bar->base_address |= ((uint64_t)orig_high << 32);

                    uint64_t full_size_mask =
                        ((uint64_t)size_mask_high << 32) | (size_mask & PCI_BAR_MEM_BASE_MASK);
                    bar->size = ~full_size_mask + 1;

                    node->bar_valid[i + 1] = 0;
                    i++;
                }
            } else {
                bar->is_64bit = 0;
            }
        }
    }

    StPciP_WriteCfg16(bus, device, function, PCI_CFGHDR_COMMAND, orig_cmd);
}

enum StPci_ScanIterationDecision iterate_function(
    void *data, uint8_t bus, uint8_t device, uint8_t function
)
{
    StStatus status;
    struct pci_iter_data *iter_data = (struct pci_iter_data *)data;
    char node_name[2];
    St_Utf32Char node_name_utf32[2] = {
        0,
    };
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t base_class;
    uint8_t sub_class;
    uint8_t interface;
    uint16_t subsystem_vendor_id;
    uint16_t subsystem_id;
    struct pci_device_node info = {
        0,
    };

    status = StPciP_ReadCfg16(bus, device, function, PCI_CFGHDR_VENDORID, &vendor_id);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = StPciP_ReadCfg16(bus, device, function, PCI_CFGHDR_DEVICEID, &device_id);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = StPciP_ReadCfg8(bus, device, function, PCI_CFGHDR_BASE_CLASS, &base_class);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = StPciP_ReadCfg8(bus, device, function, PCI_CFGHDR_SUB_CLASS, &sub_class);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = StPciP_ReadCfg8(bus, device, function, PCI_CFGHDR_INTERFACE, &interface);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status =
        StPciP_ReadCfg16(bus, device, function, PCI_CFGHDR0_SUBSYS_VENDOR_ID, &subsystem_vendor_id);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = StPciP_ReadCfg16(bus, device, function, PCI_CFGHDR0_SUBSYS_ID, &subsystem_id);
    if (!CHECK_SUCCESS(status)) goto has_error;

    snprintf(node_name, sizeof(node_name), "%X", function);

    status = StUtf_Utf8ToUtf32(
        (const St_Utf8Char *)node_name,
        sizeof(node_name),
        node_name_utf32,
        ARRAY_SIZE(node_name_utf32),
        NULL
    );
    if (!CHECK_SUCCESS(status)) {
        iter_data->status = status;
        return ITERATION_ABORT;
    }

    status = StGnt_AddNode(
        iter_data->current_device_node,
        node_name_utf32,
        &iter_data->current_function_node
    );
    if (!CHECK_SUCCESS(status)) {
        iter_data->status = status;
        return ITERATION_ABORT;
    }

    info.bus = bus;
    info.device = device;
    info.function = function;

    pci_scan_bars(&info);

    LOG_DEBUG(
        LM_CAT_PCI,
        "device %02X:%02X.%02X, PCI\\VEN_%04X&DEV_%04X&CC_%02X%02X%02X&SUBSYS_%04X%04X\n",
        bus,
        device,
        function,
        vendor_id,
        device_id,
        base_class,
        sub_class,
        interface,
        subsystem_vendor_id,
        subsystem_id
    );
    for (unsigned int i = 0; i < ARRAY_SIZE(info.bars); i++) {
        if (!info.bar_valid[i]) continue;

        LOG_DEBUG(
            LM_CAT_PCI,
            "[BAR%d] Type: %s, Prefetchable: %s, 64-bit: %s, Addr: %lX - %lX\n",
            i,
            info.bars[i].type == 1 ? "I/O" : "Memory",
            info.bars[i].is_prefetchable ? "Yes" : "No",
            info.bars[i].is_64bit ? "Yes" : "No",
            info.bars[i].base_address,
            info.bars[i].base_address + info.bars[i].size - 1
        );
    }

    return ITERATION_CONTINUE;

has_error:
    iter_data->status = status;
    return ITERATION_ABORT;
}

static StStatus register_gnt_nodes(struct StGnt_Node *parent_node)
{
    struct pci_iter_data iter_data = {
        .status = STATUS_SUCCESS,
    };
    StStatus status;
    struct StGnt_Node *pci_root;

    status = StGnt_AddNode(parent_node, U"PCI", &pci_root);
    if (!CHECK_SUCCESS(status)) return status;
    pci_root->type = GNT_NODETYPE_DIRECTORY;

    iter_data.pci_root_node = pci_root;
    status = StPci_ScanBus(&iter_data, iterate_bus, iterate_device, iterate_function);
    if (!CHECK_SUCCESS(status)) return status;
    if (!CHECK_SUCCESS(iter_data.status)) return iter_data.status;

    return STATUS_SUCCESS;
}

StStatus pci_module_main(struct StGnt_Node *parent_node)
{
    StStatus status;

    status = register_gnt_nodes(parent_node);
    if (!CHECK_SUCCESS(status)) return status;

    return STATUS_SUCCESS;
}
