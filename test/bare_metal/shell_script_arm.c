/*
 * KABUK SCRIPT RUNNER (script/değişken yorumlayıcı) — aarch64 bare-metal.
 *
 * D-188 (shell_arm.c EL1 interaktif kabuk + canlı UART RX) TEMEL. D-188 tek komut
 * okur+dağıtır; BU kabuk bir BETİK YORUMLAYICISI: değişken tablosu + değişken
 * yerine geçme (echo $ad) + sayı↔metin çevrimi + basit döngü (tekrar). Böylece
 * kabuk sadece FS komutları değil, DEĞERLERİ olan bir "script runner"a dönüşür.
 *
 * === Mimari: EL1 kabuk (shell_arm.c ile aynı — en basit + güvenli) ===
 * Kabuk main() ile EL1'de koşar (QEMU virt boot EL1'e düşer, boot/start_aarch64.S
 * kdl_mmu_kur() → identity map → main). EL0 süreç / TTBR swap YOK. Sonuç:
 *   (1) UART RX MMIO (0x09000000, Device sayfa) doğrudan EL1'den okunur.
 *   (2) FS syscall'ları `svc #0` ile çağrılır (EL1'den de kdl_syscall_isle'ye düşer).
 *   (3) Metin çıktısı EL1'den runtime TX helper'ları ile yazılır (syscall gerekmez).
 *
 * === Değişken tablosu ===
 * `set <ad> <deger>` → ad→sayı eşlemesi (küçük lineer dizi, KDL_VAR_MAX slot).
 * `echo <ad>` / `echo $<ad>` → değişkenin sayısal değerini bas. `$` öneki
 * opsiyoneldir (script yazımı ergonomisi). `yaz <dosya> <ad>` değişken değerini
 * ÖNCE metne çevirir (itoa) sonra num-17 (dosya_yaz_metin) ile dosyaya yazar →
 * `oku <dosya>` num-18 ile içeriği (metin "42") geri okur+bas. `tekrar <n> <komut>`
 * kalan satırı n kez çalıştırır (basit sayaç döngüsü).
 *
 * === Güvenlik (D-150/D-151) uyumu ===
 * FS syscall'ları kullanıcı-pointer'larını doğrular (izin = [0x42000000,0x42400000)
 * VEYA .rodata). Bu yüzden: RX satır tamponu + yaz-için-metin tamponu + oku çıktı
 * tamponu KULLANICI-VA aralığında (0x4221xxxx) tutulur. Boot identity map tüm RAM'i
 * EL1-RW yapar → EL1 kabuk bu adreslere serbest yazar/okur, validator kabul eder.
 * `yaz`'da dosya adı da RX satır tamponundan (user VA) geldiği için okuma-validatörü
 * geçer; değeri metne çevirdiğimiz tampon da user VA → içerik okuma-validatörü geçer.
 *
 * === Boot-burst yarışı + PACE'li giriş (shell_arm.c ile aynı deterministiklik) ===
 * `-serial stdio` + pipe: QEMU virt reset'te PL011 FIFO KAPALI → 1-byte holding →
 * burst overrun. İki savunma: (1) FEN=1 (16-byte RX slack), (2) Makefile giriş PACE
 * (lider gecikme + satır-arası gecikme). Kabuk satır-satır CANLI okur.
 *
 * === Deadlock-guard ===
 * satir_oku her byte için RXFE'yi BOUNDED poll eder; giriş biterse EOF (-1) → dur.
 * KDL_MAX_KOMUT ikinci bounded çıkış. Sonsuz bekleme YOK.
 *
 * Kanıt (deterministik sabit betik pipe):
 *   set x 42  → (sessiz)
 *   echo x    → "42"
 *   yaz gunluk x → dosyaya "42" yazar
 *   oku gunluk → "42"
 * "SCRIPT BASLA" + echo "42" + oku "42" + "SCRIPT OK".
 */
#include <stdint.h>

extern void kdl_yazdir_metin(const char *);   /* metin + newline */
extern void kdl_yaz_metin(const char *);       /* metin, newline'sız */
extern void kdl_yazdir_satir(void);
extern void kdl_yaz_tam(int32_t);              /* newline'sız ondalık sayı */

/* --- PL011 UART0 RX (shell_arm.c ile aynı harita) --- */
#define KDL_PL011_BASE    0x09000000UL
#define KDL_PL011_DR      0x00u              /* Data Register (RX/TX byte) */
#define KDL_PL011_FR      0x18u              /* Flag Register */
#define KDL_PL011_LCRH    0x2Cu              /* Satır kontrol (FEN bit4) */
#define KDL_PL011_FR_RXFE (1u << 4)          /* RX FIFO boş (1=boş) */
#define KDL_PL011_LCRH_FEN (1u << 4)         /* FIFO etkin (16-byte RX slack) */

/* Bir byte bekleme sınırı (deadlock-guard): PACE'li girişte satır-arası ~1s
 * gecikmeyi köprüler; giriş bitince bounded düşer → sonsuz bekleme YOK. */
#define KDL_RX_BAYT_SINIR  8000000UL

/* İşlenecek azami komut (giriş kesilmese de sınırlı çalış → deterministik son).
 * `tekrar` de bu sayaçtan bağımsız kendi iç sınırıyla çalışır. */
#define KDL_MAX_KOMUT      12

/* Tamponlar KULLANICI-VA sayfasında (FS syscall D-150/D-151 validator'ları yalnız
 * [0x42000000,0x42400000) VEYA .rodata kabul eder). Ayrı adresler → çakışma yok. */
#define KDL_SATIR_BUF   0x42210000UL         /* RX satır tamponu (user VA) */
#define KDL_CIKTI_BUF   0x42214000UL         /* oku çıktı tamponu (user VA) */
#define KDL_METIN_BUF   0x42218000UL         /* yaz: sayı→metin çevrim tamponu (user VA) */
#define KDL_SATIR_MAX   200                  /* satır azami uzunluk (< blok) */

/* Değişken tablosu: ad→sayı. Küçük sabit dizi + lineer arama (basit + belleksiz). */
#define KDL_VAR_MAX     16                   /* azami değişken sayısı */
#define KDL_AD_UZUN     16                   /* değişken adı azami uzunluk (null dâhil) */

/* Tekrar (döngü) azami yineleme — DoS/patlamayı önle (bounded). */
#define KDL_TEKRAR_MAX  32

static _Noreturn void dur(void) { for (;;) { __asm__ volatile("wfe"); } }

/* MMIO 32-bit oku/yaz (shell_arm.c deseni). */
static inline uint32_t pl011_oku(uint32_t ofs) {
    return *(volatile uint32_t *)(uintptr_t)(KDL_PL011_BASE + ofs);
}
static inline void pl011_yaz(uint32_t ofs, uint32_t deger) {
    *(volatile uint32_t *)(uintptr_t)(KDL_PL011_BASE + ofs) = deger;
}

/* RX FIFO'yu aç (RMW: yalnız LCRH.FEN set). QEMU virt reset FEN=0 → 1-byte holding
 * → burst overrun. FEN=1 → 16-byte FIFO. main'in İLK işi, TX ÖNCESİ. */
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

/* İki null-sonlu string eşit mi. */
static int str_esit(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

/* String kopyala (hedef tamponu KDL_AD_UZUN ile sınırlı, null-sonlu). */
static void str_kopya(char *hedef, const char *kaynak, int azami) {
    int n = 0;
    while (n < azami - 1 && kaynak[n]) { hedef[n] = kaynak[n]; n++; }
    hedef[n] = 0;
}

/* Metin → sayı (basit atoi; negatif işaret desteği). */
static int str_sayi(const char *s) {
    int isaret = 1;
    if (*s == '-') { isaret = -1; s++; }
    int n = 0;
    while (*s >= '0' && *s <= '9') { n = n * 10 + (*s - '0'); s++; }
    return n * isaret;
}

/* Sayı → metin (itoa; buf yeterince büyük — 12 byte int32 için yeter). Dönen: buf. */
static char *sayi_str(int deger, char *buf) {
    char gecici[16];
    int i = 0;
    unsigned int u;
    int negatif = 0;
    if (deger < 0) { negatif = 1; u = (unsigned int)(-(int64_t)deger); }
    else           { u = (unsigned int)deger; }
    if (u == 0) gecici[i++] = '0';
    while (u > 0) { gecici[i++] = (char)('0' + (u % 10u)); u /= 10u; }
    int j = 0;
    if (negatif) buf[j++] = '-';
    while (i > 0) buf[j++] = gecici[--i];       /* ters çevir */
    buf[j] = 0;
    return buf;
}

/* --- Değişken tablosu (ad→sayı, lineer arama) --- */
static char var_adlar[KDL_VAR_MAX][KDL_AD_UZUN];
static int  var_degerler[KDL_VAR_MAX];
static int  var_sayisi = 0;

/* Değişken indeksini bul (yoksa -1). */
static int var_bul(const char *ad) {
    for (int i = 0; i < var_sayisi; i++) {
        if (str_esit(var_adlar[i], ad)) return i;
    }
    return -1;
}

/* Değişken ata (varsa günceller, yoksa ekler; tablo doluysa sessizce yoksay). */
static void var_ata(const char *ad, int deger) {
    int i = var_bul(ad);
    if (i >= 0) { var_degerler[i] = deger; return; }
    if (var_sayisi < KDL_VAR_MAX) {
        str_kopya(var_adlar[var_sayisi], ad, KDL_AD_UZUN);
        var_degerler[var_sayisi] = deger;
        var_sayisi++;
    }
}

/* Değişken değerini oku (yoksa 0 + *bulundu=0). '$' öneki toleranslı. */
static int var_deger(const char *ad, int *bulundu) {
    if (*ad == '$') ad++;                    /* echo $x ve echo x ikisi de olur */
    int i = var_bul(ad);
    if (i < 0) { if (bulundu) *bulundu = 0; return 0; }
    if (bulundu) *bulundu = 1;
    return var_degerler[i];
}

/* Satırı boşluklara böl (in-place null-term), en çok `azami` token; sayı döner. */
static int tokenize(char *satir, char **tok, int azami) {
    int nt = 0;
    char *p = satir;
    while (*p && nt < azami) {
        while (*p == ' ') p++;
        if (!*p) break;
        tok[nt++] = p;
        while (*p && *p != ' ') p++;
        if (*p) *p++ = 0;
    }
    return nt;
}

/* UART RX'ten BİR SATIR CANLI oku (byte-byte). '\n'/'\r' sonlandırır; her byte için
 * RXFE'yi BOUNDED poll eder (deadlock-guard). Dönüş: byte sayısı (>=0) veya -1 = EOF. */
static int satir_oku(char *buf) {
    int n = 0;
    for (;;) {
        uint32_t geldi = 0;
        for (uint64_t i = 0; i < KDL_RX_BAYT_SINIR; i++) {
            if (!(pl011_oku(KDL_PL011_FR) & KDL_PL011_FR_RXFE)) { geldi = 1; break; }
        }
        if (!geldi) {
            buf[n] = 0;
            return n > 0 ? n : -1;
        }
        char c = (char)(pl011_oku(KDL_PL011_DR) & 0xFFu);
        if (c == '\n' || c == '\r') { buf[n] = 0; return n; }
        if (n < KDL_SATIR_MAX) buf[n++] = c;
    }
}

/* İç komut yürütücü (tokenize edilmiş). `tekrar` bunu döngüde çağırır → tek dağıtım
 * noktası. `satir` in-place tokenize edildiği için `tekrar` kalan-metni AYRI tamponda
 * saklar (aşağıda komut_calistir). Dönen anlamsız (etki: yan-etkiler + çıktı). */
static void komut_dagit(char **tok, int nt) {
    char *ciktibuf = (char *)(uintptr_t)KDL_CIKTI_BUF;   /* oku çıktı tamponu (user VA) */
    char *metinbuf = (char *)(uintptr_t)KDL_METIN_BUF;   /* sayı→metin tamponu (user VA) */

    if (nt == 0) return;                                 /* boş — atla */

    if (nt >= 3 && str_esit(tok[0], "set")) {
        /* set <ad> <deger> → değişken ata (sayı). */
        var_ata(tok[1], str_sayi(tok[2]));
    } else if (nt >= 2 && str_esit(tok[0], "echo")) {
        /* echo <ad> / echo $<ad> → değişken değerini bas (yoksa "?"). */
        int bulundu = 0;
        int d = var_deger(tok[1], &bulundu);
        if (bulundu) {
            kdl_yaz_tam((int32_t)d);
            kdl_yazdir_satir();
        } else {
            kdl_yazdir_metin("?");
        }
    } else if (nt >= 3 && str_esit(tok[0], "yaz")) {
        /* yaz <dosya> <ad> → değişken değerini METNE çevir + dosyaya yaz (num 17). */
        int bulundu = 0;
        int d = var_deger(tok[2], &bulundu);
        if (!bulundu) { kdl_yazdir_metin("?"); return; }
        sayi_str(d, metinbuf);                            /* değeri user-VA tampona itoa */
        sys2(17, (uint64_t)(uintptr_t)tok[1], (uint64_t)(uintptr_t)metinbuf);
    } else if (nt >= 2 && str_esit(tok[0], "oku")) {
        /* oku <dosya> → dosya içeriğini (metin) oku + bas (num 18). */
        sys2(18, (uint64_t)(uintptr_t)tok[1], (uint64_t)(uintptr_t)ciktibuf);
        kdl_yaz_metin(ciktibuf);
        kdl_yazdir_satir();
    } else {
        kdl_yazdir_metin("?");                            /* bilinmeyen komut */
    }
}

/* Bir komut satırını çalıştır. `tekrar <n> <komut...>` özel-durumu: kalan metni AYRI
 * bir tamponda (metin çevrim tamponunu geçici olarak değil — kendi yığın tamponu)
 * saklayıp n kez yeniden tokenize eder (in-place tokenize kalan-metni bozar).
 * Diğer komutlar doğrudan komut_dagit'e gider. */
static void komut_calistir(char *satir) {
    char *tok[4];
    int nt = tokenize(satir, tok, 4);
    if (nt == 0) return;

    if (nt >= 3 && str_esit(tok[0], "tekrar")) {
        /* tekrar <n> <komut...> → komutu n kez çalıştır (bounded). tok[2..] kalan
         * komut. Her yinelemede yeniden tokenize gerekir (komut_dagit alt-komutu
         * in-place bozabilir), bu yüzden kalan-metni bir kez ayrı tampona kopyala. */
        int say = str_sayi(tok[1]);
        if (say < 0) say = 0;
        if (say > KDL_TEKRAR_MAX) say = KDL_TEKRAR_MAX;   /* bounded — patlama yok */

        /* tok[2] ve sonrası tek bir alt-komut satırı; tokenize null'larla ayırdı.
         * Alt-komutu yeniden birleştirmek yerine tok[2..nt-1]'i her yinelemede
         * KOPYALAYIP çalıştırırız. Basitlik için alt-komut en çok 2 token (tok[2],
         * tok[3]) — `echo x` / `set x 1` gibi 1-2 argümanlı komutlar için yeter. */
        char altbuf[3][KDL_AD_UZUN];
        int alt_nt = nt - 2;
        if (alt_nt > 3) alt_nt = 3;
        for (int i = 0; i < alt_nt; i++) str_kopya(altbuf[i], tok[2 + i], KDL_AD_UZUN);

        for (int r = 0; r < say; r++) {
            char *altok[3];
            for (int i = 0; i < alt_nt; i++) altok[i] = altbuf[i];
            komut_dagit(altok, alt_nt);
        }
        return;
    }

    komut_dagit(tok, nt);
}

int main(void) {
    /* RX FIFO'yu aç (16-byte slack) — HERHANGİ bir TX'ten ÖNCE (burst overrun'ı önle). */
    pl011_fifo_ac();

    kdl_yazdir_metin("SCRIPT BASLA");
    kdl_yazdir_satir();

    /* Script runner döngüsü: prompt bas → RX'ten BİR SATIR CANLI oku → echo →
     * çalıştır. Sonlanma: EOF (-1) VEYA KDL_MAX_KOMUT — ikisi de bounded. */
    char *satir = (char *)(uintptr_t)KDL_SATIR_BUF;
    int k = 0;
    for (; k < KDL_MAX_KOMUT; k++) {
        kdl_yaz_metin("SCRIPT> ");                    /* prompt (TX) */
        int r = satir_oku(satir);                     /* RX'ten bir satır (canlı) */
        if (r < 0) { kdl_yazdir_satir(); break; }     /* EOF: giriş bitti → dur */
        kdl_yaz_metin(satir);                         /* komut echo'su */
        kdl_yazdir_satir();
        komut_calistir(satir);                        /* tokenize + değişken/FS dağıt */
    }

    kdl_yazdir_metin("SCRIPT OK");                    /* script runner kanıtı */
    kdl_yazdir_satir();
    dur();
    return 0;
}
