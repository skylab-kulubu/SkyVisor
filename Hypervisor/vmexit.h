#pragma once
#include "ia32.hpp"
#include <ntddk.h>

struct guest_registers
{
	UINT64 rax;
	UINT64 rcx;
	UINT64 rdx;
	UINT64 rbx;
	UINT64 rsp;
	UINT64 rbp;
	UINT64 rsi;
	UINT64 rdi;
	UINT64 r8;
	UINT64 r9;
	UINT64 r10;
	UINT64 r11;
	UINT64 r12;
	UINT64 r13;
	UINT64 r14;
	UINT64 r15;
};

extern "C" 
{
	int vmexit_handler(guest_registers* guest_regs);
	void vm_resume();

	UINT64 g_guest_rip;
	UINT64 g_guest_rsp;
}

void advance_guest_rip();