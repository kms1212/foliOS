#include <stdint.h>
#include <stdio.h>

#include <strata/arch/interrupt.h>

#include <strata/gnt.h>
#include <strata/log.h>
#include <strata/macros.h>
#include <strata/utf.h>

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

    return ITERATION_CONTINUE;
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

    return ITERATION_CONTINUE;
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
