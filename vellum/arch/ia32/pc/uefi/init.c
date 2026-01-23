#include <efi.h>
#include <efilib.h>

extern void main(void);

static EFI_GUID LoadedImageProtocolGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
static EFI_GUID BlockIoProtocolGuid = EFI_BLOCK_IO_PROTOCOL_GUID;

EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    InitializeLib(ImageHandle, SystemTable);
    
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    EFI_STATUS Status;
    UINTN HandleCount = 0;
    EFI_HANDLE *HandleBuffer = NULL;

    Status = SystemTable->BootServices->HandleProtocol(ImageHandle, &LoadedImageProtocolGuid, (void **)&LoadedImage);
    if (EFI_ERROR(Status)) {
        Print(L"HandleProtocol() failed: 0x%lx\n", Status);
        return Status;
    }

    Print(L"Image loaded at: 0x%x\n", (uint32_t)LoadedImage->ImageBase);

    volatile uint64_t *MarkerPtr = (uint64_t *)0x10000;
    volatile uint64_t *ImageBasePtr = (uint64_t *)0x10008;
    *ImageBasePtr = (uint32_t)LoadedImage->ImageBase;  // Store ImageBase
    *MarkerPtr = 0xDEADBEEF;   // Set marker

    Status = SystemTable->BootServices->LocateHandleBuffer(
        ByProtocol,
        &BlockIoProtocolGuid,
        NULL,
        &HandleCount,
        &HandleBuffer
    );
    if (EFI_ERROR(Status)) {
        Print(L"LocateHandleBuffer(ByProtocol) failed: 0x%lx\n", Status);
        return Status;
    }

    Print(L"Found %u block handles.\r\n", (unsigned)HandleCount);
    for (UINTN i = 0; i < HandleCount; ++i) {
        EFI_BLOCK_IO *BlockIo = NULL;
        Status = SystemTable->BootServices->HandleProtocol(
            HandleBuffer[i],
            &BlockIoProtocolGuid,
            (void**)&BlockIo
        );
        if (EFI_ERROR(Status) || BlockIo == NULL) {
            Print(L"[%5u] HandleProtocol(BlockIo) failed: %lx\r\n", (unsigned)i, Status);
            continue;
        }

        EFI_BLOCK_IO_MEDIA *Media = BlockIo->Media;
        if (Media == NULL) {
            Print(L"[%5u] No media info.\r\n", (unsigned)i);
            continue;
        }

        Print(L"[%5u] Block IO device:\r\n", (unsigned)i);
        Print(L"        MediaId: %u\r\n", Media->MediaId);
        Print(L"        RemovableMedia: %s\r\n", Media->RemovableMedia ? L"true" : L"false");
        Print(L"        MediaPresent: %s\r\n", Media->MediaPresent ? L"true" : L"false");
        Print(L"        LogicalPartition: %s\r\n", Media->LogicalPartition ? L"true" : L"false");
        Print(L"        ReadOnly: %s\r\n", Media->ReadOnly ? L"true" : L"false");
        Print(L"        BlockSize: %u\r\n", Media->BlockSize);
        Print(L"        LastBlock: %llu\r\n", Media->LastBlock);
        Print(L"        LowestAlignedLba: %llu\r\n", Media->LowestAlignedLba);
    }

    SystemTable->BootServices->FreePool(HandleBuffer);

    Print(L"Hello, world!\n");

    main();

    for (;;) {}

    return EFI_SUCCESS;
}
