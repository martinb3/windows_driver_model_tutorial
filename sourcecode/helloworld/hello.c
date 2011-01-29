#include <ntddk.h> 

void DriverUnload(PDRIVER_OBJECT pDriverObject)
{
    DbgPrint("Driver unloading\n");
}


NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) 
{
    DbgPrint("Hello, World\n");
    DriverObject->DriverUnload = DriverUnload;
    return STATUS_SUCCESS; 
}

