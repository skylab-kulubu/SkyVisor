#include "vcpu.h"
#include <ntddk.h>
#include <intrin.h>
#include "ia32.hpp"
#include "hypervisor.h"
#include "vmx.h"
#include "vmcs.h"

bool virtualize_cpu(vcpu* v_cpu)
{
	enable_vmx();
	v_cpu->vmx_enabled = true;

	if (!enter_vmx(v_cpu))
	{
		DbgPrint(DRIVER_DBG "Failed to enter VMX operation.\n");
		devirtualize_cpu(v_cpu);
		return false;
	}
	v_cpu->in_vmx_mode = true;

	if (!load_vmcs(v_cpu))
	{
		DbgPrint(DRIVER_DBG "Failed to load VMCS pointer.\n");
		devirtualize_cpu(v_cpu);
		return false;
	}
	v_cpu->vmcs_loaded = true;
	DbgPrint(DRIVER_DBG "VMCS: 0x%llx (phys: 0x%llx)\n", v_cpu->vmcs_region, v_cpu->vmcs_region_phys);

	RtlSecureZeroMemory((void*)v_cpu->vmm_stack, VMM_STACK_SIZE);

	RtlSecureZeroMemory((void*)&v_cpu->msr_bitmap, PAGE_SIZE);
	v_cpu->msr_bitmap_phys = MmGetPhysicalAddress((void*)&v_cpu->msr_bitmap).QuadPart;

	DbgPrint(DRIVER_DBG "VMXON region: 0x%llx (phys: 0x%llx)\n", v_cpu->vmxon_region, v_cpu->vmxon_region_phys);
	DbgPrint(DRIVER_DBG "VMCS region: 0x%llx (phys: 0x%llx)\n", v_cpu->vmcs_region, v_cpu->vmcs_region_phys);
	DbgPrint(DRIVER_DBG "MSR bitmap: 0x%llx (phys: 0x%llx)\n", v_cpu->msr_bitmap, v_cpu->msr_bitmap_phys);
	DbgPrint(DRIVER_DBG "VMM stack: 0x%llx\n", v_cpu->vmm_stack);

	if (!setup_vmcs(v_cpu))
	{
		DbgPrint(DRIVER_DBG "Failed to setup VMCS.\n");
		devirtualize_cpu(v_cpu);
		return false;
	}

	if (!asm_vmx_launch())
	{
		UINT64 error_code = 0;
		__vmx_vmread(VMCS_VM_INSTRUCTION_ERROR, &error_code);

		DbgPrint(DRIVER_DBG "VMLAUNCH failed, instruction error: 0x%llx\n", error_code);
		devirtualize_cpu(v_cpu);
		return false;
	}
	v_cpu->vm_launched = true;

	return true;
}

void devirtualize_cpu(vcpu* v_cpu)
{
	int regs[4]{};
	if (v_cpu->vm_launched)
	{
		__cpuidex(regs, 0xDEADBEEF, 0xDEADBEEF); // request vmxoff to the vmexit handler
	}

	if (v_cpu->vmx_enabled)
	{
		disable_vmx();
		v_cpu->vmx_enabled = false;
	}
}