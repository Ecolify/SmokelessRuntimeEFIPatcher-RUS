#include "Utility.h"

CHAR16 *
FindLoadedImageFileName(
  IN EFI_LOADED_IMAGE_PROTOCOL *LoadedImage,
  IN EFI_GUID FilterProtocol
)
{
    EFI_GUID *NameGuid;
    EFI_STATUS Status;
    EFI_FIRMWARE_VOLUME2_PROTOCOL *Fv = NULL;
    VOID *Buffer;
    UINTN BufferSize;
    UINT32 AuthenticationStatus;
    //-----------------------------------------------------

    if ((LoadedImage == NULL) || (LoadedImage->FilePath == NULL))
    {
        return NULL;
    }

    NameGuid = EfiGetNameGuidFromFwVolDevicePathNode((MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *)LoadedImage->FilePath);

    if (NameGuid == NULL)
    {
        return NULL;
    }

    //
    // Get the FilterProtocol of the device handle that this image was loaded from.
    //
    Status = gBS->HandleProtocol(LoadedImage->DeviceHandle, &FilterProtocol, (VOID **)&Fv);

    //
    // FirmwareVolume2Protocol is PI, and is not required to be available.
    //
    if (EFI_ERROR(Status))
    {
        return NULL;
    }

    //
    // Read the user interface section of the image.
    //
    Buffer = NULL;
    Status = Fv->ReadSection(Fv, NameGuid, EFI_SECTION_USER_INTERFACE, 0, &Buffer, &BufferSize, &AuthenticationStatus);

    if (EFI_ERROR(Status))
    {
        return NULL;
    }

    //
    // ReadSection returns just the section data, without any section header. For
    // a user interface section, the only data is the file name.
    //
    return Buffer;
}

UINTN
FindLoadedImageBufferSize(
  IN EFI_LOADED_IMAGE_PROTOCOL *LoadedImage,
  IN EFI_GUID FilterProtocol
)
{
    EFI_GUID *NameGuid;
    EFI_STATUS Status;
    EFI_FIRMWARE_VOLUME2_PROTOCOL *Fv = NULL;
    VOID *Buffer;
    UINTN BufferSize;
    UINT32 AuthenticationStatus;
    //-----------------------------------------------------

    if ((LoadedImage == NULL) || (LoadedImage->FilePath == NULL))
    {
        return 0;
    }

    NameGuid = EfiGetNameGuidFromFwVolDevicePathNode((MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *)LoadedImage->FilePath);

    if (NameGuid == NULL)
    {
        return 0;
    }

    //
    // Get the FirmwareVolume2Protocol of the device handle that this image was loaded from.
    //
    Status = gBS->HandleProtocol(LoadedImage->DeviceHandle, &FilterProtocol, (VOID **)&Fv);

    //
    // FirmwareVolume2Protocol is PI, and is not required to be available.
    //
    if (EFI_ERROR(Status))
    {
        return 0;
    }

    //
    // Read the user interface section of the image.
    //
    Buffer = NULL;
    Status = Fv->ReadSection(Fv, NameGuid, EFI_SECTION_PE32, 0, &Buffer, &BufferSize, &AuthenticationStatus);

    if (EFI_ERROR(Status))
    {
        return 0;
    }

    //
    // ReadSection returns just the section data, without any section header. For
    // a user interface section, the only data is the file name.
    //
    return BufferSize;
}

EFI_STATUS
LoadandRunImage(
  IN EFI_HANDLE ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable,
  IN CHAR16 *FileName,
  OUT EFI_HANDLE *AppImageHandle
)
{
    UINTN ExitDataSize;
    UINTN NumHandles;
    UINTN Index;
    EFI_HANDLE *SFS_Handles;
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_BLOCK_IO_PROTOCOL *BlkIo = NULL;
    EFI_DEVICE_PATH_PROTOCOL *FilePath;
    //-----------------------------------------------------

    Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &NumHandles, &SFS_Handles);

    if (EFI_ERROR(Status))
    {
        return Status;
    }

    for (Index = 0; Index < NumHandles; Index++)
    {
        Status = gBS->OpenProtocol(
          SFS_Handles[Index],
          &gEfiSimpleFileSystemProtocolGuid,
          (VOID **)&BlkIo,
          ImageHandle, NULL,
          EFI_OPEN_PROTOCOL_GET_PROTOCOL);

        if (EFI_ERROR(Status))
        {
            return Status;
        }

        FilePath = FileDevicePath(SFS_Handles[Index], FileName);
        Status = gBS->LoadImage(FALSE, ImageHandle, FilePath, (VOID *)NULL, 0, AppImageHandle);

        if (EFI_ERROR(Status))
        {
            continue;
        }

        Status = gBS->StartImage(*AppImageHandle, &ExitDataSize, (CHAR16 **)NULL);
        if (EFI_ERROR(Status))
        {
            return EFI_NOT_FOUND;
        }
        return EFI_SUCCESS;
    }
    return EFI_SUCCESS;
}

EFI_STATUS
LocateAndLoadFvFromName(
  IN CHAR16 *Name,
  IN EFI_SECTION_TYPE Section_Type,
  OUT UINT8 **Buffer,
  OUT UINTN *BufferSize,
  IN EFI_GUID FilterProtocol
)
{
    EFI_STATUS Status;
    EFI_HANDLE *HandleBuffer;
    UINTN NumberOfHandles;
    //UINT32 FvStatus;
    //EFI_FV_FILE_ATTRIBUTES Attributes;
    //UINTN Size;
    UINTN Index;
    EFI_FIRMWARE_VOLUME2_PROTOCOL *FvInstance = NULL;
    //-----------------------------------------------------

    // FvStatus = 0; // Leftover from Smokeless, the function is a copy of LocateFvInstanceWithTables from AcpiPlatform.c

    //
    // Locate protocol.
    //
    Status = gBS->LocateHandleBuffer(
        ByProtocol,
        &FilterProtocol,
        NULL,
        &NumberOfHandles,
        &HandleBuffer);
    if (EFI_ERROR(Status))
    {
        Print(L"%s%r\n\r", HiiGetString(HiiHandle, STRING_TOKEN(STR_NO_HANDLE), NULL), Status);
        return Status;
    }

    DEBUG_CODE(Print(L"Found %d Instances\n\r", NumberOfHandles););

    for (Index = 0; Index < NumberOfHandles; Index++)
    {

        //
        // Get the protocol on this handle
        // This should not fail because of LocateHandleBuffer
        //

        Status = gBS->HandleProtocol(
            HandleBuffer[Index],
            &FilterProtocol,
            (VOID **)&FvInstance);
        ASSERT_EFI_ERROR(Status);

        EFI_FV_FILETYPE FileType = EFI_FV_FILETYPE_ALL;
        EFI_FV_FILE_ATTRIBUTES FileAttributes;
        UINTN FileSize;
        EFI_GUID NameGuid = {0};
        VOID *Keys = AllocateZeroPool(FvInstance->KeySize);
        while (TRUE)
        {
            FileType = EFI_FV_FILETYPE_ALL;
            ZeroMem(&NameGuid, sizeof(EFI_GUID));
            Status = FvInstance->GetNextFile(FvInstance, Keys, &FileType, &NameGuid, &FileAttributes, &FileSize);
            if (EFI_ERROR(Status))
            {
              Print(L"%s%r\n\r", HiiGetString(HiiHandle, STRING_TOKEN(STR_BREAKING_CAUSE), NULL), Status);
              break;
            }
            VOID *String;
            UINTN StringSize = 0;
            UINT32 AuthenticationStatus;
            String = NULL;
            Status = FvInstance->ReadSection(FvInstance, &NameGuid, EFI_SECTION_USER_INTERFACE, 0, &String, &StringSize, &AuthenticationStatus);
            if (StrCmp(Name, String) == 0)
            {
                DEBUG_CODE(Print(L"Guid: %g, FileSize: %d, Name: %s, Type: %d\n\r", NameGuid, FileSize, String, FileType););

                Status = FvInstance->ReadSection(FvInstance, &NameGuid, Section_Type, 0,(VOID **) Buffer, BufferSize, &AuthenticationStatus);
                Print(L"%s%r\n\r", HiiGetString(HiiHandle, STRING_TOKEN(STR_SEARCH_RESULT), NULL), Status);

                FreePool(String);
                return EFI_SUCCESS;
            }
            FreePool(String);
        }
    }
    return EFI_NOT_FOUND;
}

EFI_STATUS
LocateAndLoadFvFromGuid(
  IN EFI_GUID GUID16,
  IN EFI_SECTION_TYPE Section_Type,
  OUT UINT8 **Buffer,
  OUT UINTN *BufferSize,
  IN EFI_GUID FilterProtocol
)
{
  EFI_STATUS Status;
  EFI_HANDLE *HandleBuffer;
  UINTN NumberOfHandles;
  UINTN Index;
  EFI_FIRMWARE_VOLUME2_PROTOCOL *FvInstance = NULL;
  //-----------------------------------------------------

  //
  // Locate protocol.
  //
  Status = gBS->LocateHandleBuffer(
    ByProtocol,
    &FilterProtocol,
    NULL,
    &NumberOfHandles,
    &HandleBuffer);
  if (EFI_ERROR(Status))
  {
    Print(L"%s%r\n\r", HiiGetString(HiiHandle, STRING_TOKEN(STR_NO_HANDLE), NULL), Status);

    return Status;
  }

  DEBUG_CODE(Print(L"Found %d Instances\n\r", NumberOfHandles););

  for (Index = 0; Index < NumberOfHandles; Index++)
  {

    //
    // Get the protocol on this handle
    // This should not fail because of LocateHandleBuffer
    //

    Status = gBS->HandleProtocol(
      HandleBuffer[Index],
      &FilterProtocol,
      (VOID**)&FvInstance);
    ASSERT_EFI_ERROR(Status);

    EFI_FV_FILETYPE FileType = EFI_FV_FILETYPE_ALL;
    EFI_FV_FILE_ATTRIBUTES FileAttributes;
    UINTN FileSize;
    EFI_GUID NameGuid = { 0 };
    VOID *Keys = AllocateZeroPool(FvInstance->KeySize);
    while (TRUE)
    {
      FileType = EFI_FV_FILETYPE_ALL;
      ZeroMem(&NameGuid, sizeof(EFI_GUID));
      Status = FvInstance->GetNextFile(FvInstance, Keys, &FileType, &NameGuid, &FileAttributes, &FileSize);
      if (EFI_ERROR(Status))
      {
        Print(L"%s%r\n\r", HiiGetString(HiiHandle, STRING_TOKEN(STR_BREAKING_CAUSE), NULL), Status);
        break;
      }
      VOID *String;
      UINTN StringSize = 0;
      UINT32 AuthenticationStatus;
      String = NULL;
      Status = FvInstance->ReadSection(FvInstance, &NameGuid, EFI_SECTION_USER_INTERFACE, 0, &String, &StringSize, &AuthenticationStatus);

      DEBUG_CODE(
        // Debug
        Print(L"Current GUID: %g\n\r", NameGuid);      //Current processing guid per While iteration
      );

      if (CompareGuid(&GUID16, &NameGuid) == 1) //I can do via "&"
      {

        DEBUG_CODE(Print(L"Found Guid: %g, FileSize: %d, Name: %s, Type: %d\n\r", NameGuid, FileSize, String, FileType););

        Status = FvInstance->ReadSection(FvInstance, &NameGuid, Section_Type, 0, (VOID **)Buffer, BufferSize, &AuthenticationStatus);

        Print(L"%s%r\n\r", HiiGetString(HiiHandle, STRING_TOKEN(STR_SEARCH_RESULT), NULL), Status);

        FreePool(String);
        return EFI_SUCCESS;
      }
      FreePool(String);
    }
  }
  return EFI_NOT_FOUND;
}

EFI_STATUS
RegexMatch(
  IN UINT8 *Dump,
  IN CHAR8 *Pattern,
  IN UINT16 Size,
  IN EFI_REGULAR_EXPRESSION_PROTOCOL *Oniguruma,
  OUT BOOLEAN *CResult
)
{
  EFI_STATUS Status;
  CHAR16 tmp[0x100] = { 0 };
  CHAR16 Dump16[0x100] = { 0 };
  CHAR16 Pattern16[0x100] = { 0 };
  UINTN CapturesCount = 0;  //Reserved for now, as it's always 1.
  //-----------------------------------------------------

  Size = Size > 0x100 ? 0x100 : Size;
  for (UINT16 i = 0; i < Size; i++) //Get string from UINT buffer
  {
    UnicodeSPrint(tmp, 0x200, u"%02x", Dump[i]);
    //Print(L"Dump %s\n\r", tmp);
    Status = StrCatS(Dump16, sizeof(tmp) + 0x2, tmp);
  }

  UnicodeSPrint(Pattern16, sizeof(Pattern16), L"%a", Pattern);  //Convert Pattern string

  DEBUG_CODE(
    //Debug
    Print(L"Append result: %r\n\r", Status);
    Print(L"Strings Dump/Pattern %s / %s\n\r", &Dump16, &Pattern16);
    INTN m = StrCmp(Dump16, Pattern16);
    Print(L"Strings match? : %a\n\r", m ? L"False" : L"True"); //INT m = 0 is yes, any other means they dont
    Print(L"Dump16 : %d, Pattern16 : %d\n\r", StrLen(Dump16), StrLen(Pattern16));
  );

  Status = Oniguruma->MatchString(
    Oniguruma,
    Dump16,
    Pattern16,
    NULL,
    CResult,
    NULL,
    &CapturesCount
  );

  return Status;
}

//Unused
UINT8 *
FindBaseAddressFromName(IN const CHAR16 *Name)
{
    EFI_STATUS Status;
    UINTN NumberOfHandles = 0;
    EFI_HANDLE *Handles;
    //-----------------------------------------------------

    Status = gBS->LocateHandle(ByProtocol, &gEfiLoadedImageProtocolGuid, NULL, &NumberOfHandles, NULL);
    if (Status == EFI_BUFFER_TOO_SMALL)
    {
        Handles = AllocateZeroPool(NumberOfHandles * sizeof(EFI_HANDLE));
        Status = gBS->LocateHandle(ByProtocol, &gEfiLoadedImageProtocolGuid, NULL, &NumberOfHandles, Handles);
        DEBUG_CODE(Print(L"Found %d Instances\n\r", NumberOfHandles););
    }

    EFI_LOADED_IMAGE_PROTOCOL *LoadedImageProtocol = NULL;
    for (UINTN i = 0; i < NumberOfHandles; i++)
    {
        Status = gBS->HandleProtocol(Handles[i], &gEfiLoadedImageProtocolGuid, (VOID **)&LoadedImageProtocol);  //Process every found handle
        if (Status == EFI_SUCCESS)
        {
            CHAR16 *String = FindLoadedImageFileName(LoadedImageProtocol, gEfiFirmwareVolume2ProtocolGuid);
            if (String != NULL)
            {
                if (StrCmp(Name, String) == 0)  //If SUCCESS, compare the handle' name with the one specified in cfg
                {
                    DEBUG_CODE(Print(L"Found %s at Address 0x%X\n\r", String, LoadedImageProtocol->ImageBase););
                    return LoadedImageProtocol->ImageBase;
                }
            }
        }
    }
    return NULL;
}
