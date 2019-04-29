





default rel

global __string__add
global __string__substring
global __string__ord
global __string__parseInt
global __string___ctor_
global getInt
global print
global _printInt
global getString
global toString
global println

extern putchar
extern sprintf
extern strlen
extern __isoc99_scanf
extern printf
extern getchar
extern __stack_chk_fail
extern strtol
extern memcpy
extern malloc
extern _GLOBAL_OFFSET_TABLE_


SECTION .text   

__string__add:
        push    rbp
        mov     rbp, rsp
        sub     rsp, 48
        mov     qword [rbp-28H], rdi
        mov     qword [rbp-30H], rsi
        mov     rax, qword [rbp-28H]
        sub     rax, 8
        mov     rax, qword [rax]
        mov     qword [rbp-20H], rax
        mov     rax, qword [rbp-30H]
        sub     rax, 8
        mov     rax, qword [rax]
        mov     qword [rbp-18H], rax
        mov     rdx, qword [rbp-20H]
        mov     rax, qword [rbp-18H]
        add     rax, rdx
        mov     qword [rbp-10H], rax
        mov     rax, qword [rbp-10H]
        add     rax, 8
        mov     rdi, rax
        call    malloc
        mov     qword [rbp-8H], rax
        mov     rdx, qword [rbp-10H]
        mov     rax, qword [rbp-8H]
        mov     qword [rax], rdx
        add     qword [rbp-8H], 8
        mov     rdx, qword [rbp-20H]
        mov     rcx, qword [rbp-28H]
        mov     rax, qword [rbp-8H]
        mov     rsi, rcx
        mov     rdi, rax
        call    memcpy
        mov     rdx, qword [rbp-8H]
        mov     rax, qword [rbp-20H]
        lea     rcx, [rdx+rax]
        mov     rdx, qword [rbp-18H]
        mov     rax, qword [rbp-30H]
        mov     rsi, rax
        mov     rdi, rcx
        call    memcpy
        mov     rax, qword [rbp-8H]
        leave
        ret


__string__substring:
        push    rbp
        mov     rbp, rsp
        sub     rsp, 64
        mov     qword [rbp-28H], rdi
        mov     qword [rbp-30H], rsi
        mov     qword [rbp-38H], rdx
        mov     rax, qword [rbp-38H]
        mov     edx, eax
        mov     rax, qword [rbp-30H]
        sub     edx, eax
        mov     eax, edx
        mov     dword [rbp-14H], eax
        mov     eax, dword [rbp-14H]
        add     eax, 8
        cdqe
        mov     rdi, rax
        call    malloc
        mov     qword [rbp-10H], rax
        mov     eax, dword [rbp-14H]
        movsxd  rdx, eax
        mov     rax, qword [rbp-10H]
        mov     qword [rax], rdx
        add     qword [rbp-10H], 8
        mov     rdx, qword [rbp-30H]
        mov     rax, qword [rbp-28H]
        add     rax, rdx
        mov     qword [rbp-8H], rax
        mov     eax, dword [rbp-14H]
        movsxd  rdx, eax
        mov     rcx, qword [rbp-8H]
        mov     rax, qword [rbp-10H]
        mov     rsi, rcx
        mov     rdi, rax
        call    memcpy
        mov     rax, qword [rbp-10H]
        leave
        ret


__string__ord:
        push    rbp
        mov     rbp, rsp
        mov     qword [rbp-8H], rdi
        mov     qword [rbp-10H], rsi
        mov     rdx, qword [rbp-10H]
        mov     rax, qword [rbp-8H]
        add     rax, rdx
        movzx   eax, byte [rax]
        movsx   rax, al
        pop     rbp
        ret


__string__parseInt:
        push    rbp
        mov     rbp, rsp
        sub     rsp, 304
        mov     qword [rbp-128H], rdi


        mov     rax, qword [fs:abs 28H]
        mov     qword [rbp-8H], rax
        xor     eax, eax
        mov     rax, qword [rbp-128H]
        sub     rax, 8
        mov     rax, qword [rax]
        mov     qword [rbp-118H], rax
        lea     rdx, [rbp-110H]
        mov     rax, qword [rbp-118H]
        add     rax, rdx
        mov     byte [rax], 0
        mov     rdx, qword [rbp-118H]
        mov     rcx, qword [rbp-128H]
        lea     rax, [rbp-110H]
        mov     rsi, rcx
        mov     rdi, rax
        call    memcpy
        lea     rax, [rbp-110H]
        mov     edx, 10
        mov     esi, 0
        mov     rdi, rax
        call    strtol
        mov     rcx, qword [rbp-8H]


        xor     rcx, qword [fs:abs 28H]
        jz      L_001
        call    __stack_chk_fail
L_001:  leave
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
        call    getchar
        mov     byte [rbp-9H], al
        mov     dword [rbp-8H], 0
        jmp     L_004

L_002:  cmp     byte [rbp-9H], 45
        jnz     L_003
        mov     dword [rbp-8H], 1
L_003:  call    getchar
        mov     byte [rbp-9H], al
L_004:  cmp     byte [rbp-9H], 47
        jle     L_002
        cmp     byte [rbp-9H], 57
        jg      L_002
        movsx   eax, byte [rbp-9H]
        sub     eax, 48
        mov     dword [rbp-4H], eax
        call    getchar
        mov     byte [rbp-9H], al
        jmp     L_006

L_005:  mov     edx, dword [rbp-4H]
        mov     eax, edx
        shl     eax, 2
        add     eax, edx
        add     eax, eax
        mov     edx, eax
        movsx   eax, byte [rbp-9H]
        add     eax, edx
        sub     eax, 48
        mov     dword [rbp-4H], eax
        call    getchar
        mov     byte [rbp-9H], al
L_006:  cmp     byte [rbp-9H], 47
        jle     L_007
        cmp     byte [rbp-9H], 57
        jle     L_005
L_007:  cmp     dword [rbp-8H], 0
        jz      L_008
        mov     eax, dword [rbp-4H]
        neg     eax
        jmp     L_009

L_008:  mov     eax, dword [rbp-4H]
L_009:  leave
        ret


print:
        push    rbp
        mov     rbp, rsp
        sub     rsp, 32
        mov     qword [rbp-18H], rdi
        mov     rax, qword [rbp-18H]
        sub     rax, 8
        mov     rax, qword [rax]
        mov     qword [rbp-8H], rax
        mov     rax, qword [rbp-8H]
        mov     ecx, eax
        mov     rax, qword [rbp-18H]
        mov     rdx, rax
        mov     esi, ecx
        lea     rdi, [rel L_012]
        mov     eax, 0
        call    printf
        nop
        leave
        ret


_printInt:
        push    rbp
        mov     rbp, rsp
        sub     rsp, 16
        mov     dword [rbp-4H], edi
        mov     eax, dword [rbp-4H]
        mov     esi, eax
        lea     rdi, [rel L_013]
        mov     eax, 0
        call    printf
        nop
        leave
        ret


getString:
        push    rbp
        mov     rbp, rsp
        sub     rsp, 288


        mov     rax, qword [fs:abs 28H]
        mov     qword [rbp-8H], rax
        xor     eax, eax
        lea     rax, [rbp-110H]
        mov     rsi, rax
        lea     rdi, [rel L_014]
        mov     eax, 0
        call    __isoc99_scanf
        lea     rax, [rbp-110H]
        mov     rdi, rax
        call    strlen
        mov     qword [rbp-120H], rax
        mov     rax, qword [rbp-120H]
        add     rax, 8
        mov     rdi, rax
        call    malloc
        mov     qword [rbp-118H], rax
        mov     rdx, qword [rbp-120H]
        mov     rax, qword [rbp-118H]
        mov     qword [rax], rdx
        add     qword [rbp-118H], 8
        mov     rdx, qword [rbp-120H]
        lea     rcx, [rbp-110H]
        mov     rax, qword [rbp-118H]
        mov     rsi, rcx
        mov     rdi, rax
        call    memcpy
        mov     rax, qword [rbp-118H]
        mov     rcx, qword [rbp-8H]


        xor     rcx, qword [fs:abs 28H]
        jz      L_010
        call    __stack_chk_fail
L_010:  leave
        ret


toString:
        push    rbp
        mov     rbp, rsp
        sub     rsp, 112
        mov     qword [rbp-68H], rdi


        mov     rax, qword [fs:abs 28H]
        mov     qword [rbp-8H], rax
        xor     eax, eax
        mov     rdx, qword [rbp-68H]
        lea     rax, [rbp-50H]
        lea     rsi, [rel L_015]
        mov     rdi, rax
        mov     eax, 0
        call    sprintf
        lea     rax, [rbp-50H]
        mov     rdi, rax
        call    strlen
        mov     qword [rbp-60H], rax
        mov     rax, qword [rbp-60H]
        add     rax, 8
        mov     rdi, rax
        call    malloc
        mov     qword [rbp-58H], rax
        mov     rax, qword [rbp-58H]
        mov     rdx, qword [rbp-60H]
        mov     qword [rax], rdx
        add     qword [rbp-58H], 8
        mov     rdx, qword [rbp-60H]
        lea     rcx, [rbp-50H]
        mov     rax, qword [rbp-58H]
        mov     rsi, rcx
        mov     rdi, rax
        call    memcpy
        mov     rax, qword [rbp-58H]
        mov     rcx, qword [rbp-8H]


        xor     rcx, qword [fs:abs 28H]
        jz      L_011
        call    __stack_chk_fail
L_011:  leave
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

L_012:
        db 25H, 2EH, 2AH, 73H, 00H

L_013:
        db 25H, 64H, 00H

L_014:
        db 25H, 32H, 35H, 36H, 73H, 00H

L_015:
        db 25H, 6CH, 64H, 00H


