#pragma once
#include <Uefi.h>
#include <Guid/FileInfo.h>
#include <Guid/FileSystemInfo.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/BlockIo.h>
#include <Library/PrintLib.h>
#include <Protocol/DevicePath.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/FormBrowser2.h>
#include <Protocol/FormBrowserEx.h>
#include <Protocol/FormBrowserEx2.h>
#include <Protocol/AcpiSystemDescriptionTable.h>
#include <Protocol/DisplayProtocol.h>
#include <Protocol/HiiPopup.h>
#include <Library/MemoryAllocationLib.h>
#include <PiDxe.h>
#include <Protocol/FirmwareVolume2.h>
#include <Library/BaseMemoryLib.h>

EFI_STATUS FindLoadedImageFromName(
  IN EFI_HANDLE ImageHandle,
  IN CHAR8 *FileName,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  IN EFI_GUID FilterProtocol
);

EFI_STATUS FindLoadedImageFromGUID(
  IN EFI_HANDLE ImageHandle,
  IN CHAR8 *FileGuid,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  IN EFI_SECTION_TYPE Section_Type,
  IN EFI_GUID FilterProtocol
);

EFI_STATUS LoadFromFS(
  IN EFI_HANDLE ImageHandle,
  IN CHAR8 *FileName,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  OUT EFI_HANDLE *AppImageHandle
);

EFI_STATUS LoadFromFV(
  IN EFI_HANDLE ImageHandle,
  IN CHAR8 *FileName,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  OUT EFI_HANDLE *AppImageHandle,
  IN EFI_SECTION_TYPE Section_Type,
  IN EFI_GUID FilterProtocol
);

EFI_STATUS LoadGUIDandSavePE(
  IN EFI_HANDLE ImageHandle,
  IN CHAR8 *FileGuid,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  OUT EFI_HANDLE *AppImageHandle,
  IN EFI_SECTION_TYPE Section_Type,
  IN EFI_SYSTEM_TABLE *SystemTable,
  IN EFI_GUID FilterProtocol
);

EFI_STATUS
LoadGUIDandSaveFreeform(
  IN EFI_HANDLE ImageHandle,
  OUT VOID **Pointer,
  OUT UINT64 *Size,
  IN CHAR8 *FileGuid,
  IN CHAR8 *SectionGuid OPTIONAL,
  IN EFI_SYSTEM_TABLE *SystemTable,
  IN EFI_GUID FilterProtocol
);

EFI_STATUS Exec(
  IN EFI_HANDLE *AppImageHandle
);

EFI_STATUS
UninstallProtocol(
  IN CHAR8 *ProtocolGuid,
  OUT UINTN Indexes
);

//Unused
EFI_STATUS
UpdateHiiDB(
  IN CHAR8 *FormIdChar
);

//Unused
UINTN GetAptioHiiDB(
  IN BOOLEAN BufferSizeOrPointer
);
