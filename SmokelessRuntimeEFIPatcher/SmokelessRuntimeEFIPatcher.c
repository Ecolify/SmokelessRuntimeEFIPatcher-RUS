//Include sources
#include "Utility.h"
#include "Opcode.h"

//Specify app version
#define SREP_VERSION L"0.1.6rc1 RUS"

//Get font data having external linkage
extern EFI_WIDE_GLYPH gSimpleFontWideGlyphData[];
extern UINT32 gSimpleFontWideBytes;
extern EFI_NARROW_GLYPH gSimpleFontNarrowGlyphData[];
extern UINT32 gSimpleFontNarrowBytes;

EFI_BOOT_SERVICES *_gBS = NULL;
EFI_RUNTIME_SERVICES *_gRS = NULL;
EFI_FILE *LogFile = NULL;

//Set log buffer size
CHAR16 Log[512];

//Declare two sets of named constants
enum
{
    OFFSET = 1,
    PATTERN,
    REL_NEG_OFFSET,
    REL_POS_OFFSET
};
enum OPCODE
{
    NO_OP,
    LOADED,
    LOADED_GUID_PE,
    LOADED_GUID_TE,
    LOAD_FS,
    LOAD_FV,
    LOAD_GUID,
    PATCH,
    EXEC
};

//Declare data structure for a single operation
struct OP_DATA
{
    enum OPCODE ID;               //Loaded, LoadFromFV, LoadFromFS and etc. See the corresp. OPCODE.
    CHAR8 *Name;                  //<FileName>, argument for the OPCODE.
    BOOLEAN Name_Dyn_Alloc;
    UINT64 PatchType;             //Pattern, Offset, Rel...
    BOOLEAN PatchType_Dyn_Alloc;
    INT64 ARG3;                   //Offset
    BOOLEAN ARG3_Dyn_Alloc;
    UINT64 ARG4;                  //Patch length
    BOOLEAN ARG4_Dyn_Alloc;
    UINT64 ARG5;                  //Patch buffer
    BOOLEAN ARG5_Dyn_Alloc;
    UINT64 ARG6;                  //Pattern length
    BOOLEAN ARG6_Dyn_Alloc;
    UINT64 ARG7;                  //Pattern to search for
    BOOLEAN ARG7_Dyn_Alloc;
    CHAR8 *Regex;
    BOOLEAN Regex_Dyn_Alloc;
    struct OP_DATA *next;
    struct OP_DATA *prev;
};

typedef struct {
  VOID* BaseAddress;
  UINTN BufferSize;
  UINTN Width;
  UINTN Height;
  UINTN PixelsPerScanLine;
} FRAME_BUFFER;

//C2220 suppression due to log filename issues
#pragma warning(disable:4459)
#pragma warning(disable:4456)
#pragma warning(disable:4244)

//Writes string from the buffer to SREP.log
VOID LogToFile( EFI_FILE *LogFile, CHAR16 *String)
{
        UINTN Size = StrLen(String) * 2;      //Size variable equals to the string size
        LogFile->Write(LogFile,&Size,String); //EFI_FILE_WRITE (Filename, size, the actual string to write)
        LogFile->Flush(LogFile);              //Flushes all modified data associated with a file to a device
}

//Collect OPCODEs from cfg
static VOID Add_OP_CODE(struct OP_DATA *Start, struct OP_DATA *opCode)
{
    struct OP_DATA *next = Start;
    while (next->next != NULL)
    {
        next = next->next;
    }
    next->next = opCode;
    opCode->prev = next;
}

//Detect OPCODEs in read buffer and dispatch immediatelly. Currently unused
static VOID PrintOPChain(struct OP_DATA *Start)
{
    struct OP_DATA *next = Start;
    while (next != NULL)
    {
        UnicodeSPrint(Log,512,u"%s","OPCODE: ");
        LogToFile(LogFile,Log);
        switch (next->ID)
        {
        case NO_OP:
            UnicodeSPrint(Log,512,u"%a","NOP\n\r");
            LogToFile(LogFile,Log);
            break;
        case LOADED:
            UnicodeSPrint(Log,512,u"%a","LOADED\n\r");
            LogToFile(LogFile,Log);
            break;
        case LOADED_GUID_PE:
            UnicodeSPrint(Log,512,u"%a","LOADED_GUID_PE\n\r");
            LogToFile(LogFile,Log);
            break;
        case LOADED_GUID_TE:
            UnicodeSPrint(Log,512,u"%a","LOADED_GUID_TE\n\r");
            LogToFile(LogFile,Log);
            break;
        case LOAD_FS:
            UnicodeSPrint(Log,512,u"%a","LOAD_FS\n\r");
            LogToFile(LogFile,Log);
            UnicodeSPrint(Log,512,u"%a","\t FileName %a\n\r", next->Name);
            LogToFile(LogFile,Log);
            break;
        case LOAD_FV:
            UnicodeSPrint(Log,512,u"%a","LOAD_FV\n\r");
            LogToFile(LogFile,Log);
            UnicodeSPrint(Log,512,u"%a","\t FileName %a\n\r", next->Name);
            LogToFile(LogFile,Log);
            break;
        case LOAD_GUID:
            UnicodeSPrint(Log,512,u"%a","LOAD_GUID\n\r");
            LogToFile(LogFile,Log);
            UnicodeSPrint(Log,512,u"%a","\t GUID %a\n\r", next->Name);
            LogToFile(LogFile,Log);
            break;
        case PATCH:
            UnicodeSPrint(Log,512,u"%a","PATCH\n\r");
            LogToFile(LogFile,Log);
            break;
        case EXEC:
            UnicodeSPrint(Log,512,u"%a","EXEC\n\r");
            LogToFile(LogFile,Log);
            break;

        default:
            break;
        }
        next = next->next;
    }
}

//Prints those 16 symbols wide dumps
static VOID PrintDump(UINT16 Size, UINT8 *DUMP)
{
    for (UINT16 i = 0; i < Size; i++)
    {
        if (i % 0x10 == 0)
        {
            UnicodeSPrint(Log,512,u"%a","\n\t");
            LogToFile(LogFile,Log);
        }
        UnicodeSPrint(Log,512,u"%02x", DUMP[i]);
        LogToFile(LogFile,Log);
    }
    UnicodeSPrint(Log,512,u"%a","\n\t");
    LogToFile(LogFile,Log);
}

//Simple font support function
static UINT8 *CreateSimpleFontPkg(EFI_WIDE_GLYPH *WideGlyph,
  UINT32 WideGlyphSizeInBytes,
  EFI_NARROW_GLYPH *NarrowGlyph,
  UINT32 NarrowGlyphSizeInBytes)
{
  UINT32 PackageLen = sizeof(EFI_HII_SIMPLE_FONT_PACKAGE_HDR) + WideGlyphSizeInBytes + NarrowGlyphSizeInBytes + 4;
  UINT8* FontPackage = (UINT8*)AllocateZeroPool(PackageLen);
  *(UINT32*)FontPackage = PackageLen;

  EFI_HII_SIMPLE_FONT_PACKAGE_HDR* SimpleFont;
  SimpleFont = (EFI_HII_SIMPLE_FONT_PACKAGE_HDR*)(FontPackage + 4);
  SimpleFont->Header.Length = (UINT32)(PackageLen - 4);
  SimpleFont->Header.Type = EFI_HII_PACKAGE_SIMPLE_FONTS;
  SimpleFont->NumberOfNarrowGlyphs = (UINT16)(NarrowGlyphSizeInBytes / sizeof(EFI_NARROW_GLYPH));
  SimpleFont->NumberOfWideGlyphs = (UINT16)(WideGlyphSizeInBytes / sizeof(EFI_WIDE_GLYPH));

  UINT8* Location = (UINT8*)(&SimpleFont->NumberOfWideGlyphs + 1);
  CopyMem(Location, NarrowGlyph, NarrowGlyphSizeInBytes);
  CopyMem(Location + NarrowGlyphSizeInBytes, WideGlyph, WideGlyphSizeInBytes);

  return FontPackage;
}

//Main function, entry point
EFI_STATUS EFIAPI SREPEntry(
  IN EFI_HANDLE ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
){
  /*-----------------------------------------------------------------------------------*/
  //
  //For the font
  //
  EFI_GUID gHIIRussianFontGuid;
  UINT8 *FontPackage = CreateSimpleFontPkg(gSimpleFontWideGlyphData,
                                          gSimpleFontWideBytes,
                                          gSimpleFontNarrowGlyphData,
                                          gSimpleFontNarrowBytes);

  EFI_HII_HANDLE Handle = HiiAddPackages(&gHIIRussianFontGuid,     //This is OK
                                         NULL,
                                         FontPackage,
                                         NULL,
                                         NULL);
  
  FreePool(FontPackage);

  if (Handle == NULL)
  {
    Print(L"Error! Can't Add Any more Lang Packages! Futher messages may be crippled!\n\n");
  }
  /*-----------------------------------------------------------------------------------*/

    EFI_STATUS Status;
    EFI_HANDLE AppImageHandle;
    EFI_HANDLE_PROTOCOL HandleProtocol;
    EFI_LOADED_IMAGE_PROTOCOL *ImageInfo;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    EFI_SHELL_PARAMETERS_PROTOCOL *ShellParameters;
    EFI_REGULAR_EXPRESSION_PROTOCOL *RegularExpressionProtocol;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_FILE *Root;
    EFI_FILE *ConfigFile;
    CHAR16 FileName[255];

    /*-----------------------------------------------------------------------------------*/
    //
    //For Regex
    //
    EFI_HANDLE *HandleBuffer;
    HandleBuffer = NULL;
    UINTN BufferSize = 0;

    LoadandRunImage(ImageHandle, SystemTable, L"Oniguruma.efi", &AppImageHandle); //Produce RegularExpressionProtocol

    Status = gBS->LocateHandle(
      ByProtocol,
      &gEfiRegularExpressionProtocolGuid,
      NULL,
      &BufferSize,
      HandleBuffer
    );

    //Catch not enough memory there, print Status from LocateHandle
    //Print(L"Point 1 - %r\n\r", Status);
    if (Status == EFI_BUFFER_TOO_SMALL) {
      HandleBuffer = AllocateZeroPool(BufferSize);
      if (HandleBuffer == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        //Print(L"Point 2 - %r\n\r", Status);
        return Status;
      }
    };
    if (Status == EFI_NOT_FOUND)
    {
      if (ENG == TRUE) {
        Print(L"Failed on Locating RegularExpressionProtocol: %r\n\r", Status);
        UnicodeSPrint(Log, 512, L"Failed on Locating RegularExpressionProtocol: %r\n\r", Status);
        LogToFile(LogFile, Log);
      }
      else
      {
        Print(L"Зависимости не найдены: RegularExpressionProtocol\n");
        Print(L"Убедитесь что файл Oniguruma.efi доступен\n\r");
        UnicodeSPrint(Log, 512, u"Не хватает зависимостей, причина: %r\n", Status);
        LogToFile(LogFile, Log);
      }
      gBS->Stall(3000000);
      return Status;
    }
    //Try again after allocation
    Status = gBS->LocateHandle(
      ByProtocol,
      &gEfiRegularExpressionProtocolGuid,
      NULL,
      &BufferSize,
      HandleBuffer
    );

    for (UINTN Index = 0; Index < BufferSize / sizeof(EFI_HANDLE); Index++) {
      Status = gBS->HandleProtocol(
        HandleBuffer[Index],
        &gEfiRegularExpressionProtocolGuid,
        (VOID**)&RegularExpressionProtocol
      );
    }

    //Print(L"Point 3 - %r\n\r", Status);
  /*-----------------------------------------------------------------------------------*/

  //Setup the program depending on the presence of arguments
  Status = gBS->HandleProtocol(
    ImageHandle,
    &gEfiShellParametersProtocolGuid,
    (VOID **) &ShellParameters
  );

  Print(L"Welcome to Smokeless Runtime EFI Patcher %s\n\r", SREP_VERSION);

  if (Status == EFI_SUCCESS) {
    if (ShellParameters->Argc == 2) {
      if (!StrCmp(ShellParameters->Argv[1], L"ENG")) //Check for ENG arg
      {
        ENG = TRUE;
        Print(L"English mode\n\n");
      }
    }
  }
  else
  {
    Print(L"Switch to English is disabled due to the outdated UEFI shell\n\n");
  }
    
    //Print(L"ShellParameters init status: %r\n\r", Status); //Shell Debug

    gBS->SetWatchdogTimer(0, 0, 0, 0);  //Disable watchdog so the system doesn't reboot by timer

    HandleProtocol = SystemTable->BootServices->HandleProtocol;
    HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (void **)&LoadedImage);
    HandleProtocol(LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (void **)&FileSystem);
    FileSystem->OpenVolume(FileSystem, &Root);

    //Get log
    Status = Root->Open(Root, &LogFile, L"SREP.log", EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ | EFI_FILE_MODE_CREATE, 0);
    if (Status != EFI_SUCCESS)
    {
        if (ENG == TRUE) {
          Print(L"Failed on Opening Log File: %r\n\r", Status);
          UnicodeSPrint(Log, 512, L"Failed on Opening Log File: %r\n\r", Status);
          LogToFile(LogFile, Log);
        }
        else
        {
          Print(L"Не удалось подготовить логирование, причина: %r,\n", Status);
          Print(L"Убедитесь что накопитель в формате FAT\n\r");
          UnicodeSPrint(Log, 512, u"Не удалось подготовить логирование: %r\n", Status);
          LogToFile(LogFile, Log);
          UnicodeSPrint(Log, 512, u"Убедитесь что накопитель в формате FAT\n\r");
          LogToFile(LogFile, Log);
        }
        gBS->Stall(3000000);
        return Status;
    }

    if (ENG == TRUE) {
      UnicodeSPrint(Log, 512, u"SREP Launched\n\r");
      LogToFile(LogFile, Log);
    }
    else
    {
      Print(L"SREP запущен\n\r");
      UnicodeSPrint(Log, 512, u"SREP запущен\n\r");
      LogToFile(LogFile, Log);
    }
    UnicodeSPrint(FileName, sizeof(FileName), u"%a", "SREP_Config.cfg");
    Status = Root->Open(Root, &ConfigFile, FileName, EFI_FILE_MODE_READ, 0);
    if (Status != EFI_SUCCESS)
    {
        if (ENG == TRUE) {
          Print(L"Failed on Opening cfg\n\r");
          UnicodeSPrint(Log, 512, u"Failed on Opening cfg: %r\n\r", Status);
          LogToFile(LogFile, Log);
        }
        else
        {
          Print(L"Не удалось открыть SREP_Config.cfg\n\r");
          UnicodeSPrint(Log, 512, u"Не удалось открыть конфиг, причина: %r\n\r", Status);
          LogToFile(LogFile, Log);
        }
        gBS->Stall(3000000);
        return Status;
    }
   if (ENG == TRUE) {
     UnicodeSPrint(Log, 512, u"%a", "Opened Config\n\r");
     LogToFile(LogFile, Log);
   }
   else
   {
     Print(L"SREP_Config.cfg найден\n\r");
     UnicodeSPrint(Log, 512, u"Конфиг открыт\n\r");
     LogToFile(LogFile, Log);
   }

    //Check config
    EFI_GUID gFileInfo = EFI_FILE_INFO_ID;
    EFI_FILE_INFO *FileInfo = NULL;
    UINTN FileInfoSize = 0;
    Status = ConfigFile->GetInfo(ConfigFile, &gFileInfo, &FileInfoSize, &FileInfo);
    if (Status == EFI_BUFFER_TOO_SMALL)
    {
        FileInfo = AllocatePool(FileInfoSize);
        Status = ConfigFile->GetInfo(ConfigFile, &gFileInfo, &FileInfoSize, FileInfo);
        if (Status != EFI_SUCCESS)
        {
            if (ENG == TRUE) {
              Print(L"Failed Getting SREP_Config Info\n\r");
              UnicodeSPrint(Log, 512, u"Failed Getting SREP_Config Info: %r\n\r", Status);
              LogToFile(LogFile, Log);
            }
            else
            {
              Print(L"Не удалось получить данные о файле\n\r");
              UnicodeSPrint(Log, 512, u"Не удалось получить данные о файле, причина: %r\n\r", Status);
              LogToFile(LogFile, Log);
            }
            gBS->Stall(3000000);
            return Status;
        }
    }
    UINTN ConfigDataSize = FileInfo->FileSize + 1; //Add Last null Terminator
    if (ENG == TRUE) {
      UnicodeSPrint(Log, 512, u"Config Size: 0x%x\n\r", ConfigDataSize);
      LogToFile(LogFile, Log);
    }
    else
    {
      UnicodeSPrint(Log, 512, u"Размер конфига в HEX: 0x%x\n\r", ConfigDataSize);
      LogToFile(LogFile, Log);
    }
    CHAR8 *ConfigData = AllocateZeroPool(ConfigDataSize);
    FreePool(FileInfo);

    //Get config
    Status = ConfigFile->Read(ConfigFile, &ConfigDataSize, ConfigData);
    if (Status != EFI_SUCCESS)
    {
        if (ENG == TRUE) {
          Print(L"Failed on Reading SREP_Config\n\r");
          UnicodeSPrint(Log, 512, u"Failed on Reading SREP_Config: %r\n\r", Status);
          LogToFile(LogFile, Log);
        }
        else
        {
          Print(L"Не удалось прочитать конфиг\n\r");
          UnicodeSPrint(Log, 512, u"Не удалось прочитать конфиг, причина: %r\n\r", Status);
          LogToFile(LogFile, Log);
        }
        gBS->Stall(3000000);
        return Status;
    }
    if (ENG == TRUE) {
      Print(L"Parsing Config\n\r");
      UnicodeSPrint(Log, 512, u"%a", "Parsing Config\n\r");
      LogToFile(LogFile, Log);
    }
    else
    {
      Print(L"Начало обработки данных\n\r");
      UnicodeSPrint(Log, 512, u"Начало обработки данных\n\r");
      LogToFile(LogFile, Log);
    }
    ConfigFile->Close(ConfigFile);
    if (ENG != TRUE) {
      Print(L"Стираем ненужные переносы строк\n\r");
    }
    for (UINTN i = 0; i < ConfigDataSize; i++)
    {
        if (ConfigData[i] == '\n' || ConfigData[i] == '\r' || ConfigData[i] == '\t')
        {
            ConfigData[i] = '\0';
        }
    }
    UINTN curr_pos = 0;

    //Read config data, and dispatch
    struct OP_DATA *Start = AllocateZeroPool(sizeof(struct OP_DATA));
    struct OP_DATA *Prev_OP;
    Start->ID = NO_OP;
    Start->next = NULL;
    BOOLEAN NullByteSkipped = FALSE;
    CHAR16 *Pattern16 = NULL;

    while (curr_pos < ConfigDataSize)
    {

        if (curr_pos != 0 && !NullByteSkipped)
            curr_pos += AsciiStrLen(&ConfigData[curr_pos]);
        if (ConfigData[curr_pos] == '\0')
        {
            curr_pos += 1;
            NullByteSkipped = TRUE;
            continue;
        }
        NullByteSkipped = FALSE;
        if (ENG == TRUE) {
          UnicodeSPrint(Log, 512, u"Current Parsing %a\n\r", &ConfigData[curr_pos]);
          LogToFile(LogFile, Log);
        }
        else
        {
          Print(L"В данный момент обрабатываем строку %a\n\r", &ConfigData[curr_pos]);
          UnicodeSPrint(Log, 512, u"В данный момент обрабатываем строку %a\n\r", &ConfigData[curr_pos]);
          LogToFile(LogFile, Log);
        }
        if (AsciiStrStr(&ConfigData[curr_pos], "End"))
        {
            if (ENG == TRUE) {
              UnicodeSPrint(Log, 512, u"%a", "End OP Detected\n\r");
              LogToFile(LogFile, Log);
            }
            else
            {
              Print(L"Обнаружен арг. End, операции над дескриптором доб. в очередь\n\r");
              UnicodeSPrint(Log, 512, u"Обнаружен арг. End\n\r");
              LogToFile(LogFile, Log);
            }
            continue;
        }
        if (AsciiStrStr(&ConfigData[curr_pos], "Op"))
        {
            if (ENG == TRUE) {
              UnicodeSPrint(Log, 512, u"%a", "OP Detected\n\r");
              LogToFile(LogFile, Log);
            }
            else
            {
              UnicodeSPrint(Log, 512, u"Обнаружена OP\n\r");
              LogToFile(LogFile, Log);
            }
            curr_pos += 3;
            if (ENG == TRUE) {
              UnicodeSPrint(Log, 512, u"Command %a\n\r", &ConfigData[curr_pos]);
              LogToFile(LogFile, Log);
            }
            else
            {
              UnicodeSPrint(Log, 512, u"Команда %a\n\r", &ConfigData[curr_pos]);
              LogToFile(LogFile, Log);
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "LoadFromFS"))
            {
                Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
                Prev_OP->ID = LOAD_FS;
                Add_OP_CODE(Start, Prev_OP);
                continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "LoadFromFV"))
            {
                Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
                Prev_OP->ID = LOAD_FV;
                Add_OP_CODE(Start, Prev_OP);
                continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "LoadGUIDandSavePE"))
            {
                Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
                Prev_OP->ID = LOAD_GUID;
                Add_OP_CODE(Start, Prev_OP);
                continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "Loaded"))
            {
                Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
                Prev_OP->ID = LOADED;
                Add_OP_CODE(Start, Prev_OP);
                continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "NonamePE"))
            {
                Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
                Prev_OP->ID = LOADED_GUID_PE;
                Add_OP_CODE(Start, Prev_OP);
                continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "NonameTE"))
            {
                Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
                Prev_OP->ID = LOADED_GUID_TE;
                Add_OP_CODE(Start, Prev_OP);
                continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "Patch"))
            {
                Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
                Prev_OP->ID = PATCH;
                Add_OP_CODE(Start, Prev_OP);
                continue;
            }

            if (AsciiStrStr(&ConfigData[curr_pos], "Exec"))
            {
              if (ENG != TRUE) { Print(L"Обнаружена команда Exec, запуск приложения доб. в очередь\n\r"); }
                Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
                Prev_OP->ID = EXEC;
                Add_OP_CODE(Start, Prev_OP);
                continue;
            }
            if (ENG == TRUE) {
              UnicodeSPrint(Log, 512, u"Command %a Invalid\n\r", &ConfigData[curr_pos]);
              LogToFile(LogFile, Log);
            }
            else
            {
              Print(L"Аргумент %a неверен\n\r");
              UnicodeSPrint(Log, 512, u"Аргумент %a неверен\n\r", &ConfigData[curr_pos]);
              LogToFile(LogFile, Log);
            }
            return EFI_INVALID_PARAMETER;
        }
        if ((Prev_OP->ID == LOAD_FS || Prev_OP->ID == LOAD_FV || Prev_OP->ID == LOAD_GUID || Prev_OP->ID == LOADED || Prev_OP->ID == LOADED_GUID_PE || Prev_OP->ID == LOADED_GUID_TE) && Prev_OP->Name == 0)
        {
            if (ENG == TRUE) {
              UnicodeSPrint(Log, 512, u"Found File %a\n\r", &ConfigData[curr_pos]);
              LogToFile(LogFile, Log);
            }
            else
            {
              UnicodeSPrint(Log, 512, u"Обнаружен ввод названия\n\r");
              LogToFile(LogFile, Log);
            }
            UINTN FileNameLength = AsciiStrLen(&ConfigData[curr_pos]) + 1;
            CHAR8 *FileName = AllocateZeroPool(FileNameLength);
            CopyMem(FileName, &ConfigData[curr_pos], FileNameLength);
            Prev_OP->Name = FileName;
            Prev_OP->Name_Dyn_Alloc = TRUE;
            continue;
        }
        if (Prev_OP->ID == PATCH && Prev_OP->PatchType == 0)
        {
            if (AsciiStrStr(&ConfigData[curr_pos], "Offset"))
            {
                if (ENG == TRUE) {
                  UnicodeSPrint(Log, 512, u"%a", "Found Offset\n\r");
                  LogToFile(LogFile, Log);
                }
                else
                {
                  UnicodeSPrint(Log, 512, u"Смещение получено\n\r");
                  LogToFile(LogFile, Log);
                }
                Prev_OP->PatchType = OFFSET;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "Pattern"))
            {
                if (ENG == TRUE) {
                  UnicodeSPrint(Log, 512, u"%a", "Found Pattern\n\r");
                  LogToFile(LogFile, Log);
                }
                else
                {
                  UnicodeSPrint(Log, 512, u"Найден шаблон\n\r");
                  LogToFile(LogFile, Log);
                }
                Prev_OP->PatchType = PATTERN;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "RelNegOffset"))
            {
                if (ENG == TRUE) {
                  UnicodeSPrint(Log, 512, u"%a", "Found Offset\n\r");
                  LogToFile(LogFile, Log);
                }
                else
                {
                  UnicodeSPrint(Log, 512, u"Смещение получено\n\r");
                  LogToFile(LogFile, Log);
                }
                Prev_OP->PatchType = REL_NEG_OFFSET;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "RelPosOffset"))
            {
                if (ENG == TRUE) {
                  UnicodeSPrint(Log, 512, u"%a", "Found Offset\n\r");
                  LogToFile(LogFile, Log);
                }
                else
                {
                  UnicodeSPrint(Log, 512, u"Смещение получено\n\r");
                  LogToFile(LogFile, Log);
                }
                Prev_OP->PatchType = REL_POS_OFFSET;
            }
            continue;
        }

        //This is the new itereration, we are just in from the Pattern OPCODE
        //Check which arguments are present, remember to patch the location by OFFSET
        //ARG3 is Offset, ARG6 is Pattern Length, ARG7 is Pattern Buffer, Regex is raw pattern string
        if (Prev_OP->ID == PATCH && Prev_OP->PatchType != 0 && Prev_OP->ARG3 == 0)
        {
            if (Prev_OP->PatchType == OFFSET || Prev_OP->PatchType == REL_NEG_OFFSET || Prev_OP->PatchType == REL_POS_OFFSET) //Patch by offset
            {
                if (ENG == TRUE) {
                  UnicodeSPrint(Log, 512, u"%a", "Decode Offset\n\r");
                  LogToFile(LogFile, Log);
                }
                else
                {
                  UnicodeSPrint(Log, 512, u"Смещение посчитано\n\r");
                  LogToFile(LogFile, Log);
                }
                Prev_OP->ARG3 = AsciiStrHexToUint64(&ConfigData[curr_pos]);
            }
            if (Prev_OP->PatchType == PATTERN)  //Take offset from Prev_OP if it was PATTERN patch
            {
                Prev_OP->ARG3 = 0xFFFFFFFF;
                Prev_OP->ARG6 = AsciiStrLen(&ConfigData[curr_pos]) / 2;
                if (ENG == TRUE) {
                  UnicodeSPrint(Log, 512, u"Found %d Bytes\n\r", Prev_OP->ARG6);
                  LogToFile(LogFile, Log);
                }
                else
                {
                  UnicodeSPrint(Log, 512, u"Найдено %d байт\n\r", Prev_OP->ARG6);
                  LogToFile(LogFile, Log);
                }
                
                UINTN RegexLength = AsciiStrLen(&ConfigData[curr_pos]) + 1;
                CHAR8 *Regex = AllocateZeroPool(RegexLength);
                CopyMem(Regex, &ConfigData[curr_pos], RegexLength);
                Prev_OP->Regex = Regex;
                Prev_OP->Regex_Dyn_Alloc = TRUE;

                //Old imp, no regex 
                Prev_OP->ARG7 = (UINT64)AllocateZeroPool(Prev_OP->ARG6);
                AsciiStrHexToBytes(&ConfigData[curr_pos], Prev_OP->ARG6 * 2, (UINT8 *)Prev_OP->ARG7, Prev_OP->ARG6);
                //
            }
            continue;
        }

        //Check which arguments are present, remember to patch the location by PATTERN
        //ARG4 is Patch Length, ARG5 is Patch Buffer
        if (Prev_OP->ID == PATCH && Prev_OP->PatchType != 0 && Prev_OP->ARG3 != 0)
        {

            Prev_OP->ARG4 = AsciiStrLen(&ConfigData[curr_pos]) / 2;
            if (ENG == TRUE) {
              UnicodeSPrint(Log, 512, u"Found %d Bytes\n\r", Prev_OP->ARG4);
              LogToFile(LogFile, Log);
            }
            else
            {
              UnicodeSPrint(Log, 512, u"Найдено %d байт\n\r", Prev_OP->ARG4);
              LogToFile(LogFile, Log);
            }
            Prev_OP->ARG5_Dyn_Alloc = TRUE;
            Prev_OP->ARG5 = (UINT64)AllocateZeroPool(Prev_OP->ARG4);
            AsciiStrHexToBytes(&ConfigData[curr_pos], Prev_OP->ARG4 * 2, (UINT8 *)Prev_OP->ARG5, Prev_OP->ARG4);
            
            AsciiStrToUnicodeStrS(&ConfigData[curr_pos], Pattern16, Prev_OP->ARG4 * 2);
            if (ENG == TRUE) {
              UnicodeSPrint(Log, 512, u"%a", "Patch was set for execution\n\r");
              LogToFile(LogFile, Log);
            }
            else
            {
              UnicodeSPrint(Log, 512, u"Патч добавлен в очередь\n\r");
              LogToFile(LogFile, Log);
            }
            PrintDump(Prev_OP->ARG4, (UINT8 *)Prev_OP->ARG5);
            UnicodeSPrint(Log, 512, u"\n\r");
            LogToFile(LogFile, Log);
            continue;
        }
    }
    FreePool(ConfigData);

    // A leftover from Smokeless
    /*
    PrintOPChain(Start);
    dispatch
    */

    //The actual execution
    struct OP_DATA *next;
    INT64 BaseOffset;

    for (next = Start; next != NULL; next = next->next)
    {
        switch (next->ID)
        {
        case NO_OP:
            UnicodeSPrint(Log,512,u"NOP\n\r");
            break;
        case LOADED:
            if (ENG == TRUE) {
              UnicodeSPrint(Log, 512, u"%a", "Executing Loaded\n\r");
              LogToFile(LogFile, Log);
            }
            else
            {
              Print(L"Выполняется аргумент Loaded\n\r");
              UnicodeSPrint(Log, 512, u"Выполняется аргумент Loaded\n\r");
              LogToFile(LogFile, Log);
            }
            Status = FindLoadedImageFromName(ImageHandle, next->Name, &ImageInfo);
            if (ENG == TRUE) {
              UnicodeSPrint(Log, 512, u"Loaded Image %r -> %x\n\r", Status, ImageInfo->ImageBase);
              LogToFile(LogFile, Log);
            }
            else
            {
              Print(L"Результат попытки поиска: %r\n\r", Status);
              UnicodeSPrint(Log, 512, u"Результат попытки поиска: %r\n\r", Status);
              LogToFile(LogFile, Log);
            }
            break;
        case LOADED_GUID_PE:
            if (ENG == TRUE) {
              UnicodeSPrint(Log, 512, u"%a", "Executing NonamePE\n\r");
              LogToFile(LogFile, Log);
            }
            else
            {
              Print(L"Выполняется аргумент NonamePE\n\r");
              UnicodeSPrint(Log, 512, u"Выполняется аргумент NonamePE\n\r");
              LogToFile(LogFile, Log);
            }
            Status = FindLoadedImageFromGUID(ImageHandle, next->Name, &ImageInfo, EFI_SECTION_PE32);
            if (ENG == TRUE) {
              UnicodeSPrint(Log, 512, u"Loaded Image %r -> %x\n\r", Status, ImageInfo->ImageBase);
              LogToFile(LogFile, Log);
            }
            else
            {
              Print(L"Результат попытки поиска: %r\n\r", Status);
              UnicodeSPrint(Log, 512, u"Результат попытки поиска: %r\n\r", Status);
              LogToFile(LogFile, Log);
            }
            break;
        case LOADED_GUID_TE:
            if (ENG == TRUE) {
              UnicodeSPrint(Log, 512, u"%a", "Executing NonameTE\n\r");
              LogToFile(LogFile, Log);
            }
            else
            {
              Print(L"Выполняется аргумент NonameTE\n\r");
              UnicodeSPrint(Log, 512, u"Выполняется аргумент NonameTE\n\r");
              LogToFile(LogFile, Log);
            }
            Status = FindLoadedImageFromGUID(ImageHandle, next->Name, &ImageInfo, EFI_SECTION_TE);
            if (ENG == TRUE) {
              UnicodeSPrint(Log, 512, u"Loaded Image %r -> %x\n\r", Status, ImageInfo->ImageBase);
              LogToFile(LogFile, Log);
            }
            else
            {
              Print(L"Результат попытки поиска: %r\n\r", Status);
              UnicodeSPrint(Log, 512, u"Результат попытки поиска: %r\n\r", Status);
              LogToFile(LogFile, Log);
            }
            break;
        case LOAD_FS:
            if (ENG == TRUE) {
              UnicodeSPrint(Log, 512, u"%a", "Executing LoadFromFS\n\r");
              LogToFile(LogFile, Log);
            }
            else
            {
              Print(L"Выполняется аргумент LoadFromFS\n\r");
              UnicodeSPrint(Log, 512, u"Выполняется аргумент LoadFromFS\n\r");
              LogToFile(LogFile, Log);
            }
            Status = LoadFS(ImageHandle, next->Name, &ImageInfo, &AppImageHandle);
            if (ENG == TRUE) {
              UnicodeSPrint(Log, 512, u"Loaded Image %r -> %x\n\r", Status, ImageInfo->ImageBase);
              LogToFile(LogFile, Log);
            }
            else
            {
              Print(L"Ответ драйвера: %r, смещение загрузки: 0x%x\n\r", Status, ImageInfo->ImageBase);
              UnicodeSPrint(Log, 512, u"Ответ драйвера: %r, смещение загрузки: 0x%x\n\r", Status, ImageInfo->ImageBase);
              LogToFile(LogFile, Log);
            }
            // UnicodeSPrint(Log,512,u"\t FileName %a\n\r", next->ARG2); // A leftover from Smokeless
            break;
        case LOAD_FV:
            if (ENG == TRUE) {
              UnicodeSPrint(Log, 512, u"%a", "Executing LoadFromFV\n\r");
              LogToFile(LogFile, Log);
            }
            else
            {
              Print(L"Выполняется аргумент LoadFromFV\n\r");
              UnicodeSPrint(Log, 512, u"Выполняется аргумент LoadFromFV\n\r");
              LogToFile(LogFile, Log);
            }
            Status = LoadFV(ImageHandle, next->Name, &ImageInfo, &AppImageHandle, EFI_SECTION_PE32);
            if (ENG == TRUE) {
              UnicodeSPrint(Log, 512, u"Loaded Image %r -> %x\n\r", Status, ImageInfo->ImageBase);
              LogToFile(LogFile, Log);
            }
            else
            {
              Print(L"Смещение загрузки: 0x%x\n\r", ImageInfo->ImageBase);
              UnicodeSPrint(Log, 512, u"Смещение загрузки: 0x%x\n\r", ImageInfo->ImageBase);
              LogToFile(LogFile, Log);
            }
            break;
        case LOAD_GUID:
            if (ENG == TRUE) {
              UnicodeSPrint(Log, 512, u"%a", "Executing LoadGUIDandSavePE\n\r");
              LogToFile(LogFile, Log);
            }
            else
            {
              Print(L"Выполняется аргумент LoadGUIDandSavePE\n\r");
              UnicodeSPrint(Log, 512, u"Выполняется аргумент LoadGUIDandSavePE\n\r");
              LogToFile(LogFile, Log);
            }
            Status = LoadFVbyGUID(ImageHandle, next->Name, &ImageInfo, &AppImageHandle, EFI_SECTION_PE32, SystemTable);
            if (Status == EFI_NOT_FOUND) {
              if (ENG == TRUE) {
                Print(L"Search result: %r\n\r", Status);
                UnicodeSPrint(Log, 512, u"Search result: %r\n\r", Status);
                LogToFile(LogFile, Log);
              }
              else
              {
                Print(L"Результат поиска: %r\n\r", Status);
                UnicodeSPrint(Log, 512, u"Результат поиска: %r\n\r", Status);
                LogToFile(LogFile, Log);
              }
              break;
            };
            if (ENG == TRUE) {
              UnicodeSPrint(Log, 512, u"Loaded Image %r -> %x\n\r", Status, ImageInfo->ImageBase);
              LogToFile(LogFile, Log);
            }
            else
            {
              Print(L"Смещение загрузки: 0x%x\n\r", ImageInfo->ImageBase);
              UnicodeSPrint(Log, 512, u"Смещение загрузки: 0x%x\n\r", ImageInfo->ImageBase);
              LogToFile(LogFile, Log);
            }
            break;
        case PATCH:
            if (ENG == TRUE) {
              UnicodeSPrint(Log, 512, u"%a", "Executing Patch\n\r");
              LogToFile(LogFile, Log);
              UnicodeSPrint(Log, 512, u"Patching Image Size %x:\n\r", ImageInfo->ImageSize);
              LogToFile(LogFile, Log);
            }
            else
            {
              Print(L"Выполняется аргумент Patch\n\r");
              Print(L"Размер целевого, последнего загруженного драйвера в HEX: 0x%x\n\r", ImageInfo->ImageSize);
              UnicodeSPrint(Log, 512, u"Выполняется аргумент Patch\n\r");
              LogToFile(LogFile, Log);
              UnicodeSPrint(Log, 512, u"Размер целевого, последнего загруженного драйвера в HEX: 0x%x\n\r", ImageInfo->ImageSize);
              LogToFile(LogFile, Log);
            }

            // PrintDump(0x200, (UINT8 *)(LoadedImage->ImageBase));     // A leftover from Smokeless
            if (next->PatchType == PATTERN)
            {
                if (ENG == TRUE) {
                  UnicodeSPrint(Log, 512, u"%a", "Finding Offset\n\r");
                  LogToFile(LogFile, Log);
                }
                else
                {
                  Print(L"Поиск шаблона\n\r");
                  UnicodeSPrint(Log, 512, u"Поиск шаблона\n\r");
                  LogToFile(LogFile, Log);
                }
                Print(L"\n%a\n\n\r", (CHAR8 *)next->Regex); //PrintDump ascii

                BOOLEAN CResult = FALSE; // Comparison Result

                for (UINTN i = 0; i < ImageInfo->ImageSize - next->ARG6; i += 1)
                {
                  //Old imp, no regex
                  CompareMem(((UINT8 *)ImageInfo->ImageBase), (UINT8 *)next->ARG7, next->ARG6);

                  //Regex match
                  RegexMatch(((UINT8 *)ImageInfo->ImageBase) + i, (CHAR8 *)next->Regex, (UINT8)next->ARG6, RegularExpressionProtocol, &CResult);
                  if (CResult != FALSE)
                  {
                    if (ENG == TRUE) {
                      Print(L"Regex: %a\n\r", CResult ? "Worked" : "Failed");
                      UnicodeSPrint(Log, 512, u"Regex: %a\n\r", CResult ? "Worked" : "Failed");
                      LogToFile(LogFile, Log);
                      UnicodeSPrint(Log, 512, u"\rFound\n\r");
                      LogToFile(LogFile, Log);
                    }
                    else
                    {
                      Print(L"РегВыр: %a\n\r", CResult ? "Worked" : "Failed");
                      UnicodeSPrint(Log, 512, u"РегВыр: %a\n\r", CResult ? "Worked" : "Failed");
                      LogToFile(LogFile, Log);
                      Print(L"Место выбрано\n\r");
                      UnicodeSPrint(Log, 512, u"\rМесто выбрано\n\r");
                      LogToFile(LogFile, Log);
                    }

                        next->ARG3 = i; //Save offset to ARG3
                        PrintDump(next->ARG6, (UINT8 *)ImageInfo->ImageBase + next->ARG3);  //The cause of my edit: winraid.level1techs.com/t/89351/6
                        break;
                    }
                }
                if (next->ARG3 == 0xFFFFFFFF) //Stopped near overflow
                {
                    if (ENG == TRUE) {
                      UnicodeSPrint(Log, 512, u"%a", "No Pattern Found\n\r");
                      LogToFile(LogFile, Log);
                    }
                    else
                    {
                      Print(L"Шаблон не найден\n\r");
                      UnicodeSPrint(Log, 512, u"Шаблон не найден\n\r");
                      LogToFile(LogFile, Log);
                    }
                    //goto cleanup; //A leftover from Smokeless
                break;
                }
            }
            if (next->PatchType == REL_POS_OFFSET)
            {
                next->ARG3 = BaseOffset + next->ARG3;
                if (ENG != TRUE) {
                  Print(L"Поиск смещения вперёд\n\r");
                  UnicodeSPrint(Log, 512, u"Поиск смещения вперёд\n\r");
                  LogToFile(LogFile, Log);
                }
            }
            if (next->PatchType == REL_NEG_OFFSET)
            {
                next->ARG3 = BaseOffset - next->ARG3;
                if (ENG != TRUE) {
                  Print(L"Поиск смещения назад\n\r");
                  UnicodeSPrint(Log, 512, u"Поиск смещения назад\n\r");
                  LogToFile(LogFile, Log);
                }
            }
            BaseOffset = next->ARG3;
            if (ENG == TRUE) {
              UnicodeSPrint(Log, 512, u"Offset %x\n\r", next->ARG3);
              LogToFile(LogFile, Log);
            }
            else
            {
              Print(L"Смещение 0x%x\n\r", next->ARG3);
              UnicodeSPrint(Log, 512, u"Смещение 0x%x\n\r", next->ARG3);
              LogToFile(LogFile, Log);
            }
            // PrintDump(next->ARG4+10,ImageInfo->ImageBase + next->ARG3 -5 ); //A leftover from Smokeless
            CopyMem((UINT8 *)ImageInfo->ImageBase + next->ARG3, (UINT8 *)next->ARG5, next->ARG4);
            if (ENG == TRUE) {
              UnicodeSPrint(Log, 512, u"%a", "Patched\n\r");
              LogToFile(LogFile, Log);
            }
            else
            {
              Print(L"Патч выполнен\n\r");
              UnicodeSPrint(Log, 512, u"Патч выполнен\n\r");
              LogToFile(LogFile, Log);
            }
            // PrintDump(next->ARG4+10,ImageInfo->ImageBase + next->ARG3 -5 ); //A leftover from Smokeless
            break;
        case EXEC:
            if (ENG == TRUE) {
              UnicodeSPrint(Log, 512, u"%a", "EXEC\n\r");
              LogToFile(LogFile, Log);
            }
            else
            {
              Print(L"Запуск приложения через 3 секунды\n\r");
              UnicodeSPrint(Log, 512, u"Запуск приложения(аргумент EXEC)\n\r");
              LogToFile(LogFile, Log);
            }
            gBS->Stall(3000000);
            Exec(&AppImageHandle);
            break;
        default:
            break;
        }
    }
    //cleanup: //A leftover from Smokeless
    for (next = Start; next != NULL; next = next->next)
    {
        if (next->Name_Dyn_Alloc)
            FreePool((VOID *)next->Name);
        if (next->PatchType_Dyn_Alloc)
            FreePool((VOID *)next->PatchType);
        if (next->ARG3_Dyn_Alloc)
            FreePool((VOID *)next->ARG3);
        if (next->ARG4_Dyn_Alloc)
            FreePool((VOID *)next->ARG4);
        if (next->ARG5_Dyn_Alloc)
            FreePool((VOID *)next->ARG5);
        if (next->ARG6_Dyn_Alloc)
            FreePool((VOID *)next->ARG6);
        if (next->ARG7_Dyn_Alloc)
            FreePool((VOID *)next->ARG7);
        if (next->Regex_Dyn_Alloc)
            FreePool((VOID *)next->Regex);
    }
    next = Start;
    while (next->next != NULL)
    {
        struct OP_DATA *tmp = next;
        next = next->next;
        FreePool(tmp);
    }
    if (ENG == TRUE) {
      Print(L"\nProgram has ended\n\r");
      UnicodeSPrint(Log, 512, u"\nProgram has ended\n\r");
      LogToFile(LogFile, Log);
    }
    else
    {
      Print(L"\nКонец программы\n\r");
      UnicodeSPrint(Log, 512, u"\nКонец программы\n\r");
      LogToFile(LogFile, Log);
    }
    gBS->Stall(3000000);
    return EFI_SUCCESS;
}
