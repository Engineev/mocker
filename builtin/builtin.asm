





default rel

global __alloc
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
extern __sprintf_chk
extern __isoc99_scanf
extern __printf_chk
extern _IO_getc
extern stdin
extern __stack_chk_fail
extern strtol
extern __memcpy_chk
extern memcpy
extern malloc
extern _GLOBAL_OFFSET_TABLE_


SECTION .text 

__alloc:
        cmp     rdi, 127
        ja      L_002
        mov     rdx, qword [rel used.3358]
        push    rbx
        lea     rbx, [rdx+rdi]
        cmp     rbx, 131072
        ja      L_001
        mov     rax, qword [rel storage.3357]
        mov     qword [rel used.3358], rbx
        pop     rbx
        add     rax, rdx
        ret





ALIGN   8
L_001:  mov     rbx, rdi
        mov     edi, 131072
        call    malloc
        xor     edx, edx
        mov     qword [rel storage.3357], rax
        mov     qword [rel used.3358], rbx
        add     rax, rdx
        pop     rbx
        ret





ALIGN   8
L_002:  jmp     malloc






ALIGN   8

__string__add:
        push    r15
        push    r14
        mov     r15, rdi
        push    r13
        push    r12
        mov     r12, rsi
        push    rbp
        push    rbx
        sub     rsp, 8
        mov     rbp, qword [rdi-8H]
        mov     r13, qword [rsi-8H]
        lea     rbx, [rbp+r13]
        lea     r14, [rbx+8H]
        cmp     r14, 127
        ja      L_006
        mov     rcx, qword [rel used.3358]
        lea     rdx, [r14+rcx]
        cmp     rdx, 131072
        ja      L_005
        mov     rax, qword [rel storage.3357]
L_003:  add     rax, rcx
        mov     qword [rel used.3358], rdx
L_004:  mov     qword [rax], rbx
        lea     rbx, [rax+8H]
        mov     rdx, rbp
        mov     rsi, r15
        mov     rdi, rbx
        call    memcpy
        lea     rdi, [rbx+rbp]
        mov     rdx, r13
        mov     rsi, r12
        call    memcpy
        add     rsp, 8
        mov     rax, rbx
        pop     rbx
        pop     rbp
        pop     r12
        pop     r13
        pop     r14
        pop     r15
        ret





ALIGN   8
L_005:  mov     edi, 131072
        call    malloc
        mov     rdx, r14
        mov     qword [rel storage.3357], rax
        xor     ecx, ecx
        jmp     L_003





ALIGN   16
L_006:  mov     rdi, r14
        call    malloc
        jmp     L_004






ALIGN   8

__string__substring:
        push    r13
        push    r12
        mov     r13, rsi
        push    rbp
        push    rbx
        mov     ebx, edx
        sub     ebx, esi
        mov     r12, rdi
        lea     ebp, [rbx+8H]
        sub     rsp, 8
        movsxd  rbp, ebp
        cmp     rbp, 127
        ja      L_010
        mov     rcx, qword [rel used.3358]
        lea     rdx, [rbp+rcx]
        cmp     rdx, 131072
        ja      L_009
        mov     rax, qword [rel storage.3357]
L_007:  add     rax, rcx
        mov     qword [rel used.3358], rdx
L_008:  lea     rcx, [rax+8H]
        movsxd  rdx, ebx
        lea     rsi, [r12+r13]
        mov     qword [rax], rdx
        mov     rdi, rcx
        call    memcpy
        add     rsp, 8
        pop     rbx
        pop     rbp
        pop     r12
        pop     r13
        ret






ALIGN   16
L_009:  mov     edi, 131072
        call    malloc
        mov     rdx, rbp
        mov     qword [rel storage.3357], rax
        xor     ecx, ecx
        jmp     L_007





ALIGN   16
L_010:  mov     rdi, rbp
        call    malloc
        jmp     L_008






ALIGN   8

__string__ord:
        movsx   rax, byte [rdi+rsi]
        ret







ALIGN   16

__string__parseInt:
        sub     rsp, 280
        mov     rdx, qword [rdi-8H]
        mov     rsi, rdi
        mov     r8, rsp
        mov     ecx, 256
        mov     rdi, r8


        mov     rax, qword [fs:abs 28H]
        mov     qword [rsp+108H], rax
        xor     eax, eax
        mov     byte [rsp+rdx], 0
        call    __memcpy_chk
        xor     esi, esi
        mov     edx, 10
        mov     rdi, rax
        call    strtol
        mov     rcx, qword [rsp+108H]


        xor     rcx, qword [fs:abs 28H]
        jnz     L_011
        add     rsp, 280
        ret


L_011:
        call    __stack_chk_fail





ALIGN   16

__string___ctor_:
        mov     qword [rdi], 0
        ret





ALIGN   16

getInt:
        mov     rdi, qword [rel stdin]
        push    r12
        push    rbp
        push    rbx
        xor     ebp, ebp
        call    _IO_getc
        movsx   ebx, al
        sub     eax, 48
        cmp     al, 9
        jbe     L_013
        mov     r12d, 1




ALIGN   8
L_012:  mov     rdi, qword [rel stdin]
        cmp     bl, 45
        cmove   ebp, r12d
        call    _IO_getc
        movsx   ebx, al
        sub     eax, 48
        cmp     al, 9
        ja      L_012
L_013:  sub     ebx, 48
        jmp     L_015





ALIGN   8
L_014:  lea     eax, [rbx+rbx*4]
        lea     ebx, [rdx+rax*2-30H]
L_015:  mov     rdi, qword [rel stdin]
        call    _IO_getc
        movsx   edx, al
        sub     eax, 48
        cmp     al, 9
        jbe     L_014
        mov     eax, ebx
        neg     eax
        test    ebp, ebp
        cmovne  ebx, eax
        mov     eax, ebx
        pop     rbx
        pop     rbp
        pop     r12
        ret






ALIGN   8

print:
        mov     rdx, qword [rdi-8H]
        lea     rsi, [rel LC0]
        mov     rcx, rdi
        xor     eax, eax
        mov     edi, 1
        jmp     __printf_chk






ALIGN   8

_printInt:
        lea     rsi, [rel LC1]
        mov     edx, edi
        xor     eax, eax
        mov     edi, 1
        jmp     __printf_chk


        nop





ALIGN   16

getString:
        push    r12
        push    rbp
        lea     rdi, [rel LC2]
        push    rbx
        sub     rsp, 272
        mov     rbp, rsp


        mov     rax, qword [fs:abs 28H]
        mov     qword [rsp+108H], rax
        xor     eax, eax
        mov     rsi, rbp
        mov     rbx, rbp
        call    __isoc99_scanf
L_016:  mov     edx, dword [rbx]
        add     rbx, 4
        lea     eax, [rdx-1010101H]
        not     edx
        and     eax, edx
        and     eax, 80808080H
        jz      L_016
        mov     edx, eax
        shr     edx, 16
        test    eax, 8080H
        cmove   eax, edx
        lea     rdx, [rbx+2H]
        mov     edi, eax
        cmove   rbx, rdx
        add     dil, al
        sbb     rbx, 3
        sub     rbx, rbp
        lea     r12, [rbx+8H]
        cmp     r12, 127
        ja      L_020
        mov     rcx, qword [rel used.3358]
        lea     rdx, [r12+rcx]
        cmp     rdx, 131072
        ja      L_019
        mov     rax, qword [rel storage.3357]
L_017:  add     rax, rcx
        mov     qword [rel used.3358], rdx
L_018:  lea     rcx, [rax+8H]
        mov     rsi, rbp
        mov     qword [rax], rbx
        mov     rdx, rbx
        mov     rdi, rcx
        call    memcpy
        mov     rsi, qword [rsp+108H]


        xor     rsi, qword [fs:abs 28H]
        jnz     L_021
        add     rsp, 272
        pop     rbx
        pop     rbp
        pop     r12
        ret





ALIGN   8
L_019:  mov     edi, 131072
        call    malloc
        mov     rdx, r12
        mov     qword [rel storage.3357], rax
        xor     ecx, ecx
        jmp     L_017





ALIGN   16
L_020:  mov     rdi, r12
        call    malloc
        jmp     L_018


L_021:
        call    __stack_chk_fail
        nop
ALIGN   16

toString:
        push    r12
        push    rbp
        lea     rcx, [rel LC3]
        push    rbx
        mov     r8, rdi
        mov     edx, 64
        mov     esi, 1
        sub     rsp, 80
        mov     rbp, rsp


        mov     rax, qword [fs:abs 28H]
        mov     qword [rsp+48H], rax
        xor     eax, eax
        mov     rdi, rbp
        mov     rbx, rbp
        call    __sprintf_chk
L_022:  mov     edx, dword [rbx]
        add     rbx, 4
        lea     eax, [rdx-1010101H]
        not     edx
        and     eax, edx
        and     eax, 80808080H
        jz      L_022
        mov     edx, eax
        shr     edx, 16
        test    eax, 8080H
        cmove   eax, edx
        lea     rdx, [rbx+2H]
        mov     esi, eax
        cmove   rbx, rdx
        add     sil, al
        sbb     rbx, 3
        sub     rbx, rbp
        lea     r12, [rbx+8H]
        cmp     r12, 127
        jg      L_026
        mov     rcx, qword [rel used.3358]
        lea     rdx, [r12+rcx]
        cmp     rdx, 131072
        ja      L_025
        mov     rax, qword [rel storage.3357]
L_023:  add     rax, rcx
        mov     qword [rel used.3358], rdx
L_024:  lea     rcx, [rax+8H]
        mov     rsi, rbp
        mov     qword [rax], rbx
        mov     rdx, rbx
        mov     rdi, rcx
        call    memcpy
        mov     rsi, qword [rsp+48H]


        xor     rsi, qword [fs:abs 28H]
        jnz     L_027
        add     rsp, 80
        pop     rbx
        pop     rbp
        pop     r12
        ret





ALIGN   8
L_025:  mov     edi, 131072
        call    malloc
        mov     rdx, r12
        mov     qword [rel storage.3357], rax
        xor     ecx, ecx
        jmp     L_023





ALIGN   16
L_026:  mov     rdi, r12
        call    malloc
        jmp     L_024

L_027:
        call    __stack_chk_fail
        nop
ALIGN   16

println:
        sub     rsp, 8
        mov     rdx, qword [rdi-8H]
        lea     rsi, [rel LC0]
        mov     rcx, rdi
        xor     eax, eax
        mov     edi, 1
        call    __printf_chk
        mov     edi, 10
        add     rsp, 8
        jmp     putchar


SECTION .data   align=8

used.3358:
        dq 0000000000020000H


SECTION .bss    align=8

storage.3357:
        resq    1


SECTION .rodata.str1.1 

LC0:
        db 25H, 2EH, 2AH, 73H, 00H

LC1:
        db 25H, 64H, 00H

LC2:
        db 25H, 32H, 35H, 36H, 73H, 00H

LC3:
        db 25H, 6CH, 64H, 00H


