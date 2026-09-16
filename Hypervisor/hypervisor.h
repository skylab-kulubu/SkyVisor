#pragma once
#include "vcpu.h"
#include <ntddk.h>

#define DRIVER_POOLTAG 0x56594B53 //VYKS
#define DRIVER_DBG "[SkyVisor] "

struct hypervisor
{
	ULONG cpu_count;
	vcpu* vcpus;
};

bool init();

extern hypervisor g_hv;

extern  "C" 
{
	unsigned char asm_vmx_launch();
}