#pragma once

#include <ntddk.h>
extern NTSTATUS jjAttachDeviceByName(PDRIVER_OBJECT driver_object, LPCWSTR device_name,
	PDEVICE_OBJECT* ppobj, PDEVICE_OBJECT* ppTopDev);
extern NTSTATUS jjAttachDeviceByPointer(PDRIVER_OBJECT driver_object, PDEVICE_OBJECT target_object,
	PDEVICE_OBJECT* pptarget_object, PDEVICE_OBJECT* pptop_dev);
extern NTSTATUS jjCopyFile(PUNICODE_STRING dstFilename, PUNICODE_STRING srcFilename);

