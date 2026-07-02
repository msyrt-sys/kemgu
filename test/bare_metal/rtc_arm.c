/*
 * PL031 RTC (Real-Time Clock) okuma testi (aarch64) — DONANIM gerçek-zaman
 * saati (Faz H — ZAMAN milestone, ağsız/deterministik).
 *
 * NTP (D-167) zamanı AĞDAN (SNTP → time.google.com) aldı. Bu test aynı bilgiyi
 * DONANIMDAN alır: QEMU virt makinesinde ARM PL031 RTC cihazı 0x09010000
 * adresinde memory-mapped. Ağ, DNS, virtio-net GEREKMEZ → deterministik.
 *
 * PL031 register haritası (ARM PrimeCell PL031):
 *   offset 0x00  DR  (Data Register)  — mevcut zaman, Unix epoch'tan saniye (u32)
 *   offset 0x04  MR  (Match Register)
 *   offset 0x08  LR  (Load Register)
 *   offset 0x0C  CR  (Control Register)
 * DR okuması: 32-bit volatile MMIO oku → o anki Unix zaman (saniye).
 *
 * MMU: boot/start_aarch64.S main'den ÖNCE kdl_mmu_kur() çağırır → main MMU-ON
 * koşar. MMU L1[0] (0x0-0x3FFFFFFF) Device-nGnRnE map eder (GICv2, UART, RTC
 * hepsi bu aralıkta). 0x09010000 < 0x40000000 → Device-map → DR doğrudan okunur.
 *
 * Makul kontrol: DR non-zero VE 2020 sonrası (>1_600_000_000) VE 2033 öncesi
 * (<2_000_000_000). QEMU virt PL031 host wall-clock'unu yansıtır → 2026'da
 * ~1.78 milyar. Makulse → "RTC OK".
 *
 * Kanıt: "RTC OK" + okunan DR değeri (onaltılık + ondalık).
 */
#include <stdint.h>

extern void kdl_yazdir_metin(const char *);   /* metin + newline */
extern void kdl_yaz_metin(const char *);       /* metin, newline'sız (inline etiket) */
extern void kdl_yazdir_satir(void);
extern void kdl_yaz_onaltilik(uint64_t);       /* onaltılık, newline'sız */

/* PL031 RTC — QEMU virt standart cihazı. */
#define KDL_PL031_BASE   0x09010000UL   /* QEMU virt PL031 RTC taban adresi */
#define KDL_PL031_DR     0x00u          /* Data Register offset (Unix saniye, u32) */

/* Makul Unix zaman penceresi (deterministik makul-kontrol). */
#define KDL_UNIX_ALT     1600000000ULL  /* 2020-09-13 sonrası */
#define KDL_UNIX_UST     2000000000ULL  /* 2033-05-18 öncesi */

static _Noreturn void halt(void) { for (;;) { __asm__ volatile("wfe"); } }

/* Ondalık (base-10) yaz — newline'sız. Yerel + libc'siz (bare-metal).
 * uint64_t tüm aralığı güvenli işler (kdl_yaz_tam int32_t ile sınırlı). */
static void yaz_ondalik(uint64_t n) {
    char buf[21];                       /* 2^64 = 20 hane + NUL */
    int i = 20;
    buf[i] = '\0';
    if (n == 0) {
        buf[--i] = '0';
    } else {
        while (n > 0) {
            buf[--i] = (char)('0' + (int)(n % 10));
            n /= 10;
        }
    }
    kdl_yaz_metin(&buf[i]);             /* newline'sız (inline) */
}

int main(void) {
    kdl_yazdir_metin("RTC BASLA");
    kdl_yazdir_satir();

    /* PL031 DR (offset 0x00) oku — 32-bit MMIO, Unix epoch saniyesi.
     * MMU-ON: 0x09010000 Device-map (L1[0]) → doğrudan okunabilir. */
    uint32_t rtc = *(volatile uint32_t *)(uintptr_t)(KDL_PL031_BASE + KDL_PL031_DR);
    uint64_t unix_zaman = (uint64_t)rtc;

    /* Ham DR değerini bas: onaltılık + ondalık (tek satır).
     * kdl_yaz_onaltilik "0x" önekini zaten ekler → etikette tekrar etme. */
    kdl_yaz_metin("RTC DR=");
    kdl_yaz_onaltilik(unix_zaman);
    kdl_yaz_metin(" (");
    yaz_ondalik(unix_zaman);
    kdl_yaz_metin(")");
    kdl_yazdir_satir();

    kdl_yaz_metin("UNIX=");
    yaz_ondalik(unix_zaman);
    kdl_yazdir_satir();

    /* Makul kontrol: non-zero VE 2020-2033 penceresinde. */
    if (unix_zaman >= KDL_UNIX_ALT && unix_zaman < KDL_UNIX_UST) {
        kdl_yazdir_metin("RTC OK");
        kdl_yazdir_satir();
    } else {
        kdl_yazdir_metin("RTC MAKUL DEGIL");
        kdl_yazdir_satir();
    }

    halt();
    return 0;
}
