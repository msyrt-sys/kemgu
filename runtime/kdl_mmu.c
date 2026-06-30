/*
 * KEMGU Bare-Metal MMU / Sayfalama (kdl_mmu.c)
 * ============================================
 *
 * C8a: aarch64 MMU'yu identity-map ile AÇ. Bu, RAM'i Device-nGnRnE'den (MMU-off
 * varsayılanı) Normal-WB cacheable belleğe çevirir → 16-baytlık SIMD/q-register
 * erişimi artık alignment-fault VERMEZ (C8b'de -mgeneral-regs-only kalkar) +
 * cache → performans + ileride sanal-bellek / process izolasyonu temeli (D fazı).
 *
 * x86_64: long mode ZATEN paging gerektirir → boot/start_x86_64.S identity
 * sayfa tablolarını kurup paging'i açar (PVH→long mode). x86 için ayrı MMU
 * kurulumu gerekmez; bu dosya yalnız aarch64.
 *
 * Identity harita (VA == PA), 4KB granül, 39-bit VA, L1 1GB blok:
 *   L1[0] 0x00000000-0x3FFFFFFF → Device (GICv2 0x0800_0000, UART 0x0900_0000)
 *   L1[1] 0x40000000-0x7FFFFFFF → Normal-WB (kernel + 16MB heap @ 0x4000_0000)
 * Diğer girişler geçersiz (erişilirse → C8c sayfa-hata).
 */
#include <stdint.h>

#if defined(__aarch64__)

/* L1 çeviri tablosu: 512 giriş × 1GB blok, 4KB-hizalı (TTBR0 gereği). */
static uint64_t kdl_l1_tablo[512] __attribute__((aligned(4096)));

/* L1 blok tanımlayıcı (1GB): bit0=geçerli, bit1=0(blok), bit[4:2]=AttrIdx,
 * bit[9:8]=SH, bit10=AF, bit[47:30]=çıkış adresi. */
#define KDL_BLOK_DEVICE  0x0000000000000401UL  /* PA=0, AttrIdx0(Device), AF */
#define KDL_BLOK_NORMAL  0x0000000040000705UL  /* PA=0x40000000, AttrIdx1(Normal), SH=inner, AF */

void kdl_mmu_kur(void) {
    for (int i = 0; i < 512; i++) kdl_l1_tablo[i] = 0;
    kdl_l1_tablo[0] = KDL_BLOK_DEVICE;        /* 0-1GB Device (MMIO) */
    kdl_l1_tablo[1] = KDL_BLOK_NORMAL;        /* 1-2GB Normal-WB (RAM) */

    /* MAIR: attr0 = Device-nGnRnE (0x00), attr1 = Normal WB cacheable (0xFF). */
    uint64_t mair = 0x000000000000FF00UL;
    /* TCR: T0SZ=25 (39-bit VA), TG0=4KB, IRGN0/ORGN0=WB, SH0=inner, EPD1=1
     * (TTBR1 kapalı), IPS=001 (36-bit PA). */
    uint64_t tcr  = 0x0000000100803519UL;
    uint64_t ttbr = (uint64_t)(uintptr_t)kdl_l1_tablo;

    __asm__ volatile("msr mair_el1, %0"  :: "r"(mair));
    __asm__ volatile("msr tcr_el1,  %0"  :: "r"(tcr));
    __asm__ volatile("msr ttbr0_el1, %0" :: "r"(ttbr));
    __asm__ volatile("dsb ish; tlbi vmalle1; dsb ish; isb");

    /* SCTLR_EL1: M (MMU) + C (data cache) + I (icache). */
    uint64_t sctlr;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    sctlr |= (1UL << 0) | (1UL << 2) | (1UL << 12);
    __asm__ volatile("msr sctlr_el1, %0; isb" :: "r"(sctlr));
}

#endif /* __aarch64__ */
