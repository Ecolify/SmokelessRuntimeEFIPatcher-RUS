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
#include <Library/UefiRuntimeServicesTableLib.h>  //Needed for gRT

//Set default lang Rus
BOOLEAN ENG = FALSE;

//Initizalize log function
VOID LogToFile(
  EFI_FILE *LogFile,
  CHAR16 *String
);

CHAR16 *FindLoadedImageFileName(
  IN EFI_LOADED_IMAGE_PROTOCOL *LoadedImage
);

UINTN FindLoadedImageBufferSize(
  IN EFI_LOADED_IMAGE_PROTOCOL *LoadedImage
);

EFI_STATUS LoadandRunImage(
  EFI_HANDLE ImageHandle,
  EFI_SYSTEM_TABLE *SystemTable,
  CHAR16 *FileName,
  EFI_HANDLE *AppImageHandle
);

UINT8 *FindBaseAddressFromName(
  const CHAR16 *Name
);

EFI_STATUS LocateAndLoadFvFromName(
  CHAR16 *Name,
  EFI_SECTION_TYPE Section_Type,
  UINT8 **Buffer,
  UINTN *BufferSize
);

EFI_STATUS LocateAndLoadFvFromGuid(
  EFI_GUID GUID16,
  EFI_SECTION_TYPE Section_Type,
  UINT8 **Buffer,
  UINTN *BufferSize
);

EFI_STATUS
RegexMatch(
  IN      UINT8 *DUMP,
  IN      CHAR8 *Pattern,
  IN      UINT16 Size,
  IN      EFI_REGULAR_EXPRESSION_PROTOCOL *Oniguruma,
  OUT     BOOLEAN *CResult
);
