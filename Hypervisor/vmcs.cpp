#include "vmcs.h"
#include "hypervisor.h"
#include <intrin.h>
#include "ia32.hpp"

UINT32 control_capability_msr(UINT32 standard_msr, UINT32 true_msr)
{
	ia32_vmx_basic_register basic{};
	basic.flags = __readmsr(IA32_VMX_BASIC);
	return basic.vmx_controls ? true_msr : standard_msr;
}

UINT64 apply_fixed_bits(UINT64 value, UINT32 fixed0_msr, UINT32 fixed1_msr)
{
	value |= __readmsr(fixed0_msr);
	value &= __readmsr(fixed1_msr);
	return value;
}

UINT32 vmcs_access_rights(UINT32 descriptor_word, bool unusable)
{
	UINT32 ar = 0;
	ar |= (descriptor_word >> 8) & 0xF;
	ar |= ((descriptor_word >> 12) & 1u) << 4;
	ar |= ((descriptor_word >> 13) & 3u) << 5;
	ar |= ((descriptor_word >> 15) & 1u) << 7;
	ar |= ((descriptor_word >> 20) & 1u) << 12;
	ar |= ((descriptor_word >> 21) & 1u) << 13;
	ar |= ((descriptor_word >> 22) & 1u) << 14;
	ar |= ((descriptor_word >> 23) & 1u) << 15;
	if (unusable)
		ar |= 1u << 16;
	return ar;
}

bool decode_segment(UINT16 selector, UINT8* gdt_base, UINT16 gdt_limit, decoded_segment& out)
{
	out = {};
	out.selector = selector;

	if (selector == 0)
	{
		out.access_rights = 1u << 16;
		return true;
	}

	if (selector & SEGMENT_SELECTOR_TABLE_FLAG)
		return false;

	UINT32 offset = (UINT32)(SEGMENT_SELECTOR_INDEX(selector)) * 8u;
	if (offset + sizeof(segment_descriptor_32) - 1 > gdt_limit)
		return false;

	segment_descriptor_32* desc32 = (segment_descriptor_32*)(gdt_base + offset);

	UINT32 word = desc32->flags;
	bool system_segment = ((word >> 12) & 1u) == 0;
	bool granularity = ((word >> 23) & 1u) != 0;

	out.base = desc32->base_address_low
		| (static_cast<UINT64>(word & 0xFF) << 16)
		| (static_cast<UINT64>((word >> 24) & 0xFF) << 24);

	out.limit = desc32->segment_limit_low | (((word >> 16) & 0xFu) << 16);
	if (granularity)
		out.limit = (out.limit << 12) | 0xFFF;

	if (system_segment)
	{
		if (offset + sizeof(segment_descriptor_64) - 1 > gdt_limit)
			return false;

		segment_descriptor_64* desc64 = (segment_descriptor_64*)(gdt_base + offset);
		out.base |= (UINT64)(desc64->base_address_upper) << 32;
	}

	out.access_rights = vmcs_access_rights(word, false);
	return true;
}

void write_guest_segment(UINT16 selector, UINT8* gdt_base, UINT16 gdt_limit, UINT64 selector_field, UINT64 limit_field, UINT64 access_rights_field, UINT64 base_field)
{
	decoded_segment seg{};
	if (!decode_segment(selector, gdt_base, gdt_limit, seg))
	{
		seg.selector = selector;
		seg.access_rights = 1u << 16;
	}

	__vmx_vmwrite(selector_field, seg.selector);
	__vmx_vmwrite(limit_field, seg.limit);
	__vmx_vmwrite(access_rights_field, seg.access_rights);
	__vmx_vmwrite(base_field, seg.base);
}


UINT32 adjust_controls(UINT32 requested, UINT32 capability_msr)
{
	const UINT64 cap = __readmsr(capability_msr);
	const UINT32 allowed0 = (UINT32)(cap);
	const UINT32 allowed1 = (UINT32)(cap >> 32);
	return (requested | allowed0) & allowed1;
}

bool setup_vmcs(vcpu* v_cpu)
{
	UINT16 cs = get_cs();
	UINT16 ds = get_ds();
	UINT16 es = get_es();
	UINT16 fs = get_fs();
	UINT16 gs = get_gs();
	UINT16 ss = get_ss();
	UINT16 tr = get_tr();
	UINT16 ldtr = get_ldtr();

	UINT8* gdt_base = (UINT8*)(get_gdt_base());
	UINT16 gdt_limit = get_gdt_limit();

	__vmx_vmwrite(VMCS_HOST_ES_SELECTOR, es & 0xF8);
	__vmx_vmwrite(VMCS_HOST_CS_SELECTOR, cs & 0xF8);
	__vmx_vmwrite(VMCS_HOST_SS_SELECTOR, ss & 0xF8);
	__vmx_vmwrite(VMCS_HOST_DS_SELECTOR, ds & 0xF8);
	__vmx_vmwrite(VMCS_HOST_FS_SELECTOR, fs & 0xF8);
	__vmx_vmwrite(VMCS_HOST_GS_SELECTOR, gs & 0xF8);
	__vmx_vmwrite(VMCS_HOST_TR_SELECTOR, tr & 0xF8);

	__vmx_vmwrite(VMCS_GUEST_VMCS_LINK_POINTER, ~0ULL);
	__vmx_vmwrite(VMCS_GUEST_DEBUGCTL, __readmsr(IA32_DEBUGCTL));
	__vmx_vmwrite(VMCS_CTRL_TSC_OFFSET, 0);
	__vmx_vmwrite(VMCS_CTRL_PAGEFAULT_ERROR_CODE_MASK, 0);
	__vmx_vmwrite(VMCS_CTRL_PAGEFAULT_ERROR_CODE_MATCH, 0);
	__vmx_vmwrite(VMCS_CTRL_VMEXIT_MSR_STORE_COUNT, 0);
	__vmx_vmwrite(VMCS_CTRL_VMEXIT_MSR_LOAD_COUNT, 0);
	__vmx_vmwrite(VMCS_CTRL_VMENTRY_MSR_LOAD_COUNT, 0);
	__vmx_vmwrite(VMCS_CTRL_VMENTRY_INTERRUPTION_INFORMATION_FIELD, 0);
	__vmx_vmwrite(VMCS_CTRL_EXCEPTION_BITMAP, 0);
	__vmx_vmwrite(VMCS_CTRL_CR3_TARGET_COUNT, 0);

	write_guest_segment(es, gdt_base, gdt_limit,
		VMCS_GUEST_ES_SELECTOR, VMCS_GUEST_ES_LIMIT, VMCS_GUEST_ES_ACCESS_RIGHTS, VMCS_GUEST_ES_BASE);
	write_guest_segment(cs, gdt_base, gdt_limit,
		VMCS_GUEST_CS_SELECTOR, VMCS_GUEST_CS_LIMIT, VMCS_GUEST_CS_ACCESS_RIGHTS, VMCS_GUEST_CS_BASE);
	write_guest_segment(ss, gdt_base, gdt_limit,
		VMCS_GUEST_SS_SELECTOR, VMCS_GUEST_SS_LIMIT, VMCS_GUEST_SS_ACCESS_RIGHTS, VMCS_GUEST_SS_BASE);
	write_guest_segment(ds, gdt_base, gdt_limit,
		VMCS_GUEST_DS_SELECTOR, VMCS_GUEST_DS_LIMIT, VMCS_GUEST_DS_ACCESS_RIGHTS, VMCS_GUEST_DS_BASE);
	write_guest_segment(fs, gdt_base, gdt_limit,
		VMCS_GUEST_FS_SELECTOR, VMCS_GUEST_FS_LIMIT, VMCS_GUEST_FS_ACCESS_RIGHTS, VMCS_GUEST_FS_BASE);
	write_guest_segment(gs, gdt_base, gdt_limit,
		VMCS_GUEST_GS_SELECTOR, VMCS_GUEST_GS_LIMIT, VMCS_GUEST_GS_ACCESS_RIGHTS, VMCS_GUEST_GS_BASE);
	write_guest_segment(ldtr, gdt_base, gdt_limit,
		VMCS_GUEST_LDTR_SELECTOR, VMCS_GUEST_LDTR_LIMIT, VMCS_GUEST_LDTR_ACCESS_RIGHTS, VMCS_GUEST_LDTR_BASE);
	write_guest_segment(tr, gdt_base, gdt_limit,
		VMCS_GUEST_TR_SELECTOR, VMCS_GUEST_TR_LIMIT, VMCS_GUEST_TR_ACCESS_RIGHTS, VMCS_GUEST_TR_BASE);

	__vmx_vmwrite(VMCS_GUEST_FS_BASE, __readmsr(IA32_FS_BASE));
	__vmx_vmwrite(VMCS_GUEST_GS_BASE, __readmsr(IA32_GS_BASE));

	__vmx_vmwrite(VMCS_GUEST_INTERRUPTIBILITY_STATE, 0);
	__vmx_vmwrite(VMCS_GUEST_ACTIVITY_STATE, 0);
	__vmx_vmwrite(VMCS_GUEST_PENDING_DEBUG_EXCEPTIONS, 0);
	__vmx_vmwrite(VMCS_GUEST_DR7, 0x400);

	UINT32 pin_msr = control_capability_msr(IA32_VMX_PINBASED_CTLS, IA32_VMX_TRUE_PINBASED_CTLS);
	UINT32 proc_msr = control_capability_msr(IA32_VMX_PROCBASED_CTLS, IA32_VMX_TRUE_PROCBASED_CTLS);
	UINT32 exit_msr = control_capability_msr(IA32_VMX_EXIT_CTLS, IA32_VMX_TRUE_EXIT_CTLS);
	UINT32 entry_msr = control_capability_msr(IA32_VMX_ENTRY_CTLS, IA32_VMX_TRUE_ENTRY_CTLS);

	UINT32 procbased = adjust_controls(
		IA32_VMX_PROCBASED_CTLS_USE_MSR_BITMAPS_FLAG |
		IA32_VMX_PROCBASED_CTLS_ACTIVATE_SECONDARY_CONTROLS_FLAG,
		proc_msr);

	UINT32 secondary = adjust_controls(
		IA32_VMX_PROCBASED_CTLS2_ENABLE_RDTSCP_FLAG|
		IA32_VMX_PROCBASED_CTLS2_ENABLE_INVPCID_FLAG|
		IA32_VMX_PROCBASED_CTLS2_ENABLE_XSAVES_FLAG|
		IA32_VMX_PROCBASED_CTLS2_ENABLE_USER_WAIT_PAUSE_FLAG,
		IA32_VMX_PROCBASED_CTLS2);

	UINT32 pinbased = adjust_controls(0, pin_msr);

	UINT32 exit_ctls = adjust_controls(
		IA32_VMX_EXIT_CTLS_HOST_ADDRESS_SPACE_SIZE_FLAG |
		IA32_VMX_EXIT_CTLS_ACKNOWLEDGE_INTERRUPT_ON_EXIT_FLAG,
		exit_msr);

	UINT32 entry_ctls = adjust_controls(
		IA32_VMX_ENTRY_CTLS_IA32E_MODE_GUEST_FLAG,
		entry_msr);

	__vmx_vmwrite(VMCS_CTRL_PIN_BASED_VM_EXECUTION_CONTROLS, pinbased);
	__vmx_vmwrite(VMCS_CTRL_PROCESSOR_BASED_VM_EXECUTION_CONTROLS, procbased);
	__vmx_vmwrite(VMCS_CTRL_SECONDARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS, secondary);
	__vmx_vmwrite(VMCS_CTRL_PRIMARY_VMEXIT_CONTROLS, exit_ctls);
	__vmx_vmwrite(VMCS_CTRL_VMENTRY_CONTROLS, entry_ctls);
	__vmx_vmwrite(VMCS_CTRL_MSR_BITMAP_ADDRESS, v_cpu->msr_bitmap_phys);

	UINT64 cr0 = __readcr0();
	UINT64 cr3 = __readcr3();
	UINT64 cr4 = __readcr4();

	__vmx_vmwrite(VMCS_GUEST_CR0, cr0);
	__vmx_vmwrite(VMCS_GUEST_CR3, cr3);
	__vmx_vmwrite(VMCS_GUEST_CR4, cr4);
	__vmx_vmwrite(VMCS_HOST_CR0, cr0);
	__vmx_vmwrite(VMCS_HOST_CR3, cr3);
	__vmx_vmwrite(VMCS_HOST_CR4, cr4);

	__vmx_vmwrite(VMCS_CTRL_CR0_GUEST_HOST_MASK, 0);
	__vmx_vmwrite(VMCS_CTRL_CR4_GUEST_HOST_MASK, 0);
	__vmx_vmwrite(VMCS_CTRL_CR0_READ_SHADOW, cr0);
	__vmx_vmwrite(VMCS_CTRL_CR4_READ_SHADOW, cr4);

	__vmx_vmwrite(VMCS_GUEST_GDTR_BASE, get_gdt_base());
	__vmx_vmwrite(VMCS_GUEST_IDTR_BASE, get_idt_base());
	__vmx_vmwrite(VMCS_GUEST_GDTR_LIMIT, gdt_limit);
	__vmx_vmwrite(VMCS_GUEST_IDTR_LIMIT, get_idt_limit());

	__vmx_vmwrite(VMCS_GUEST_RFLAGS, get_rflags());

	__vmx_vmwrite(VMCS_GUEST_SYSENTER_CS, __readmsr(IA32_SYSENTER_CS));
	__vmx_vmwrite(VMCS_GUEST_SYSENTER_EIP, __readmsr(IA32_SYSENTER_EIP));
	__vmx_vmwrite(VMCS_GUEST_SYSENTER_ESP, __readmsr(IA32_SYSENTER_ESP));
	__vmx_vmwrite(VMCS_HOST_SYSENTER_CS, __readmsr(IA32_SYSENTER_CS));
	__vmx_vmwrite(VMCS_HOST_SYSENTER_EIP, __readmsr(IA32_SYSENTER_EIP));
	__vmx_vmwrite(VMCS_HOST_SYSENTER_ESP, __readmsr(IA32_SYSENTER_ESP));

	decoded_segment tr_seg{};
	if (!decode_segment(tr, gdt_base, gdt_limit, tr_seg))
		return false;

	__vmx_vmwrite(VMCS_HOST_TR_BASE, tr_seg.base);
	__vmx_vmwrite(VMCS_HOST_FS_BASE, __readmsr(IA32_FS_BASE));
	__vmx_vmwrite(VMCS_HOST_GS_BASE, __readmsr(IA32_GS_BASE));
	__vmx_vmwrite(VMCS_HOST_GDTR_BASE, get_gdt_base());
	__vmx_vmwrite(VMCS_HOST_IDTR_BASE, get_idt_base());

	__vmx_vmwrite(VMCS_GUEST_RSP, 0); // set in asm_vmx_launch
	__vmx_vmwrite(VMCS_GUEST_RIP, 0); // set in asm_vmx_launch
	__vmx_vmwrite(VMCS_HOST_RSP, (UINT64)v_cpu->vmm_stack + VMM_STACK_SIZE);
	__vmx_vmwrite(VMCS_HOST_RIP, (UINT64)asm_vmexit_handler);

	return true;
}