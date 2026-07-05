/*
 * CMOS RTC (Real-Time Clock) okuma testi (x86_64) — DONANIM gerçek-zaman
 * saati. D-172 (aarch64 PL031 RTC) testinin x86 ikizi/paritesi.
 *
 * aarch64 tarafında PL031 RTC 0x09010000 adresinde memory-mapped idi ve tek
 * bir 32-bit Unix epoch saniyesi veriyordu. PC uyumlu x86 makinelerinde ise
 * saat, MC146818 uyumlu CMOS RTC cihazından PORT I/O ile okunur:
 *
 *   port 0x70  CMOS index/adres portu (okunacak register numarası buraya yazılır)
 *   port 0x71  CMOS veri portu (index'lenen register'ın değeri buradan okunur)
 *
 * CMOS RTC register haritası (varsayılan BCD kodlama):
 *   0x00  saniye     0x02  dakika    0x04  saat
 *   0x07  gun        0x08  ay        0x09  yil (2 hane)
 *   0x0A  Status A  — bit 7 = UIP (Update In Progress)
 *
 * UIP (Update In Progress): RTC saniyede bir kendini gunceller; guncelleme
 * sirasinda (UIP=1) okunan degerler tutarsiz olabilir. Tutarli okuma icin
 * UIP'nin 0 olmasini bekleyip sonra register'lari okuruz.
 *
 * BCD -> ondalik donusum: her nibble bir ondalik hane tutar.
 *   ondalik = (bcd >> 4) * 10 + (bcd & 0x0F)
 *
 * Makul kontrol (deterministik-pass): yil makul pencerede (2024..2099),
 * ay 1..12, gun 1..31. QEMU `-rtc base=utc` host wall-clock'unu yansitir →
 * her kosuda deger DEGISIR ama makul-pencere HER ZAMAN gecer.
 *
 * Kanit: "RTC X86 BASLA" + okunan tarih/saat (YYYY-AA-GG SS:DD:SS) + "RTC X86 OK".
 */
#include <stdint.h>

extern void kdl_yazdir_metin(const char *);   /* metin + newline */
extern void kdl_yaz_metin(const char *);       /* metin, newline'siz (inline etiket) */
extern void kdl_yazdir_satir(void);
extern void kdl_yaz_onaltilik(uint64_t);       /* onaltilik, newline'siz */

/* CMOS RTC port adresleri (PC uyumlu MC146818). */
#define KDL_CMOS_INDEX   0x70u   /* index/adres portu */
#define KDL_CMOS_DATA    0x71u   /* veri portu */

/* CMOS register offsetleri (BCD kodlu). */
#define KDL_CMOS_SANIYE  0x00u
#define KDL_CMOS_DAKIKA  0x02u
#define KDL_CMOS_SAAT    0x04u
#define KDL_CMOS_GUN     0x07u
#define KDL_CMOS_AY      0x08u
#define KDL_CMOS_YIL     0x09u
#define KDL_CMOS_STATUS_A 0x0Au  /* bit 7 = UIP */

/* Makul yil penceresi (2 haneli) — 24..99 → 2024..2099. */
#define KDL_YIL_ALT      24u
#define KDL_YIL_UST      99u

static _Noreturn void halt(void) { for (;;) { __asm__ volatile("hlt"); } }

/* Tek byte port cikisi (out imm/al). */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* Tek byte port girisi (in al, dx). */
static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

/* CMOS register oku: once index portuna register numarasini yaz, sonra veri
 * portundan degeri oku. */
static uint8_t cmos_oku(uint8_t reg) {
    outb(KDL_CMOS_INDEX, reg);
    return inb(KDL_CMOS_DATA);
}

/* UIP (Update In Progress, Status A bit 7) kalkana kadar bekle. */
static int cmos_uip_bekle(void) {
    int uip_goruldu = 0;
    /* Guvenlik tavani: sonsuz donguyu onle (QEMU'da UIP nadiren uzun surer). */
    for (int i = 0; i < 1000000; i++) {
        if ((cmos_oku(KDL_CMOS_STATUS_A) & 0x80u) == 0) {
            return uip_goruldu;   /* UIP=0 → tutarli okuma zamani */
        }
        uip_goruldu = 1;          /* en az bir kez UIP=1 goruldu */
    }
    return uip_goruldu;
}

/* BCD (Binary-Coded Decimal) → ondalik donusum. */
static uint8_t bcd_ondalik(uint8_t v) {
    return (uint8_t)(((v >> 4) * 10) + (v & 0x0Fu));
}

/* Ondalik (base-10) yaz — newline'siz, libc'siz (bare-metal). */
static void yaz_ondalik(uint64_t n) {
    char buf[21];
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
    kdl_yaz_metin(&buf[i]);
}

/* Iki haneli ondalik yaz (sifir-dolgulu — tarih/saat bicimi icin). */
static void yaz_iki_hane(uint8_t n) {
    if (n < 10) {
        kdl_yaz_metin("0");
    }
    yaz_ondalik((uint64_t)n);
}

int main(void) {
    kdl_yazdir_metin("RTC X86 BASLA");
    kdl_yazdir_satir();

    /* Tutarli okuma icin UIP kalkmasini bekle, sonra tum alanlari oku. */
    int uip_beklendi = cmos_uip_bekle();

    uint8_t saniye = bcd_ondalik(cmos_oku(KDL_CMOS_SANIYE));
    uint8_t dakika = bcd_ondalik(cmos_oku(KDL_CMOS_DAKIKA));
    uint8_t saat   = bcd_ondalik(cmos_oku(KDL_CMOS_SAAT));
    uint8_t gun    = bcd_ondalik(cmos_oku(KDL_CMOS_GUN));
    uint8_t ay     = bcd_ondalik(cmos_oku(KDL_CMOS_AY));
    uint8_t yil2   = bcd_ondalik(cmos_oku(KDL_CMOS_YIL));   /* 2 haneli yil */
    uint64_t yil   = 2000u + (uint64_t)yil2;                 /* tam yil (2000+) */

    /* Ham (BCD) yil register'ini de bas — port I/O calistiginin kaniti. */
    kdl_yaz_metin("RTC YIL_BCD=");
    kdl_yaz_onaltilik((uint64_t)cmos_oku(KDL_CMOS_YIL));
    kdl_yazdir_satir();

    /* UIP beklendi mi bilgisi (donanim gozlem kaniti). */
    kdl_yaz_metin("RTC UIP_BEKLENDI=");
    yaz_ondalik((uint64_t)uip_beklendi);
    kdl_yazdir_satir();

    /* Okunan tarih/saat: YYYY-AA-GG SS:DD:SS bicimi. */
    kdl_yaz_metin("RTC TARIH=");
    yaz_ondalik(yil);
    kdl_yaz_metin("-");
    yaz_iki_hane(ay);
    kdl_yaz_metin("-");
    yaz_iki_hane(gun);
    kdl_yaz_metin(" ");
    yaz_iki_hane(saat);
    kdl_yaz_metin(":");
    yaz_iki_hane(dakika);
    kdl_yaz_metin(":");
    yaz_iki_hane(saniye);
    kdl_yazdir_satir();

    /* Makul kontrol: yil 2024..2099, ay 1..12, gun 1..31. */
    if (yil2 >= KDL_YIL_ALT && yil2 <= KDL_YIL_UST &&
        ay >= 1u && ay <= 12u &&
        gun >= 1u && gun <= 31u) {
        kdl_yazdir_metin("RTC X86 OK");
        kdl_yazdir_satir();
    } else {
        kdl_yazdir_metin("RTC X86 MAKUL DEGIL");
        kdl_yazdir_satir();
    }

    halt();
    return 0;
}
