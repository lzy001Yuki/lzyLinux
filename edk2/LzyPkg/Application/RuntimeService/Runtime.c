#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>

#include <Protocol/AcpiTable.h>
#include <IndustryStandard/Acpi.h> 
#define MY_HARDWARE_INFO_TABLE_SIGNATURE  SIGNATURE_32('H', 'R', 'T', 'I')

typedef struct {
  EFI_ACPI_DESCRIPTION_HEADER Header;
  UINT32                      DeviceId;
  UINT64                      BootCounter; 
  UINT16                      CpuTemperature; 
  CHAR8                       FirmwareVersion[32]; 
} EFI_ACPI_MY_HARDWARE_INFO_TABLE;


static UINT64 mSystemBootCount = 0;

/**
  Helper function to compute ACPI table checksum.
**/
VOID
AcpiPlatformChecksum (
  IN UINT8      *Buffer,
  IN UINTN      Size
  )
{
  UINTN ChecksumOffset;

  ChecksumOffset = OFFSET_OF (EFI_ACPI_DESCRIPTION_HEADER, Checksum);

  // Set checksum to 0 first
  Buffer[ChecksumOffset] = 0;

  // Update checksum value
  Buffer[ChecksumOffset] = CalculateCheckSum8(Buffer, Size);
}


EFI_STATUS
EFIAPI
RuntimeMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                       Status;
  EFI_ACPI_TABLE_PROTOCOL          *AcpiTableProtocol;
  EFI_ACPI_MY_HARDWARE_INFO_TABLE  *MyTable;
  UINTN                            TableKey;


  // 1. Locate ACPI Table Protocol
  Status = gBS->LocateProtocol (
                  &gEfiAcpiTableProtocolGuid,
                  NULL,
                  (VOID **)&AcpiTableProtocol
                  );
  if (EFI_ERROR (Status)) return Status;

  // 2. Allocate memory for our custom ACPI table
  MyTable = AllocateZeroPool (sizeof (EFI_ACPI_MY_HARDWARE_INFO_TABLE));
  if (MyTable == NULL) return EFI_OUT_OF_RESOURCES;
  

  // 3. Populate ACPI table header
  MyTable->Header.Signature = MY_HARDWARE_INFO_TABLE_SIGNATURE;
  MyTable->Header.Length    = sizeof (EFI_ACPI_MY_HARDWARE_INFO_TABLE);
  MyTable->Header.Revision  = 1;
  // OEMID, OEMTableID, etc. can be filled as needed.
  // CopyMem (MyTable->Header.OemId, "MYOEM ", 6);
  // MyTable->Header.OemTableId = SIGNATURE_64('M', 'Y', 'T', 'A', 'B', 'L', 'E', ' ');
  // MyTable->Header.OemRevision = 0x00000001;
  // MyTable->Header.CreatorId = SIGNATURE_32('M', 'S', 'F', 'T'); // Or your creator ID
  // MyTable->Header.CreatorRevision = 0x01000013; // Example

  // 4. Populate custom data fields
  // (This is where you'd actually gather hardware info)
  // For this example, we use hardcoded values.
  mSystemBootCount++; // Simulate incrementing boot count
  MyTable->DeviceId       = 0xAABBCCDD;
  MyTable->BootCounter    = mSystemBootCount;
  MyTable->CpuTemperature = 75; // Degrees Celsius (example)
  AsciiStrCpyS (MyTable->FirmwareVersion, sizeof(MyTable->FirmwareVersion), "MyFirmware v1.2.3");

  // 5. Calculate checksum
  AcpiPlatformChecksum ((UINT8 *)MyTable, MyTable->Header.Length);

  // 6. Install ACPI table
  TableKey = 0;
  Status = AcpiTableProtocol->InstallAcpiTable (
                                AcpiTableProtocol,
                                MyTable,
                                MyTable->Header.Length,
                                &TableKey
                                );
  if (EFI_ERROR (Status)) {
    FreePool (MyTable);
    return Status;
  }
  // Table is installed. OS will find it. We don't free MyTable here as ACPI protocol now owns it.
  // If this driver were to be unloaded (which is rare for this kind),
  // AcpiTableProtocol->UninstallAcpiTable(AcpiTableProtocol, TableKey) would be called.
  
  return EFI_SUCCESS;
}