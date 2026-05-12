#include <pci/scan.h>

#include <stdint.h>

#include <strata/status.h>

#include <pci/cfgspace.h>
#include <plat/pci/cfgspace.h>

#define MODULE_NAME "pci"

struct callback_status_set {
    StPci_BusIterationCallback bus_callback;
    StPci_DeviceIterationCallback device_callback;
    StPci_FunctionIterationCallback function_callback;
    int is_aborted;
};

static StStatus scan_pci_bus(
    void *context,
    struct callback_status_set *iter_status,
    enum StPci_ScanIterationDecision parent_iter_decision,
    uint8_t bus
);

static StStatus scan_pci_function(
    void *context,
    struct callback_status_set *iter_status,
    enum StPci_ScanIterationDecision parent_iter_decision,
    uint8_t bus,
    uint8_t device,
    uint8_t function
)
{
    StStatus status;
    enum StPci_ScanIterationDecision iter_decision = parent_iter_decision;
    uint8_t base_class;
    uint8_t sub_class;

    status = StPciP_ReadCfg8(bus, device, function, PCI_CFGHDR_BASE_CLASS, &base_class);
    if (!CHECK_SUCCESS(status)) return status;

    status = StPciP_ReadCfg8(bus, device, function, PCI_CFGHDR_SUB_CLASS, &sub_class);
    if (!CHECK_SUCCESS(status)) return status;

    if (base_class == 0x06 && sub_class == 0x04) {
        uint8_t secondary_bus;

        status = StPciP_ReadCfg8(bus, device, function, PCI_CFGHDR1_SECONDARY_BUS, &secondary_bus);
        if (!CHECK_SUCCESS(status)) return status;

        if (secondary_bus) {
            if (iter_decision != ITERATION_BREAK_SIBLINGS && iter_status->bus_callback) {
                iter_decision = iter_status->bus_callback(context, secondary_bus);

                if (iter_decision == ITERATION_ABORT) {
                    iter_status->is_aborted = 1;
                    return STATUS_SUCCESS;
                }
            }

            status = scan_pci_bus(context, iter_status, iter_decision, secondary_bus);
            if (!CHECK_SUCCESS(status)) return status;
            if (iter_status->is_aborted) return STATUS_SUCCESS;
        }
    }

    return STATUS_SUCCESS;
}

static StStatus scan_pci_device(
    void *context,
    struct callback_status_set *iter_status,
    enum StPci_ScanIterationDecision parent_iter_decision,
    uint8_t bus,
    uint8_t device
)
{
    StStatus status;
    int mute_siblings = 0;
    uint8_t header_type;

    status = StPciP_ReadCfg8(bus, device, 0, PCI_CFGHDR_HEADER_TYPE, &header_type);
    if (!CHECK_SUCCESS(status)) return status;

    for (int function = 0; function < ((header_type & PCI_HEADER_TYPE_MULTIFUNC) ? 8 : 1);
         function++) {
        enum StPci_ScanIterationDecision iter_decision = parent_iter_decision;

        if (function > 0) {
            uint16_t vendor_id;

            status = StPciP_ReadCfg16(bus, device, function, PCI_CFGHDR_VENDORID, &vendor_id);
            if (!CHECK_SUCCESS(status)) return status;

            if (vendor_id == 0xFFFF) continue;
        }

        if (iter_decision != ITERATION_SKIP_CHILDREN && iter_decision != ITERATION_BREAK_SIBLINGS &&
            !mute_siblings && iter_status->function_callback) {
            iter_decision = iter_status->function_callback(context, bus, device, function);

            if (iter_decision == ITERATION_BREAK_SIBLINGS) {
                mute_siblings = 1;
            } else if (iter_decision == ITERATION_ABORT) {
                iter_status->is_aborted = 1;
                return STATUS_SUCCESS;
            }
        }

        status = scan_pci_function(context, iter_status, iter_decision, bus, device, function);
        if (!CHECK_SUCCESS(status)) return status;
        if (iter_status->is_aborted) return STATUS_SUCCESS;
    }

    return STATUS_SUCCESS;
}

static StStatus scan_pci_bus(
    void *context,
    struct callback_status_set *iter_status,
    enum StPci_ScanIterationDecision parent_iter_decision,
    uint8_t bus
)
{
    StStatus status;
    int mute_siblings = 0;

    for (int device = 0; device < 32; device++) {
        enum StPci_ScanIterationDecision iter_decision = parent_iter_decision;
        uint16_t vendor_id;

        status = StPciP_ReadCfg16(bus, device, 0, PCI_CFGHDR_VENDORID, &vendor_id);
        if (!CHECK_SUCCESS(status)) return status;

        if (vendor_id == 0xFFFF) continue;

        if (iter_decision != ITERATION_SKIP_CHILDREN && iter_decision != ITERATION_BREAK_SIBLINGS &&
            !mute_siblings && iter_status->device_callback) {
            iter_decision = iter_status->device_callback(context, bus, device);

            if (iter_decision == ITERATION_BREAK_SIBLINGS) {
                mute_siblings = 1;
            } else if (iter_decision == ITERATION_ABORT) {
                iter_status->is_aborted = 1;
                return STATUS_SUCCESS;
            }
        }

        status = scan_pci_device(context, iter_status, iter_decision, bus, device);
        if (!CHECK_SUCCESS(status)) return status;
        if (iter_status->is_aborted) return STATUS_SUCCESS;
    }

    return STATUS_SUCCESS;
}

StStatus StPci_ScanBus(
    void *context,
    StPci_BusIterationCallback bus_callback,
    StPci_DeviceIterationCallback device_callback,
    StPci_FunctionIterationCallback function_callback
)
{
    StStatus status;
    struct callback_status_set iter_status;
    enum StPci_ScanIterationDecision iter_decision = ITERATION_CONTINUE;

    iter_status.bus_callback = bus_callback;
    iter_status.device_callback = device_callback;
    iter_status.function_callback = function_callback;
    iter_status.is_aborted = 0;

    if (bus_callback) {
        iter_decision = bus_callback(context, 0);

        if (iter_decision == ITERATION_ABORT || iter_decision == ITERATION_BREAK_SIBLINGS) {
            return STATUS_SUCCESS;
        }
    }

    status = scan_pci_bus(context, &iter_status, iter_decision, 0);
    if (!CHECK_SUCCESS(status)) return status;
    if (iter_status.is_aborted) return STATUS_SUCCESS;

    return STATUS_SUCCESS;
}
