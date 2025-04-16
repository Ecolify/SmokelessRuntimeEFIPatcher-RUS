#include "Utility.h"

CHAR16 *
FindLoadedImageFileName(IN EFI_LOADED_IMAGE_PROTOCOL *LoadedImage)
{
    EFI_GUID *NameGuid;
    EFI_STATUS Status;
    EFI_FIRMWARE_VOLUME2_PROTOCOL *Fv;
    VOID *Buffer;
    UINTN BufferSize;
    UINT32 AuthenticationStatus;

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
    // Get the FirmwareVolume2Protocol of the device handle that this image was loaded from.
    //
    Status = gBS->HandleProtocol(LoadedImage->DeviceHandle, &gEfiFirmwareVolume2ProtocolGuid, (VOID **)&Fv);

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
FindLoadedImageBufferSize(IN EFI_LOADED_IMAGE_PROTOCOL *LoadedImage)
{
    EFI_GUID *NameGuid;
    EFI_STATUS Status;
    EFI_FIRMWARE_VOLUME2_PROTOCOL *Fv;
    VOID *Buffer;
    UINTN BufferSize;
    UINT32 AuthenticationStatus;

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
    Status = gBS->HandleProtocol(LoadedImage->DeviceHandle, &gEfiFirmwareVolume2ProtocolGuid, (VOID **)&Fv);

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

UINT8 *
FindBaseAddressFromName(const CHAR16 *Name)
{
    EFI_STATUS Status;
    UINTN HandleSize = 0;
    EFI_HANDLE *Handles;

    Status = gBS->LocateHandle(ByProtocol, &gEfiLoadedImageProtocolGuid, NULL, &HandleSize, NULL);
    if (Status == EFI_BUFFER_TOO_SMALL)
    {
        Handles = AllocateZeroPool(HandleSize * sizeof(EFI_HANDLE));
        Status = gBS->LocateHandle(ByProtocol, &gEfiLoadedImageProtocolGuid, NULL, &HandleSize, Handles);
        if (ENG == TRUE) { Print(L"Retrived %d Handle, with %r\n\r", HandleSize, Status); }
        else
        {
          Print(L"Всего найдено дескрипторов: %d\n\r", HandleSize);
        }
    }

    EFI_LOADED_IMAGE_PROTOCOL *LoadedImageProtocol;
    for (UINTN i = 0; i < HandleSize; i++)
    {
        Status = gBS->HandleProtocol(Handles[i], &gEfiLoadedImageProtocolGuid, (VOID **)&LoadedImageProtocol);  //Process every found handle
        if (Status == EFI_SUCCESS)
        {
            CHAR16 *String = FindLoadedImageFileName(LoadedImageProtocol);
            if (String != NULL)
            {
                if (StrCmp(Name, String) == 0)  //If SUCCESS, compare the handle' name with the one specified in cfg
                {
                  if (ENG == TRUE) { Print(L"Found %s at Address 0x%X\n\r", String, LoadedImageProtocol->ImageBase); }
                  else
                  {
                    Print(L"Найден %s по смещению 0x%X\n\r", String, LoadedImageProtocol->ImageBase);
                  }
                    return LoadedImageProtocol->ImageBase;
                }
            }
        }
    }
    return NULL;
}

EFI_STATUS
LoadandRunImage(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, CHAR16 *FileName, EFI_HANDLE *AppImageHandle)
{
    UINTN ExitDataSize;
    UINTN NumHandles;
    UINTN Index;
    EFI_HANDLE *SFS_Handles;
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_BLOCK_IO_PROTOCOL *BlkIo;
    EFI_DEVICE_PATH_PROTOCOL *FilePath;

    Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &NumHandles, &SFS_Handles);

    if (Status != EFI_SUCCESS)
    {
        return Status;
    }

    for (Index = 0; Index < NumHandles; Index++)
    {
        Status = gBS->OpenProtocol(
            SFS_Handles[Index], &gEfiSimpleFileSystemProtocolGuid, (VOID **)&BlkIo,
            ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);

        if (Status != EFI_SUCCESS)
        {
            return Status;
        }

        FilePath = FileDevicePath(SFS_Handles[Index], FileName);
        Status = gBS->LoadImage(FALSE, ImageHandle, FilePath, (VOID *)NULL, 0, AppImageHandle);

        if (Status != EFI_SUCCESS)
        {
            continue;
        }

        Status = gBS->StartImage(*AppImageHandle, &ExitDataSize, (CHAR16 **)NULL);
        if (Status != EFI_SUCCESS)
        {
            return EFI_NOT_FOUND;
        }
        return EFI_SUCCESS;
    }
    return EFI_SUCCESS;
}

EFI_STATUS
LocateAndLoadFvFromName(CHAR16 *Name, EFI_SECTION_TYPE Section_Type, UINT8 **Buffer, UINTN *BufferSize)
{
    EFI_STATUS Status;
    EFI_HANDLE *HandleBuffer;
    UINTN NumberOfHandles;
    //UINT32 FvStatus;
    //EFI_FV_FILE_ATTRIBUTES Attributes;
    //UINTN Size;
    UINTN Index;
    EFI_FIRMWARE_VOLUME2_PROTOCOL *FvInstance;

    // FvStatus = 0; // Leftover from Smokeless

    //
    // Locate protocol.
    //
    Status = gBS->LocateHandleBuffer(
        ByProtocol,
        &gEfiFirmwareVolume2ProtocolGuid,
        NULL,
        &NumberOfHandles,
        &HandleBuffer);
    if (EFI_ERROR(Status))
    {
        //
        // Defined errors at this time are not found and out of resources.
        //
        return Status;
    }

    //
    // Looking for FV with ACPI storage file
    //
    if (ENG == TRUE) { Print(L"Found %d Instances\n\r", NumberOfHandles); }
    else
    {
      Print(L"Всего найдено дескрипторов: %d\n\r", NumberOfHandles);
    }
    for (Index = 0; Index < NumberOfHandles; Index++)
    {

        //
        // Get the protocol on this handle
        // This should not fail because of LocateHandleBuffer
        //

        Status = gBS->HandleProtocol(
            HandleBuffer[Index],
            &gEfiFirmwareVolume2ProtocolGuid,
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
            if (Status != EFI_SUCCESS)
            {
              if (ENG == TRUE) { Print(L"Breaking Cause %r\n\r", Status); }
                else
                {
                  Print(L"Причина остановки итерации: %r\n\r", Status);
                }
                break;
            }
            VOID *String;
            UINTN StringSize = 0;
            UINT32 AuthenticationStatus;
            String = NULL;
            Status = FvInstance->ReadSection(FvInstance, &NameGuid, EFI_SECTION_USER_INTERFACE, 0, &String, &StringSize, &AuthenticationStatus);
            if (StrCmp(Name, String) == 0)
            {
              if (ENG == TRUE) { Print(L"Guid: %g, FileSize: %d, Name: %s, Type: %d\n\r", NameGuid, FileSize, String, FileType); }
              else
              {
                Print(L"GUID: %g, Размер: %d, Имя: %s, Тип: %d\n\r", NameGuid, FileSize, String, FileType);
              }
                Status = FvInstance->ReadSection(FvInstance, &NameGuid, Section_Type, 0,(VOID **) Buffer, BufferSize, &AuthenticationStatus);
                if (ENG == TRUE) { Print(L"Result Cause %r\n\r", Status); }
                else
                {
                  Print(L"Результат %r\n\r", Status);
                }
                FreePool(String);
                return EFI_SUCCESS;
            }
            FreePool(String);
        }
    }
    return EFI_NOT_FOUND;
}

EFI_STATUS
LocateAndLoadFvFromGuid(EFI_GUID GUID16, EFI_SECTION_TYPE Section_Type, UINT8 **Buffer, UINTN *BufferSize)
{
  EFI_STATUS Status;
  EFI_HANDLE* HandleBuffer;
  UINTN NumberOfHandles;
  //UINT32 FvStatus;
  //EFI_FV_FILE_ATTRIBUTES Attributes;
  // UINTN Size;
  UINTN Index;
  EFI_FIRMWARE_VOLUME2_PROTOCOL* FvInstance;
  EFI_GUID Guid = GUID16;

  // FvStatus = 0; // Leftover from Smokeless

  //
  // Locate protocol.
  //
  Status = gBS->LocateHandleBuffer(
    ByProtocol,
    &gEfiFirmwareVolume2ProtocolGuid,
    NULL,
    &NumberOfHandles,
    &HandleBuffer);
  if (EFI_ERROR(Status))
  {
    //
    // Defined errors at this time are not found and out of resources.
    //
    return Status;
  }

  //
  // Looking for FV with ACPI storage file
  //
  if (ENG == TRUE) { Print(L"Found %d Instances\n\r", NumberOfHandles); }
  else
  {
    Print(L"Всего найдено дескрипторов: %d\n\r", NumberOfHandles);
  }
  for (Index = 0; Index < NumberOfHandles; Index++)
  {

    //
    // Get the protocol on this handle
    // This should not fail because of LocateHandleBuffer
    //

    Status = gBS->HandleProtocol(
      HandleBuffer[Index],
      &gEfiFirmwareVolume2ProtocolGuid,
      (VOID**)&FvInstance);
    ASSERT_EFI_ERROR(Status);

    EFI_FV_FILETYPE FileType = EFI_FV_FILETYPE_ALL;
    EFI_FV_FILE_ATTRIBUTES FileAttributes;
    UINTN FileSize;
    EFI_GUID NameGuid = { 0 };
    VOID* Keys = AllocateZeroPool(FvInstance->KeySize);
    while (TRUE)
    {
      FileType = EFI_FV_FILETYPE_ALL;
      ZeroMem(&NameGuid, sizeof(EFI_GUID));
      Status = FvInstance->GetNextFile(FvInstance, Keys, &FileType, &NameGuid, &FileAttributes, &FileSize);
      if (Status != EFI_SUCCESS)
      {
        if (ENG == TRUE) { Print(L"Breaking Cause %r\n\r", Status); }
        else
        {
          Print(L"Причина остановки итерации: %r\n\r", Status);
        }
        break;
      }
      VOID* String;
      UINTN StringSize = 0;
      UINT32 AuthenticationStatus;
      String = NULL;
      Status = FvInstance->ReadSection(FvInstance, &NameGuid, EFI_SECTION_USER_INTERFACE, 0, &String, &StringSize, &AuthenticationStatus);
      /* Debug
      //if (ENG != TRUE) Print(L"Current GUID: %g\n\r", NameGuid);      //Current processing guid per While iteration
      */
      if (CompareGuid(&Guid, &NameGuid) == 1) //I can do via "&"
      {
        if (ENG == TRUE) { Print(L"Found Guid: %g, FileSize: %d, Name: %s, Type: %d\n\r", NameGuid, FileSize, String, FileType); }
        else
        {
          Print(L"GUID: %g, Размер: %d, Имя: %s, Тип: %d\n\r", NameGuid, FileSize, String, FileType);
        }
        Status = FvInstance->ReadSection(FvInstance, &NameGuid, Section_Type, 0, (VOID**)Buffer, BufferSize, &AuthenticationStatus);
        if (ENG == TRUE) { Print(L"Result Cause %r\n\r", Status); }
        else
        {
          Print(L"Результат: %r\n\r", Status);
        }
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
  IN      UINT8 *DUMP,
  IN      CHAR16 *Pattern16,
  IN      UINT16 Size,
  OUT     BOOLEAN *Result
)
{
  EFI_STATUS Status;
  EFI_REGULAR_EXPRESSION_PROTOCOL Oniguruma;
  EFI_REGULAR_EXPRESSION_MATCH Match = EFI_SUCCESS;

  //Debug
  CHAR16 *Str = (L"Patt");
  UINTN *CC = 0;

  /* Not reading dump yet, testing
  for (UINT16 i = 0; i < Size; i++) {
    UnicodeSPrint(Str, 512, u"%02x ", DUMP[i]);
  }
  */

  Status = Match(&Oniguruma, Str, Pattern16, NULL, Result, NULL, CC);

  if (Result || Status == EFI_SUCCESS) { Print(L"Regex worked -> %s\n\r", &Pattern16); return Status; };
  return Status;
}

/*
EFI_STATUS
ConvertStrToBuf(
  OUT     UINT8 *Buf,
  IN      UINTN  BufferLength,
  IN      CHAR16 *Str
)
{
  UINTN       Index;
  UINTN       StrLength;
  UINT8       Digit;
  UINT8       Byte;

  Digit = 0;

  //
  // Two hex char make up one byte
  //
  StrLength = BufferLength * sizeof(CHAR16);

  for (Index = 0; Index < StrLength; Index++, Str++) {

    if ((*Str >= L'a') && (*Str <= L'f')) {
      Digit = (UINT8)(*Str - L'a' + 0x0A);
    }
    else if ((*Str >= L'A') && (*Str <= L'F')) {
      Digit = (UINT8)(*Str - L'A' + 0x0A);
    }
    else if ((*Str >= L'0') && (*Str <= L'9')) {
      Digit = (UINT8)(*Str - L'0');
    }
    else {
      return EFI_INVALID_PARAMETER;
    }

    //
    // For odd characters, write the upper nibble for each buffer byte,
    // and for even characters, the lower nibble.
    //
    if ((Index & 1) == 0) {
      Byte = (UINT8)(Digit << 4);
    }
    else {
      Byte = Buf[Index / 2];
      Byte &= 0xF0;
      Byte = (UINT8)(Byte | Digit);
    }

    Buf[Index / 2] = Byte;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
ConvertStrToGuid(
  IN      CHAR16 *Str,
  OUT     EFI_GUID *Guid
)
{
  //
  // Get the first UINT32 data
  //
  Guid->Data1 = (UINT32)StrHexToUint64(Str);
  while (!IS_HYPHEN(*Str) && !IS_NULL(*Str)) {
    Str++;
  }

  if (IS_HYPHEN(*Str)) {
    Str++;
  }
  else {
    return EFI_UNSUPPORTED;
  }

  //
  // Get the second UINT16 data
  //
  Guid->Data2 = (UINT16)StrHexToUint64(Str);
  while (!IS_HYPHEN(*Str) && !IS_NULL(*Str)) {
    Str++;
  }

  if (IS_HYPHEN(*Str)) {
    Str++;
  }
  else {
    return EFI_UNSUPPORTED;
  }

  //
  // Get the third UINT16 data
  //
  Guid->Data3 = (UINT16)StrHexToUint64(Str);
  while (!IS_HYPHEN(*Str) && !IS_NULL(*Str)) {
    Str++;
  }

  if (IS_HYPHEN(*Str)) {
    Str++;
  }
  else {
    return EFI_UNSUPPORTED;
  }

  //
  // Get the following 8 bytes data
  //
  ConvertStrToBuf(&Guid->Data4[0], 2, Str);
  //
  // Skip 2 byte hex chars
  //
  Str += 2 * 2;

  if (IS_HYPHEN(*Str)) {
    Str++;
  }
  else {
    return EFI_UNSUPPORTED;
  }
  ConvertStrToBuf(&Guid->Data4[2], 6, Str);

  return EFI_SUCCESS;
}
*/
