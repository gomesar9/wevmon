/*
 * Event Monitor
 * Marcus Botacin
 * Alexandre R Gomes
 * 2018
 */
/* Branch Monitor
* Marcus Botacin
* 2017
* adapted by Alexandre Gomes
*/

#include "device.h"
#include "../io/IO.h"

/* Create Device Routine
* THis driver will work as a FS resource
* You can open a handle to this driver
*/
NTSTATUS CreateDevice(PDRIVER_OBJECT DriverObject)
{
	int i;
	NTSTATUS status;
	PDEVICE_OBJECT dev;
	UNICODE_STRING namestring, linkstring;
	debug("Creating Device V.0.04");

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
	
	char msg[256];
	debug(msg);

	return STATUS_SUCCESS;
}
