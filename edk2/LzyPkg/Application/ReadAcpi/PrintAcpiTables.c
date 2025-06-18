#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

#include <IndustryStandard/Acpi.h>  
#include <Guid/Acpi.h>                

/* 
    parse RSDP/RSDT/XSDT/other...(as the same)
    norm the table header
*/

VOID 
EFIAPI
PrintASCII(
    IN  UINT8  *Char,
    IN  UINTN  Length
)
{
    UINT32 i = 0;
    for (i = 0; i < Length; i++) {
        Print(L"%c", Char[i]);
    }
}

VOID
EFIAPI
ParseAcpiHeader(
    IN  VOID     *TablePointer,
    OUT  UINT8   *Signature,
    OUT  UINT32  *Length,
    OUT  UINT8   *CheckSum,
    OUT  UINT8   *OemId
)
{
    UINT32 i;
    for (i = 0; i < 4; i++) {
        Signature[i] = *(UINT8 *) (TablePointer + i);
    }
    *Length = ReadUnaligned32((UINT32 *) (TablePointer + 4));
    *CheckSum = *(UINT8 *)(TablePointer + 9);
    for (i = 10; i < 16; i++) {
        OemId[i - 10] = *(UINT8 *) (TablePointer + i);
    }
}

EFI_STATUS
EFIAPI
PrintTable(
  IN  VOID   *TablePointer,
  IN  UINT8  *Signature,
  IN  UINT32  Length,
  IN  UINT8   CheckSum,
  IN  UINT8   *OemId
) {
    Print(L"Printing ");
    PrintASCII(Signature, 4);
    Print(L"...\n");
    Print(L"Address: 0x%016lx\n", (UINTN)TablePointer);
    // checksum
    UINT8 sum = 0;
    UINTN i;
    for (i = 0; i < Length; i++) {
        sum += ((UINT8 *) TablePointer)[i];
    }
    if (sum != 0) {
        Print(L"Error: Check_sum is not zero\n");
        return EFI_UNSUPPORTED;
    }
    Print(L"Signature:");
    PrintASCII(Signature, 4);
    Print(L"\n");
    Print(L"Length:%d\n", Length);
    Print(L"CheckSum:0x%02x\n", CheckSum);
    Print(L"OemId:");
    PrintASCII(OemId, 6);
    Print(L"\n");
    Print(L"*************END*************\n");
    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
PrintXSDT(
    IN  VOID   *TablePointer,
    IN  UINT8  *Signature,
    IN  UINT32  Length,
    IN  UINT8   CheckSum,
    IN  UINT8   *OemId
)
{
    PrintTable(TablePointer, Signature, Length, CheckSum, OemId);
    UINTN TotalTables = (Length - 36) / 8;
    if ((Length - 36) % 8 != 0) {
        Print(L"Error: No sub tables satisfies\n");
        return EFI_UNSUPPORTED;
    }
    UINTN i;
    EFI_STATUS CurStatus;
    for (i = 0; i < TotalTables; i++) {
        VOID *NextTablePointer = (VOID *)(UINTN)(ReadUnaligned64((UINT64 *) (TablePointer + 36 + i * 8)));
        UINT8 NextSignature[4], NextOemId[6];
        UINT8 NextChecksum;
        UINT32  NextLength;
        ParseAcpiHeader(NextTablePointer, NextSignature, &NextLength, &NextChecksum, NextOemId);
        if (AsciiStrnCmp((CHAR8 *)NextSignature, "XSDT", 4) == 0) CurStatus = PrintXSDT(NextTablePointer, NextSignature, NextLength, NextChecksum, NextOemId);
        else CurStatus = PrintTable(NextTablePointer, NextSignature, NextLength, NextChecksum, NextOemId);
        if (CurStatus != EFI_SUCCESS) return EFI_UNSUPPORTED;
    }
    return EFI_SUCCESS;
}




UINT8*
EFIAPI
GetRSDP(
    VOID
)
{
    UINT32 i;
    for (i = 0; i < gST->NumberOfTableEntries; i++) {
        if (CompareGuid(&gEfiAcpiTableGuid, &(gST->ConfigurationTable[i].VendorGuid))) 
        return (UINT8 *)(&gST->ConfigurationTable[i])->VendorTable;
    }
    return NULL;
}


EFI_STATUS
EFIAPI
PrintAcpiTables (
    IN EFI_HANDLE        ImageHandle,
    IN EFI_SYSTEM_TABLE  *SystemTable
)
{
    Print(L"Start printing ACPI Tables...\n");
    UINT8 *RSDPptr = GetRSDP();
    UINT8 Revision;
    if (RSDPptr == NULL) {
        Print(L"ERROR: RSDP NOT FOUND");
        return EFI_NOT_FOUND;
    } 
    Revision = *(RSDPptr + 15);
    if (Revision < 2) {
        Print (
            L"ERROR: RSDP version less than 2 is not supported.\n"
            );
          return EFI_UNSUPPORTED;
    }
    Print(L"Address of RSDP:%p\n", RSDPptr);
    // acpi 后向兼容
    UINT8 *XSDTptr = (UINT8 *)(UINTN)(ReadUnaligned64((UINT64 *)(RSDPptr + 24)));
    UINT8   Signature[4], OemId[6];
    UINT8 CheckSum;
    UINT32  Length;
    ParseAcpiHeader(XSDTptr, Signature, &Length, &CheckSum, OemId);
    return PrintXSDT(XSDTptr, Signature, Length, CheckSum, OemId);
} 