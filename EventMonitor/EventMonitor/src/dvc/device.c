#include "device.h"
#include "../io/IO.h"

/* 
- V.0.31, 14/05/2018:
  - Supports dynamic configuration of:
    - Collector sleep interval (milliseconds)
  - Lock to buffer containers added
  - Refactor:
	- Interruptions now means sampling count

- V.0.3, 25/05/2018:
  - Supports dynamic configuration of:
    - Threshold
	- Interruptions
	- Events
  - Refactor:
    - Flags in bits of UINT32 structure
- V.0.2a, 03/05/2018:
  - Supports PEBS re-enable
  - Supports all PEBS events listed in Nehalem Performance Monitoring Unit Programming Guide (Enum)
  - Device `Read` now inform counted events
*/
NTSTATUS CreateDevice(PDRIVER_OBJECT DriverObject)
{
	int i;
	NTSTATUS status;
	PDEVICE_OBJECT dev;
	UNICODE_STRING namestring, linkstring;
	debug("Creating Device V.0.31");

	RtlInitUnicodeString(&namestring, DRIVERNAME);
	status = IoCreateDevice(DriverObject, 0, &namestring, FILE_DEVICE_DISK_FILE_SYSTEM, FILE_DEVICE_SECURE_OPEN, FALSE, &dev);
	if (!NT_SUCCESS(status))
	{
		debug("Error Creating Device");
		return status; /* In Case of failing */
	}

	DriverObject->DeviceObject = dev;

	RtlInitUnicodeString(&linkstring, DOSDRIVERNAME);
	status = IoCreateSymbolicLink(&linkstring, &namestring); /* Linking the name */
	if (!NT_SUCCESS(status))
	{
		debug("Error Creating Link");
		IoDeleteDevice(dev); /* Error, remove and unload */
		return status;
	}

	/* registering generic I/O routines */
	for (i = 0; i<IRP_MJ_MAXIMUM_FUNCTION; i++)
	{
		DriverObject->MajorFunction[i] = NotSupported;
	}
	/* registering specific I/O routines */
	DriverObject->MajorFunction[IRP_MJ_CREATE] = Create;
	DriverObject->MajorFunction[IRP_MJ_READ] = Read;
	DriverObject->MajorFunction[IRP_MJ_WRITE] = Write;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = Close;
	
	dev->Flags |= DO_BUFFERED_IO;

	return STATUS_SUCCESS;
}
