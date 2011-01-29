/*++

This filter driver was adopted from the MouFiltr example in the 
Windows DDK version 3790.1830.

These are the structures and defines that are used in the
keyboard class driver, mouse class driver, and keyboard/mouse port
driver examples from the DDKs


File: kbdmou.h
Last Modified: 2005-August-30

--*/

#ifndef _KBDMOU_
#define _KBDMOU_

#include <ntddmou.h>

//
// Define the mouse port device name strings.
//

#define DD_POINTER_PORT_DEVICE_NAME     "\\Device\\PointerPort"
#define DD_POINTER_PORT_DEVICE_NAME_U  L"\\Device\\PointerPort"
#define DD_POINTER_PORT_BASE_NAME_U    L"PointerPort"

//
// Define the keyboard/mouse class device name strings.
//

#define DD_POINTER_CLASS_BASE_NAME_U    L"PointerClass"

//
// Define the keyboard/mouse resource class names.
//

#define DD_POINTER_RESOURCE_CLASS_NAME_U              L"Pointer"
//
// Define the maximum number of pointer/keyboard port names the port driver
// will use in an attempt to IoCreateDevice.
//

#define POINTER_PORTS_MAXIMUM  8

//
// Define the port connection data structure.
//

typedef struct _CONNECT_DATA {
    IN PDEVICE_OBJECT ClassDeviceObject;
    IN PVOID ClassService;
} CONNECT_DATA, *PCONNECT_DATA;

//
// Define the service callback routine's structure.
//

typedef
VOID
(*PSERVICE_CALLBACK_ROUTINE) (
    IN PVOID NormalContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2,
    IN OUT PVOID SystemArgument3
    );

//
// WMI structures returned by port drivers
//
#include <wmidata.h>

//
// NtDeviceIoControlFile internal IoControlCode values for mouse device.
//


#define IOCTL_INTERNAL_MOUSE_CONNECT    CTL_CODE(FILE_DEVICE_MOUSE, 0x0080, METHOD_NEITHER, FILE_ANY_ACCESS)
#define IOCTL_INTERNAL_MOUSE_DISCONNECT CTL_CODE(FILE_DEVICE_MOUSE, 0x0100, METHOD_NEITHER, FILE_ANY_ACCESS)
#define IOCTL_INTERNAL_MOUSE_ENABLE     CTL_CODE(FILE_DEVICE_MOUSE, 0x0200, METHOD_NEITHER, FILE_ANY_ACCESS)
#define IOCTL_INTERNAL_MOUSE_DISABLE    CTL_CODE(FILE_DEVICE_MOUSE, 0x0400, METHOD_NEITHER, FILE_ANY_ACCESS)

//
// Error log definitions (specific to the keyboard/mouse) for DumpData[0]
// in the IO_ERROR_LOG_PACKET.
//
//     DumpData[1] <= hardware port/register
//     DumpData[2] <= {command byte || expected response byte}
//     DumpData[3] <= {command's parameter byte || actual response byte}
//
//

#define KBDMOU_COULD_NOT_SEND_COMMAND  0x0000
#define KBDMOU_COULD_NOT_SEND_PARAM    0x0001
#define KBDMOU_NO_RESPONSE             0x0002
#define KBDMOU_INCORRECT_RESPONSE      0x0004

//
// Define the base values for the error log packet's UniqueErrorValue field.
//

#define I8042_ERROR_VALUE_BASE        1000
#define INPORT_ERROR_VALUE_BASE       2000
#define SERIAL_MOUSE_ERROR_VALUE_BASE 3000

#endif // _KBDMOU_

