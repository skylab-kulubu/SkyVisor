.code _text

EXTERN g_guest_rip:QWORD
EXTERN g_guest_rsp:QWORD
EXTERN vmexit_handler:PROC
EXTERN vm_resume:PROC

PUBLIC asm_vmx_launch
asm_vmx_launch PROC
    mov rax, 681Ch ; VMCS_GUEST_RSP
    vmwrite rax, rsp

    mov rax, 681Eh ; VMCS_GUEST_RIP
    mov rdx, successful_launch
    vmwrite rax, rdx

    vmlaunch

    xor rax, rax
    ret

successful_launch:
    mov rax, 1
    ret
asm_vmx_launch ENDP

PUBLIC get_cs
get_cs PROC
    mov rax, cs
    ret
get_cs ENDP

PUBLIC get_ds
get_ds PROC
    mov rax, ds
    ret
get_ds ENDP

PUBLIC get_es
get_es PROC
    mov rax, es
    ret
get_es ENDP

PUBLIC get_fs
get_fs PROC
    mov rax, fs
    ret
get_fs ENDP

PUBLIC get_gs
get_gs PROC
    mov rax, gs
    ret
get_gs ENDP

PUBLIC get_ss
get_ss PROC
    mov rax, ss
    ret
get_ss ENDP

PUBLIC get_ldtr
get_ldtr PROC
    sldt rax
    ret
get_ldtr ENDP

PUBLIC get_tr
get_tr PROC
    str rax
    ret
get_tr ENDP

PUBLIC get_idt_base
get_idt_base PROC
    LOCAL IDTR[10]:BYTE
    sidt IDTR
    lea rax, IDTR
    mov rax, qword ptr [rax+2]
    ret
get_idt_base ENDP

PUBLIC get_idt_limit
get_idt_limit PROC
    LOCAL IDTR[10]:BYTE
    sidt IDTR
    lea rax, IDTR
    movzx rax, word ptr [rax]
    ret
get_idt_limit ENDP

PUBLIC get_gdt_base
get_gdt_base PROC
    LOCAL GDTR[10]:BYTE
    sgdt GDTR
    lea rax, GDTR
    mov rax, qword ptr [rax+2]
    ret
get_gdt_base ENDP

PUBLIC get_gdt_limit
get_gdt_limit PROC
    LOCAL GDTR[10]:BYTE
    sgdt GDTR
    lea rax, GDTR
    movzx rax, word ptr [rax]
    ret
get_gdt_limit ENDP

PUBLIC get_rflags
get_rflags PROC
    pushfq
    pop rax
    ret
get_rflags ENDP

PUBLIC asm_stop_virtualization
asm_stop_virtualization PROC
    vmxoff

    pop rax
    pop rcx
    pop rdx
    pop rbx
    pop rbp
    pop rbp
    pop rsi
    pop rdi
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    popfq

    mov rsp, g_guest_rsp
    jmp g_guest_rip
asm_stop_virtualization ENDP

PUBLIC asm_vmexit_handler
asm_vmexit_handler PROC
    pushfq
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rdi
    push rsi
    push rbp
    push rbp
    push rbx
    push rdx
    push rcx
    push rax

    mov rcx, rsp
    sub rsp, 28h
    call vmexit_handler
    add rsp, 28h

    cmp al, 1
    jz asm_stop_virtualization

    pop rax
    pop rcx
    pop rdx
    pop rbx
    pop rbp
    pop rbp
    pop rsi
    pop rdi
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    popfq

    sub rsp, 20h
    call vm_resume
    add rsp, 20h
    ret
asm_vmexit_handler ENDP

END

