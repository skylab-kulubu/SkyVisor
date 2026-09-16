#include "vmexit.h"
#include "hypervisor.h"
#include <intrin.h>

void advance_guest_rip()
{
	UINT64 current_rip = 0;
	UINT64 instruction_length = 0;
	
	__vmx_vmread(VMCS_GUEST_RIP, &current_rip);
	__vmx_vmread(VMCS_VMEXIT_INSTRUCTION_LENGTH, &instruction_length);
	__vmx_vmwrite(VMCS_GUEST_RIP, current_rip + instruction_length);
}

extern "C" void vm_resume()
{
	__vmx_vmresume();

	UINT64 error_code = 0;
	__vmx_vmread(VMCS_VM_INSTRUCTION_ERROR, &error_code);
	__vmx_off();

	DbgPrint(DRIVER_DBG "VMRESUME failed with error code: %llu\n", error_code);
}

extern "C" int vmexit_handler(guest_registers* guest_regs)
{
	UINT64 exit_reason{};
	UINT64 exit_qualification{};

	__vmx_vmread(VMCS_EXIT_REASON, &exit_reason);
	__vmx_vmread(VMCS_EXIT_QUALIFICATION, &exit_qualification);

	UINT16 basic_reason = (UINT16)(exit_reason & 0xFFFF);
	DbgPrint(DRIVER_DBG "VMEXIT basic reason: %hu, qualification: %llu\n", basic_reason, exit_qualification);

	switch (basic_reason)
	{
	case VMX_EXIT_REASON_EXECUTE_CPUID:
	{
		DbgPrint(DRIVER_DBG "Handling CPUID.\n");
		int regs[4]{};
		int leaf = (int)guest_regs->rax;
		int sub_leaf = (int)guest_regs->rcx;

		if (leaf == 0xDEADBEEF && sub_leaf == 0xDEADBEEF)
		{
			g_guest_rip = 0;
			g_guest_rsp = 0;

			__vmx_vmread(VMCS_GUEST_RSP, &g_guest_rsp);
			__vmx_vmread(VMCS_GUEST_RIP, &g_guest_rip);

			SIZE_T instruction_length{};
			__vmx_vmread(VMCS_VMEXIT_INSTRUCTION_LENGTH, &instruction_length);

			g_guest_rip += instruction_length;
			guest_regs->rax = (UINT64)regs[0];
			guest_regs->rbx = (UINT64)regs[1];
			guest_regs->rcx = (UINT64)regs[2];
			guest_regs->rdx = (UINT64)regs[3];
			return 1;
		}

		__cpuidex(regs, leaf, sub_leaf);
		guest_regs->rax = (UINT64)regs[0];
		guest_regs->rbx = (UINT64)regs[1];
		guest_regs->rcx = (UINT64)regs[2];
		guest_regs->rdx = (UINT64)regs[3];

		advance_guest_rip();
		return 0;
	}

	default:
		DbgPrint("Unhandled VM exit reason: %hu\n", basic_reason);
		advance_guest_rip();
		return 0;
	}
}