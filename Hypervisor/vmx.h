#pragma once
#include "ia32.hpp"
#include "vcpu.h"

bool is_vmx_supported();
bool virtualize_system();
void devirtualize_system();

void enable_vmxe_bit();
void disable_vmxe_bit();
bool enter_vmx(vcpu* v_cpu);
bool load_vmcs(vcpu* v_cpu);


