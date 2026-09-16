#include "vmx.h"
#include <ntddk.h>
#include <intrin.h>
#include "hypervisor.h"
#include "vmcs.h"

bool is_vmx_supported()
{
	cpuid_eax_01 data{};

	__cpuid((int*)&data, 1);
	if (!data.cpuid_feature_information_ecx.virtual_machine_extensions)
		return false;

	ia32_feature_control_register control{};
	control.flags = __readmsr(IA32_FEATURE_CONTROL);
	if (!control.lock_bit)
	{
		DbgPrint(DRIVER_DBG "IA32_FEATURE_CONTROL lock bit is 0?\n"); // should never really happen, just keeping it for debugging
		control.enable_vmx_outside_smx = true;
		control.lock_bit = true;
		__writemsr(IA32_FEATURE_CONTROL, control.flags);

	}
	else if (!control.enable_vmx_outside_smx)
	{
		DbgPrint(DRIVER_DBG "VMX locked off in BIOS.\n");
		return false;
	}

	return true;
}

bool virtualize_system()
{
	if (!is_vmx_supported())
	{
		DbgPrint(DRIVER_DBG "VMX is not supported.\n");
		return false;
	}

	for (SIZE_T i = 0; i < g_hv.cpu_count; ++i)
	{
		KAFFINITY original = KeSetSystemAffinityThreadEx(1ULL << i);

		DbgPrint(DRIVER_DBG "Virtualizing CPU %llu.\n", i);
		if (!virtualize_cpu(&g_hv.vcpus[i]))
		{
			DbgPrint(DRIVER_DBG "Failed to virtualize CPU %llu. Safely unloading.\n", i);
			KeRevertToUserAffinityThreadEx(original);
			for (LONG_PTR j = (LONG_PTR)i - 1; j >= 0; --j)
			{
				KAFFINITY prev = KeSetSystemAffinityThreadEx(1ULL << j);
				DbgPrint(DRIVER_DBG "Devirtualizing CPU %llu\n", j);
				devirtualize_cpu(&g_hv.vcpus[j]);
				KeRevertToUserAffinityThreadEx(prev);
			}
			return false;
		}
		KeRevertToUserAffinityThreadEx(original);
	}

	return true;
}

void devirtualize_system()
{
	DbgPrint(DRIVER_DBG "Terminating VMX.\n");

	for (SIZE_T i = 0; i < g_hv.cpu_count; ++i)
	{
		KAFFINITY affinity_mask = (1ULL << i);
		KAFFINITY prev = KeSetSystemAffinityThreadEx(affinity_mask);

		DbgPrint(DRIVER_DBG "Devirtualizing CPU: %llu\n", (UINT64)i);

		devirtualize_cpu(&g_hv.vcpus[i]);
		KeRevertToUserAffinityThreadEx(prev);
	}

	DbgPrint(DRIVER_DBG "Terminated VMX.\n");
}

void enable_vmxe_bit()
{
	ULONGLONG cr4 = __readcr4();
	cr4 |= CR4_VMX_ENABLE_FLAG;
	__writecr4(cr4);
}

void disable_vmxe_bit()
{
	ULONGLONG cr4 = __readcr4();
	cr4 &= ~CR4_VMX_ENABLE_FLAG;
	__writecr4(cr4);
}

bool enter_vmx(vcpu* v_cpu)
{
	UINT64 cr0 = apply_fixed_bits(__readcr0(), IA32_VMX_CR0_FIXED0, IA32_VMX_CR0_FIXED1);
	UINT64 cr4 = apply_fixed_bits(__readcr4(), IA32_VMX_CR4_FIXED0, IA32_VMX_CR4_FIXED1);

	__writecr0(cr0);
	__writecr4(cr4);

	vmxon* vmxon_region = &v_cpu->vmxon_region;

	ia32_vmx_basic_register basic{};
	basic.flags = __readmsr(IA32_VMX_BASIC);

	vmxon_region->revision_id = basic.vmcs_revision_id;
	vmxon_region->must_be_zero = 0;

	v_cpu->vmxon_region_phys = MmGetPhysicalAddress(vmxon_region).QuadPart;

	int status = __vmx_on(&v_cpu->vmxon_region_phys);
	if (status)
	{
		DbgPrint(DRIVER_DBG "VMXON failed with status %d\n", status);
		return false;
	}

	return true;
}

bool load_vmcs(vcpu* v_cpu)
{
	vmcs* vmcs_region = &v_cpu->vmcs_region;

	ia32_vmx_basic_register basic{};
	basic.flags = __readmsr(IA32_VMX_BASIC);

	vmcs_region->revision_id = basic.vmcs_revision_id;
	vmcs_region->shadow_vmcs_indicator = 0;

	v_cpu->vmcs_region_phys = MmGetPhysicalAddress(vmcs_region).QuadPart;

	int status = __vmx_vmclear(&v_cpu->vmcs_region_phys);
	if (status)
	{
		DbgPrint(DRIVER_DBG "VMCLEAR failed with status %d\n", status);
		return false;
	}

	status = __vmx_vmptrld(&v_cpu->vmcs_region_phys);
	if (status)
	{
		DbgPrint(DRIVER_DBG "VMPTRLD failed with status %d\n", status);
		return false;
	}

	return true;
}
