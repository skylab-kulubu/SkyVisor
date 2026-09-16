#pragma once
#include <ntddk.h>
#include "vcpu.h"

struct decoded_segment
{
	UINT16 selector;
	UINT64 base;
	UINT32 limit;
	UINT32 access_rights;
};

UINT32 control_capability_msr(UINT32 standard_msr, UINT32 true_msr);
UINT64 apply_fixed_bits(UINT64 value, UINT32 fixed0_msr, UINT32 fixed1_msr);
UINT32 vmcs_access_rights(UINT32 descriptor_word, bool unusable);
bool decode_segment(UINT16 selector, UINT8* gdt_base, UINT16 gdt_limit, decoded_segment& out);
void write_guest_segment(UINT16 selector, UINT8* gdt_base, UINT16 gdt_limit, UINT64 selector_field, UINT64 limit_field, UINT64 access_rights_field, UINT64 base_field);
UINT32 adjust_controls(UINT32 requested, UINT32 capability_msr);

bool setup_vmcs(vcpu* v_cpu);

extern "C" 
{
	UINT16 get_cs();
	UINT16 get_ds();
	UINT16 get_es();
	UINT16 get_fs();
	UINT16 get_gs();
	UINT16 get_ss();
	UINT16 get_tr();
	UINT16 get_ldtr();

	UINT64 get_gdt_base();
	UINT16 get_gdt_limit();
	UINT64 get_idt_base();
	UINT16 get_idt_limit();
	UINT64 get_rflags();

	void asm_vmexit_handler();
}


