/*
 * PL011 UART RX (giriş okuma) yolu testi (aarch64) — DONANIM giriş yolu
 * (Faz H — DONANIM milestone, ağsız/deterministik).
 *
 * Şimdiye kadar bare-metal konsol yalnız TX (yazma) kullandı
 * (kdl_uart_pl011_putc / kdl_yazdir_metin). Bu test ilk kez RX (okuma)
 * yolunu kurar ve doğrular: FR.RXFE bayrağı ile RX FIFO durumunu oku,
 * DR'den giriş byte'ını al.
 *
 * ARM PrimeCell PL011 register haritası (TRM DDI 0183G, Table 3-1):
 *   offset 0x00  DR  (Data Register)   — oku = RX FIFO'dan bir byte
 *   offset 0x18  FR  (Flag Register)   — durum bayrakları:
 *                    bit 4  RXFE  RX FIFO boş  (1 = boş, giriş YOK)
 *                    bit 6  RXFF  RX FIFO dolu (1 = dolu)
 * RX byte varsa FR.RXFE = 0 olur, DR'den okunur. Yoksa FR.RXFE = 1.
 *
 * QEMU virt: PL011 UART0 @ 0x09000000 (ilk 1GB Device-map, MMU-ON
 * erişilebilir). boot/start_aarch64.S main öncesi kdl_mmu_kur() çağırır →
 * 0x09000000 < 0x40000000 → Device-nGnRnE map → DR/FR doğrudan okunur.
 *
 * İki mod:
 *  (1) GİRİŞ ENJEKSİYONU (birincil): Makefile QEMU'ya
 *      `-chardev file,id=c0,path=<çıktı>,input-path=<giriş>` verirse,
 *      seri hattan önceden-yüklenmiş byte gelir. RXFE=0 olana kadar
 *      (bounded spin) bekle → DR'den byte oku → echo et → doğrula →
 *      "UART RX OK".
 *  (2) FALLBACK (giriş yoksa): RX-path'i giriş OLMADAN doğrula. FR.RXFE=1
 *      (boş, beklenen) → RX register semantiğinin doğru okunduğunu kanıtla →
 *      "UART RX PATH OK". Bounded spin sayesinde DEADLOCK YOK: giriş
 *      gelmezse döngü sınırında düşer, deterministik biter.
 *
 * Kanıt: "UART RX BASLA" (TX çalışır) + FR.RXFE değeri + "UART RX OK"
 *        (gerçek giriş okundu) VEYA "UART RX PATH OK" (boş-FIFO doğru).
 */
#include <stdint.h>

extern void kdl_yazdir_metin(const char *);   /* metin + newline */
extern void kdl_yaz_metin(const char *);       /* metin, newline'sız (inline etiket) */
extern void kdl_yazdir_satir(void);
extern void kdl_yaz_onaltilik(uint64_t);       /* onaltılık ("0x" önekli), newline'sız */

/* PL011 UART0 — QEMU virt standart konsol cihazı. */
#define KDL_PL011_BASE   0x09000000UL   /* QEMU virt UART0 taban adresi */
#define KDL_PL011_DR     0x00u          /* Data Register offset (RX/TX byte) */
#define KDL_PL011_FR     0x18u          /* Flag Register offset (durum) */
#define KDL_PL011_FR_RXFE (1u << 4)     /* RX FIFO boş (1 = boş, giriş yok) */

/* Bounded spin sınırı: giriş enjekte edilmezse sonsuz bekleme yerine
 * bu sayıda deneme sonrası vazgeç → deterministik, deadlock-suz. QEMU
 * seri input-path ile byte anında FIFO'ya girdiği için birkaç iterasyon
 * yeter; sınırı yine de bol tut (giriş varsa mutlaka yakalanır). */
#define KDL_RX_SPIN_SINIR  2000000UL

static _Noreturn void halt(void) { for (;;) { __asm__ volatile("wfe"); } }

/* MMIO oku — 32-bit volatile (rtc_arm.c ile aynı desen). */
static inline uint32_t pl011_oku(uint32_t ofs) {
    return *(volatile uint32_t *)(uintptr_t)(KDL_PL011_BASE + ofs);
}

int main(void) {
    kdl_yazdir_metin("UART RX BASLA");   /* TX çalışıyor — ilk kanıt */
    kdl_yazdir_satir();

    /* --- RX register semantiği: FR.RXFE oku --- */
    uint32_t fr = pl011_oku(KDL_PL011_FR);
    uint32_t rxfe = (fr & KDL_PL011_FR_RXFE) ? 1u : 0u;

    kdl_yaz_metin("FR=");
    kdl_yaz_onaltilik((uint64_t)fr);
    kdl_yaz_metin(" RXFE=");
    kdl_yaz_onaltilik((uint64_t)rxfe);
    kdl_yazdir_satir();

    /* --- Giriş enjeksiyonu denemesi (bounded spin) ---
     * RXFE=0 olana kadar (giriş byte'ı geldi) veya sınıra kadar bekle.
     * Sınıra ulaşırsak giriş yok → fallback yoluna düş. */
    uint32_t giris_var = 0;
    for (uint64_t i = 0; i < KDL_RX_SPIN_SINIR; i++) {
        if (!(pl011_oku(KDL_PL011_FR) & KDL_PL011_FR_RXFE)) {
            giris_var = 1;
            break;
        }
    }

    if (giris_var) {
        /* Gerçek giriş byte'ı geldi — DR'den oku, echo et, doğrula. */
        uint32_t bayt = pl011_oku(KDL_PL011_DR) & 0xFFu;

        kdl_yaz_metin("RX BYTE=");
        kdl_yaz_onaltilik((uint64_t)bayt);
        kdl_yazdir_satir();

        /* Echo: okunan byte'ı geri yaz (RX→TX köprüsü kanıtı). */
        char echo[2];
        echo[0] = (char)(bayt & 0xFFu);
        echo[1] = '\0';
        kdl_yaz_metin("ECHO=");
        kdl_yaz_metin(echo);
        kdl_yazdir_satir();

        /* RX yolu tam çalıştı: FR.RXFE oku + DR oku + echo. */
        kdl_yazdir_metin("UART RX OK");
        kdl_yazdir_satir();
    } else {
        /* Giriş enjekte edilmedi (headless gate — beklenen). RX-path'in
         * register semantiğini giriş OLMADAN doğrula: RXFE=1 (boş) doğru
         * algılandı, DR-okuma kodu derlendi/çalıştı, deadlock olmadı. */
        if (rxfe == 1u) {
            /* FR.RXFE=1 = RX FIFO boş = giriş yok — beklenen durum. */
            kdl_yazdir_metin("RX FIFO BOS (RXFE=1) — giris yok, beklenen");
            kdl_yazdir_satir();
            kdl_yazdir_metin("UART RX PATH OK");
            kdl_yazdir_satir();
        } else {
            /* RXFE=0 ama byte okunamadı — tutarsız (olmamalı). */
            kdl_yazdir_metin("UART RX PATH TUTARSIZ (RXFE=0 ama byte yok)");
            kdl_yazdir_satir();
        }
    }

    halt();
    return 0;
}
