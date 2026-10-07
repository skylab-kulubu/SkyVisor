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
		int leaf = (UINT32)guest_regs->rax;
		int sub_leaf = (UINT32)guest_regs->rcx;

		if (leaf == 0xDEADBEEF && sub_leaf == 0xDEADBEEF)
		{
			g_guest_rip = 0;
			g_guest_rsp = 0;

			__vmx_vmread(VMCS_GUEST_RSP, &g_guest_rsp);
			__vmx_vmread(VMCS_GUEST_RIP, &g_guest_rip);

			SIZE_T instruction_length{};
			__vmx_vmread(VMCS_VMEXIT_INSTRUCTION_LENGTH, &instruction_length);

			g_guest_rip += instruction_length;
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
	case VMX_EXIT_REASON_EXECUTE_RDMSR:
	{
		UINT64 msr{};
		UINT32 msr_to_read = (UINT32)guest_regs->rcx;
		DbgPrint(DRIVER_DBG "RDMSR:%llX\n", msr_to_read);

		if (msr_to_read <= 0x1FFF || (msr_to_read >= 0xC0000000 && msr_to_read <= 0xC0001FFF))
		{
			msr = __readmsr(msr_to_read);
		}
		guest_regs->rax = (UINT32)(msr & 0xFFFFFFFF);
		guest_regs->rdx = (UINT32)((msr >> 32) & 0xFFFFFFFF);
		advance_guest_rip();

		return 0;
	}
	case VMX_EXIT_REASON_EXECUTE_WRMSR:
	{
		UINT64 msr{};
		UINT32 msr_to_write = (UINT32)guest_regs->rcx;
		DbgPrint(DRIVER_DBG "WRMSR:%llX\n", msr_to_write);

		if (msr_to_write <= 0x1FFF || (msr_to_write >= 0xC0000000 && msr_to_write <= 0xC0001FFF))
		{
			msr = (guest_regs->rax & 0xFFFFFFFF) | guest_regs->rdx << 32;
			__writemsr(msr_to_write, msr);
		}
		advance_guest_rip();

		return 0;
	}
	case VMX_EXIT_REASON_MOV_CR:
	{
		vmx_exit_qualification_mov_cr* mov_cr_qual = (vmx_exit_qualification_mov_cr*)&exit_qualification;

		UINT64* reg = &guest_regs->rax + mov_cr_qual->general_purpose_register; // table 30-3

		switch (mov_cr_qual->access_type)
		{
		case VMX_EXIT_QUALIFICATION_ACCESS_MOV_TO_CR:
		{
			switch (mov_cr_qual->control_register)
			{
			case 0:
			{
				__vmx_vmwrite(VMCS_GUEST_CR0, *reg);
				break;
			}
			case 3:
			{
				//DbgPrint(DRIVER_DBG "Mov to CR3: %llX.\n", *reg);
				__vmx_vmwrite(VMCS_GUEST_CR3, *reg);
				break;
			}
			case 4:
			{
				__vmx_vmwrite(VMCS_GUEST_CR4, *reg);
				break;
			}
			}
			break;
		}
		case VMX_EXIT_QUALIFICATION_ACCESS_MOV_FROM_CR:
		{
			switch (mov_cr_qual->control_register)
			{
			case 0:
			{
				__vmx_vmread(VMCS_GUEST_CR0, reg);
				break;
			}
			case 3:
			{
				//DbgPrint(DRIVER_DBG "Mov from CR3: %llX.\n", *reg);
				__vmx_vmread(VMCS_GUEST_CR3, reg);
				break;
			}
			case 4:
			{
				__vmx_vmread(VMCS_GUEST_CR4, reg);
				break;
			}
			}
			break;
		}
		}

		advance_guest_rip();
		return 0;
	}

	default:
		DbgPrint("Unhandled VM exit reason: %hu\n", basic_reason);
		advance_guest_rip();
		return 0;
	}
}