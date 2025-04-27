#include "Opcode.h"
#include "Utility.h"

EFI_STATUS FindLoadedImageFromName(EFI_HANDLE ImageHandle, CHAR8 *FileName, EFI_LOADED_IMAGE_PROTOCOL **ImageInfo)
{
    EFI_STATUS Status;
    UINTN HandleSize = 0;
    EFI_HANDLE *Handles;
    CHAR16 FileName16[255] = {0};
    UnicodeSPrint(FileName16, sizeof(FileName16), L"%a", FileName);
    Status = gBS->LocateHandle(ByProtocol, &gEfiLoadedImageProtocolGuid, NULL, &HandleSize, NULL);
    if (Status == EFI_BUFFER_TOO_SMALL)
    {
        Handles = AllocateZeroPool(HandleSize * sizeof(EFI_HANDLE));
        Status = gBS->LocateHandle(ByProtocol, &gEfiLoadedImageProtocolGuid, NULL, &HandleSize, Handles);
        if (ENG == TRUE) { Print(L"Retrived %d Handle, with %r\n\r", HandleSize, Status); }
        else { Print(L"Всего найдено дескрипторов: %d\n\r", HandleSize); }
    }

    for (UINTN i = 0; i < HandleSize; i++)
    {
        Status = gBS->HandleProtocol(Handles[i], &gEfiLoadedImageProtocolGuid, (VOID **)ImageInfo); //Process every found handle
        if (Status == EFI_SUCCESS)
        {
            CHAR16 *String = FindLoadedImageFileName(*ImageInfo);
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

EFI_STATUS FindLoadedImageFromGUID(EFI_HANDLE ImageHandle, CHAR8 *FileName, EFI_LOADED_IMAGE_PROTOCOL **ImageInfo, EFI_SECTION_TYPE Section_Type)
{
    EFI_STATUS Status;
    UINTN HandleSize = 0;
    EFI_HANDLE *Handles;

    EFI_GUID GUID = { 0 };  //Initialize beforehand
    AsciiStrToGuid(FileName, &GUID);

    UINT8 *Buffer = NULL;
    UINTN BufferSize = 0;
    Status = LocateAndLoadFvFromGuid(GUID, Section_Type, &Buffer, &BufferSize); //Pass the converted guid to Utility to get the target BufferSize

    Status = gBS->LocateHandle(ByProtocol, &gEfiLoadedImageProtocolGuid, NULL, &HandleSize, NULL);
    if (Status == EFI_BUFFER_TOO_SMALL)
    {
        Handles = AllocateZeroPool(HandleSize * sizeof(EFI_HANDLE));
        Status = gBS->LocateHandle(ByProtocol, &gEfiLoadedImageProtocolGuid, NULL, &HandleSize, Handles);
        if (ENG == TRUE) { Print(L"Retrived %d Handle, with %r\n\r", HandleSize, Status); }
        else { Print(L"Всего найдено дескрипторов: %d\n\r", HandleSize); }
    }

    if (Section_Type == EFI_SECTION_FREEFORM_SUBTYPE_GUID) {
      if (BufferSize != 0) {
        if (ENG == TRUE) { Print(L"Although the section with size: %d was found in BIOS,\nSREP may not be able to find Freeform in RAM\n", BufferSize); }
        else { Print(L"Хотя в БИОС нашлась секция с размером: %d,\nSREP может быть не способен найти Freeform в RAM\n", BufferSize); }
      }
    }

    for (UINTN i = 0; i < HandleSize; i++)
    {
        Status = gBS->HandleProtocol(Handles[i], &gEfiLoadedImageProtocolGuid, (VOID **)ImageInfo); //Process every found handle
        if (Status == EFI_SUCCESS)
        {
            UINTN FoundSize = FindLoadedImageBufferSize(*ImageInfo); //Read ImageInfo, so we get BufferSize from Handles[i]

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

EFI_STATUS LoadFS(EFI_HANDLE ImageHandle, CHAR8 *FileName, EFI_LOADED_IMAGE_PROTOCOL **ImageInfo, EFI_HANDLE *AppImageHandle)
{
    //UINTN ExitDataSize;
    UINTN NumHandles;
    UINTN Index;
    EFI_HANDLE *SFS_Handles;
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_BLOCK_IO_PROTOCOL *BlkIo;
    EFI_DEVICE_PATH_PROTOCOL *FilePath;
    Status =
        gBS->LocateHandleBuffer(ByProtocol, &gEfiSimpleFileSystemProtocolGuid,
                                NULL, &NumHandles, &SFS_Handles);

    if (Status != EFI_SUCCESS)
    {
        if (ENG == TRUE) { Print(L"Could not find handles - %r\n", Status); }
        else { Print(L"Не удалось найти ни одного дескриптора с\n поддержкой EfiSimpleFileSystemProtocol"); }
        return Status;
    }
    if (ENG == TRUE) { Print(L"No of Handle - %d\n", NumHandles); }
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
        Status = gBS->LoadImage(FALSE, ImageHandle, FilePath, (VOID *)NULL, 0,
                                AppImageHandle);

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

EFI_STATUS LoadFV(EFI_HANDLE ImageHandle, CHAR8 *FileName, EFI_LOADED_IMAGE_PROTOCOL **ImageInfo, EFI_HANDLE *AppImageHandle, EFI_SECTION_TYPE Section_Type)
{
    EFI_STATUS Status = EFI_SUCCESS;

    CHAR16 FileName16[255] = {0};
    UnicodeSPrint(FileName16, sizeof(FileName16), L"%a", FileName);

    UINT8 *Buffer = NULL;
    UINTN BufferSize = 0;
    Status = LocateAndLoadFvFromName(FileName16, Section_Type, &Buffer, &BufferSize);

    Status = gBS->LoadImage(FALSE, ImageHandle, (VOID *)NULL, Buffer, BufferSize,
                            AppImageHandle);
    if (Buffer != NULL)
        FreePool(Buffer);
    if (Status != EFI_SUCCESS)
    {
        if (ENG == TRUE) { Print(L"Could not Locate the image from FV %r\n", Status); }
        else { Print(L"Не удалось загрузить драйвер из FV\n"); }
    }
    else
    {
        Status = gBS->OpenProtocol(*AppImageHandle, &gEfiLoadedImageProtocolGuid,
                                   (VOID **)ImageInfo, ImageHandle, (VOID *)NULL,
                                   EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    }

    return Status;
};

EFI_STATUS LoadFVbyGUID(EFI_HANDLE ImageHandle, CHAR8 *FileName, EFI_LOADED_IMAGE_PROTOCOL **ImageInfo, EFI_HANDLE *AppImageHandle, EFI_SECTION_TYPE Section_Type, EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS Status = EFI_SUCCESS;
  EFI_HANDLE_PROTOCOL HandleProtocol;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
  EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
  EFI_FILE *Root;
  EFI_FILE *DumpFile = NULL;

  HandleProtocol = SystemTable->BootServices->HandleProtocol;
  HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (void**)&LoadedImage);
  HandleProtocol(LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (void**)&FileSystem);
  FileSystem->OpenVolume(FileSystem, &Root);

  EFI_GUID GUID = { 0 };  //Initialize beforehand
  AsciiStrToGuid(FileName, &GUID);

  /* Debug
  EFI_GUID GUID = { 0x899407d7,0x99fe,0x43d8,0x9a,0x21,0x79,0xec,0x32,0x8c,0xac,0x21 };
  Print(L"GUID from OpCode.c raw: %s\n\r", Guid);
  Print(L"GUID from OpCode.c converted: %g\n\r", GUID);
  */

  UINT8 *Buffer = NULL;
  UINTN BufferSize = 0;
  Status = LocateAndLoadFvFromGuid(GUID, Section_Type, &Buffer, &BufferSize); //Pass the converted guid to Utility

  //Dumping section
  if (Buffer == NULL) {
    if (ENG == TRUE) { Print(L"Buffer of the specified driver is too small: %d\n", Buffer); } //Print result
    else { Print(L"Размер указанного драйвера слишком мал: %d\n", BufferSize); }
    return EFI_NOT_FOUND;
  }

  CHAR16 DFName[41] = { 0 };                                  //36 chars for guid, 4 for the ".bin" ext and 1 for null-terminator
  UnicodeSPrint(DFName, sizeof(DFName), L"%a.bin", FileName); //Append ".bin"
  Root->Open(Root, &DumpFile, DFName, EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ | EFI_FILE_MODE_CREATE, 0);
  DumpFile->Write(DumpFile, &BufferSize, (VOID**)Buffer);
  DumpFile->Flush(DumpFile);

  if (ENG == TRUE) { Print(L"Creating image dump on FS: %r\n", Status); } //Print result
  else { Print(L"Создаем дамп PE секции драйвера, размер должен быть: %d,\nрезультат: %r\n", BufferSize, Status); }

  Status = gBS->LoadImage(FALSE, ImageHandle, (VOID*)NULL, Buffer, BufferSize, AppImageHandle); //Parse saved buffer to LoadImage BS
  if (Buffer != NULL)
    FreePool(Buffer);
  if (Status != EFI_SUCCESS)
  {
    if (ENG == TRUE) { Print(L"Could not Locate the image from FV: %r\n", Status); }
    else { Print(L"Не удалось загрузить драйвер из FV\n"); }
  }
  else
  {
    Status = gBS->OpenProtocol(*AppImageHandle, &gEfiLoadedImageProtocolGuid,
      (VOID**)ImageInfo, ImageHandle, (VOID*)NULL,
      EFI_OPEN_PROTOCOL_GET_PROTOCOL);
  }

  return Status;
};

EFI_STATUS Exec(EFI_HANDLE *AppImageHandle)
{
    UINTN ExitDataSize;
    EFI_STATUS Status = gBS->StartImage(*AppImageHandle, &ExitDataSize, (CHAR16 **)NULL);
    if (ENG == TRUE) { Print(L"Image Retuned - %r %x\n", Status); }
    else { Print(L"Драйвер сообщил - %r %x\n", Status); }
    return Status;
}
