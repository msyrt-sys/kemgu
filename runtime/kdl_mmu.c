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

/* L1 çeviri tablosu: 512 giriş × 1GB, 4KB-hizalı (TTBR0 gereği).
 * L2 tablosu: RAM 1GB bloğu (0x40000000-0x7FFFFFFF) için 512 × 2MB sayfa —
 * per-region izin (D2 privilege ayrımı: kernel AP=00, user sayfası AP=01). */
static uint64_t kdl_l1_tablo[512] __attribute__((aligned(4096)));
static uint64_t kdl_l2_tablo[512] __attribute__((aligned(4096)));

/* L1[0] Device (1GB blok): bit0=geçerli, bit1=0(blok), AttrIdx0, AF. */
#define KDL_BLOK_DEVICE  0x0000000000000401UL  /* PA=0, Device, AF, AP=00(EL1) */
/* L2 2MB blok düşük-bit'leri: AttrIdx1(Normal), SH=inner, AF, AP=00 (EL1 RW). */
#define KDL_L2_NORMAL    0x0000000000000705UL
/* EL0 user-region: 1 adet 2MB sayfa (D2). AP=01 (EL0+EL1 RW). EL0-writable →
 * EL1'de non-exec (ARMv8) ama EL0'da çalıştırılabilir (UXN=0) → EL0 kodu burada. */
#define KDL_USER_VA      0x0000000042000000UL

void kdl_mmu_kur(void) {
    for (int i = 0; i < 512; i++) kdl_l1_tablo[i] = 0;

    /* RAM 1GB → L2 (512 × 2MB identity). Çoğu sayfa kernel (AP=00); yalnız
     * user sayfası (0x42000000) AP=01 → EL0 erişimi (privilege ayrımı). */
    for (int n = 0; n < 512; n++) {
        uint64_t pa = 0x40000000UL + (uint64_t)n * 0x200000UL;
        uint64_t desc = pa | KDL_L2_NORMAL;
        if (pa == KDL_USER_VA) desc |= (1UL << 6);   /* AP=01: EL0+EL1 RW */
        kdl_l2_tablo[n] = desc;
    }

    kdl_l1_tablo[0] = KDL_BLOK_DEVICE;                            /* 0-1GB Device (MMIO) */
    kdl_l1_tablo[1] = (uint64_t)(uintptr_t)kdl_l2_tablo | 0x3UL;  /* 1-2GB → L2 tablo (2MB sayfalar) */

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

/* === D1: per-process adres-uzayı ===
 * Bir sürecin sayfa tablolarını kur: kernel identity (paylaşılan) + user VA
 * (0x42000000) → sürece-özel user_pa. l1/l2 çağıran-sağlanan 4KB-hizalı tablolar.
 * Süreçler kernel'i aynı (identity) görür ama user VA'ları FARKLI fiziksel
 * sayfalara gider → izolasyon. */
void kdl_surec_kur(uint64_t *l1, uint64_t *l2, uint64_t user_pa) {
    for (int n = 0; n < 512; n++) {
        uint64_t pa = 0x40000000UL + (uint64_t)n * 0x200000UL;
        l2[n] = pa | KDL_L2_NORMAL;                  /* kernel identity (AP=00) */
    }
    /* user VA (L2[16]) → sürece-özel fiziksel sayfa */
    l2[(KDL_USER_VA - 0x40000000UL) / 0x200000UL] = user_pa | KDL_L2_NORMAL;

    for (int i = 0; i < 512; i++) l1[i] = 0;
    l1[0] = KDL_BLOK_DEVICE;
    l1[1] = (uint64_t)(uintptr_t)l2 | 0x3UL;          /* 1-2GB → bu sürecin L2'si */
    __asm__ volatile("dsb ish");                      /* tablo yazımları görünür olsun */
}

/* === D3: EL0 (korumalı user-process) adres-uzayı ===
 * kdl_surec_kur gibi ama user VA sayfası AP=01 (EL0+EL1 RW) → süreç kodu KENDİ
 * TTBR'ı altında EL0'da koşabilir. Kernel identity (AP=00) EL0'a KAPALI kalır →
 * süreç kernel belleğine erişince permission-fault (bellek koruması / hapis).
 * user_pa = user kodun fiziksel yeri (D3'te .user section = 0x42000000 identity). */
void kdl_surec_kur_el0(uint64_t *l1, uint64_t *l2, uint64_t user_pa) {
    for (int n = 0; n < 512; n++) {
        uint64_t pa = 0x40000000UL + (uint64_t)n * 0x200000UL;
        l2[n] = pa | KDL_L2_NORMAL;                  /* kernel identity (AP=00, EL1-only) */
    }
    /* user VA (L2[16]) → user_pa, AP=01 (EL0 erişimi + UXN=0 → EL0-exec) */
    l2[(KDL_USER_VA - 0x40000000UL) / 0x200000UL] = user_pa | KDL_L2_NORMAL | (1UL << 6);

    for (int i = 0; i < 512; i++) l1[i] = 0;
    l1[0] = KDL_BLOK_DEVICE;
    l1[1] = (uint64_t)(uintptr_t)l2 | 0x3UL;
    __asm__ volatile("dsb ish");
}

/* TTBR0_EL1'i değiştir + TLB temizle → adres-uzayı geçişi (per-process). */
void kdl_ttbr_degis(uint64_t *l1) {
    __asm__ volatile("msr ttbr0_el1, %0" :: "r"((uint64_t)(uintptr_t)l1));
    __asm__ volatile("dsb ish; tlbi vmalle1; dsb ish; isb");
}

#endif /* __aarch64__ */
