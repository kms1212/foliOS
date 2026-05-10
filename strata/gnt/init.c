#include <strata/gnt.h>
#include <strata/gnt/interface.h>
#include <strata/process.h>
#include <strata/status.h>

struct StGnt_Node *g_gnt_root_network;
struct StGnt_Node *g_gnt_root_local;
struct StGnt_Node *g_gnt_system_processes;

static StStatus register_directory_interfaces(struct StGnt_Node *node)
{
    StStatus status;

    status = StGnt_RegisterInterface(node, &StGntIf_Uuid_Directory, 0, 11);
    if (!CHECK_SUCCESS(status)) return status;

    return StGnt_RegisterInterface(node, &StGntIf_Uuid_FileInfo, 0, 2);
}

StStatus StGnt_Init(void)
{
    StStatus status;
    struct StGnt_Node *localhost_node;
    struct StGnt_Node *system_node;
    struct StGnt_Node *kernel_node;
    struct StGnt_Node *processes_node;
    struct StGnt_Node *threads_node;
    struct StGnt_Node *pipes_node;
    struct StGnt_Node *sockets_node;
    struct StGnt_Node *sharedmemories_node;
    struct StGnt_Node *modules_node;
    struct StGnt_Node *devices_node;
    struct StGnt_Node *hardware_node;
    struct StGnt_Node *firmware_node;
    struct StGnt_Node *volumes_node;

    status = StGnt_AddNode(NULL, NULL, &g_gnt_root_network);
    if (!CHECK_SUCCESS(status)) goto has_error;
    g_gnt_root_network->type = GNT_NODETYPE_DIRECTORY;

    status = StGnt_AddNode(g_gnt_root_network, U"Localhost", &localhost_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = StGnt_AddNode(NULL, NULL, &g_gnt_root_local);
    if (!CHECK_SUCCESS(status)) goto has_error;
    g_gnt_root_local->type = GNT_NODETYPE_DIRECTORY;

    localhost_node->type = GNT_NODETYPE_LINK;
    localhost_node->link.is_virtual = 1;
    localhost_node->link.virtual.target_node = g_gnt_root_local;

    status = StGnt_AddNode(g_gnt_root_local, U"System", &system_node);
    if (!CHECK_SUCCESS(status)) goto has_error;
    system_node->type = GNT_NODETYPE_DIRECTORY;

    status = StGnt_AddNode(system_node, U"Kernel", &kernel_node);
    if (!CHECK_SUCCESS(status)) goto has_error;
    kernel_node->type = GNT_NODETYPE_DIRECTORY;

    status = StGnt_AddNode(system_node, U"Processes", &processes_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    processes_node->type = GNT_NODETYPE_DIRECTORY;
    processes_node->handler_module = StProcess_Module;
    g_gnt_system_processes = processes_node;

    status = StGnt_AddNode(system_node, U"Threads", &threads_node);
    if (!CHECK_SUCCESS(status)) goto has_error;
    threads_node->type = GNT_NODETYPE_DIRECTORY;

    status = StGnt_AddNode(system_node, U"Pipes", &pipes_node);
    if (!CHECK_SUCCESS(status)) goto has_error;
    pipes_node->type = GNT_NODETYPE_DIRECTORY;

    status = StGnt_AddNode(system_node, U"Sockets", &sockets_node);
    if (!CHECK_SUCCESS(status)) goto has_error;
    sockets_node->type = GNT_NODETYPE_DIRECTORY;

    status = StGnt_AddNode(system_node, U"SharedMemories", &sharedmemories_node);
    if (!CHECK_SUCCESS(status)) goto has_error;
    sharedmemories_node->type = GNT_NODETYPE_DIRECTORY;

    status = StGnt_AddNode(system_node, U"Modules", &modules_node);
    if (!CHECK_SUCCESS(status)) goto has_error;
    modules_node->type = GNT_NODETYPE_DIRECTORY;

    status = StGnt_AddNode(system_node, U"Devices", &devices_node);
    if (!CHECK_SUCCESS(status)) goto has_error;
    devices_node->type = GNT_NODETYPE_DIRECTORY;

    status = StGnt_AddNode(system_node, U"Hardware", &hardware_node);
    if (!CHECK_SUCCESS(status)) goto has_error;
    hardware_node->type = GNT_NODETYPE_DIRECTORY;

    status = StGnt_AddNode(system_node, U"Firmware", &firmware_node);
    if (!CHECK_SUCCESS(status)) goto has_error;
    firmware_node->type = GNT_NODETYPE_DIRECTORY;

    status = StGnt_AddNode(system_node, U"Volumes", &volumes_node);
    if (!CHECK_SUCCESS(status)) goto has_error;
    volumes_node->type = GNT_NODETYPE_DIRECTORY;

    status = register_directory_interfaces(g_gnt_root_network);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = register_directory_interfaces(g_gnt_root_local);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = register_directory_interfaces(system_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = register_directory_interfaces(kernel_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = register_directory_interfaces(processes_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = register_directory_interfaces(threads_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = register_directory_interfaces(pipes_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = register_directory_interfaces(sockets_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = register_directory_interfaces(sharedmemories_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = register_directory_interfaces(modules_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = register_directory_interfaces(devices_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = register_directory_interfaces(hardware_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = register_directory_interfaces(volumes_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    return STATUS_SUCCESS;

has_error:
    return status;
}
