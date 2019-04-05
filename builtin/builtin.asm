





default rel

global ___array____ctor_
global ___array___size
global __string__length
global __string__add
global __string__substring
global __string__ord
global __string__parseInt
global __string___ctor_
global getInt
global print
global getString
global toString
global println

extern putchar
extern strcpy
extern sprintf
extern strlen
extern printf
extern __isoc99_scanf
extern __stack_chk_fail
extern strtol
extern memcpy
extern malloc
extern _GLOBAL_OFFSET_TABLE_


SECTION .text   

___array____ctor_:
        push    rbp
        mov     rbp, rsp
        sub     rsp, 48
        mov     qword [rbp-18H], rdi
        mov     qword [rbp-20H], rsi
        mov     qword [rbp-28H], rdx
        mov     rax, qword [rbp-18H]
        mov     qword [rbp-10H], rax
        mov     rax, qword [rbp-10H]
        mov     rdx, qword [rbp-20H]
        mov     qword [rax], rdx
        mov     rax, qword [rbp-20H]
        imul    rax, qword [rbp-28H]
        mov     rdi, rax
        call    malloc
        mov     qword [rbp-8H], rax
        mov     rax, qword [rbp-10H]
        lea     rdx, [rax+8H]
        mov     rax, qword [rbp-8H]
        mov     qword [rdx], rax
        nop
        leave
        ret


___array___size:
        push    rbp
        mov     rbp, rsp
        mov     qword [rbp-8H], rdi
        mov     rax, qword [rbp-8H]
        mov     rax, qword [rax]
        pop     rbp
        ret


__string__length:
        push    rbp
        mov     rbp, rsp
        mov     qword [rbp-8H], rdi
        mov     rax, qword [rbp-8H]
        mov     rax, qword [rax]
        pop     rbp
        ret


__string__add:
        push    rbp
        mov     rbp, rsp
        sub     rsp, 80
        mov     qword [rbp-48H], rdi
        mov     qword [rbp-50H], rsi
        mov     edi, 16
        call    malloc
        mov     qword [rbp-38H], rax
        mov     rax, qword [rbp-48H]
        mov     rdi, rax
        call    __string__length
        mov     qword [rbp-30H], rax
        mov     rax, qword [rbp-50H]
        mov     rdi, rax
        call    __string__length
        mov     qword [rbp-28H], rax
        mov     rdx, qword [rbp-30H]
        mov     rax, qword [rbp-28H]
        add     rax, rdx
        mov     qword [rbp-20H], rax
        mov     rax, qword [rbp-38H]
        mov     rdx, qword [rbp-20H]
        mov     qword [rax], rdx
        mov     rax, qword [rbp-20H]
        mov     rdi, rax
        call    malloc
        mov     qword [rbp-18H], rax
        mov     rax, qword [rbp-38H]
        lea     rdx, [rax+8H]
        mov     rax, qword [rbp-18H]
        mov     qword [rdx], rax
        mov     rax, qword [rbp-48H]
        mov     rax, qword [rax+8H]
        mov     qword [rbp-10H], rax
        mov     rax, qword [rbp-50H]
        mov     rax, qword [rax+8H]
        mov     qword [rbp-8H], rax
        mov     rdx, qword [rbp-30H]
        mov     rcx, qword [rbp-10H]
        mov     rax, qword [rbp-18H]
        mov     rsi, rcx
        mov     rdi, rax
        call    memcpy
        mov     rdx, qword [rbp-18H]
        mov     rax, qword [rbp-30H]
        lea     rcx, [rdx+rax]
        mov     rdx, qword [rbp-28H]
        mov     rax, qword [rbp-8H]
        mov     rsi, rax
        mov     rdi, rcx
        call    memcpy
        mov     rax, qword [rbp-38H]
        leave
        ret


__string__substring:
        push    rbp
        mov     rbp, rsp
        sub     rsp, 80
        mov     qword [rbp-38H], rdi
        mov     qword [rbp-40H], rsi
        mov     qword [rbp-48H], rdx
        mov     edi, 16
        call    malloc
        mov     qword [rbp-28H], rax
        mov     rax, qword [rbp-48H]
        mov     edx, eax
        mov     rax, qword [rbp-40H]
        sub     edx, eax
        mov     eax, edx
        mov     dword [rbp-2CH], eax
        mov     eax, dword [rbp-2CH]
        movsxd  rdx, eax
        mov     rax, qword [rbp-28H]
        mov     qword [rax], rdx
        mov     eax, dword [rbp-2CH]
        cdqe
        mov     rdi, rax
        call    malloc
        mov     qword [rbp-20H], rax
        mov     rax, qword [rbp-28H]
        add     rax, 8
        mov     qword [rbp-18H], rax
        mov     rax, qword [rbp-18H]
        mov     rdx, qword [rbp-20H]
        mov     qword [rax], rdx
        mov     rax, qword [rbp-38H]
        add     rax, 8
        mov     qword [rbp-10H], rax
        mov     rax, qword [rbp-10H]
        mov     rdx, qword [rax]
        mov     rax, qword [rbp-40H]
        add     rax, rdx
        mov     qword [rbp-8H], rax
        mov     eax, dword [rbp-2CH]
        movsxd  rdx, eax
        mov     rcx, qword [rbp-8H]
        mov     rax, qword [rbp-20H]
        mov     rsi, rcx
        mov     rdi, rax
        call    memcpy
        mov     rax, qword [rbp-28H]
        leave
        ret


__string__ord:
        push    rbp
        mov     rbp, rsp
        mov     qword [rbp-8H], rdi
        mov     qword [rbp-10H], rsi
        mov     rax, qword [rbp-8H]
        add     rax, 8
        mov     rdx, qword [rax]
        mov     rax, qword [rbp-10H]
        add     rax, rdx
        movzx   eax, byte [rax]
        movsx   rax, al
        pop     rbp
        ret


__string__parseInt:
        push    rbp
        mov     rbp, rsp
        push    rbx
        sub     rsp, 72
        mov     qword [rbp-48H], rdi


        mov     rax, qword [fs:abs 28H]
        mov     qword [rbp-18H], rax
        xor     eax, eax
        mov     rax, rsp
        mov     rbx, rax
        mov     rax, qword [rbp-48H]
        mov     rax, qword [rax]
        mov     qword [rbp-40H], rax
        mov     rax, qword [rbp-40H]
        add     rax, 1
        mov     rdx, rax
        sub     rdx, 1
        mov     qword [rbp-38H], rdx
        mov     r10, rax
        mov     r11d, 0
        mov     r8, rax
        mov     r9d, 0
        mov     edx, 16
        sub     rdx, 1
        add     rax, rdx
        mov     esi, 16
        mov     edx, 0
        div     rsi
        imul    rax, rax, 16
        sub     rsp, rax
        mov     rax, rsp
        add     rax, 0
        mov     qword [rbp-30H], rax
        mov     rdx, qword [rbp-30H]
        mov     rax, qword [rbp-40H]
        add     rax, rdx
        mov     byte [rax], 0
        mov     rax, qword [rbp-48H]
        add     rax, 8
        mov     qword [rbp-28H], rax
        mov     rax, qword [rbp-28H]
        mov     rax, qword [rax]
        mov     qword [rbp-20H], rax
        mov     rax, qword [rbp-30H]
        mov     rdx, qword [rbp-40H]
        mov     rcx, qword [rbp-20H]
        mov     rsi, rcx
        mov     rdi, rax
        call    memcpy
        mov     rax, qword [rbp-30H]
        mov     edx, 10
        mov     esi, 0
        mov     rdi, rax
        call    strtol
        mov     rsp, rbx
        mov     rcx, qword [rbp-18H]


        xor     rcx, qword [fs:abs 28H]
        jz      L_001
        call    __stack_chk_fail
L_001:  mov     rbx, qword [rbp-8H]
        leave
        ret


__string___ctor_:
        push    rbp
        mov     rbp, rsp
        mov     qword [rbp-8H], rdi
        mov     rax, qword [rbp-8H]
        mov     qword [rax], 0
        nop
        pop     rbp
        ret


getInt:
        push    rbp
        mov     rbp, rsp
        sub     rsp, 16


        mov     rax, qword [fs:abs 28H]
        mov     qword [rbp-8H], rax
        xor     eax, eax
        lea     rax, [rbp-10H]
        mov     rsi, rax
        lea     rdi, [rel L_005]
        mov     eax, 0
        call    __isoc99_scanf
        mov     rax, qword [rbp-10H]
        mov     rdx, qword [rbp-8H]


        xor     rdx, qword [fs:abs 28H]
        jz      L_002
        call    __stack_chk_fail
L_002:  leave
        ret


print:
        push    rbp
        mov     rbp, rsp
        sub     rsp, 48
        mov     qword [rbp-28H], rdi
        mov     rax, qword [rbp-28H]
        mov     rax, qword [rax]
        mov     qword [rbp-18H], rax
        mov     rax, qword [rbp-28H]
        add     rax, 8
        mov     qword [rbp-10H], rax
        mov     rax, qword [rbp-10H]
        mov     rax, qword [rax]
        mov     qword [rbp-8H], rax
        mov     rax, qword [rbp-18H]
        mov     ecx, eax
        mov     rax, qword [rbp-8H]
        mov     rdx, rax
        mov     esi, ecx
        lea     rdi, [rel L_006]
        mov     eax, 0
        call    printf
        nop
        leave
        ret


getString:
        push    rbp
        mov     rbp, rsp
        sub     rsp, 304


        mov     rax, qword [fs:abs 28H]
        mov     qword [rbp-8H], rax
        xor     eax, eax
        mov     edi, 16
        call    malloc
        mov     qword [rbp-128H], rax
        lea     rax, [rbp-110H]
        mov     rsi, rax
        lea     rdi, [rel L_007]
        mov     eax, 0
        call    __isoc99_scanf
        lea     rax, [rbp-110H]
        mov     rdi, rax
        call    strlen
        mov     qword [rbp-120H], rax
        mov     rdx, qword [rbp-120H]
        mov     rax, qword [rbp-128H]
        mov     qword [rax], rdx
        mov     rax, qword [rbp-120H]
        mov     rdi, rax
        call    malloc
        mov     qword [rbp-118H], rax
        mov     rax, qword [rbp-128H]
        lea     rdx, [rax+8H]
        mov     rax, qword [rbp-118H]
        mov     qword [rdx], rax
        mov     rdx, qword [rbp-120H]
        lea     rcx, [rbp-110H]
        mov     rax, qword [rbp-118H]
        mov     rsi, rcx
        mov     rdi, rax
        call    memcpy
        mov     rax, qword [rbp-128H]
        mov     rcx, qword [rbp-8H]


        xor     rcx, qword [fs:abs 28H]
        jz      L_003
        call    __stack_chk_fail
L_003:  leave
        ret


toString:
        push    rbp
        mov     rbp, rsp
        sub     rsp, 80
        mov     qword [rbp-48H], rdi


        mov     rax, qword [fs:abs 28H]
        mov     qword [rbp-8H], rax
        xor     eax, eax
        mov     rdx, qword [rbp-48H]
        lea     rax, [rbp-20H]
        lea     rsi, [rel L_005]
        mov     rdi, rax
        mov     eax, 0
        call    sprintf
        mov     edi, 16
        call    malloc
        mov     qword [rbp-38H], rax
        lea     rax, [rbp-20H]
        mov     rdi, rax
        call    strlen
        mov     qword [rbp-30H], rax
        mov     rax, qword [rbp-38H]
        mov     rdx, qword [rbp-30H]
        mov     qword [rax], rdx
        mov     rax, qword [rbp-30H]
        mov     rdi, rax
        call    malloc
        mov     qword [rbp-28H], rax
        mov     rax, qword [rbp-38H]
        lea     rdx, [rax+8H]
        mov     rax, qword [rbp-28H]
        mov     qword [rdx], rax
        lea     rdx, [rbp-20H]
        mov     rax, qword [rbp-28H]
        mov     rsi, rdx
        mov     rdi, rax
        call    strcpy
        mov     rax, qword [rbp-38H]
        mov     rcx, qword [rbp-8H]


        xor     rcx, qword [fs:abs 28H]
        jz      L_004
        call    __stack_chk_fail
L_004:  leave
        ret


println:
        push    rbp
        mov     rbp, rsp
        sub     rsp, 16
        mov     qword [rbp-8H], rdi
        mov     rax, qword [rbp-8H]
        mov     rdi, rax
        call    print
        mov     edi, 10
        call    putchar
        nop
        leave
        ret



SECTION .data   


SECTION .bss    


SECTION .rodata 

L_005:
        db 25H, 6CH, 64H, 00H

L_006:
        db 25H, 2EH, 2AH, 73H, 00H

L_007:
        db 25H, 73H, 00H


