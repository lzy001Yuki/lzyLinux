#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

#include <IndustryStandard/Acpi.h>  
#include <Guid/Acpi.h>    


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
PrintHeader(
    IN EFI_ACPI_DESCRIPTION_HEADER* Table
)
{
    Print(L"==================\n");
    Print(L"Signature: %d\n", Table->Signature);
    Print(L"Address: 0x%p\n", Table);
    Print(L"Length: %d\n", Table->Length);
    Print(L"CheckSum: %d\n", Table->Checksum);
    Print(L"OemId: ");
    PrintASCII((Table->OemId), 6);
    Print(L"\n");
    Print(L"==================\n");
}

VOID 
EFIAPI
ChangeTable(
    IN EFI_ACPI_DESCRIPTION_HEADER* Previous,
    IN EFI_ACPI_DESCRIPTION_HEADER* New
)
{
    CopyMem(Previous, New, New->Length);
    Previous->Checksum = CalculateCheckSum8((UINT8* )Previous, Previous->Length);
    PrintHeader(Previous);
}


EFI_STATUS
EFIAPI
ChangeACPITable(
    IN UINT32        Signature,
    IN EFI_ACPI_DESCRIPTION_HEADER* NewTable
)
{
    UINT32 i;
    UINT8* RSDPptr;
    for (i = 0; i < gST->NumberOfTableEntries; i++) {
        if (CompareGuid(&gEfiAcpiTableGuid, &(gST->ConfigurationTable[i].VendorGuid))) 
        RSDPptr = (UINT8*)(&gST->ConfigurationTable[i])->VendorTable;
    }
    EFI_ACPI_DESCRIPTION_HEADER *XSDTptr = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)(ReadUnaligned64((UINT64 *)(RSDPptr + 24)));
    if (XSDTptr->Signature == Signature) {
        ChangeTable(XSDTptr, NewTable);
    } else {
        UINT64 *EntryPtr = (UINT64 *)(XSDTptr + 1);
        UINT32 TotalTables = (XSDTptr->Length - 36) / 8;
        UINT32 j;
        for (j = 0; j < TotalTables; j++) {
            EFI_ACPI_DESCRIPTION_HEADER* curTable = (EFI_ACPI_DESCRIPTION_HEADER* )(UINTN) EntryPtr[j];
            if (curTable->Signature == Signature) {
                ChangeTable(curTable, NewTable);
            }
            if (curTable->Signature == EFI_ACPI_6_3_FIXED_ACPI_DESCRIPTION_TABLE_SIGNATURE) {
                // find  DSDT and FACS
                EFI_ACPI_DESCRIPTION_HEADER* DSDTPtr = (EFI_ACPI_DESCRIPTION_HEADER*) (UINTN)((EFI_ACPI_6_3_FIXED_ACPI_DESCRIPTION_TABLE* ) curTable)->Dsdt;
                EFI_ACPI_DESCRIPTION_HEADER* FACSPtr = (EFI_ACPI_DESCRIPTION_HEADER*)(UINTN)((EFI_ACPI_6_3_FIXED_ACPI_DESCRIPTION_TABLE* ) curTable)->FirmwareCtrl;
                if (DSDTPtr->Signature == Signature) {
                    ChangeTable(DSDTPtr, NewTable);
                } else if (FACSPtr->Signature == Signature) {
                    ChangeTable(FACSPtr, NewTable);
                }
            }
        }
    }
    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
HackMain(
    IN EFI_HANDLE        ImageHandle,
    IN EFI_SYSTEM_TABLE  *SystemTable
)
{
    Print(L"-----\n");
    return EFI_SUCCESS;
}