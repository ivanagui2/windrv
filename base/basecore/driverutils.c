#include <ntddk.h>
#include "driverutils.h"

NTSTATUS jjAttachDeviceByPointer(PDRIVER_OBJECT driver_object, PDEVICE_OBJECT target_object,
	PDEVICE_OBJECT* ppfilter_object, PDEVICE_OBJECT* pptop_dev)
{
	if ( !target_object || !ppfilter_object || !pptop_dev) {
		return STATUS_INVALID_PARAMETER;
	}

	PDEVICE_OBJECT filter_object;
	NTSTATUS status = STATUS_SUCCESS;

	status = IoCreateDevice(driver_object, 0, NULL,
		target_object->DeviceType, 0, FALSE, &filter_object);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	if (target_object->Flags & DO_BUFFERED_IO) {
		filter_object->Flags |= DO_BUFFERED_IO;
	}
	if (target_object->Flags & DO_DIRECT_IO) {
		filter_object->Flags |= DO_DIRECT_IO;
	}
	if (target_object->Characteristics & FILE_DEVICE_SECURE_OPEN) {
		filter_object->Characteristics |= FILE_DEVICE_SECURE_OPEN;
	}
	filter_object->Flags |= DO_POWER_PAGABLE;

	*pptop_dev = IoAttachDeviceToDeviceStack(filter_object, target_object);
	if (!*pptop_dev) {
		*ppfilter_object = NULL;
		IoDeleteDevice(filter_object);
		status = STATUS_UNSUCCESSFUL;
		return status;
	} else {
		*ppfilter_object = filter_object;
	}
	filter_object->Flags = filter_object->Flags & ~DO_DEVICE_INITIALIZING;
	return STATUS_SUCCESS;
}

NTSTATUS jjAttachDeviceByName(PDRIVER_OBJECT driver_object, LPCWSTR device_name,
	PDEVICE_OBJECT* ppfilter_object, PDEVICE_OBJECT* pptop_device)
{
	if (!ppfilter_object || !pptop_device || !device_name) {
		return STATUS_INVALID_PARAMETER;
	}

	*ppfilter_object = NULL;
	*pptop_device = NULL;

	NTSTATUS status = STATUS_SUCCESS;
	PDEVICE_OBJECT filter_object;
	status = IoCreateDevice(driver_object, 0, NULL, FILE_DEVICE_UNKNOWN,
		FILE_DEVICE_SECURE_OPEN, FALSE, &filter_object);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	UNICODE_STRING u8_device_name = { 0 };
	RtlInitUnicodeString(&u8_device_name, device_name);
	status = IoAttachDevice(filter_object, &u8_device_name, pptop_device);
	if (!NT_SUCCESS(status)) {
		IoDeleteDevice(filter_object);
	} else {
		if ((*pptop_device)->Flags & DO_BUFFERED_IO) {
			filter_object->Flags |= DO_BUFFERED_IO;
		}
		if ((*pptop_device)->Flags & DO_DIRECT_IO) {
			filter_object->Flags |= DO_DIRECT_IO;
		}
		if ((*pptop_device)->Characteristics & FILE_DEVICE_SECURE_OPEN) {
			filter_object->Characteristics |= FILE_DEVICE_SECURE_OPEN;
		}
		filter_object->Flags |= DO_POWER_PAGABLE;
	}
	*ppfilter_object = filter_object;
	return status;
}

NTSTATUS jjCopyFile(PUNICODE_STRING dstFilename, PUNICODE_STRING srcFilename)
{
	HANDLE src_file_handle = NULL;
	HANDLE dst_file_handle = NULL;
	NTSTATUS status = STATUS_SUCCESS;
	PVOID buffer = NULL;

	{
		OBJECT_ATTRIBUTES oa;
		InitializeObjectAttributes(&oa, srcFilename, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
		IO_STATUS_BLOCK isb;
		status = ZwCreateFile(
			&src_file_handle,
			GENERIC_READ,
			&oa,
			&isb,
			NULL,
			FILE_ATTRIBUTE_NORMAL,
			FILE_SHARE_READ,
			FILE_OPEN,
			FILE_SYNCHRONOUS_IO_NONALERT | FILE_RANDOM_ACCESS | FILE_NON_DIRECTORY_FILE,
			NULL,
			0);
		if (status != STATUS_SUCCESS) {
			goto out;
		}
	}

	{
		OBJECT_ATTRIBUTES oa;
		InitializeObjectAttributes(&oa, dstFilename, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
		IO_STATUS_BLOCK isb;
		status = ZwCreateFile(&dst_file_handle,
			GENERIC_WRITE,
			&oa,
			&isb,
			NULL,
			FILE_ATTRIBUTE_NORMAL,
			0,
			FILE_SUPERSEDE,
			FILE_SYNCHRONOUS_IO_NONALERT | FILE_RANDOM_ACCESS | FILE_NON_DIRECTORY_FILE,
			NULL,
			0);
		if (status != STATUS_SUCCESS) {
			goto out;
		}
	}

	IO_STATUS_BLOCK isb;
	buffer = ExAllocatePool(NonPagedPool, 4 * 1024);
	if (!buffer) {
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto out;
	}

	LARGE_INTEGER offset = { 0 };
	while (1) {
		ULONG len = 4 * 1024;
		status = ZwReadFile(src_file_handle, NULL, NULL, NULL, &isb, buffer, len, &offset, NULL);
		if (!NT_SUCCESS(status)) {
			if (status == STATUS_END_OF_FILE)
				status = STATUS_SUCCESS;
			goto out;
		}
		len = (ULONG)isb.Information;
		status = ZwWriteFile(dst_file_handle, NULL, NULL, NULL, &isb, buffer, len, &offset, NULL);
		if (!NT_SUCCESS(status)) {
			goto out;
		}
		offset.QuadPart += len;
	}

out:
	if (buffer) {
		ExFreePool(buffer);
	}

	if (src_file_handle) {
		ZwClose(src_file_handle);
	}
	if (dst_file_handle) {
		ZwClose(dst_file_handle);
	}
	return status;
}

