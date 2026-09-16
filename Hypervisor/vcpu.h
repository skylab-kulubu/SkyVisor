#pragma once
#include <ntddk.h>
#include "ia32.hpp"

#define VMM_STACK_SIZE 0x6000

struct alignas(PAGE_SIZE) vcpu
{
	alignas(PAGE_SIZE) vmxon vmxon_region;
	alignas(PAGE_SIZE) vmcs vmcs_region;
	alignas(PAGE_SIZE) vmx_msr_bitmap msr_bitmap;
	alignas(PAGE_SIZE) UINT8 vmm_stack[VMM_STACK_SIZE];

	UINT64 vmxon_region_phys;
	UINT64 vmcs_region_phys;
	UINT64 msr_bitmap_phys;
	UINT64 eptp;
	bool vmx_enabled;
	bool in_vmx_mode;
	bool vmcs_loaded;
	bool vm_launched;
};

bool virtualize_cpu(vcpu* v_cpu);
void devirtualize_cpu(vcpu* v_cpu);

