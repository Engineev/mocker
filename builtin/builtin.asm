





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
extern __stack_chk_fail
extern strtol
extern __memcpy_chk
extern memcpy
extern malloc
extern _GLOBAL_OFFSET_TABLE_


SECTION .text   6

__alloc:
        cmp     rdi, 127
        ja      L_002
        mov     rdx, qword [rel used.3358]
        push    rbx
        lea     rbx, [rdx+rdi]
        cmp     rbx, 2048
        ja      L_001
        mov     rax, qword [rel storage.3357]
        mov     qword [rel used.3358], rbx
        pop     rbx
        add     rax, rdx
        ret





ALIGN   8
L_001:  mov     rbx, rdi
        mov     edi, 2048
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
        cmp     rdx, 2048
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
L_005:  mov     edi, 2048
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
        sub     edx, esi
        push    r13
        push    r12
        push    rbp
        push    rbx
        lea     ebx, [rdx+9H]
        mov     r12, rdi
        mov     r13, rsi
        lea     ebp, [rdx+1H]
        movsxd  rbx, ebx
        sub     rsp, 8
        cmp     rbx, 127
        ja      L_010
        mov     rcx, qword [rel used.3358]
        lea     rdx, [rbx+rcx]
        cmp     rdx, 2048
        ja      L_009
        mov     rax, qword [rel storage.3357]
L_007:  add     rax, rcx
        mov     qword [rel used.3358], rdx
L_008:  lea     rcx, [rax+8H]
        movsxd  rdx, ebp
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
L_009:  mov     edi, 2048
        call    malloc
        mov     rdx, rbx
        mov     qword [rel storage.3357], rax
        xor     ecx, ecx
        jmp     L_007





ALIGN   16
L_010:  mov     rdi, rbx
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
        push    rbp
        push    rbx
        sub     rsp, 56


        mov     rax, qword [fs:abs 28H]
        mov     qword [rsp+28H], rax
        xor     eax, eax
        test    edi, edi
        je      L_020
        mov     ebx, edi
        js      L_019
L_016:  mov     eax, ebx
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
        je      L_022
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
        je      L_023
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
        je      L_024
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
        je      L_021
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
        je      L_025
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
        je      L_026
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
        je      L_028
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
        je      L_029
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
        je      L_027
        mov     dword [rsp+24H], edx
        mov     eax, 10
L_017:  sub     eax, 1
        lea     rbp, [rsp-4H]
        cdqe
        shl     rax, 2
        lea     rbx, [rsp+rax]




ALIGN   8
L_018:  mov     eax, dword [rbx]
        mov     rsi, qword [rel stdout]
        sub     rbx, 4
        lea     edi, [rax+30H]
        call    _IO_putc
        cmp     rbx, rbp
        jnz     L_018
        mov     rax, qword [rsp+28H]


        xor     rax, qword [fs:abs 28H]
        jne     L_030
        add     rsp, 56
        pop     rbx
        pop     rbp
        ret





ALIGN   8
L_019:  mov     rsi, qword [rel stdout]
        mov     edi, 45
        neg     ebx
        call    _IO_putc
        jmp     L_016

L_020:  mov     rax, qword [rsp+28H]


        xor     rax, qword [fs:abs 28H]
        jne     L_030
        mov     rsi, qword [rel stdout]
        add     rsp, 56
        mov     edi, 48
        pop     rbx
        pop     rbp
        jmp     _IO_putc





ALIGN   8
L_021:  mov     eax, 4
        jmp     L_017





ALIGN   8
L_022:  mov     eax, 1
        jmp     L_017





ALIGN   8
L_023:  mov     eax, 2
        jmp     L_017





ALIGN   8
L_024:  mov     eax, 3
        jmp     L_017





ALIGN   8
L_025:  mov     eax, 5
        jmp     L_017





ALIGN   8
L_026:  mov     eax, 6
        jmp     L_017

L_027:  mov     eax, 9
        jmp     L_017

L_028:  mov     eax, 7
        jmp     L_017

L_029:  mov     eax, 8
        jmp     L_017


L_030:
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
        je      L_035
        mov     ebx, edi
        js      L_034
L_031:  mov     eax, ebx
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
        je      L_038
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
        je      L_039
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
        je      L_040
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
        je      L_037
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
        je      L_041
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
        je      L_042
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
        je      L_044
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
        je      L_045
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
        je      L_043
        mov     dword [rsp+24H], edx
        mov     eax, 10
L_032:  sub     eax, 1
        lea     rbp, [rsp-4H]
        cdqe
        shl     rax, 2
        lea     rbx, [rsp+rax]
L_033:  mov     eax, dword [rbx]
        sub     rbx, 4
        lea     edi, [rax+30H]
        call    _IO_putc
        cmp     rbx, rbp
        mov     rsi, qword [rel stdout]
        jnz     L_033
        jmp     L_036





ALIGN   8
L_034:  mov     edi, 45
        neg     ebx
        call    _IO_putc
        mov     rsi, qword [rel stdout]
        jmp     L_031





ALIGN   16
L_035:  mov     edi, 48
        call    _IO_putc
        mov     rsi, qword [rel stdout]
L_036:  mov     rax, qword [rsp+28H]


        xor     rax, qword [fs:abs 28H]
        jne     L_046
        add     rsp, 56
        mov     edi, 10
        pop     rbx
        pop     rbp
        jmp     _IO_putc





ALIGN   8
L_037:  mov     eax, 4
        jmp     L_032





ALIGN   8
L_038:  mov     eax, 1
        jmp     L_032





ALIGN   8
L_039:  mov     eax, 2
        jmp     L_032





ALIGN   8
L_040:  mov     eax, 3
        jmp     L_032





ALIGN   8
L_041:  mov     eax, 5
        jmp     L_032





ALIGN   8
L_042:  mov     eax, 6
        jmp     L_032

L_043:  mov     eax, 9
        jmp     L_032

L_044:  mov     eax, 7
        jmp     L_032

L_045:  mov     eax, 8
        jmp     L_032

L_046:
        call    __stack_chk_fail
        nop





ALIGN   16

getString:
        push    r12
        push    rbp
        lea     rdi, [rel LC1]
        push    rbx
        sub     rsp, 272
        mov     rbp, rsp


        mov     rax, qword [fs:abs 28H]
        mov     qword [rsp+108H], rax
        xor     eax, eax
        mov     rsi, rbp
        mov     rbx, rbp
        call    __isoc99_scanf
L_047:  mov     edx, dword [rbx]
        add     rbx, 4
        lea     eax, [rdx-1010101H]
        not     edx
        and     eax, edx
        and     eax, 80808080H
        jz      L_047
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
        lea     r12, [rbx+0CH]
        cmp     r12, 127
        ja      L_051
        mov     rcx, qword [rel used.3358]
        lea     rdx, [r12+rcx]
        cmp     rdx, 2048
        ja      L_050
        mov     rax, qword [rel storage.3357]
L_048:  add     rax, rcx
        mov     qword [rel used.3358], rdx
L_049:  lea     rcx, [rax+8H]
        mov     rsi, rbp
        mov     qword [rax], rbx
        mov     rdx, rbx
        mov     rdi, rcx
        call    memcpy
        mov     rsi, qword [rsp+108H]


        xor     rsi, qword [fs:abs 28H]
        jnz     L_052
        add     rsp, 272
        pop     rbx
        pop     rbp
        pop     r12
        ret





ALIGN   8
L_050:  mov     edi, 2048
        call    malloc
        mov     rdx, r12
        mov     qword [rel storage.3357], rax
        xor     ecx, ecx
        jmp     L_048





ALIGN   16
L_051:  mov     rdi, r12
        call    malloc
        jmp     L_049

L_052:
        call    __stack_chk_fail
        nop
ALIGN   16

toString:
        push    r12
        push    rbp
        lea     rcx, [rel LC2]
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
L_053:  mov     edx, dword [rbx]
        add     rbx, 4
        lea     eax, [rdx-1010101H]
        not     edx
        and     eax, edx
        and     eax, 80808080H
        jz      L_053
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
        jg      L_057
        mov     rcx, qword [rel used.3358]
        lea     rdx, [r12+rcx]
        cmp     rdx, 2048
        ja      L_056
        mov     rax, qword [rel storage.3357]
L_054:  add     rax, rcx
        mov     qword [rel used.3358], rdx
L_055:  lea     rcx, [rax+8H]
        mov     rsi, rbp
        mov     qword [rax], rbx
        mov     rdx, rbx
        mov     rdi, rcx
        call    memcpy
        mov     rsi, qword [rsp+48H]


        xor     rsi, qword [fs:abs 28H]
        jnz     L_058
        add     rsp, 80
        pop     rbx
        pop     rbp
        pop     r12
        ret





ALIGN   8
L_056:  mov     edi, 2048
        call    malloc
        mov     rdx, r12
        mov     qword [rel storage.3357], rax
        xor     ecx, ecx
        jmp     L_054





ALIGN   16
L_057:  mov     rdi, r12
        call    malloc
        jmp     L_055

L_058:
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
        dq 0000000000000800H


SECTION .bss    align=8

storage.3357:
        resq    1


SECTION .rodata.str1.1 

LC0:
        db 25H, 2EH, 2AH, 73H, 00H

LC1:
        db 25H, 32H, 35H, 36H, 73H, 00H

LC2:
        db 25H, 6CH, 64H, 00H


