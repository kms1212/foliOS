#include <strata/gnt.h>

#include <strata/status.h>

struct StGnt_Node *g_gnt_root_network;
struct StGnt_Node *g_gnt_root_local;

StStatus StGnt_Init(void)
{
    StStatus status;
    struct StGnt_Node *system_node;
    struct StGnt_Node *kernel_node;
    struct StGnt_Node *processes_node;
    struct StGnt_Node *threads_node;
    struct StGnt_Node *pipes_node;
    struct StGnt_Node *sockets_node;
    struct StGnt_Node *sharedmemories_node;
    struct StGnt_Node *modules_node;
    struct StGnt_Node *devices_node;
    struct StGnt_Node *hardwares_node;
    struct StGnt_Node *volumes_node;

    status = StGnt_AddNode(NULL, NULL, &g_gnt_root_local);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = StGnt_AddNode(NULL, NULL, &g_gnt_root_network);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = StGnt_AddNode(g_gnt_root_local, U"System", &system_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = StGnt_AddNode(system_node, U"Kernel", &kernel_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = StGnt_AddNode(system_node, U"Processes", &processes_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = StGnt_AddNode(system_node, U"Threads", &threads_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = StGnt_AddNode(system_node, U"Pipes", &pipes_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = StGnt_AddNode(system_node, U"Sockets", &sockets_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = StGnt_AddNode(system_node, U"SharedMemories", &sharedmemories_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = StGnt_AddNode(system_node, U"Modules", &modules_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = StGnt_AddNode(system_node, U"Devices", &devices_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = StGnt_AddNode(system_node, U"Hardwares", &hardwares_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = StGnt_AddNode(system_node, U"Volumes", &volumes_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    return STATUS_SUCCESS;

has_error:
    return status;
}
