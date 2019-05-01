




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

__string__add:
        push    r14
        push    r13
        mov     r14, rdi
        push    r12
        push    rbp
        mov     r12, rsi
        push    rbx
        mov     rbp, qword [rdi-8H]
        mov     r13, qword [rsi-8H]
        lea     rbx, [rbp+r13]
        lea     rdi, [rbx+8H]
        call    malloc
        mov     qword [rax], rbx
        lea     rbx, [rax+8H]
        mov     rdx, rbp
        mov     rsi, r14
        mov     rdi, rbx
        call    memcpy
        lea     rdi, [rbx+rbp]
        mov     rdx, r13
        mov     rsi, r12
        call    memcpy
        mov     rax, rbx
        pop     rbx
        pop     rbp
        pop     r12
        pop     r13
        pop     r14
        ret







ALIGN   16

__string__substring:
        push    r12
        push    rbp
        mov     rbp, rdi
        push    rbx
        mov     ebx, edx
        mov     r12, rsi
        sub     ebx, esi
        lea     edi, [rbx+8H]
        movsxd  rdi, edi
        call    malloc
        lea     rcx, [rax+8H]
        movsxd  rdx, ebx
        lea     rsi, [rbp+r12]
        mov     qword [rax], rdx
        mov     rdi, rcx
        call    memcpy
        pop     rbx
        pop     rbp
        pop     r12
        ret


        nop





ALIGN   16

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
        jnz     L_001
        add     rsp, 280
        ret


L_001:
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
        jbe     L_003
        mov     r12d, 1




ALIGN   8
L_002:  mov     rdi, qword [rel stdin]
        cmp     bl, 45
        cmove   ebp, r12d
        call    _IO_getc
        movsx   ebx, al
        sub     eax, 48
        cmp     al, 9
        ja      L_002
L_003:  sub     ebx, 48
        jmp     L_005





ALIGN   8
L_004:  lea     eax, [rbx+rbx*4]
        lea     ebx, [rdx+rax*2-30H]
L_005:  mov     rdi, qword [rel stdin]
        call    _IO_getc
        movsx   edx, al
        sub     eax, 48
        cmp     al, 9
        jbe     L_004
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
        push    rbp
        push    rbx
        lea     rdi, [rel LC2]
        sub     rsp, 280
        mov     rbp, rsp


        mov     rax, qword [fs:abs 28H]
        mov     qword [rsp+108H], rax
        xor     eax, eax
        mov     rsi, rbp
        mov     rbx, rbp
        call    __isoc99_scanf
L_006:  mov     edx, dword [rbx]
        add     rbx, 4
        lea     eax, [rdx-1010101H]
        not     edx
        and     eax, edx
        and     eax, 80808080H
        jz      L_006
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
        lea     rdi, [rbx+8H]
        call    malloc
        lea     rcx, [rax+8H]
        mov     qword [rax], rbx
        mov     rdx, rbx
        mov     rsi, rbp
        mov     rdi, rcx
        call    memcpy
        mov     rcx, qword [rsp+108H]


        xor     rcx, qword [fs:abs 28H]
        jnz     L_007
        add     rsp, 280
        pop     rbx
        pop     rbp
        ret


L_007:
        call    __stack_chk_fail




ALIGN   8

toString:
        push    rbp
        push    rbx
        lea     rcx, [rel LC3]
        mov     r8, rdi
        mov     edx, 64
        mov     esi, 1
        sub     rsp, 88
        mov     rbp, rsp


        mov     rax, qword [fs:abs 28H]
        mov     qword [rsp+48H], rax
        xor     eax, eax
        mov     rdi, rbp
        mov     rbx, rbp
        call    __sprintf_chk
L_008:  mov     edx, dword [rbx]
        add     rbx, 4
        lea     eax, [rdx-1010101H]
        not     edx
        and     eax, edx
        and     eax, 80808080H
        jz      L_008
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
        lea     rdi, [rbx+8H]
        call    malloc
        lea     rcx, [rax+8H]
        mov     rsi, rbp
        mov     qword [rax], rbx
        mov     rdx, rbx
        mov     rdi, rcx
        call    memcpy
        mov     rsi, qword [rsp+48H]


        xor     rsi, qword [fs:abs 28H]
        jnz     L_009
        add     rsp, 88
        pop     rbx
        pop     rbp
        ret

L_009:
        call    __stack_chk_fail




ALIGN   8

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


SECTION .data   


SECTION .bss    


SECTION .rodata.str1.1 

LC0:
        db 25H, 2EH, 2AH, 73H, 00H

LC1:
        db 25H, 64H, 00H

LC2:
        db 25H, 32H, 35H, 36H, 73H, 00H

LC3:
        db 25H, 6CH, 64H, 00H


