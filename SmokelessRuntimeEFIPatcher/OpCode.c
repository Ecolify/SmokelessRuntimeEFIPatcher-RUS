#include "Opcode.h"
#include "Utility.h"

EFI_STATUS
FindLoadedImageFromName(
  IN EFI_HANDLE ImageHandle,
  IN CHAR8 *FileName,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  IN EFI_GUID FilterProtocol
)
{
    EFI_STATUS Status;
    UINTN HandleSize = 0;
    EFI_HANDLE *Handles;
    //-----------------------------------------------------

    CHAR16 FileName16[255] = {0};
    UnicodeSPrint(FileName16, sizeof(FileName16), L"%a", FileName);
    Status = gBS->LocateHandle(ByProtocol, &gEfiLoadedImageProtocolGuid, NULL, &HandleSize, NULL);
    if (Status == EFI_BUFFER_TOO_SMALL)
    {
        Handles = AllocateZeroPool(HandleSize * sizeof(EFI_HANDLE));
        Status = gBS->LocateHandle(ByProtocol, &gEfiLoadedImageProtocolGuid, NULL, &HandleSize, Handles);
        if (ENG == TRUE) { Print(L"Retrived %d Handles, with %r\n\r", HandleSize, Status); }
        else { Print(L"Всего найдено дескрипторов: %d\n\r", HandleSize); }
    }

    for (UINTN i = 0; i < HandleSize; i++)
    {
        Status = gBS->HandleProtocol(Handles[i], &gEfiLoadedImageProtocolGuid, (VOID **)ImageInfo); //Process every found handle
        if (Status == EFI_SUCCESS)
        {
            CHAR16 *String = FindLoadedImageFileName(*ImageInfo, FilterProtocol);
            if (String != NULL)
            {
                if (StrCmp(FileName16, String) == 0)  //If SUCCESS, compare the handle' name with the one specified in cfg
                {
                    if (ENG == TRUE) { Print(L"Found %s at Address 0x%X\n\r", String, (*ImageInfo)->ImageBase); }
                    else { Print(L"Найден %s по смещению 0x%X\n\r", String, (*ImageInfo)->ImageBase); }
                    return EFI_SUCCESS;
                }
            }
        }
    }
    return EFI_NOT_FOUND;
}

EFI_STATUS
FindLoadedImageFromGUID(
  IN EFI_HANDLE ImageHandle,
  IN CHAR8 *FileGuid,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  IN EFI_SECTION_TYPE Section_Type,
  IN EFI_GUID FilterProtocol
)
{
    EFI_STATUS Status;
    UINTN HandleSize = 0;
    EFI_HANDLE *Handles;
    //-----------------------------------------------------

    EFI_GUID GUID = { 0 }; //Initialize beforehand
    Status = AsciiStrToGuid(FileGuid, &GUID);
    if (EFI_ERROR(Status))
    {
      if (ENG == TRUE) { Print(L"Failed to convert \"%a\" to GUID\n", FileGuid); }
      else { Print(L"Не удалось сконвертировать \"%a\" в GUID\n", FileGuid); }
      return Status;
    }

    UINT8 *Buffer = NULL;
    UINTN BufferSize = 0;
    Status = LocateAndLoadFvFromGuid(GUID, Section_Type, &Buffer, &BufferSize, FilterProtocol); //Pass the converted guid to Utility to get the target BufferSize

    Status = gBS->LocateHandle(ByProtocol, &gEfiLoadedImageProtocolGuid, NULL, &HandleSize, NULL);
    if (Status == EFI_BUFFER_TOO_SMALL)
    {
        Handles = AllocateZeroPool(HandleSize * sizeof(EFI_HANDLE));
        Status = gBS->LocateHandle(ByProtocol, &gEfiLoadedImageProtocolGuid, NULL, &HandleSize, Handles);
        if (ENG == TRUE) { Print(L"Retrived %d Handle, with %r\n\r", HandleSize, Status); }
        else { Print(L"Всего найдено дескрипторов: %d\n\r", HandleSize); }
    }

    if (Section_Type == EFI_SECTION_TE) {
      if (BufferSize != 0) {
        if (ENG == TRUE) { Print(L"Although the section with size: %d was found in BIOS,\nSREP may not be able to find TE in RAM if Op Compatibility modifier is bad\n", BufferSize); }
        else { Print(L"Хотя в БИОС нашлась секция с размером: %d,\nSREP может быть не способен найти TE в RAM если модификатор Op Compatibility не подходит\n", BufferSize); }
      }
    }

    for (UINTN i = 0; i < HandleSize; i++)
    {
        Status = gBS->HandleProtocol(Handles[i], &gEfiLoadedImageProtocolGuid, (VOID **)ImageInfo); //Process every found handle
        if (Status == EFI_SUCCESS)
        {
            UINTN FoundSize = FindLoadedImageBufferSize(*ImageInfo, FilterProtocol); //Read ImageInfo, so we get BufferSize from Handles[i]

            /*Debug
            Print(L"BufSize from Utility.c: %d\n\r", FoundSize);
            Print(L"Target BufSize: %d\n\r", BufferSize);
            */

            if (FoundSize != 0)
            {
                if (FoundSize == BufferSize)  //Compare the found handle' BufferSize with the value we got from FV by looking for guid specified in cfg
                {
                    if (ENG == TRUE) { Print(L"Found handle of size %d at Address 0x%X\n\r", FoundSize, (*ImageInfo)->ImageBase); }
                    else { Print(L"Найдена секция размера %d по смещению 0x%X\n\r", FoundSize, (*ImageInfo)->ImageBase); }
                    return EFI_SUCCESS;
                }
            }
        }
    }
    return EFI_NOT_FOUND;
}

EFI_STATUS
LoadFromFS(
  IN EFI_HANDLE ImageHandle,
  IN CHAR8 *FileName,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  OUT EFI_HANDLE *AppImageHandle
)
{
    UINTN NumHandles;
    UINTN Index;
    EFI_HANDLE *SFS_Handles;
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_BLOCK_IO_PROTOCOL *BlkIo;
    EFI_DEVICE_PATH_PROTOCOL *FilePath;
    //-----------------------------------------------------

    Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &NumHandles, &SFS_Handles);

    if (Status != EFI_SUCCESS)
    {
        if (ENG == TRUE) { Print(L"Could not find any handle on EfiSimpleFileSystemProtocol\n"); }
        else { Print(L"Не удалось найти ни одного дескриптора с\n поддержкой EfiSimpleFileSystemProtocol\n"); }
        return Status;
    }
    if (ENG == TRUE) { Print(L"No of Handles - %d\n", NumHandles); }
    else { Print(L"Кол-во доступных дескрипторов: %d\n", NumHandles); }

    for (Index = 0; Index < NumHandles; Index++)
    {
        Status = gBS->OpenProtocol(
            SFS_Handles[Index], &gEfiSimpleFileSystemProtocolGuid, (VOID **)&BlkIo,
            ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);

        if (Status != EFI_SUCCESS)
        {
            if (ENG == TRUE) { Print(L"Protocol is not supported - %r\n", Status); }
            else { Print(L"Протокол не поддерживается\n"); }
            return Status;
        }
        CHAR16 FileName16[255] = {0};
        UnicodeSPrint(FileName16, sizeof(FileName16), L"%a", FileName);
        FilePath = FileDevicePath(SFS_Handles[Index], FileName16);
        Status = gBS->LoadImage(FALSE, ImageHandle, FilePath, (VOID *)NULL, 0, AppImageHandle);

        if (Status != EFI_SUCCESS)
        {
            if (ENG == TRUE) { Print(L"Could not load the image on Handle %d - %r\n", Index, Status); }
            else { Print(L"Не удалось загрузить драйвер по дескриптору %d\n", Index); }
            continue;
        }
        else
        {
            if (ENG == TRUE) { Print(L"Loaded the image with success on Handle %d\n", Index); }
            else { Print(L"Успешно загружен драйвер по дескриптору %d\n", Index); }
            Status = gBS->OpenProtocol(*AppImageHandle, &gEfiLoadedImageProtocolGuid,
                                       (VOID **)ImageInfo, ImageHandle, (VOID *)NULL,
                                       EFI_OPEN_PROTOCOL_GET_PROTOCOL);
            return Status;
        }
    }
    return Status;
}

EFI_STATUS
LoadFromFV(
  IN EFI_HANDLE ImageHandle,
  IN CHAR8 *FileName,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  OUT EFI_HANDLE *AppImageHandle,
  IN EFI_SECTION_TYPE Section_Type,
  IN EFI_GUID FilterProtocol
)
{
    EFI_STATUS Status = EFI_SUCCESS;

    CHAR16 FileName16[255] = {0};
    UnicodeSPrint(FileName16, sizeof(FileName16), L"%a", FileName);

    UINT8 *Buffer = NULL;
    UINTN BufferSize = 0;
    Status = LocateAndLoadFvFromName(FileName16, Section_Type, &Buffer, &BufferSize, FilterProtocol);

    Status = gBS->LoadImage(FALSE, ImageHandle, (VOID *)NULL, Buffer, BufferSize, AppImageHandle);
    if (Buffer != NULL)
        FreePool(Buffer);
    if (Status != EFI_SUCCESS)
    {
        if (ENG == TRUE) { Print(L"Could not Locate the image from FV: %r\n", Status); }
        else { Print(L"Не удалось загрузить драйвер из FV: %r\n", Status); }
    }
    else
    {
        Status = gBS->OpenProtocol(*AppImageHandle, &gEfiLoadedImageProtocolGuid,
                                   (VOID **)ImageInfo, ImageHandle, (VOID *)NULL,
                                   EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    }

    return Status;
};

EFI_STATUS
LoadGUIDandSavePE(
  IN EFI_HANDLE ImageHandle,
  IN CHAR8 *FileGuid,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  OUT EFI_HANDLE *AppImageHandle,
  IN EFI_SECTION_TYPE Section_Type,
  IN EFI_SYSTEM_TABLE *SystemTable,
  IN EFI_GUID FilterProtocol
)
{
  EFI_STATUS Status = EFI_SUCCESS;
  EFI_HANDLE_PROTOCOL HandleProtocol;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
  EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
  EFI_FILE *Root;
  EFI_FILE *DumpFile = NULL;
  //-----------------------------------------------------

  HandleProtocol = SystemTable->BootServices->HandleProtocol;
  HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (void **)&LoadedImage);
  HandleProtocol(LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (void **)&FileSystem);
  FileSystem->OpenVolume(FileSystem, &Root);

  EFI_GUID GUID = { 0 };
  Status = AsciiStrToGuid(FileGuid, &GUID);
  if (EFI_ERROR(Status))
  {
    if (ENG == TRUE) { Print(L"Failed to convert \"%a\" to GUID\n", FileGuid); }
    else { Print(L"Не удалось сконвертировать \"%a\" в GUID\n", FileGuid); }
    return Status;
  }

  /* Debug
  Print(L"GUID from OpCode.c raw: %s\n\r", FileGuid);
  Print(L"GUID from OpCode.c converted: %g\n\r", GUID);
  */

  UINT8 *Buffer = NULL;
  UINTN BufferSize = 0;
  Status = LocateAndLoadFvFromGuid(GUID, Section_Type, &Buffer, &BufferSize, FilterProtocol); //Pass the converted guid to Utility

  //Dumping section
  if (Buffer == NULL) {
    if (ENG == TRUE) { Print(L"Buffer of the specified driver is too small: %d\n", Buffer); } //Print result
    else { Print(L"Размер указанного драйвера слишком мал: %d\n", BufferSize); }
    return EFI_NOT_FOUND;
  }

  CHAR16 DFName[41] = { 0 };                                  //36 chars for guid, 4 for the ".bin" ext and 1 for null-terminator
  UnicodeSPrint(DFName, sizeof(DFName), L"%a.bin", FileGuid); //Append ".bin"
  Root->Open(Root, &DumpFile, DFName, EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ | EFI_FILE_MODE_CREATE, 0);
  DumpFile->Write(DumpFile, &BufferSize, (VOID **)Buffer);
  DumpFile->Flush(DumpFile);

  if (ENG == TRUE) { Print(L"Creating section dump on FS,\nthis shouldn't take more than 3 secs.\nOtherwise, UEFI system unsupported.\n"); }
  else { Print(L"Создаем дамп секции на накопителе,\nэто должно занять не более 3 сек.\nИначе, UEFI не поддерживается.\n"); }

  Status = gBS->LoadImage(FALSE, ImageHandle, (VOID *)NULL, Buffer, BufferSize, AppImageHandle); //Parse saved buffer to LoadImage BS
  if (Buffer != NULL)
  {
    FreePool(Buffer);
  }
  if (Status != EFI_SUCCESS)
  {
    if (ENG == TRUE) { Print(L"Could not load the image: %r\n", Status); }
    else { Print(L"Не удалось загрузить драйвер: %r\n", Status); }
  }
  else
  {
    Status = gBS->OpenProtocol(*AppImageHandle, &gEfiLoadedImageProtocolGuid,
      (VOID**)ImageInfo, ImageHandle, (VOID*)NULL,
      EFI_OPEN_PROTOCOL_GET_PROTOCOL);
  }

  return Status;
};

EFI_STATUS
LoadGUIDandSaveFreeform(
  IN EFI_HANDLE ImageHandle,
  OUT VOID **Pointer,
  OUT UINT64 *Size,
  IN CHAR8 *FileGuid,
  IN CHAR8 *SectionGuid OPTIONAL,
  IN EFI_SYSTEM_TABLE *SystemTable,
  IN EFI_GUID FilterProtocol
)
{
    EFI_STATUS Status;
    EFI_GUID File = { 0 };
    EFI_GUID Section = { 0 };
    EFI_HANDLE *HandleBuffer = NULL;
    UINTN NumberOfHandles;
    UINT32 Authentication;
    UINTN i,j;
    VOID *SectionData = NULL;
    UINTN DataSize;
    EFI_SECTION_TYPE SectionType;
    EFI_FIRMWARE_VOLUME_PROTOCOL *Fv = NULL;
    EFI_FIRMWARE_VOLUME2_PROTOCOL *Fv2 = NULL;
    if (!CompareGuid(&FilterProtocol, &gEfiFirmwareVolumeProtocolGuid)) {
      if (ENG == TRUE) { Print(L"Going to search with EfiFirmwareVolume2Protocol\n"); }
      else { Print(L"Поиск будет выполнен через EfiFirmwareVolume2Protocol\n"); }
    }

    //Dump related vars
    EFI_HANDLE_PROTOCOL HandleProtocol;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    EFI_FILE *Root;
    EFI_FILE *DumpFile = NULL;
    //-----------------------------------------------------

    HandleProtocol = SystemTable->BootServices->HandleProtocol;
    HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (void **)&LoadedImage);
    HandleProtocol(LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (void **)&FileSystem);
    FileSystem->OpenVolume(FileSystem, &Root);

    Status = AsciiStrToGuid(FileGuid, &File);
    if (EFI_ERROR(Status))
    {
      if (ENG == TRUE) { Print(L"Failed to convert \"%a\" to File GUID\n", FileGuid); }
      else { Print(L"Не удалось сконвертировать \"%a\" в File GUID\n", FileGuid); }
      return Status;
    }

    if (SectionGuid != NULL)
    {
      Status = AsciiStrToGuid(SectionGuid, &Section);
      if (EFI_ERROR(Status))
      {
        if (ENG == TRUE) { Print(L"Failed to convert \"%a\" to Section Guid\n", SectionGuid); }
        else { Print(L"Не удалось сконвертировать \"%a\" в Section Guid\n", SectionGuid); }
        return Status;
      }
    }

    // Locate the Firmware volume protocol
    if (!CompareGuid(&FilterProtocol, &gEfiFirmwareVolumeProtocolGuid)) {
      Status = gBS->LocateHandleBuffer(
        ByProtocol,
        &gEfiFirmwareVolume2ProtocolGuid,
        NULL,
        &NumberOfHandles,
        &HandleBuffer
      );
    }
    else
    {
      Status = gBS->LocateHandleBuffer(
        ByProtocol,
        &gEfiFirmwareVolumeProtocolGuid,
        NULL,
        &NumberOfHandles,
        &HandleBuffer
      );
    }

    if (EFI_ERROR(Status))
    {
      if (ENG == TRUE) { Print(L"Сould not find any handle\n"); }
      else { Print(L"Не удалось найти ни одного дескриптора\n"); }
      return EFI_NOT_FOUND;
    }
    if (ENG == TRUE) { Print(L"Found %d Handle Instances\n\r", NumberOfHandles); }
    else
    {
      Print(L"Всего найдено дескрипторов: %d\n\r", NumberOfHandles);
    }

    SectionType = (SectionGuid == NULL) ? EFI_SECTION_RAW : EFI_SECTION_FREEFORM_SUBTYPE_GUID;

    //Find and read raw data files.
    for (i = 0; i < NumberOfHandles; i++) {
      if (!CompareGuid(&FilterProtocol, &gEfiFirmwareVolumeProtocolGuid)) {
        Status = gBS->HandleProtocol(HandleBuffer[i], &gEfiFirmwareVolume2ProtocolGuid, (VOID **)&Fv2);
      }
      else
      {
        Status = gBS->HandleProtocol(HandleBuffer[i], &gEfiFirmwareVolumeProtocolGuid, (VOID **)&Fv);
      }
        if (EFI_ERROR(Status)) { continue; } //[i]

        for(j = 0; ; j++){
            SectionData = NULL;

            //Read Section From FFS file
            if (!CompareGuid(&FilterProtocol, &gEfiFirmwareVolumeProtocolGuid)) {
              Status = Fv2->ReadSection(
                Fv2,
                &File,
                SectionType,
                j,
                &SectionData,
                &DataSize,
                &Authentication
              );
            /*
            * At least get PE section if
            * ReadSection failed
            * and
            * SectionGuid was not specified by caller
            */
              if (EFI_ERROR(Status) && SectionGuid == NULL) {
                Status = Fv2->ReadSection(
                  Fv2,
                  &File,
                  EFI_SECTION_PE32,
                  j,
                  &SectionData,
                  &DataSize,
                  &Authentication
                );
              }
            }
            else
            {
              Status = Fv->ReadSection(
                Fv,
                &File,
                SectionType,
                j,
                &SectionData,
                &DataSize,
                &Authentication
              );

              if (EFI_ERROR(Status) && SectionGuid == NULL) {
                Status = Fv->ReadSection(
                  Fv,
                  &File,
                  EFI_SECTION_PE32,
                  j,
                  &SectionData,
                  &DataSize,
                  &Authentication
                );
              }
            }

            //No file found advance to the next FV...
            if (Status != EFI_SUCCESS) { break; } //[j]

            //RAW sections don't have guid so we don't need doing file search for them
            if (SectionType == EFI_SECTION_FREEFORM_SUBTYPE_GUID) {
              if (ENG == TRUE) { Print(L"Trying to retrieve FREEFORM section from file\n"); }
              else { Print(L"Пытаемся найти секцию FREEFORM в файле\n"); }
              EFI_GUID *secguid;

              //Compare Section GUID, to find correct one..
              secguid = (EFI_GUID *)SectionData; //We can extract Guid from SectionData

              if (CompareGuid(secguid, &Section) != TRUE) {
                //Free section read, it was not the one we need...
                if (SectionData != NULL) { FreePool(SectionData); }
                continue; //[j]
              }

              //Update
              DataSize -= sizeof(EFI_GUID);
              // ReadSection returns pointer to a section GUID, which caller of this function does not want to see.
              // We could've just returned (EFI_GUID *)AmiData + 1 to the caller,
              // but this pointer can't be used to free the pool (can't be passed to FreePool).
              // Copy section data into a new buffer
              SectionData = AllocatePool(DataSize);
              if (SectionData == NULL) { Status = EFI_OUT_OF_RESOURCES; }
              else { CopyMem(SectionData, secguid + 1, DataSize); }

              FreePool(secguid);
            }

            if (HandleBuffer != NULL) {
              FreePool(HandleBuffer);
            }
            if (!EFI_ERROR(Status)){
                if (ENG == TRUE) { Print(L"Found\n\nCreating section dump on FS,\nthis shouldn't take more than 3 secs.\nOtherwise, UEFI system unsupported.\n"); }
                else { Print(L"Найдено\n\nСоздаем дамп секции на накопителе,\nэто должно занять не более 3 сек.\nИначе, UEFI не поддерживается.\n"); }
                *Pointer = SectionData;
                *Size = DataSize;

                CHAR16 DFName[41] = { 0 };                                  //36 chars for guid, 4 for the ".bin" ext and 1 for null-terminator
                UnicodeSPrint(DFName, sizeof(DFName), L"%a.bin", FileGuid); //Append ".bin"
                Root->Open(Root, &DumpFile, DFName, EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ | EFI_FILE_MODE_CREATE, 0);
                DumpFile->Write(DumpFile, Size, (UINT8 *)SectionData); //(UINT8 *)datum is correct
                DumpFile->Flush(DumpFile);
            }
            return Status;
        }//for [j]
    }//for [i]

    FreePool(HandleBuffer);
    return EFI_NOT_FOUND;
}

EFI_STATUS
Exec(IN EFI_HANDLE *AppImageHandle)
{
    UINTN ExitDataSize;
    EFI_STATUS Status = gBS->StartImage(*AppImageHandle, &ExitDataSize, (CHAR16 **)NULL);
    if (ENG == TRUE) { Print(L"Image Retuned - %r %x\n", Status); }
    else { Print(L"Драйвер сообщил - %r %x\n", Status); }
    return Status;
}

EFI_STATUS
UninstallProtocol(IN CHAR8 *ProtocolGuid, OUT UINTN Indexes)
{
   EFI_STATUS Status;
   EFI_HANDLE *HandlesBuffer;
   UINTN NumberOfHandles = 0;
   VOID *ProtocolInterface;
   //-----------------------------------------------------

   EFI_GUID GUID = { 0 };
   Status = AsciiStrToGuid(ProtocolGuid, &GUID);
   if (EFI_ERROR(Status))
   {
     if (ENG == TRUE) { Print(L"Failed to convert \"%a\" to GUID\n", ProtocolGuid); }
     else { Print(L"Не удалось сконвертировать \"%a\" в GUID\n", ProtocolGuid); }
     return Status;
   }

   Status = gBS->LocateHandleBuffer(ByProtocol, &GUID, NULL, &NumberOfHandles, &HandlesBuffer);

  if (EFI_ERROR(Status))
  {
    if (ENG == TRUE) { Print(L"Сould not find any handle with the specified protocol\n"); }
    else { Print(L"Не удалось найти ни одного дескриптора с указанным протоколом\n"); }
    return Status;
  } 
  
  for(UINTN i = 0; i < NumberOfHandles; i += 1)
  {
    ProtocolInterface = NULL;
    Status = gBS->HandleProtocol(HandlesBuffer[i],
                                 &GUID,
                                 (VOID **)&ProtocolInterface);

    if (EFI_ERROR(Status))
    {
      if (ENG == TRUE) { Print(L"Failed to retrieve handle %d's protocol interface: %r\n", i, Status); }
      else { Print(L"Не удалось получить смещение указателя на протокол от дескриптора %d: %r\n", i, Status); }
      continue;
    }

    Status = gBS->UninstallProtocolInterface(HandlesBuffer[i],
                                             &GUID, 
                                             ProtocolInterface);

    if (EFI_ERROR(Status))
    {
      if (ENG == TRUE) { Print(L"%r - Could not uninstall protocol: %g,\nfrom handle %d by offset pointer 0x%x\n", Status, GUID, i, ProtocolInterface); }
      else { Print(L"%r - Не удалось удалить протокол: %g,\nу дескриптора %d по смещению указателя 0x%x\n", Status, GUID, i, ProtocolInterface); }
      continue;
    }
    Indexes += 1;
  }
  return Status;
}

EFI_STATUS
UpdateHiiDB(IN CHAR8 *FormIdChar)
{
  EFI_STATUS Status;

  /*
  typedef struct {
    UINT8 byte1;
    UINT8 byte2;
    UINT8 byte3;
    UINT8 byte4;
    UINT8 byte5;
    UINT8 byte6;
    UINT8 byte7;
    UINT8 byte8;
    UINT8 byte9;
    UINT8 byte10;
    UINT8 byte11;
    UINT8 byte12;
    UINT8 byte13;
    UINT8 byte14;
    UINT8 byte15;
    UINT8 byte16;
  } RawOpCodeData;
  */

   //RawOpCodeData StaticValues = { 0x0F, 0x0F, 0x0E, 0x01, 0x0F, 0x01, 0x0A, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x54, 0x27 };
   //UINT8 *RawOpCodeBuffer = (UINT8 *)&StaticValues;
   //UINTN RawOpCodeBufferSize = 0xF;

   if (AsciiStrLen(FormIdChar) > 4)
   {
     if (ENG == TRUE) { Print(L"Can't convert \"%a\" to EFI_FORM_ID\n", FormIdChar); }
     else { Print(L"Не удастся сконвертировать \"%a\" в EFI_FORM_ID\n", FormIdChar); }
     return EFI_UNSUPPORTED;
   }
   CHAR16 FileName16[255] = { 0 };
   UnicodeSPrint(FileName16, sizeof(FileName16), L"%a", FormIdChar);

   UINTN FormIDUintn = { 0 };
   Status = StrHexToUintnS(FileName16, NULL, &FormIDUintn);
   if (EFI_ERROR(Status))
   {
     if (ENG == TRUE) { Print(L"Failed to convert \"%a\" to UINT\n", FormIdChar); }
     else { Print(L"Не удалось сконвертировать \"%a\" в UINT\n", FormIdChar); }
     return Status;
   }

   EFI_FORM_ID FormID = (UINT16)FormIDUintn;
   EFI_HII_HANDLE *HiiHandles = HiiGetHiiHandles(NULL);

  if (HiiHandles == NULL)
  {
    if (ENG == TRUE) { Print(L"Сould not retrieve any HII handle\n"); }
    else { Print(L"Не удалось получить ни одного дескриптора\n"); }
    return EFI_NOT_FOUND;
  } 

  VOID *StartOpCodeHandle = HiiAllocateOpCodeHandle();

  if (StartOpCodeHandle == NULL)
  {
    if (ENG == TRUE) { Print(L"Not enough resources\n"); }
    else { Print(L"Not enough resources\n"); }
    return EFI_OUT_OF_RESOURCES;
  }

  //EFI_IFR_REF *PointerStartOpCode = (EFI_IFR_REF *)HiiCreateRawOpCodes(StartOpCodeHandle, RawOpCodeBuffer, RawOpCodeBufferSize);
  EFI_IFR_END *PointerStartOpCode = (EFI_IFR_END *)HiiCreateEndOpCode(StartOpCodeHandle); //[29 02] can be met anywhere

  if (PointerStartOpCode == NULL)
  {
    if (ENG == TRUE) { Print(L"Not enough space in the buffer\n"); }
    else { Print(L"Not enough space in the buffer\n"); }
    return EFI_OUT_OF_RESOURCES;
  }

  //Debug
  //Print(L"Header/FormId %x / %x\n", PointerStartOpCode->Header, PointerStartOpCode->FormId);
  //Print(L"Header %x\n", PointerStartOpCode->Header);
  //EFI_GUID AptioS = { 0x4A10597B, 0x0DC0, 0x5841, {0x87, 0xFF, 0xF0, 0x4D, 0x63, 0x96, 0xA9, 0x15} };
  //EFI_GUID InsydeS = { 0xF4274AA0, 0x00DF, 0x424D, {0xB5, 0x52, 0x39, 0x51, 0x13, 0x02, 0x11, 0x3D} };

  if (ENG == TRUE) { Print(L"Hangs means could not update\n"); }
  else { Print(L"Зависание означает что обновить не удалось\n"); }
  Status = EFI_NOT_FOUND;
  for(UINT8 i = 0; i < 255; i += 1)
  {
    Status = HiiUpdateForm(HiiHandles[i],
                           NULL,              //Try updating all
                           FormID,
                           StartOpCodeHandle, //Out of res cuz of this
                           NULL);

    if (Status == EFI_SUCCESS)
    {
      if (ENG == TRUE) { Print(L"Successfully updated Form 0x%x in %d tries\n", FormID, i); }
      else { Print(L"Удалось обновить форму 0x%x c попытки %d\n", FormID, i); }
      break;
    }
  }
  HiiFreeOpCodeHandle(StartOpCodeHandle);
  FreePool(PointerStartOpCode);
  return Status;
}

UINTN
GetAptioHiiDB(IN BOOLEAN BufferSizeOrPointer)
{
  typedef struct {
    UINT32 DataSize;
    UINT32 DataPointer;
  } HiiDbBlock_DATA;

    EFI_STATUS Status;
    EFI_GUID ExportDatabaseGuid = { 0x1b838190, 0x4625, 0x4ead, {0xab, 0xc9, 0xcd, 0x5e, 0x6a, 0xf1, 0x8f, 0xe0} };
    UINTN Size = 8;
    HiiDbBlock_DATA HiiDB; //The whole var is 8 bytes, I save it into 2 by 4
    UINTN BufferSize = 0, Pointer = 0, Result = 0;
    //-----------------------------------------------------

    Status = gRT->GetVariable(
        L"HiiDB",
        &ExportDatabaseGuid,
        NULL,
        &Size,
        &HiiDB);

    if (Status == EFI_SUCCESS && Size != 0)
    {
      //Get buffers from pointer, source called with &
      CopyMem(&BufferSize, &HiiDB.DataSize, Size / 2);
      CopyMem(&Pointer, &HiiDB.DataPointer, Size / 2);

      /*Debug
      Print(L"\nPart1:\n");
      Print(L"%x\n", BufferSize);
      Print(L"\nPart2:\n");
      Print(L"%x\n", Pointer);
      */

      BufferSizeOrPointer ? (Result = Pointer) : (Result = BufferSize);
      return Result;
    }
    return 0;
}
