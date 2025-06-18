#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/PrintLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <IndustryStandard/Acpi.h>
#include <Protocol/AcpiTable.h>

#define HRTI_SIGNATURE  SIGNATURE_32('H', 'R', 'T', 'I')  // Hardware Runtime Info


#define BOOT_COUNTER_VARIABLE_NAME  L"HardwareInfoBootCounter"
#define BOOT_COUNTER_VARIABLE_GUID  { 0x9ae3c28d, 0x43b5, 0x4b73, { 0xb9, 0x19, 0x3f, 0x5e, 0x99, 0xc8, 0x47, 0x48 } }
EFI_GUID gBootCounterVariableGuid = BOOT_COUNTER_VARIABLE_GUID;

#pragma pack(1)
typedef struct {
  EFI_ACPI_DESCRIPTION_HEADER Header;    
  UINT32                      DeviceId;    // ID
  UINT16                      CpuTemperature; 
  CHAR8                       FirmwareVersion[32];
} HRTI_TABLE;
#pragma pack()

HRTI_TABLE mHrtiTable = {
  {
    HRTI_SIGNATURE,                   
    sizeof(HRTI_TABLE),               
    1,                                
    0,                                
    {'U', 'E', 'F', 'I', 'H', 'W'},   
    SIGNATURE_64('H','W','I','N','F','O','T','B'), 
    1,                               
    SIGNATURE_32('E','D','K','2'),    
    1                                 
  },
  0xAABBCCDD,                                                
  75,                                
  "MyFirmware v1.2.3"                
};

/**
  计算 ACPI 表的校验和。

  @param[in] Buffer   指向 ACPI 表的指针
  @param[in] Size     表的大小
*/
VOID
AcpiPlatformChecksum (
  IN UINT8  *Buffer,
  IN UINTN  Size
  )
{
  UINTN ChecksumOffset;

  ChecksumOffset = OFFSET_OF (EFI_ACPI_DESCRIPTION_HEADER, Checksum);

  // 清零
  Buffer[ChecksumOffset] = 0;

  //更新字段
  Buffer[ChecksumOffset] = CalculateCheckSum8(Buffer, Size);
}

EFI_STATUS
InstallHrtiAcpiTable (
  VOID
  )
{
  EFI_STATUS              Status;
  EFI_ACPI_TABLE_PROTOCOL *AcpiTable;
  UINTN                   TableKey = 0;
  
  // 查找 ACPI 表协议
  Status = gBS->LocateProtocol (
                  &gEfiAcpiTableProtocolGuid,
                  NULL,
                  (VOID **) &AcpiTable
                  );
                  
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "未找到 ACPI 表协议: %r\n", Status));
    return Status;
  }

  AcpiPlatformChecksum (
    (UINT8 *) &mHrtiTable,
    mHrtiTable.Header.Length
    );
  
  Status = AcpiTable->InstallAcpiTable (
                        AcpiTable,
                        &mHrtiTable,
                        mHrtiTable.Header.Length,
                        &TableKey
                        );
                        
  if (EFI_ERROR (Status)) {
    return Status;
  }
  
  
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
RuntimeMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS Status;

  Print (L"硬件信息 ACPI 驱动: 初始化中...\n");
  
  // 安装 ACPI 表
  Status = InstallHrtiAcpiTable();
  if (EFI_ERROR(Status)) {
    Print(L"安装 HRTI ACPI 表失败: %r\n", Status);
    return Status;
  }
  
  Print (L"硬件信息 ACPI 表已成功安装\n");
  Print (L"设备 ID: 0x%08x\n", mHrtiTable.DeviceId);
  Print (L"CPU 温度: %d°C\n", mHrtiTable.CpuTemperature);
  Print (L"固件版本: %a\n", mHrtiTable.FirmwareVersion);

  return EFI_SUCCESS;
}