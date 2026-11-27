/* İlk sayfa çalıştırılabilir kod, bitişik ikinci sayfa BSP'nin parametreleridir.
 * SIPI'den yayımlama onayına kadar BSP parametreleri aynı AP için tutar. */
.set AP_PARAM, 4096
.set AP_CR3, 0
.set AP_STACK, 8
.set AP_ENTRY, 16
.set AP_NO, 24
.set AP_GDTR, 32
.set AP_JUMP32, 40
.set AP_JUMP64, 48
.set AP_CR4, 56
.set AP_CR0, 64
.set AP_PAT, 72

.section .rodata.kem_ap,"a",@progbits
.balign 16
kem_ap_sablon:
.code16
    cli
    cld
    movw %cs, %ax
    movw %ax, %ds
    movzwl %ax, %esi
    shll $4, %esi
    lgdtl AP_PARAM+AP_GDTR
    movl %cr0, %eax
    orl $1, %eax
    movl %eax, %cr0
    ljmpl *AP_PARAM+AP_JUMP32

.code32
kem_ap_korumali:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movl AP_PARAM+AP_CR4(%esi), %eax
    movl %eax, %cr4
    movl AP_PARAM+AP_CR3(%esi), %eax
    movl %eax, %cr3
    movl $0xc0000080, %ecx
    rdmsr
    orl $0x900, %eax              /* LME, NXE: çekirdek sayfa tablolarıyla aynı. */
    wrmsr
    /* Ortak sayfa tabloları BSP ile aynı PAT ve önbellek kipi gerektirir. */
    movl $0x277, %ecx
    movl AP_PARAM+AP_PAT(%esi), %eax
    movl AP_PARAM+AP_PAT+4(%esi), %edx
    wbinvd
    wrmsr
    wbinvd
    movl AP_PARAM+AP_CR0(%esi), %eax
    orl $0x80010001, %eax         /* PG, WP, PE */
    movl %eax, %cr0
    ljmpl *AP_PARAM+AP_JUMP64(%esi)

.code64
kem_ap_uzun:
    movq AP_PARAM+AP_STACK(%rsi), %rsp
    andq $-16, %rsp
    xorq %rbp, %rbp
    movl AP_PARAM+AP_NO(%rsi), %edi
    call *AP_PARAM+AP_ENTRY(%rsi)
1:  cli
    hlt
    jmp 1b

.balign 8
kem_ap_gdt:
    .quad 0, 0x00cf9a000000ffff, 0x00cf92000000ffff, 0x00af9a000000ffff
kem_ap_sablon_son:

.text
.code64
.globl kem_ap_sayfa_hazirla
.type kem_ap_sayfa_hazirla,@function
/* SysV: RDI=düşük sayfa, RSI=CR3, RDX=yığın tepesi, RCX=giriş, R8D=çekirdek no. */
kem_ap_sayfa_hazirla:
    movq %rdi, %r9
    movq %rsi, AP_PARAM+AP_CR3(%r9)
    movq %rdx, AP_PARAM+AP_STACK(%r9)
    movq %rcx, AP_PARAM+AP_ENTRY(%r9)
    movl %r8d, AP_PARAM+AP_NO(%r9)
    movq %cr4, %rax
    movl %eax, AP_PARAM+AP_CR4(%r9)
    movq %cr0, %rax
    movl %eax, AP_PARAM+AP_CR0(%r9)
    movl $0x277, %ecx
    rdmsr
    movl %eax, AP_PARAM+AP_PAT(%r9)
    movl %edx, AP_PARAM+AP_PAT+4(%r9)
    movw $31, AP_PARAM+AP_GDTR(%r9)
    leal kem_ap_gdt-kem_ap_sablon(%r9), %eax
    movl %eax, AP_PARAM+AP_GDTR+2(%r9)
    leal kem_ap_korumali-kem_ap_sablon(%r9), %eax
    movl %eax, AP_PARAM+AP_JUMP32(%r9)
    movw $0x08, AP_PARAM+AP_JUMP32+4(%r9)
    leal kem_ap_uzun-kem_ap_sablon(%r9), %eax
    movl %eax, AP_PARAM+AP_JUMP64(%r9)
    movw $0x18, AP_PARAM+AP_JUMP64+4(%r9)
    leaq kem_ap_sablon(%rip), %rsi
    movl $kem_ap_sablon_son-kem_ap_sablon, %ecx
    cld
    rep movsb
    mfence
    ret
.size kem_ap_sayfa_hazirla, .-kem_ap_sayfa_hazirla

/* Açılış kâhini ayrı bir konuk işlevi bağladıysa aynı CPU'da çağır. */
.weak kem_smp_acilis_deneyi
.globl kem_ap_deney_adresi
.type kem_ap_deney_adresi,@function
kem_ap_deney_adresi:
    movabsq $kem_smp_acilis_deneyi, %rax
    ret
.size kem_ap_deney_adresi, .-kem_ap_deney_adresi
.globl kem_ap_acilis_dene
.type kem_ap_acilis_dene,@function
kem_ap_acilis_dene:
    movabsq $kem_smp_acilis_deneyi, %rax
    testq %rax, %rax
    jz 1f
    jmp *%rax
1:  ret
.size kem_ap_acilis_dene, .-kem_ap_acilis_dene
/* Deneyin QMP ölçüm alanı. Normal açılış bu yordamı çağırmaz. */
.weak kem_smp_olcum_boyu
.global kem_ap_olcum_boyu
.type kem_ap_olcum_boyu,@function
kem_ap_olcum_boyu:
    movabsq $kem_smp_olcum_boyu,%rax
    testq %rax,%rax
    jnz 1f
    movl %edi,%eax
    shll $6,%eax
    addl $256,%eax
    ret
1:  jmp *%rax
.size kem_ap_olcum_boyu,.-kem_ap_olcum_boyu
.section .note.GNU-stack,"",@progbits
