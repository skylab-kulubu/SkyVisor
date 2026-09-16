#include <ntddk.h>
#include <intrin.h>
#include "vmx.h"
#include "hypervisor.h"


void unload(PDRIVER_OBJECT driver_object)
{
	UNREFERENCED_PARAMETER(driver_object);
	DbgPrint(DRIVER_DBG "Unloading driver.\n");
	devirtualize_system();

	if (g_hv.vcpus)
	{
		MmFreeContiguousMemory(g_hv.vcpus);
		g_hv.vcpus = nullptr;
	}

	DbgPrint(DRIVER_DBG "Driver unloaded.\n");

}

NTSTATUS driver_entry(PDRIVER_OBJECT driver_object, PUNICODE_STRING registry_path)
{
	UNREFERENCED_PARAMETER(registry_path);

	driver_object->DriverUnload = unload;

	DbgPrint(DRIVER_DBG "Initializing the driver.\n");
	if (!init())
	{
		DbgPrint(DRIVER_DBG "Failed to initialize.\n");

		if (g_hv.vcpus)
		{
			MmFreeContiguousMemory(g_hv.vcpus);
			g_hv.vcpus = nullptr;
		}
			
		return STATUS_HV_OPERATION_FAILED;
	}

	DbgPrint(DRIVER_DBG "Successfully initialized the driver.\n");

	return STATUS_SUCCESS;
}