





default rel

global __alloc
global __string__add
global __string__substring
global __string__ord
global __string__parseInt
global __string__equal
global __string__inequal
global __string__less
global __string__less_equal
global __string___ctor_
global getInt
global print
global _printInt
global _printlnInt
global getString
global toString
global println

extern putchar
extern __sprintf_chk
extern __isoc99_scanf
extern _IO_putc
extern stdout
extern __printf_chk
extern _IO_getc
extern stdin
extern memcmp
extern __stack_chk_fail
extern strtol
extern __memcpy_chk
extern memcpy
extern malloc
extern _GLOBAL_OFFSET_TABLE_


SECTION .text   6

__alloc:
        jmp     malloc


        nop





ALIGN   16

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
        sub     edx, esi
        push    r12
        push    rbp
        lea     ebp, [rdx+1H]
        push    rbx
        add     edx, 9
        mov     rbx, rdi
        movsxd  rdi, edx
        mov     r12, rsi
        call    malloc
        lea     rcx, [rax+8H]
        movsxd  rdx, ebp
        lea     rsi, [rbx+r12]
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

__string__equal:
        mov     rdx, qword [rdi-8H]
        cmp     rdx, qword [rsi-8H]
        jz      L_002
        xor     eax, eax
        ret





ALIGN   8
L_002:  sub     rsp, 8
        call    memcmp
        test    eax, eax
        sete    al
        add     rsp, 8
        movzx   eax, al
        ret






ALIGN   16

__string__inequal:
        mov     rdx, qword [rdi-8H]
        cmp     rdx, qword [rsi-8H]
        jz      L_003
        mov     eax, 1
        ret


L_003:
        sub     rsp, 8
        call    memcmp
        test    eax, eax
        setne   al
        add     rsp, 8
        movzx   eax, al
        ret






ALIGN   16

__string__less:
        push    rbp
        push    rbx
        sub     rsp, 8
        mov     rbp, qword [rdi-8H]
        mov     rbx, qword [rsi-8H]
        cmp     rbp, rbx
        mov     rdx, rbx
        cmovbe  rdx, rbp
        call    memcmp
        test    eax, eax
        jz      L_004
        add     rsp, 8
        cdqe
        shr     rax, 63
        pop     rbx
        pop     rbp
        ret





ALIGN   8
L_004:  xor     eax, eax
        cmp     rbp, rbx
        setb    al
        add     rsp, 8
        pop     rbx
        pop     rbp
        ret


        nop

ALIGN   16
__string__less_equal:
        push    r13
        push    r12
        mov     r13, rsi
        push    rbp
        push    rbx
        mov     r12, rdi
        sub     rsp, 8
        mov     rbx, qword [rdi-8H]
        mov     rbp, qword [rsi-8H]
        cmp     rbx, rbp
        jz      L_007
L_005:  cmp     rbx, rbp
        mov     rdx, rbp
        mov     rsi, r13
        cmovbe  rdx, rbx
        mov     rdi, r12
        call    memcmp
        test    eax, eax
        jz      L_006
        add     rsp, 8
        cdqe
        pop     rbx
        shr     rax, 63
        pop     rbp
        pop     r12
        pop     r13
        ret






ALIGN   16
L_006:  xor     eax, eax
        cmp     rbx, rbp
        setb    al
        add     rsp, 8
        pop     rbx
        pop     rbp
        pop     r12
        pop     r13
        ret





ALIGN   8
L_007:  mov     rdx, rbx
        call    memcmp
        mov     edx, eax
        mov     eax, 1
        test    edx, edx
        jnz     L_005
        add     rsp, 8
        pop     rbx
        pop     rbp
        pop     r12
        pop     r13
        ret







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
        jbe     L_009
        mov     r12d, 1




ALIGN   8
L_008:  mov     rdi, qword [rel stdin]
        cmp     bl, 45
        cmove   ebp, r12d
        call    _IO_getc
        movsx   ebx, al
        sub     eax, 48
        cmp     al, 9
        ja      L_008
L_009:  sub     ebx, 48
        jmp     L_011





ALIGN   8
L_010:  lea     eax, [rbx+rbx*4]
        lea     ebx, [rdx+rax*2-30H]
L_011:  mov     rdi, qword [rel stdin]
        call    _IO_getc
        movsx   edx, al
        sub     eax, 48
        cmp     al, 9
        jbe     L_010
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
        push    rbp
        push    rbx
        sub     rsp, 56


        mov     rax, qword [fs:abs 28H]
        mov     qword [rsp+28H], rax
        xor     eax, eax
        test    edi, edi
        je      L_016
        mov     ebx, edi
        js      L_015
L_012:  mov     eax, ebx
        mov     esi, 3435973837
        mov     edi, ebx
        mul     esi
        mov     ecx, edx
        shr     ecx, 3
        lea     eax, [rcx+rcx*4]
        add     eax, eax
        sub     edi, eax
        test    ecx, ecx
        mov     dword [rsp], edi
        je      L_018
        mov     eax, ecx
        mul     esi
        shr     edx, 3
        lea     eax, [rdx+rdx*4]
        add     eax, eax
        sub     ecx, eax
        mov     eax, ebx
        mov     dword [rsp+4H], ecx
        mov     ecx, 1374389535
        mul     ecx
        mov     ecx, edx
        shr     ecx, 5
        test    ecx, ecx
        je      L_019
        mov     eax, ecx
        mul     esi
        shr     edx, 3
        lea     eax, [rdx+rdx*4]
        mov     edx, 274877907
        add     eax, eax
        sub     ecx, eax
        mov     eax, ebx
        mul     edx
        mov     dword [rsp+8H], ecx
        shr     edx, 6
        test    edx, edx
        mov     ecx, edx
        je      L_020
        mov     eax, edx
        mul     esi
        shr     edx, 3
        lea     eax, [rdx+rdx*4]
        mov     edx, 3518437209
        add     eax, eax
        sub     ecx, eax
        mov     eax, ebx
        mul     edx
        mov     dword [rsp+0CH], ecx
        mov     ecx, edx
        shr     ecx, 13
        test    ecx, ecx
        je      L_017
        mov     eax, ecx
        mul     esi
        shr     edx, 3
        lea     eax, [rdx+rdx*4]
        mov     edx, ebx
        shr     edx, 5
        add     eax, eax
        sub     ecx, eax
        mov     eax, edx
        mov     dword [rsp+10H], ecx
        mov     ecx, 175921861
        mul     ecx
        mov     ecx, edx
        shr     ecx, 7
        test    ecx, ecx
        je      L_021
        mov     eax, ecx
        mul     esi
        shr     edx, 3
        lea     eax, [rdx+rdx*4]
        mov     edx, 1125899907
        add     eax, eax
        sub     ecx, eax
        mov     eax, ebx
        mul     edx
        mov     dword [rsp+14H], ecx
        shr     edx, 18
        test    edx, edx
        mov     ecx, edx
        je      L_022
        mov     eax, edx
        mul     esi
        shr     edx, 3
        lea     eax, [rdx+rdx*4]
        mov     edx, 1801439851
        add     eax, eax
        sub     ecx, eax
        mov     eax, ebx
        mul     edx
        mov     dword [rsp+18H], ecx
        mov     ecx, edx
        shr     ecx, 22
        test    ecx, ecx
        je      L_024
        mov     eax, ecx
        mul     esi
        shr     edx, 3
        lea     eax, [rdx+rdx*4]
        mov     edx, 1441151881
        add     eax, eax
        sub     ecx, eax
        mov     eax, ebx
        mul     edx
        mov     dword [rsp+1CH], ecx
        shr     edx, 25
        test    edx, edx
        mov     ecx, edx
        je      L_025
        mov     eax, edx
        shr     ebx, 9
        mul     esi
        shr     edx, 3
        lea     eax, [rdx+rdx*4]
        mov     edx, 281475
        add     eax, eax
        sub     ecx, eax
        mov     eax, ebx
        mul     edx
        mov     dword [rsp+20H], ecx
        shr     edx, 7
        test    edx, edx
        je      L_023
        mov     dword [rsp+24H], edx
        mov     eax, 10
L_013:  sub     eax, 1
        lea     rbp, [rsp-4H]
        cdqe
        shl     rax, 2
        lea     rbx, [rsp+rax]




ALIGN   8
L_014:  mov     eax, dword [rbx]
        mov     rsi, qword [rel stdout]
        sub     rbx, 4
        lea     edi, [rax+30H]
        call    _IO_putc
        cmp     rbx, rbp
        jnz     L_014
        mov     rax, qword [rsp+28H]


        xor     rax, qword [fs:abs 28H]
        jne     L_026
        add     rsp, 56
        pop     rbx
        pop     rbp
        ret





ALIGN   8
L_015:  mov     rsi, qword [rel stdout]
        mov     edi, 45
        neg     ebx
        call    _IO_putc
        jmp     L_012

L_016:  mov     rax, qword [rsp+28H]


        xor     rax, qword [fs:abs 28H]
        jne     L_026
        mov     rsi, qword [rel stdout]
        add     rsp, 56
        mov     edi, 48
        pop     rbx
        pop     rbp
        jmp     _IO_putc





ALIGN   8
L_017:  mov     eax, 4
        jmp     L_013





ALIGN   8
L_018:  mov     eax, 1
        jmp     L_013





ALIGN   8
L_019:  mov     eax, 2
        jmp     L_013





ALIGN   8
L_020:  mov     eax, 3
        jmp     L_013





ALIGN   8
L_021:  mov     eax, 5
        jmp     L_013





ALIGN   8
L_022:  mov     eax, 6
        jmp     L_013

L_023:  mov     eax, 9
        jmp     L_013

L_024:  mov     eax, 7
        jmp     L_013

L_025:  mov     eax, 8
        jmp     L_013


L_026:
        call    __stack_chk_fail




ALIGN   8

_printlnInt:
        push    rbp
        push    rbx
        sub     rsp, 56
        mov     rsi, qword [rel stdout]


        mov     rax, qword [fs:abs 28H]
        mov     qword [rsp+28H], rax
        xor     eax, eax
        test    edi, edi
        je      L_031
        mov     ebx, edi
        js      L_030
L_027:  mov     eax, ebx
        mov     edi, 3435973837
        mul     edi
        mov     ecx, edx
        mov     edx, ebx
        shr     ecx, 3
        lea     eax, [rcx+rcx*4]
        add     eax, eax
        sub     edx, eax
        test    ecx, ecx
        mov     dword [rsp], edx
        je      L_034
        mov     eax, ecx
        mul     edi
        shr     edx, 3
        lea     eax, [rdx+rdx*4]
        add     eax, eax
        sub     ecx, eax
        mov     eax, ebx
        mov     dword [rsp+4H], ecx
        mov     ecx, 1374389535
        mul     ecx
        mov     ecx, edx
        shr     ecx, 5
        test    ecx, ecx
        je      L_035
        mov     eax, ecx
        mul     edi
        shr     edx, 3
        lea     eax, [rdx+rdx*4]
        mov     edx, 274877907
        add     eax, eax
        sub     ecx, eax
        mov     eax, ebx
        mul     edx
        mov     dword [rsp+8H], ecx
        mov     ecx, edx
        shr     ecx, 6
        test    ecx, ecx
        je      L_036
        mov     eax, ecx
        mul     edi
        shr     edx, 3
        lea     eax, [rdx+rdx*4]
        mov     edx, 3518437209
        add     eax, eax
        sub     ecx, eax
        mov     eax, ebx
        mul     edx
        mov     dword [rsp+0CH], ecx
        mov     ecx, edx
        shr     ecx, 13
        test    ecx, ecx
        je      L_033
        mov     eax, ecx
        mul     edi
        shr     edx, 3
        lea     eax, [rdx+rdx*4]
        mov     edx, ebx
        shr     edx, 5
        add     eax, eax
        sub     ecx, eax
        mov     eax, edx
        mov     dword [rsp+10H], ecx
        mov     ecx, 175921861
        mul     ecx
        shr     edx, 7
        test    edx, edx
        mov     ecx, edx
        je      L_037
        mov     eax, edx
        mul     edi
        shr     edx, 3
        lea     eax, [rdx+rdx*4]
        mov     edx, 1125899907
        add     eax, eax
        sub     ecx, eax
        mov     eax, ebx
        mul     edx
        mov     dword [rsp+14H], ecx
        mov     ecx, edx
        shr     ecx, 18
        test    ecx, ecx
        je      L_038
        mov     eax, ecx
        mul     edi
        shr     edx, 3
        lea     eax, [rdx+rdx*4]
        mov     edx, 1801439851
        add     eax, eax
        sub     ecx, eax
        mov     eax, ebx
        mul     edx
        mov     dword [rsp+18H], ecx
        mov     ecx, edx
        shr     ecx, 22
        test    ecx, ecx
        je      L_040
        mov     eax, ecx
        mul     edi
        shr     edx, 3
        lea     eax, [rdx+rdx*4]
        mov     edx, 1441151881
        add     eax, eax
        sub     ecx, eax
        mov     eax, ebx
        mul     edx
        mov     dword [rsp+1CH], ecx
        shr     edx, 25
        test    edx, edx
        mov     ecx, edx
        je      L_041
        mov     eax, edx
        shr     ebx, 9
        mul     edi
        shr     edx, 3
        lea     eax, [rdx+rdx*4]
        mov     edx, 281475
        add     eax, eax
        sub     ecx, eax
        mov     eax, ebx
        mul     edx
        mov     dword [rsp+20H], ecx
        shr     edx, 7
        test    edx, edx
        je      L_039
        mov     dword [rsp+24H], edx
        mov     eax, 10
L_028:  sub     eax, 1
        lea     rbp, [rsp-4H]
        cdqe
        shl     rax, 2
        lea     rbx, [rsp+rax]
L_029:  mov     eax, dword [rbx]
        sub     rbx, 4
        lea     edi, [rax+30H]
        call    _IO_putc
        cmp     rbx, rbp
        mov     rsi, qword [rel stdout]
        jnz     L_029
        jmp     L_032





ALIGN   8
L_030:  mov     edi, 45
        neg     ebx
        call    _IO_putc
        mov     rsi, qword [rel stdout]
        jmp     L_027





ALIGN   16
L_031:  mov     edi, 48
        call    _IO_putc
        mov     rsi, qword [rel stdout]
L_032:  mov     rax, qword [rsp+28H]


        xor     rax, qword [fs:abs 28H]
        jne     L_042
        add     rsp, 56
        mov     edi, 10
        pop     rbx
        pop     rbp
        jmp     _IO_putc





ALIGN   8
L_033:  mov     eax, 4
        jmp     L_028





ALIGN   8
L_034:  mov     eax, 1
        jmp     L_028





ALIGN   8
L_035:  mov     eax, 2
        jmp     L_028





ALIGN   8
L_036:  mov     eax, 3
        jmp     L_028





ALIGN   8
L_037:  mov     eax, 5
        jmp     L_028





ALIGN   8
L_038:  mov     eax, 6
        jmp     L_028

L_039:  mov     eax, 9
        jmp     L_028

L_040:  mov     eax, 7
        jmp     L_028

L_041:  mov     eax, 8
        jmp     L_028

L_042:
        call    __stack_chk_fail
        nop





ALIGN   16

getString:
        push    rbp
        push    rbx
        lea     rdi, [rel LC1]
        sub     rsp, 280
        mov     rbp, rsp


        mov     rax, qword [fs:abs 28H]
        mov     qword [rsp+108H], rax
        xor     eax, eax
        mov     rsi, rbp
        mov     rbx, rbp
        call    __isoc99_scanf
L_043:  mov     edx, dword [rbx]
        add     rbx, 4
        lea     eax, [rdx-1010101H]
        not     edx
        and     eax, edx
        and     eax, 80808080H
        jz      L_043
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
        jnz     L_044
        add     rsp, 280
        pop     rbx
        pop     rbp
        ret

L_044:
        call    __stack_chk_fail




ALIGN   8

toString:
        push    rbp
        push    rbx
        lea     rcx, [rel LC2]
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
L_045:  mov     edx, dword [rbx]
        add     rbx, 4
        lea     eax, [rdx-1010101H]
        not     edx
        and     eax, edx
        and     eax, 80808080H
        jz      L_045
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
        jnz     L_046
        add     rsp, 88
        pop     rbx
        pop     rbp
        ret

L_046:
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
        db 25H, 32H, 35H, 36H, 73H, 00H

LC2:
        db 25H, 6CH, 64H, 00H


