#pragma once
#include <Uefi.h>
#include <PiDxe.h>
#include <Guid/FileInfo.h>
#include <Guid/FileSystemInfo.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Protocol/FirmwareVolume.h>              //FirmwareVolumeProtocol. Is not part of edk2, may add manually.
#include <Protocol/FirmwareVolume2.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DevicePath.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/FormBrowser2.h>
#include <Protocol/FormBrowserEx.h>
#include <Protocol/FormBrowserEx2.h>
#include <Protocol/AcpiSystemDescriptionTable.h>
#include <Protocol/DisplayProtocol.h>
#include <Protocol/HiiPopup.h>
#include <Protocol/ShellParameters.h>             //Needed for GetArgs
#include <Protocol/RegularExpressionProtocol.h>   //Needed for regex
#include <Library/HiiLib.h>                       //Needed for fonts

//C2220 suppression due to log filename issues
#pragma warning(disable:4459)
#pragma warning(disable:4456)
#pragma warning(disable:4244)

//Set default lang Rus
BOOLEAN ENG = FALSE;

//Initizalize log function
VOID LogToFile(
  IN EFI_FILE *LogFile,
  IN CHAR16 *String
);

CHAR16 *FindLoadedImageFileName(
  IN EFI_LOADED_IMAGE_PROTOCOL *LoadedImage,
  IN EFI_GUID FilterProtocol
);

UINTN FindLoadedImageBufferSize(
  IN EFI_LOADED_IMAGE_PROTOCOL *LoadedImage,
  IN EFI_GUID FilterProtocol
);

EFI_STATUS LoadandRunImage(
  IN EFI_HANDLE ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable,
  IN CHAR16 *FileName,
  OUT EFI_HANDLE *AppImageHandle
);

EFI_STATUS LocateAndLoadFvFromName(
  IN CHAR16 *Name,
  IN EFI_SECTION_TYPE Section_Type,
  OUT UINT8 **Buffer,
  OUT UINTN *BufferSize,
  IN EFI_GUID FilterProtocol
);

EFI_STATUS LocateAndLoadFvFromGuid(
  IN EFI_GUID GUID16,
  IN EFI_SECTION_TYPE Section_Type,
  OUT UINT8 **Buffer,
  OUT UINTN *BufferSize,
  IN EFI_GUID FilterProtocol
);

EFI_STATUS RegexMatch(
  IN      UINT8 *DUMP,
  IN      CHAR8 *Pattern,
  IN      UINT16 Size,
  IN      EFI_REGULAR_EXPRESSION_PROTOCOL *Oniguruma,
  OUT     BOOLEAN *CResult
);

UINT8 *FindBaseAddressFromName(
  IN const CHAR16 *Name
);
