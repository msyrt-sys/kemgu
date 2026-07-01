/*
 * D-153 testi (aarch64) — KALICI FS DESERIALIZE GÜVENLİK: poisoned-disk boyut clamp.
 *
 * kdl_dosya_yukle diskteki kdl_dosyalar[] tablosunu OLDUĞU GİBİ (verbatim) yükler.
 * Kötü niyetli bir disk aşırı büyük `boyut` alanı içerirse (savunma-katmanı senaryosu;
 * EL0-erişilebilir DEĞİL, kötü niyetli disk gerekir), num=18 dosya_oku_metin ham
 * boyut kadar byte kopyalar → 64-byte icerik[] tamponunu AŞAN OOB okuma → kernel
 * belleği user tamponuna sızar.
 *
 * D-153 fix (kdl_kesme.c):
 *   (A) kdl_dosya_yukle deserialize SONRASI her girişi normalize/clamp eder
 *       (kullanildi 0/1, ad/icerik null-term, boyut [0,64) dışıysa 0);
 *   (B) num=18 handler boyut'u yerel `lim` ile clamp'lar → OOB okuma imkânsız.
 *
 * Test (EL1 main + EL0 launcher):
 *   main (EL1): elle bir disk-blok image üret — magic "KEMG" + kdl_dosyalar entry 0:
 *     kullanildi=1, ad="p", icerik="AB", boyut=9999 (ZEHİR). Bloğu diske YAZ (blk 0),
 *     sonra kdl_dosya_yukle(base) çağır (clamp burada devreye girer);
 *   launcher (EL0): sys2(18, "p", 0x42210000) yapsın → dönen uzunluk CLAMP'li olmalı
 *     (<=63, 9999 DEĞİL) + kernel sağ kalmalı → "KALICI GUARD OK".
 *
 * Fix ÖNCESİ: dönen uzunluk 9999 (veya OOB kopya sırasında kernel data-abort/halt) →
 * "KALICI GUARD OK" hiç basılmaz. Kanıt: seri çıktıda "KALICI GUARD OK".
 */
#include <stdint.h>
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_satir(void);
extern void kdl_yazdir_tam(int32_t);
extern uint64_t kdl_virtio_blk_bul(void);
extern int kdl_virtio_blk_kur(uint64_t base);
extern int kdl_virtio_blk_yaz(uint64_t base, uint64_t sektor, const uint8_t *kaynak);
extern int kdl_dosya_yukle(uint64_t base);
extern void kdl_surec_kur_el0_veri(uint64_t *l1, uint64_t *l2, uint64_t kod_pa, uint64_t veri_pa);
extern void kdl_kesme_kur(void);
extern void kdl_timer_baslat(void);
extern void kdl_preempt_baslat(void);
extern int  kdl_preempt_gorev_olustur_el0(void (*giris)(void), void *kernel_yigin_tepe,
                                          void *user_yigin_tepe);
extern void kdl_preempt_gorev_ttbr(int gorev, uint64_t *l1);
extern void kdl_preempt_ac(void);

static uint64_t l1_l[512] __attribute__((aligned(4096)));
static uint64_t l2_l[512] __attribute__((aligned(4096)));
static unsigned char kstack_l[8192] __attribute__((aligned(16)));

/* Disk image tamponu (blok 0). kdl_fs_buf düzeni ile aynı:
 *   [0..3]="KEMG" magic, [4..15]=pad, [16..]=kdl_dosyalar bytes.
 * Struct düzeni (kdl_kesme.c ile aynı, 96 byte):
 *   ad@0(16) deger@16(8) icerik@24(64) boyut@88(4) kullanildi@92(4). */
static uint8_t disk_buf[512] __attribute__((aligned(16)));

__attribute__((always_inline)) static inline unsigned long sys2(unsigned long num, unsigned long a0, unsigned long a1) {
    register unsigned long x8 __asm__("x8") = num;
    register unsigned long x0 __asm__("x0") = a0;
    register unsigned long x1 __asm__("x1") = a1;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8), "r"(x1) : "memory");
    return x0;
}
__attribute__((always_inline)) static inline unsigned long sys(unsigned long num, unsigned long arg) {
    register unsigned long x8 __asm__("x8") = num;
    register unsigned long x0 __asm__("x0") = arg;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}

__attribute__((section(".user"), noinline))
static void launcher(void) {
    /* Poisoned tablo diskten yüklendi (main'de). Zehirli boyut=9999 dosyayı OKU;
     * hedef user tampon 0x42210000 (EL0 veri aralığı [0x42000000,0x42400000) içi,
     * D-150 yaz-guard İZİN verir). Dönen uzunluk CLAMP'li (<=63) olmalı. */
    long uz = (long)sys2(18, (unsigned long)(uintptr_t)"p", 0x42210000UL);

    if (uz >= 0 && uz < 64) {
        sys(5, (unsigned long)(uintptr_t)"KALICI GUARD OK");
    } else {
        sys(5, (unsigned long)(uintptr_t)"KALICI GUARD HATA uz=");
        sys(6, (unsigned long)uz);
    }
    sys(7, 0);
    for (;;) { }
}

int main(void) {
    kdl_yazdir_metin("KALICI GUARD BASLA");
    kdl_yazdir_satir();

    uint64_t base = kdl_virtio_blk_bul();
    if (!base) { kdl_yazdir_metin("DISK YOK"); kdl_yazdir_satir(); for (;;) { __asm__ volatile("wfe"); } }
    if (kdl_virtio_blk_kur(base) != 0) { kdl_yazdir_metin("KUR HATA"); kdl_yazdir_satir(); for (;;) { __asm__ volatile("wfe"); } }

    /* --- Zehirli disk image üret (blok 0) --- */
    for (int i = 0; i < 512; i++) disk_buf[i] = 0;
    disk_buf[0] = 'K'; disk_buf[1] = 'E'; disk_buf[2] = 'M'; disk_buf[3] = 'G';   /* magic */
    /* entry 0 buf offset 16'dan başlar. */
    const int e0 = 16;
    disk_buf[e0 + 0] = 'p'; disk_buf[e0 + 1] = 0;                 /* ad = "p" (null-term) */
    disk_buf[e0 + 24 + 0] = 'A'; disk_buf[e0 + 24 + 1] = 'B';     /* icerik = "AB" (null zaten sıfırlı) */
    /* boyut @ e0+88 = 9999 (ZEHİR), little-endian int32. 9999 = 0x270F. */
    disk_buf[e0 + 88 + 0] = 0x0F;
    disk_buf[e0 + 88 + 1] = 0x27;
    disk_buf[e0 + 88 + 2] = 0x00;
    disk_buf[e0 + 88 + 3] = 0x00;
    /* kullanildi @ e0+92 = 1 */
    disk_buf[e0 + 92 + 0] = 0x01;

    if (kdl_virtio_blk_yaz(base, 0, disk_buf) != 0) {
        kdl_yazdir_metin("YAZ HATA"); kdl_yazdir_satir();
        for (;;) { __asm__ volatile("wfe"); }
    }
    /* Blok 1 (entry'lerin devamı) — bizim entry 0'ımız blok 0'a sığıyor; blok 1
     * sıfırlanmış image yaz (loader iki bloğu da okur; magic yalnız blok 0'da). */
    for (int i = 0; i < 512; i++) disk_buf[i] = 0;
    if (kdl_virtio_blk_yaz(base, 1, disk_buf) != 0) {
        kdl_yazdir_metin("YAZ HATA 1"); kdl_yazdir_satir();
        for (;;) { __asm__ volatile("wfe"); }
    }

    /* Poisoned tabloyu YÜKLE — D-153 clamp deserialize sonrası boyut'u [0,64)'e çeker. */
    if (kdl_dosya_yukle(base) != 0) {
        kdl_yazdir_metin("YUKLE HATA"); kdl_yazdir_satir();
        for (;;) { __asm__ volatile("wfe"); }
    }

    /* EL0 launcher başlat: zehirli dosyayı num=18 ile okusun. */
    kdl_surec_kur_el0_veri(l1_l, l2_l, 0x42000000UL, 0x44000000UL);
    kdl_preempt_baslat();
    int tl = kdl_preempt_gorev_olustur_el0(launcher, kstack_l + sizeof(kstack_l),
                                           (void *)(uintptr_t)0x42380000UL);
    kdl_preempt_gorev_ttbr(tl, l1_l);
    kdl_kesme_kur();
    kdl_timer_baslat();
    kdl_preempt_ac();

    for (;;) { __asm__ volatile("wfe"); }
}
