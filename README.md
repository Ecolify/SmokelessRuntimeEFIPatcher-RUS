# SREP in English
A copy of the utility by SmokelessCPU, but with a small addition.

# Differences compared to the original
* Added several comments in the source code files.
* Added English translation for on-screen and log messages, log output was adapted for Unicode encoding. The messages themselves are more frequent so that execution progress is better tracked, and users don't get the idea that the program has frozen.
  </br>Switching to Russian is available through the RUS argument (for example, "SREP.efi RUS"). But, as you can guess, then the SREP call needs to be made through the command line in Shell. You can pass the RUS parameter in .nsh files.
  </br>From version 0.2.0, for language switching, the PlatformLang variable needs to be writable.
* The config file no longer has to be named 'SREP_Config.cfg'. Now the patcher chooses the first found file with '.cfg' extension as the config.
* Added 7 new commands: NonamePE, NonameTE, LoadGUIDandSavePE, LoadGUIDandSaveFreeform, UninstallProtocol, Compatibility and Skip.
  </br>When they might be useful - below.
* Added support for abstract characters (regex) for use in pattern strings. Character classes are available [at this link](https://gist.github.com/kaigouthro/e8bad6a2c8df6ff13b8716027a172dc0#3-character-types).
* By default, the Patch - Pattern combination now replaces all occurrences, not just the first one, that match the specified pattern.
  </br>I separated the old Patch implementation into a separate FastPatch command.
* Added ignoring of config lines containing the hash symbol "#". Applicable for writing comments in the file.
  <details>
  <summary><strong>Comment example</strong></summary>
  # Here we choose FilterProtocol</br>
  Op Compatibility</br>
  389F751F-1838-4388-8390-CD8154BD27F8</br>
  </details>

# How to use
1. Compile the executable file yourself or download from the releases page. Current version - 0.2.0.
2. Extract the file to storage and make it bootable.
3. Create a new or copy a ready config to the root.
4. Run.

# Syntax for each of the new commands (with examples)
Operation in "<...>" is optional.

    Op OpName
        GUID
        # LoadGUIDandSaveFreeform command supports up to 2 modifiers, but other commands only 1.
        # Depending on the presence of the second GUID, the operating mode is selected.
        <GUID>
    <Op Patch>
        Argument 1
        Argument 2
        Argument 3
    <End>
    
    # If the driver is not yet in memory.
    <Op Exec>

### Values

    OpName : Patch, LoadGUIDandSavePE, NonamePE, UninstallProtocol, Compatibility and others
    GUID : Driver GUID for search or protocol
    Argument 1 : OFFSET, PATTERN, REL_NEG_OFFSET, REL_POS_OFFSET
    Argument 2 : Modifier for Argument 1 (for example, HEX pattern)
    Argument 3 : HEX patch
    
# Added commands
## LoadGUIDandSavePE, LoadGUIDandSaveFreeform
Load PE, RAW, FREEFORM sections of a module from Firmware Volume by GUID. Applicable when a module has no name, don't use Op LoadFromFV. Rarely for patches, as a second copy of the module is loaded into memory.
</br>That is, the first command is needed for special cases when an App, whose launch initiates BIOS entry, has no UI section.
</br>What makes the commands less useless is saving the module section as a file to flash drive. Can help when no dump method works.
</br>GUID format as in UEFITool.
  <details>
  <summary><strong>LoadGUIDandSavePE</strong></summary>
    
  ```
  Op LoadGUIDandSavePE
  
  # This is SetupUtility
  FE3542FE-C1D3-4EF8-657C-8048606FF670
  ```

  </details>
  <details>
  <summary><strong>LoadGUIDandSaveFreeform</strong></summary>
    
  ```
  Op LoadGUIDandSaveFreeform
  
  # This is SmallLogo with section subtype RAW. GUID from File.
  63819805-67BB-46EF-AA8D-1524A19A01E4


  Op LoadGUIDandSaveFreeform
  
  # This is setupdata. Section subtype FREEFORM has its own GUID, it also needs to be specified, even if they are the same.
  FE612B72-203C-47B1-8560-A66D946EB371
  FE612B72-203C-47B1-8560-A66D946EB371
  ```

  </details>

## NonamePE, NonameTE
Use the same function, with only one passed value difference.

* NonamePE - searches for PE section by GUID;
* NonameTE - searches for TE section by GUID.
</br>Syntax is like LoadGUIDandSavePE, but these commands search in RAM instead of FV.

Works in such a way that first a module is found in FV, but not RAM, according to the entered GUID. Instead of this module's name, its size is put into a separate variable. Then an array is created from modules already loaded into RAM. And the size of each of these modules is compared with the saved value in the mentioned separate variable. When matched, the comparison loop stops, function returns ImageInfo of the current module.

This approach has a disadvantage where **in case of modules of the same size, the first one in order will be selected**.

## UninstallProtocol
Finds all handles with the given protocol and removes this protocol from them one by one.
In case of failure, it will indicate the serial number of the handle in the buffer from which the protocol could not be removed.

## Compatibility
Allows setting a filtering protocol in all search functions. If the specified GUID is random, the program doesn't reject it, but recommended ones are displayed on screen.
<details>
<summary><strong>Recommended output</strong></summary>
  
```
Recommended protocols are:
EFI_FIRMWARE_VOLUME_PROTOCOL_GUID(good for HP Insyde Rev.3)
389F751F-1838-4388-8390-CD8154BD27F8
 
EFI_LEGACY_BIOS_PROTOCOL_GUID(good for Aptio 4, Insyde Rev.3)
DB9A1E3D-45CB-4ABB-853B-E5387FDB2E2D
```

</details>
If Compatibility is not used at all, SREP works as usual. This way support for Insyde Rev. 3 is organized.

## Skip
Allows skipping the specified number of commands (only Op commands are counted) if the previous one completed successfully.

# Todos

    [x] Regex Matching
    [x] Batch Replacement
    [x] Uninstall Protocol
    [x] Insyde Rev. 3 Support
    [x] Rework on-screen and log outputs (make separate IFR package for each language)
    [ ] ?

* Regex and Batch Replacement implemented in tag 0.1.6.
* UninstallProtocol and Insyde Rev. 3 Support implemented in tag 0.1.7.
* Text output reworked in tag 0.2.0.

Versions 0.1.6 and 0.1.7 were removed from the releases page due to a bug. It consisted of the fact that before each next patch command (Op Patch), the array storing offsets of found pattern occurrences was not cleared. Therefore, the very last "Op Patch" always replaced absolutely all occurrences that were found during the SREP session.