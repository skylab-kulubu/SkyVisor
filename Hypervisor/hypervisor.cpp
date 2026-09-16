#include "hypervisor.h"
#include <ntddk.h>
#include "vmx.h"

hypervisor g_hv;

bool init()
{
	RtlSecureZeroMemory(&g_hv, sizeof(g_hv));

	g_hv.cpu_count = KeQueryActiveProcessorCount(nullptr);

	PHYSICAL_ADDRESS lowest_phys{};
	PHYSICAL_ADDRESS highest_phys;
	highest_phys.QuadPart = ~0ull;

	g_hv.vcpus = (vcpu*)MmAllocateContiguousMemorySpecifyCache(sizeof(vcpu) * g_hv.cpu_count, lowest_phys, highest_phys, lowest_phys, MmCached);
	if (!g_hv.vcpus)
	{
		DbgPrint(DRIVER_DBG "Failed to allocate VCPU memory for %u CPUs.\n", g_hv.cpu_count);
		return false;
	}
	RtlSecureZeroMemory(g_hv.vcpus, sizeof(vcpu) * g_hv.cpu_count);

	DbgPrint(DRIVER_DBG "Allocated memory for %u VCPUs.\n", g_hv.cpu_count);

	DbgPrint(DRIVER_DBG "Virtualizing the system.\n");
	if (!virtualize_system())
	{
		DbgPrint(DRIVER_DBG "Failed to virtualize the system.\n");
		return false;
	}

	DbgPrint(DRIVER_DBG "Successfully virtualized the system.\n");

	return true;
}
