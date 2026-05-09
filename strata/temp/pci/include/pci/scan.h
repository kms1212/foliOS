#ifndef __PCI_SCAN_H__
#define __PCI_SCAN_H__

#include <strata/status.h>

enum StPci_ScanIterationDecision {
    ITERATION_CONTINUE,
    ITERATION_SKIP_CHILDREN,
    ITERATION_BREAK_SIBLINGS,
    ITERATION_ABORT,
};

typedef enum StPci_ScanIterationDecision (*StPci_BusIterationCallback)(void *, uint8_t);
typedef enum StPci_ScanIterationDecision (*StPci_DeviceIterationCallback)(void *, uint8_t, uint8_t);
typedef enum StPci_ScanIterationDecision (*StPci_FunctionIterationCallback)(
    void *, uint8_t, uint8_t, uint8_t
);

StStatus StPci_ScanBus(
    void *context,
    StPci_BusIterationCallback bus_callback,
    StPci_DeviceIterationCallback device_callback,
    StPci_FunctionIterationCallback function_callback
);

#endif  // __PCI_SCAN_H__
