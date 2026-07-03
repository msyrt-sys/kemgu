/*
 * İNTERAKTİF KABUK (shell) — aarch64 bare-metal — DONANIM + OS DORUĞU.
 *
 * D-181 (UART RX, uart_rx_arm.c) + D-135 (komut kabuk, kabuk_arm.c) BİRLEŞİMİ.
 * D-135 kabuğu SABİT bir script'i (.user_data) ayrıştırıyordu; bu kabuk komut
 * satırlarını PL011 UART RX'ten CANLI okur → gerçek interaktiflik. Deterministik
 * gate için Makefile stdin'e sabit komut dizisi PIPE'lar (`-serial stdio`).
 *
 * === Mimari: EL1 kabuk (en basit + güvenli) ===
 * Kabuk main() ile EL1'de koşar (QEMU virt boot EL1'e düşer, boot/start_aarch64.S
 * kdl_mmu_kur() → identity map → main). EL0 süreç / TTBR swap YOK — gereksiz
 * karmaşıklık. Sonuç:
 *   (1) UART RX MMIO (0x09000000, Device sayfa) doğrudan EL1'den okunur (uart_rx
 *       gibi) — EL0'dan Device erişimi fault üretirdi, EL1'den serbest.
 *   (2) FS syscall'ları `svc #0` ile çağrılır. SVC EL1'den (Cur-EL-SPx sync,
 *       vektör 0x200) kdl_exc_ortak'a düşer → EC=0x15 → kdl_syscall_isle → eret
 *       geri EL1 çağırana. (boot/start_aarch64.S: 168, 176-183, 196 doğrular.)
 *   (3) Metin çıktısı (prompt/echo/sonuç) EL1'den doğrudan runtime TX helper'ları
 *       ile (kdl_yaz_metin / kdl_yazdir_satir / kdl_uart_pl011_putc / kdl_yaz_tam)
 *       yazılır — syscall gerekmez.
 *
 * === Güvenlik (D-150/D-151) uyumu ===
 * FS syscall'ları kullanıcı-pointer'larını doğrular:
 *   - kdl_user_oku_str_gecerli (num 17/18/21 ad+içerik): izin = [0x42000000,
 *     0x42400000) VEYA kernel .rodata.
 *   - kdl_user_yaz_ptr_gecerli (num 18/20 buf): izin = [0x42000000, 0x42400000).
 * Bu yüzden RX satır tamponu + oku/ls çıktı tamponu KULLANICI-VA aralığında
 * (0x42210000) tutulur. Boot identity map (kdl_mmu.c) tüm RAM'i EL1-RW yapar
 * (0x42200000 bloğu AP=00, EL1 RW) → EL1 kabuk bu adrese serbest yazar/okur,
 * validator da kabul eder. Komut-adı literalleri .rodata'da (kernel) → num-17/18
 * ad-string'i RX tamponundan (user VA) geçtiği için okuma-validatörü geçer.
 *
 * === Boot-burst yarışı + PACE'li giriş (KRİTİK deterministiklik) ===
 * `-serial stdio` + pipe: QEMU stdin'i guest RX'ine besler. QEMU virt reset'te
 * PL011 FIFO KAPALI → RX = 1-byte holding reg → guest okumadan gelen çoklu byte
 * OVERRUN olur. Tüm komut akışını tek burst'te pipe'lamak → guest boot ederken
 * baş bytelar kaybolur, RUN'lar arası deterministik DEĞİL (yaşandı: "yl MHABA",
 * "ls" ilk satır vs.). İKİ savunma birlikte:
 *   (1) FIFO'yu aç (pl011_fifo_ac, FEN=1 → 16-byte RX slack) — herhangi bir TX'ten
 *       ÖNCE. QEMU can_receive FIFO-boşluğu döndürür → dolunca pipe'ı DURDURUR.
 *       Yalnız FEN (RMW) yazılır; CR/UARTEN'e dokunulmaz (LCRH+CR reset yazımı
 *       QEMU'da FIFO'yu FLUSH eder — denendi, sıfır byte).
 *   (2) Makefile girişi PACE eder: LİDER gecikme (guest drain döngüsüne girene
 *       kadar bekle → ilk satır ready guest'e gelir) + SATIR-ARASI gecikme (her
 *       satır < FIFO derinliği drain edilecek kadar boşluk). Sonuç: her satır
 *       ready + boşalmış FIFO'ya damlar → kayıpsız, RUN-tekrarlanır.
 * Kabuk satır-satır CANLI okur (prompt → satir_oku → çalıştır) — sabit script
 * DEĞİL (gerçek interaktiflik).
 *
 * === Deadlock-guard (D-181 dersi) ===
 * satir_oku her byte için RXFE'yi BOUNDED poll eder; giriş biterse spin sınırında
 * düşer → satır başındaysa EOF (-1) döner → kabuk durur. KDL_MAX_KOMUT ikinci
 * bounded çıkış. Sonsuz bekleme YOK.
 *
 * Kanıt: "KABUK BASLA" + "KABUK> " promptları + echo'lu komutlar + oku çıktısı
 *        ("MERHABA") + ls çıktısı ("gunluk") + "SHELL OK".
 */
#include <stdint.h>

extern void kdl_yazdir_metin(const char *);   /* metin + newline */
extern void kdl_yaz_metin(const char *);       /* metin, newline'sız */
extern void kdl_yazdir_satir(void);
extern void kdl_yaz_tam(int32_t);              /* newline'sız ondalık sayı */

/* --- PL011 UART0 RX (uart_rx_arm.c ile aynı harita) --- */
#define KDL_PL011_BASE    0x09000000UL
#define KDL_PL011_DR      0x00u              /* Data Register (RX/TX byte) */
#define KDL_PL011_FR      0x18u              /* Flag Register */
#define KDL_PL011_LCRH    0x2Cu              /* Satır kontrol (FEN bit4) */
#define KDL_PL011_FR_RXFE (1u << 4)          /* RX FIFO boş (1=boş) */
#define KDL_PL011_LCRH_FEN (1u << 4)         /* FIFO etkin (16-byte RX slack) */

/* Bir byte bekleme sınırı (deadlock-guard): RXFE bu kadar spin'de temizlenmezse
 * "giriş bitti" (EOF). PACE'li girişte satır-arası ~1s gecikmeyi köprülemesi için
 * yeterince BÜYÜK (aksi halde satır ortasında/başında erken EOF); giriş gerçekten
 * bitince (son satır sonrası) bounded düşer → sonsuz bekleme YOK. Wall-clock'ta
 * emülasyon-hızına göre ayarlı (8M spin > pipe satır-arası gap; 3× doğrulandı). */
#define KDL_RX_BAYT_SINIR  8000000UL

/* İşlenecek azami komut (giriş kesilmese de sınırlı çalış → deterministik son). */
#define KDL_MAX_KOMUT      8

/* Tamponlar KULLANICI-VA sayfasında: FS syscall pointer validator'ları
 * (D-150/D-151) yalnız [0x42000000,0x42400000) VEYA .rodata kabul eder. RX'ten
 * gelen ad/içerik string'i bu adreste → validator geçer. Boot identity map
 * (kdl_mmu.c) bu bloğu (L2[17], AP=00) EL1-RW yapar. */
#define KDL_SATIR_BUF   0x42210000UL         /* RX satır tamponu (user VA) */
#define KDL_CIKTI_BUF   0x42214000UL         /* oku/ls çıktı tamponu (user VA) */
#define KDL_SATIR_MAX   200                  /* satır azami uzunluk (< blok) */

static _Noreturn void dur(void) { for (;;) { __asm__ volatile("wfe"); } }

/* MMIO 32-bit oku/yaz (uart_rx_arm.c deseni). */
static inline uint32_t pl011_oku(uint32_t ofs) {
    return *(volatile uint32_t *)(uintptr_t)(KDL_PL011_BASE + ofs);
}
static inline void pl011_yaz(uint32_t ofs, uint32_t deger) {
    *(volatile uint32_t *)(uintptr_t)(KDL_PL011_BASE + ofs) = deger;
}

/* RX FIFO'yu aç (RMW: yalnız LCRH.FEN set, diğer bitler korunur). QEMU virt
 * reset'te FEN=0 → RX tek-byte holding reg → pipe-burst overrun. FEN=1 → 16-byte
 * RX FIFO → guest okurken QEMU can_receive boşluğu döndürür, backpressure ile
 * damla damla besler, overrun azalır. Yalnız FEN yazılır (CR/UARTEN'e dokunulmaz
 * → UART reset/flush yok). main'in İLK işi olarak, TX ÖNCESİ çağrılır. */
static void pl011_fifo_ac(void) {
    uint32_t lcrh = pl011_oku(KDL_PL011_LCRH);
    pl011_yaz(KDL_PL011_LCRH, lcrh | KDL_PL011_LCRH_FEN);
}

/* --- 2-argümanlı FS syscall (SVC ABI: x8=num, x0=arg0, x1=arg1 → x0=dönüş) --- */
static inline uint64_t sys2(uint64_t num, uint64_t a0, uint64_t a1) {
    register uint64_t x8 __asm__("x8") = num;
    register uint64_t x0 __asm__("x0") = a0;
    register uint64_t x1 __asm__("x1") = a1;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8), "r"(x1) : "memory");
    return x0;
}
static inline uint64_t sys1(uint64_t num, uint64_t a0) {
    return sys2(num, a0, 0);
}

/* İki null-sonlu string eşit mi (kabuk_arm.c str_esit ile aynı). */
static int str_esit(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

/* Satırı boşluklara böl (in-place null-term), en çok 3 token; sayı döner
 * (kabuk_arm.c tokenize ile aynı). */
static int tokenize(char *satir, char **tok) {
    int nt = 0;
    char *p = satir;
    while (*p && nt < 3) {
        while (*p == ' ') p++;
        if (!*p) break;
        tok[nt++] = p;
        while (*p && *p != ' ') p++;
        if (*p) *p++ = 0;
    }
    return nt;
}

/* UART RX'ten BİR SATIR CANLI oku (byte-byte). '\n'/'\r' satırı sonlandırır.
 * Her byte için RXFE'yi BOUNDED poll eder (deadlock-guard): PACE'li girişte
 * satır-arası gecikmeyi köprüler (spin sınırı 1s gap'ten uzun sürer). Byte
 * gelmezse (giriş bitti) sınırda düşer.
 *
 * Dönüş: okunan byte sayısı (>=0, satır null-sonlu), veya -1 = EOF (satır başında
 * hiç byte gelmeden giriş tükendi → kabuk durur). Satır sonu echo EDİLMEZ;
 * yürütme döngüsü komut echo'sunu + kendi newline'ını basar. */
static int satir_oku(char *buf) {
    int n = 0;
    for (;;) {
        uint32_t geldi = 0;
        for (uint64_t i = 0; i < KDL_RX_BAYT_SINIR; i++) {
            if (!(pl011_oku(KDL_PL011_FR) & KDL_PL011_FR_RXFE)) { geldi = 1; break; }
        }
        if (!geldi) {
            /* Byte gelmedi: satır ortasındaysak elde olanı ver; başındaysak EOF. */
            buf[n] = 0;
            return n > 0 ? n : -1;
        }
        char c = (char)(pl011_oku(KDL_PL011_DR) & 0xFFu);
        if (c == '\n' || c == '\r') { buf[n] = 0; return n; }   /* satır sonu */
        if (n < KDL_SATIR_MAX) buf[n++] = c;                    /* taşma-korumalı */
    }
}

/* Bir komut satırını çalıştır (kabuk_arm.c dispatch mantığı; FS syscall'ları). */
static void komut_calistir(char *satir) {
    char *buf = (char *)(uintptr_t)KDL_CIKTI_BUF;    /* oku/ls çıktı tamponu (user VA) */
    char *tok[3];
    int nt = tokenize(satir, tok);
    if (nt == 0) return;                             /* boş satır — atla */

    if (nt >= 3 && str_esit(tok[0], "yaz")) {
        /* dosya_yaz_metin(ad=tok[1], metin=tok[2]) → num 17. */
        sys2(17, (uint64_t)(uintptr_t)tok[1], (uint64_t)(uintptr_t)tok[2]);
    } else if (nt >= 2 && str_esit(tok[0], "oku")) {
        /* dosya_oku_metin(ad=tok[1], buf) → num 18 + çıktı bas. */
        sys2(18, (uint64_t)(uintptr_t)tok[1], (uint64_t)(uintptr_t)buf);
        kdl_yaz_metin("  ");
        kdl_yaz_metin(buf);
        kdl_yazdir_satir();
    } else if (nt >= 1 && str_esit(tok[0], "ls")) {
        /* dosya_sayisi() num 19; her ad dosya_ad(i, buf) num 20 + bas. */
        uint64_t n = sys1(19, 0);
        for (uint64_t i = 0; i < n; i++) {
            sys2(20, i, (uint64_t)(uintptr_t)buf);
            kdl_yaz_metin("  ");
            kdl_yaz_metin(buf);
            kdl_yazdir_satir();
        }
    } else if (nt >= 1 && str_esit(tok[0], "say")) {
        /* dosya_sayisi() → adet bas (ekstra kapsam). */
        kdl_yaz_metin("COUNT=");
        kdl_yaz_tam((int32_t)sys1(19, 0));
        kdl_yazdir_satir();
    } else {
        kdl_yazdir_metin("?");                        /* bilinmeyen komut */
    }
}

int main(void) {
    /* RX FIFO'yu aç (16-byte slack) — HERHANGİ bir TX'ten ÖNCE. QEMU virt PL011
     * reset'te FIFO kapalı (1-byte holding reg) → pipe-burst overrun. FEN=1 slack
     * verir + QEMU can_receive backpressure ile besler. Yalnız FEN (RMW) → UART
     * reset/flush yok. */
    pl011_fifo_ac();

    kdl_yazdir_metin("KABUK BASLA");
    kdl_yazdir_satir();

    /* İnteraktif kabuk döngüsü: prompt bas → RX'ten BİR SATIR CANLI oku → echo →
     * çalıştır → tekrar. Makefile girişi lider-gecikme + satır-arası gecikme ile
     * PACE eder (boot yarışını ve FIFO overrun'ı önler → deterministik). Sonlanma:
     * satir_oku EOF (-1) döndürünce (giriş tükendi) VEYA KDL_MAX_KOMUT sınırı —
     * her ikisi de bounded → sonsuz bekleme YOK. */
    char *satir = (char *)(uintptr_t)KDL_SATIR_BUF;
    int k = 0;
    for (; k < KDL_MAX_KOMUT; k++) {
        kdl_yaz_metin("KABUK> ");                     /* prompt (TX) */
        int r = satir_oku(satir);                     /* RX'ten bir satır (canlı) */
        if (r < 0) { kdl_yazdir_satir(); break; }     /* EOF: giriş bitti → dur */
        kdl_yaz_metin(satir);                         /* komut echo'su */
        kdl_yazdir_satir();
        komut_calistir(satir);                        /* tokenize + FS syscall dağıt */
    }

    kdl_yazdir_metin("SHELL OK");                     /* interaktif kabuk kanıtı */
    kdl_yazdir_satir();
    dur();
    return 0;
}
