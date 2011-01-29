/*++

This filter driver was adopted from the MouFiltr example in the 
Windows DDK version 3790.1830.

File: moufiltr.c
Last Modified: 2005-August-30

--*/


#include "moufiltr.h"

NTSTATUS DriverEntry (PDRIVER_OBJECT, PUNICODE_STRING);


// Suggest to the compiler different memory allocation
// settings for different driver functions
#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, MouFilter_AddDevice)
#pragma alloc_text (PAGE, MouFilter_CreateClose)
#pragma alloc_text (PAGE, MouFilter_IoCtl)
#pragma alloc_text (PAGE, MouFilter_InternIoCtl)
#pragma alloc_text (PAGE, MouFilter_PnP)
#pragma alloc_text (PAGE, MouFilter_Power)
#pragma alloc_text (PAGE, MouFilter_Unload)
#endif

NTSTATUS
DriverEntry (
    IN  PDRIVER_OBJECT  DriverObject,
    IN  PUNICODE_STRING RegistryPath
    )
/*++
Routine Description:

    Initialize the entry points of the driver.

--*/
{
    ULONG i;

    UNREFERENCED_PARAMETER (RegistryPath);

	DbgPrint(("MouFilter_DriverEntry() called\n"));
    // 
    // Fill in all the dispatch entry points with the pass through function
    // and the explicitly fill in the functions we are going to intercept
    // 
	for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++) {
        DriverObject->MajorFunction[i] = MouFilter_DispatchPassThrough;
    }

    DriverObject->MajorFunction [IRP_MJ_CREATE] =		MouFilter_CreateClose;
    DriverObject->MajorFunction [IRP_MJ_CLOSE] =        MouFilter_CreateClose;
    DriverObject->MajorFunction [IRP_MJ_PNP] =          MouFilter_PnP;
    DriverObject->MajorFunction [IRP_MJ_POWER] =        MouFilter_Power;
    DriverObject->MajorFunction [IRP_MJ_INTERNAL_DEVICE_CONTROL] = MouFilter_InternIoCtl;

    DriverObject->DriverUnload = MouFilter_Unload;
    DriverObject->DriverExtension->AddDevice = MouFilter_AddDevice;

    return STATUS_SUCCESS;
}

NTSTATUS
MouFilter_AddDevice(
    IN PDRIVER_OBJECT   Driver,
    IN PDEVICE_OBJECT   PDO
    )
{

    PDEVICE_EXTENSION        devExt;
    IO_ERROR_LOG_PACKET      errorLogEntry;
    PDEVICE_OBJECT           device;
    NTSTATUS                 status = STATUS_SUCCESS;

    PAGED_CODE();

	DbgPrint(("MouFilter_AddDevice() called\n"));

    status = IoCreateDevice(Driver,                   
                            sizeof(DEVICE_EXTENSION), 
                            NULL,                    
                            FILE_DEVICE_MOUSE,    
                            0,                   
                            FALSE,              
                            &device            
                            );

    if (!NT_SUCCESS(status)) {
        return (status);
    }

    RtlZeroMemory(device->DeviceExtension, sizeof(DEVICE_EXTENSION));

    devExt = (PDEVICE_EXTENSION) device->DeviceExtension;
    devExt->TopOfStack = IoAttachDeviceToDeviceStack(device, PDO);
    if (devExt->TopOfStack == NULL) {
        IoDeleteDevice(device);
        return STATUS_DEVICE_NOT_CONNECTED; 
    }

    ASSERT(devExt->TopOfStack);

    devExt->Self =          device;
    devExt->PDO =           PDO;
    devExt->DeviceState =   PowerDeviceD0;

    devExt->SurpriseRemoved = FALSE;
    devExt->Removed =         FALSE;
    devExt->Started =         FALSE;

    device->Flags |= (DO_BUFFERED_IO | DO_POWER_PAGABLE);
    device->Flags &= ~DO_DEVICE_INITIALIZING;

    return status;
}

NTSTATUS
MouFilter_Complete(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN PVOID            Context
    )
/*++
Routine Description:

    Generic completion routine that allows the driver to send the irp down the 
    stack, catch it on the way up, and do more processing at the original IRQL.
    
--*/
{
    PKEVENT             event;

    event = (PKEVENT) Context;

	// this is a unique way to "reference" a parameter
	// to avoid compile-time warnings, but still avoid using it
	//
	//It's defined as: #define UNREFERENCED_PARAMETER(P) (P)
    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(Irp);

	DbgPrint(("MouFilter_Complete() called\n"));

    //
    // We could switch on the major and minor functions of the IRP to perform
    // different functions, but we know that Context is an event that needs
    // to be set.
    //
	// Wake this event. We set it previously.
    KeSetEvent(event, 0, FALSE);

    //
    // Allows the event we just woke to do something to the IRP
	// before it gets sent on its way
    //
    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS
MouFilter_CreateClose (
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    )
/*++
Routine Description:

    Maintain a simple count of the creates and closes sent against this device
    
--*/
{
    PIO_STACK_LOCATION  irpStack;
    NTSTATUS            status;
    PDEVICE_EXTENSION   devExt;

    PAGED_CODE();

	DbgPrint(("MouFilter_CreateClose() called\n"));
	
	irpStack = IoGetCurrentIrpStackLocation(Irp);
    devExt = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

    status = Irp->IoStatus.Status;

    switch (irpStack->MajorFunction) {
    case IRP_MJ_CREATE:
    
        if (NULL == devExt->UpperConnectData.ClassService) {
            //
            // No Connection yet.  How can we be enabled?
            //
            status = STATUS_INVALID_DEVICE_STATE;
        }
        else if ( 1 >= InterlockedIncrement(&devExt->EnableCount)) {
            //
            // First time enable here
            //
        }
        else {
            //
            // More than one create was sent down (ignore the rest?)
            //
        }
    
        break;

    case IRP_MJ_CLOSE:

        ASSERT(0 < devExt->EnableCount);
    
        if (0 >= InterlockedDecrement(&devExt->EnableCount)) {
            //
            // successfully closed the device, do any appropriate work here
			// this would be place to free any resources allocated earlier
			// that are still pointed to in the devExt device extension
            //
        }

        break;
    }

    Irp->IoStatus.Status = status;

    //
    // Pass on the create and the close
    //
    return MouFilter_DispatchPassThrough(DeviceObject, Irp);
}

NTSTATUS
MouFilter_DispatchPassThrough(
        IN PDEVICE_OBJECT DeviceObject,
        IN PIRP Irp
        )
/*++
Routine Description:

    Passes a request on to the lower driver.
 

--*/
{
    
	PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);


	DbgPrint(("MouFilter_DispatchPassThrough() called -- "));
	switch(irpStack->MajorFunction) {
		case IRP_MJ_CREATE:
			DbgPrint(("MouFiltr.sys saw IRP_MJ_CREATE\n"));
			break;
		case IRP_MJ_PNP:
			DbgPrint(("MouFiltr.sys saw IRP_MJ_PNP\n"));
			break;
		case IRP_MJ_POWER:
			DbgPrint(("MouFiltr.sys saw IRP_MJ_POWER\n"));
			break;
		case IRP_MJ_READ: 
			DbgPrint(("MouFiltr.sys saw IRP_MJ_READ\n"));
			break;
		case IRP_MJ_WRITE: 
			DbgPrint(("MouFiltr.sys saw IRP_MJ_WRITE\n"));
			break;
		case IRP_MJ_FLUSH_BUFFERS: 
			DbgPrint(("MouFiltr.sys saw IRP_MJ_FLUSH_BUFFERS\n"));
			break;
		case IRP_MJ_QUERY_INFORMATION: 
			DbgPrint(("MouFiltr.sys saw IRP_MJ_QUERY_INFORMATION\n"));
			break;
		case IRP_MJ_SET_INFORMATION: 
			DbgPrint(("MouFiltr.sys saw IRP_MJ_SET_INFORMATION\n"));
			break;
		case IRP_MJ_DEVICE_CONTROL:
			DbgPrint(("MouFiltr.sys saw IRP_MJ_DEVICE_CONTROL\n"));
			break;
		case IRP_MJ_INTERNAL_DEVICE_CONTROL:
			DbgPrint(("MouFiltr.sys saw IRP_MJ_INTERNAL_DEVICE_CONTROL\n"));
			break;
		case IRP_MJ_SYSTEM_CONTROL:
			DbgPrint(("MouFiltr.sys saw IRP_MJ_SYSTEM_CONTROL\n"));
			break;
		case IRP_MJ_CLEANUP:
			DbgPrint(("MouFiltr.sys saw IRP_MJ_CLEANUP\n"));
			break;
		case IRP_MJ_CLOSE:
			DbgPrint(("MouFiltr.sys saw IRP_MJ_CLOSE\n"));
			break;
		case IRP_MJ_SHUTDOWN:
			DbgPrint(("MouFiltr.sys saw IRP_MJ_SHUTDOWN\n"));
			break;
		default:
			DbgPrint(("MouFiltr.sys saw an unknown IRP Major Function Code\n"));
	}
	

    //
    // Pass the IRP to the target
    //
    IoSkipCurrentIrpStackLocation(Irp);
        
    return IoCallDriver(((PDEVICE_EXTENSION) DeviceObject->DeviceExtension)->TopOfStack, Irp);
}           

NTSTATUS
MouFilter_InternIoCtl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

    This routine is the dispatch routine for internal device control requests.
    There are two specific control codes that are of interest:
    
    IOCTL_INTERNAL_MOUSE_CONNECT:
        Store the old context and function pointer and replace it with our own.
        This makes life much simpler than intercepting IRPs sent by the RIT and
        modifying them on the way back up.

	RIT = Raw Input Thread that sends IRPs like IRP_MJ_READ
                                         
Arguments:

    DeviceObject - Pointer to the device object.

    Irp - Pointer to the request packet.

Return Value:

    Status is returned.

--*/
{
	
    PIO_STACK_LOCATION          irpStack;
    PDEVICE_EXTENSION           devExt;
    KEVENT                      event;
    PCONNECT_DATA               connectData;
    
    NTSTATUS                    status = STATUS_SUCCESS;

	DbgPrint(("MouFilter_InternIoCtl() called\n"));

    devExt = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    Irp->IoStatus.Information = 0;
    irpStack = IoGetCurrentIrpStackLocation(Irp);

    switch (irpStack->Parameters.DeviceIoControl.IoControlCode) {

    //
    // Connect a mouse class device driver to the port driver.
    //
    case IOCTL_INTERNAL_MOUSE_CONNECT:
        //
        // Only allow one connection. Check for already-used function pointer.
        //
        if (devExt->UpperConnectData.ClassService != NULL) {
            status = STATUS_SHARING_VIOLATION;
            break;
        }
        else if (irpStack->Parameters.DeviceIoControl.InputBufferLength <
                sizeof(CONNECT_DATA)) {
            //
            // invalid buffer, it must not really be a CONNECT_DATA
            //
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        //
        // Store the original function pointer in the device extension.
        //
        connectData = ((PCONNECT_DATA)
            (irpStack->Parameters.DeviceIoControl.Type3InputBuffer));

        devExt->UpperConnectData = *connectData;

        //
        // Hook into the report chain.  Everytime a mouse packet is reported to
        // the system, MouFilter_ServiceCallback will be called
        //
        connectData->ClassDeviceObject = devExt->Self;
        connectData->ClassService = MouFilter_ServiceCallback;

        break;

    //
    // Disconnect a mouse class device driver from the port driver.
    //
    case IOCTL_INTERNAL_MOUSE_DISCONNECT:

        //
        // Clear the connection parameters in the device extension.
        //
        // devExt->UpperConnectData.ClassDeviceObject = NULL;
        // devExt->UpperConnectData.ClassService = NULL;
		//
		// As the DDK uses this example, it should hopefully not be
		// neccessary for our use. At least we return "not-implemented"...

        status = STATUS_NOT_IMPLEMENTED;
        break;

	//
    // Might want to capture this in the future.  For now, then pass it down
    // the stack.  These queries must be successful for the RIT to communicate
    // with the mouse.
    //
    case IOCTL_MOUSE_QUERY_ATTRIBUTES:
    default:
        break;
    }

    if (!NT_SUCCESS(status)) {
        Irp->IoStatus.Status = status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);

        return status;
    }

    return MouFilter_DispatchPassThrough(DeviceObject, Irp);
}

NTSTATUS
MouFilter_PnP(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

    This routine is the dispatch routine for plug and play irps 

Arguments:

    DeviceObject - Pointer to the device object.

    Irp - Pointer to the request packet.

Return Value:

    Status is returned.

--*/
{

    PDEVICE_EXTENSION           devExt; 
    PIO_STACK_LOCATION          irpStack;
    NTSTATUS                    status = STATUS_SUCCESS;
    KIRQL                       oldIrql;
    KEVENT                      event;        

    PAGED_CODE();

	DbgPrint(("MouFilter_PnP() called\n"));

	devExt = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    irpStack = IoGetCurrentIrpStackLocation(Irp);

    switch (irpStack->MinorFunction) {
    case IRP_MN_START_DEVICE: {

        //
        // The device is starting.
        //
        // We cannot touch the device (send it any non pnp irps) until a
        // start device has been passed down to the lower drivers.
        //

		// prepare to pass this irp to the next stack location for processing
		IoCopyCurrentIrpStackLocationToNext(Irp);

		// set a kernel wait "event" so that this driver doesn't run off
        KeInitializeEvent(&event,
                          NotificationEvent,
                          FALSE
                          );

		// when this IRP is completed, call MouFilter_Complete
        IoSetCompletionRoutine(Irp,
                               (PIO_COMPLETION_ROUTINE) MouFilter_Complete, 
                               &event,
                               TRUE,
                               TRUE,
                               TRUE); // No need for Cancel

		// if the driver below us completes the IRP, we'll get a success
		// otherwise, it will pend the IRP and we'll wait
        status = IoCallDriver(devExt->TopOfStack, Irp);

        if (STATUS_PENDING == status) {
            KeWaitForSingleObject(
               &event,
               Executive, // Waiting for reason of a driver
               KernelMode, // Waiting in kernel mode
               FALSE, // No allert
               NULL); // No timeout
        }

        if (NT_SUCCESS(status) && NT_SUCCESS(Irp->IoStatus.Status)) {
            //
            // As we are successfully now back from our start device
            // we can do work.
            //
            devExt->Started = TRUE;
            devExt->Removed = FALSE;
            devExt->SurpriseRemoved = FALSE;
        }

        //
        // We must now complete the IRP, since we stopped it in the
        // completetion routine with MORE_PROCESSING_REQUIRED.
        //
        Irp->IoStatus.Status = status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);

        break;
    }

    case IRP_MN_SURPRISE_REMOVAL:
        //
        // Same as a remove device, but don't call IoDetach or IoDeleteDevice
        //
        devExt->SurpriseRemoved = TRUE;

        // Remove code here

        IoSkipCurrentIrpStackLocation(Irp);
        status = IoCallDriver(devExt->TopOfStack, Irp);
        break;

    case IRP_MN_REMOVE_DEVICE:
        
        devExt->Removed = TRUE;

        // remove code here
        Irp->IoStatus.Status = STATUS_SUCCESS;

        IoSkipCurrentIrpStackLocation(Irp);
        status = IoCallDriver(devExt->TopOfStack, Irp);
		
		// we must release the device since it wasn't surprise_removal
        IoDetachDevice(devExt->TopOfStack); 
        IoDeleteDevice(DeviceObject);

        break;

    case IRP_MN_QUERY_REMOVE_DEVICE:
    case IRP_MN_QUERY_STOP_DEVICE:
    case IRP_MN_CANCEL_REMOVE_DEVICE:
    case IRP_MN_CANCEL_STOP_DEVICE:
    case IRP_MN_FILTER_RESOURCE_REQUIREMENTS: 
    case IRP_MN_STOP_DEVICE:
    case IRP_MN_QUERY_DEVICE_RELATIONS:
    case IRP_MN_QUERY_INTERFACE:
    case IRP_MN_QUERY_CAPABILITIES:
    case IRP_MN_QUERY_DEVICE_TEXT:
    case IRP_MN_QUERY_RESOURCES:
    case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:
    case IRP_MN_READ_CONFIG:
    case IRP_MN_WRITE_CONFIG:
    case IRP_MN_EJECT:
    case IRP_MN_SET_LOCK:
    case IRP_MN_QUERY_ID:
    case IRP_MN_QUERY_PNP_DEVICE_STATE:
    default:
        //
        // Here the filter driver might modify the behavior of these IRPS
        // Please see PlugPlay DDK Example's documentation for use of these IRPs.
        //
		// this would be place to add new "virtual" functionality or something.
        IoSkipCurrentIrpStackLocation(Irp);
        status = IoCallDriver(devExt->TopOfStack, Irp);
        break;
    }

    return status;
}

NTSTATUS
MouFilter_Power(
    IN PDEVICE_OBJECT    DeviceObject,
    IN PIRP              Irp
    )
/*++

Routine Description:

    This routine is the dispatch routine for power irps   Does nothing except
    record the state of the device.

Arguments:

    DeviceObject - Pointer to the device object.

    Irp - Pointer to the request packet.

Return Value:

    Status is returned.

--*/
{
	
    PIO_STACK_LOCATION  irpStack;
    PDEVICE_EXTENSION   devExt;
    POWER_STATE         powerState;
    POWER_STATE_TYPE    powerType;

    PAGED_CODE();

	DbgPrint(("MouFilter_Power() called\n"));
	
	devExt = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    irpStack = IoGetCurrentIrpStackLocation(Irp);

    powerType = irpStack->Parameters.Power.Type;
    powerState = irpStack->Parameters.Power.State;

    switch (irpStack->MinorFunction) {
    case IRP_MN_SET_POWER:
        if (powerType  == DevicePowerState) {
            devExt->DeviceState = powerState.DeviceState;
        }

    case IRP_MN_QUERY_POWER:
    case IRP_MN_WAIT_WAKE:
    case IRP_MN_POWER_SEQUENCE:
    default:
        break;
    }


	// The PoStartNextPowerIrp routine signals the power manager that the driver is ready to handle the next power IRP.
	// This routine must be called by every driver in the device stack - from the April 2005 MSDN Library
    PoStartNextPowerIrp(Irp);
    IoSkipCurrentIrpStackLocation(Irp);
    return PoCallDriver(devExt->TopOfStack, Irp);
}


VOID
MouFilter_ServiceCallback(
    IN PDEVICE_OBJECT DeviceObject,
    IN PMOUSE_INPUT_DATA InputDataStart,
    IN PMOUSE_INPUT_DATA InputDataEnd,
    IN OUT PULONG InputDataConsumed
    )
/*++

Routine Description:

    Called when there are mouse packets to report to the RIT.  You can do 
    anything you like to the packets.  For instance:
    
    o Drop a packet altogether
    o Mutate the contents of a packet 
    o Insert packets into the stream 
                    
Arguments:

    DeviceObject - Context passed during the connect IOCTL
    
    InputDataStart - First packet to be reported
    
    InputDataEnd - One past the last packet to be reported.  Total number of
                   packets is equal to InputDataEnd - InputDataStart
    
    InputDataConsumed - Set to the total number of packets consumed by the RIT
                        (via the function pointer we replaced in the connect
                        IOCTL)

Return Value:

    Status is returned.

For reference:
		----------------------------------------
		typedef struct MOUSE_INPUT_DATA {
		USHORT  UnitId;
		USHORT  Flags;
		union {
			ULONG  Buttons;
			struct {
				USHORT  ButtonFlags;
				USHORT  ButtonData;
			};
		};
		ULONG  RawButtons;
		LONG  LastX;
		LONG  LastY;
		ULONG  ExtraInformation;
		} MOUSE_INPUT_DATA, *PMOUSE_INPUT_DATA;


		Flags:
			MOUSE_MOVE_RELATIVE The LastX and LastY are set relative to the previous location. 
			MOUSE_MOVE_ABSOLUTE The LastX and LastY values are set to absolute values. 
			MOUSE_VIRTUAL_DESKTOP The mouse coordinates are mapped to the virtual desktop. 
			MOUSE_ATTRIBUTES_CHANGED The mouse attributes have changed. The other data in the structure is not used. 

		ButtonFlags:
			MOUSE_LEFT_BUTTON_DOWN The left mouse button changed to down. 
			MOUSE_LEFT_BUTTON_UP The left mouse button changed to up. 
			MOUSE_RIGHT_BUTTON_DOWN The right mouse button changed to down. 
			MOUSE_RIGHT_BUTTON_UP The right mouse button changed to up. 
			MOUSE_MIDDLE_BUTTON_DOWN The middle mouse button changed to down. 
			MOUSE_MIDDLE_BUTTON_UP The middle mouse button changed to up. 
			MOUSE_BUTTON_4_DOWN The fourth mouse button changed to down. 
			MOUSE_BUTTON_4_UP The fourth mouse button changed to up. 
			MOUSE_BUTTON_5_DOWN The fifth mouse button changed to down. 
			MOUSE_BUTTON_5_UP The fifth mouse button changed to up. 
			MOUSE_WHEEL Mouse wheel data is present. 

		----------------------------------------
--*/
{
    PDEVICE_EXTENSION   devExt;
	PMOUSE_INPUT_DATA	pCursor; // cursor for looping
	LONG temp; // variable for swapping axis

	// if there's at least one input packet, this pointer is good. trust the executive's pointers!
	DbgPrint("MouFilter_ServiceCallback() called for UnitId %hu\n", InputDataStart->UnitId);

    devExt = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

	// this is where we can mangle/delete/add packets
	for (pCursor = InputDataStart; pCursor < InputDataEnd; pCursor++) {
		
		// swap the X and Y axis values
		temp = pCursor->LastX;
		pCursor->LastX = pCursor->LastY;
		pCursor->LastY = temp;

		// print out what the data is
		DbgPrint("Mouse moved X = %li and Y = %li\n", pCursor->LastX, pCursor->LastY);
	}



    // Here we stop playing with the data!
    // UpperConnectData must be called at DISPATCH
    //
    (*(PSERVICE_CALLBACK_ROUTINE) devExt->UpperConnectData.ClassService)(
        devExt->UpperConnectData.ClassDeviceObject,
        InputDataStart,
        InputDataEnd,
        InputDataConsumed
        );
}

VOID
MouFilter_Unload(
   IN PDRIVER_OBJECT Driver
   )
/*++

Routine Description:

   Free all the allocated resources associated with this driver.

Arguments:

   DriverObject - Pointer to the driver object.

Return Value:

   None.

--*/

{
	DbgPrint(("MouFiltr_Unload() called\n"));
    PAGED_CODE();

    UNREFERENCED_PARAMETER(Driver);

    ASSERT(NULL == Driver->DeviceObject);
}


