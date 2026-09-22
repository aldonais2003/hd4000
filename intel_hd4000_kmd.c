#include <ntddk.h>
#include <initguid.h>

#define DEVICE_NAME        L"\\Device\\IntelHD4000Graphics"
#define SYM_LINK_NAME      L"\\DosDevices\\IntelHD4000Graphics"

#define IOCTL_INTEL_EXEC_BATCH       CTL_CODE(FILE_DEVICE_VIDEO, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_INTEL_GET_INFO         CTL_CODE(FILE_DEVICE_VIDEO, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_INTEL_SET_VRAM_SIZE    CTL_CODE(FILE_DEVICE_VIDEO, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_INTEL_SET_DISPLAY_MODE CTL_CODE(FILE_DEVICE_VIDEO, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)

#pragma pack(push, 8)
typedef struct _GPU_INFO {
    ULONG VendorId;
    ULONG DeviceId;
    ULONG GttTotalPages;
    ULONG GttUsedPages;
    ULONGLONG MmioPhysAddr;
} GPU_INFO, *PGPU_INFO;
#pragma pack(pop)

typedef struct _DEVICE_EXTENSION {
    PVOID MmioVirtualAddress;
    ULONG MmioLength;
    UNICODE_STRING SymLinkName;
    FAST_MUTEX Lock;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

static LONG g_ConfiguredVramMB = 2048;

VOID DriverUnload(PDRIVER_OBJECT DriverObject) {
    PDEVICE_OBJECT deviceObject = DriverObject->DeviceObject;
    if (deviceObject != NULL) {
        PDEVICE_EXTENSION devExt = (PDEVICE_EXTENSION)deviceObject->DeviceExtension;
        if (devExt->MmioVirtualAddress != NULL) {
            MmUnmapIoSpace(devExt->MmioVirtualAddress, devExt->MmioLength);
            devExt->MmioVirtualAddress = NULL;
        }
        IoDeleteSymbolicLink(&devExt->SymLinkName);
        IoDeleteDevice(deviceObject);
    }
}

NTSTATUS DispatchCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS DispatchIoctl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PDEVICE_EXTENSION devExt = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    ULONG controlCode = stack->Parameters.DeviceIoControl.IoControlCode;
    ULONG outLength = stack->Parameters.DeviceIoControl.OutputBufferLength;
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
    ULONG_PTR info = 0;

    ExAcquireFastMutex(&devExt->Lock);

    if (controlCode == IOCTL_INTEL_GET_INFO && outLength >= sizeof(GPU_INFO)) {
        PGPU_INFO gpuInfo = (PGPU_INFO)Irp->AssociatedIrp.SystemBuffer;
        gpuInfo->VendorId = 0x8086;
        gpuInfo->DeviceId = 0x0166;
        gpuInfo->GttTotalPages = (ULONG)((g_ConfiguredVramMB * 1024ULL) / 4ULL);
        gpuInfo->GttUsedPages = 16384;
        gpuInfo->MmioPhysAddr = 0xE0000000;
        status = STATUS_SUCCESS;
        info = sizeof(GPU_INFO);
    }

    ExReleaseFastMutex(&devExt->Lock);

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = info;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(RegistryPath);
    NTSTATUS status;
    PDEVICE_OBJECT deviceObject = NULL;
    UNICODE_STRING devName, symName;

    RtlInitUnicodeString(&devName, DEVICE_NAME);
    RtlInitUnicodeString(&symName, SYM_LINK_NAME);

    status = IoCreateDevice(DriverObject, sizeof(DEVICE_EXTENSION), &devName, FILE_DEVICE_VIDEO, FILE_DEVICE_SECURE_OPEN, FALSE, &deviceObject);
    if (!NT_SUCCESS(status)) return status;

    PDEVICE_EXTENSION devExt = (PDEVICE_EXTENSION)deviceObject->DeviceExtension;
    devExt->SymLinkName = symName;
    ExInitializeFastMutex(&devExt->Lock);

    IoCreateSymbolicLink(&symName, &devName);

    DriverObject->MajorFunction[IRP_MJ_CREATE] = DispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]  = DispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchIoctl;
    DriverObject->DriverUnload = DriverUnload;

    return STATUS_SUCCESS;
}
