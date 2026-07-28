#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "ast_yazdir.h"
#include "arena.h"
#include "tip.h"
#include "sembol.h"
#include "tip_kontrol.h"
#include "llvm.h"
#include "lsp.h"
#include "wcet.h"
#include "hata.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>   /* MultiByteToWideChar — UTF-8 dosya yolu (kütüphane/) */
#include <wchar.h>
#endif

/*
 * KEMGU CLI:
 *   kemgu [--token | --parse | --check | --llvm] [dosya]
 *
 * Mod yoksa (default): --check
 *
 * --token: lexer akisini yazdir
 * --parse: parser calistir + AST yazdir
 * --check: parser + tip kontrol (default)
 * --llvm:  parser + LLVM IR text yazdir
 *
 * Dosya argumani yoksa stdin'den okur.
 */

typedef enum { MOD_TOKEN, MOD_PARSE, MOD_AST, MOD_CHECK, MOD_CHECKDUMP, MOD_LLVM, MOD_LSP } Mod;

static char *dosya_oku(const char *dosya_adi) {
    FILE *f = fopen(dosya_adi, "rb");
    if (!f) {
        fprintf(stderr, "Dosya acilamadi: %s\n", dosya_adi);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long boyut = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (boyut < 0) { fclose(f); return NULL; }
    char *tampon = (char *)malloc((size_t)boyut + 1);
    if (!tampon) { fclose(f); return NULL; }
    size_t okunan = fread(tampon, 1, (size_t)boyut, f);
    tampon[okunan] = '\0';
    fclose(f);
    return tampon;
}

static char *stdin_oku(void) {
    size_t kapasite = 4096;
    size_t uzunluk = 0;
    char *tampon = (char *)malloc(kapasite);
    if (!tampon) return NULL;
    int c;
    while ((c = getchar()) != EOF) {
        if (uzunluk + 1 >= kapasite) {
            kapasite *= 2;
            char *yeni = (char *)realloc(tampon, kapasite);
            if (!yeni) { free(tampon); return NULL; }
            tampon = yeni;
        }
        tampon[uzunluk++] = (char)c;
    }
    tampon[uzunluk] = '\0';
    return tampon;
}

/* Self-host diff-oracle formatı (D-035 ADIM-0 / lexer-M1):
 *   <TIP>\t<satır>\t<sütün>\t<offset>\t<uzunluk>
 * Eski format (`%-20s "%.*s"\t\t%d:%d`) ham lexeme'i gömüyordu — string
 * literal'deki `"`/newline formatı bozardı + 20-char padding/çift-tab parse-zor.
 * Yeni format: ham lexeme YOK (offset+uzunluk'tan kaynaktan kurtarılır) →
 * kaçış-kopyalama riski sıfır; tek-tab → makine-parse-edilebilir; KEMGU-lexer
 * birebir aynı satırı üretir → `diff` = otomatik doğruluk (sıfır-diff oracle). */
static int mode_token(const char *kaynak, const char *dosya_adi) {
    Lexer l;
    lexer_baslat(&l, kaynak, dosya_adi);
    Token t;
    do {
        t = lexer_sonraki_token(&l);
        long ofset = (long)(t.baslangic - kaynak);
        printf("%s\t%d\t%d\t%ld\t%d\n",
               token_tipi_adi(t.tip), t.satir, t.sutun, ofset, t.uzunluk);
    } while (t.tip != TOK_DOSYA_SONU);
    return 0;
}

static int mode_parse(const char *kaynak, const char *dosya_adi) {
    Arena *a = arena_olustur(0);
    if (!a) {
        fprintf(stderr, "Arena olusturulamadi\n");
        return 1;
    }

    Lexer l;
    lexer_baslat(&l, kaynak, dosya_adi);
    Parser p;
    parser_baslat(&p, &l, a, dosya_adi, kaynak);

    Dugum *prog = parser_calistir(&p);

    printf("=== AST ===\n");
    ast_yazdir(prog, stdout);
    printf("\n=== Toplam parser hata sayisi: %d ===\n", p.hata_sayisi);

    int rc = (p.hata_sayisi > 0) ? 1 : 0;
    arena_serbest(a);
    return rc;
}

/* --ast: DÜZ AST dump (self-host parser diff-oracle, D-043). Yalnız ağaç —
 * başlık/özet yok (--parse insan-okunur; --ast makine/diff). */
static int mode_ast(const char *kaynak, const char *dosya_adi) {
    Arena *a = arena_olustur(0);
    if (!a) { fprintf(stderr, "Arena olusturulamadi\n"); return 1; }
    Lexer l;
    lexer_baslat(&l, kaynak, dosya_adi);
    Parser p;
    parser_baslat(&p, &l, a, dosya_adi, kaynak);
    Dugum *prog = parser_calistir(&p);
    ast_duz_yaz(prog, stdout, 0);
    int rc = (p.hata_sayisi > 0) ? 1 : 0;
    arena_serbest(a);
    return rc;
}

/* ============================================================
 * A: Cok-dosya modul yukleyici (whole-program, iki-fazli)
 * ============================================================
 *
 * Giris dosyasinin yeni-bicim 'kullan' grafigini BFS ile gezer:
 * her erisilebilir modul dosyasi BIR KEZ parse edilir ve sentetik
 * DUGUM_MODUL (dosya_modulu=1) olarak program AST'sinin BASINA
 * eklenir. Ardindan tip_kontrol_program tek namespaced sembol
 * tablosunu kurar (faz-1 kayit + faz-2 kullan baglari) ve B'nin
 * resolver'i capraz-dosya adlari MODUL_UYESI binding'iyle cozer;
 * codegen ayni AST'den tum modulleri @modul.ad olarak emit eder.
 *
 * Arama yolu (ILK eslesme kazanir):
 *   1) ice-aktaran dosyanin dizini
 *   2) proje koku (cwd)
 *   3) kütüphane/
 * Modul = dosya: dizi.kem => modul 'dizi'. Ayni ada ikinci yukleme
 * yok (ad bazli dedup — dongusel import dogal olarak sonlanir).
 * Cok-segment ciplak 'kullan a::b;' LEGACY duzlestirme yolundadir
 * (tip_kontrol/llvm icindeki eski yol) — buraya girmez. */

/* fopen Windows'ta ANSI codepage kullanir — UTF-8 yol (kütüphane/)
 * bozulur. UTF-8 -> UTF-16 cevirip _wfopen. */
static FILE *dosya_ac_utf8(const char *yol, const char *kip) {
#ifdef _WIN32
    wchar_t wyol[1024];
    wchar_t wkip[8];
    if (MultiByteToWideChar(CP_UTF8, 0, yol, -1, wyol, 1024) <= 0) {
        return fopen(yol, kip);
    }
    if (MultiByteToWideChar(CP_UTF8, 0, kip, -1, wkip, 8) <= 0) {
        return fopen(yol, kip);
    }
    return _wfopen(wyol, wkip);
#else
    return fopen(yol, kip);
#endif
}

/* Dosya icerigini arena'ya oku; NULL = acilamadi. */
static char *dosya_icerik_oku(Arena *a, const char *yol) {
    FILE *f = dosya_ac_utf8(yol, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long boyut = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (boyut < 0) { fclose(f); return NULL; }
    char *icerik = (char *)arena_ayir(a, (size_t)boyut + 1);
    if (!icerik) { fclose(f); return NULL; }
    size_t okunan = fread(icerik, 1, (size_t)boyut, f);
    icerik[okunan] = '\0';
    fclose(f);
    return icerik;
}

/* yol'un dizin kismini out'a yaz ("a/b/c.kem" -> "a/b"); dizin yoksa "." */
static void dizin_al(const char *yol, char *out, size_t n) {
    int son = -1;
    for (int i = 0; yol[i]; i++) {
        if (yol[i] == '/' || yol[i] == '\\') son = i;
    }
    if (son < 0 || (size_t)son + 1 >= n) {
        snprintf(out, n, ".");
        return;
    }
    memcpy(out, yol, (size_t)son);
    out[son] = '\0';
}

static int kullan_yeni_bicim(const Dugum *k) {
    return k->veri.kullan.segment_sayi <= 1 ||
           k->veri.kullan.secili_sayi > 0 ||
           k->veri.kullan.alias_ad != NULL;
}

typedef struct KullanIsi {
    const Dugum *k;               /* yeni-bicim DUGUM_KULLAN */
    const char *ithalatci_yol;    /* iceren dosyanin yolu (arama + hata) */
    const char *ithalatci_kaynak; /* hata konum raporu icin */
} KullanIsi;

typedef struct YukluAd {
    const char *ad;
    int uz;
    struct YukluAd *sonraki;
} YukluAd;

/* prog uyelerini yeni-bicim kullan'larla isi listesine ekle */
static int kullan_isleri_topla(Arena *a, Dugum *const *uyeler, int sayi,
                               const char *yol, const char *kaynak,
                               KullanIsi **isler, int *is_sayi, int *kap) {
    for (int i = 0; i < sayi; i++) {
        const Dugum *u = uyeler[i];
        if (!u || u->tip != DUGUM_KULLAN) continue;
        if (!kullan_yeni_bicim(u)) continue;  /* legacy yol */
        if (*is_sayi == *kap) {
            int yeni_kap = *kap == 0 ? 16 : *kap * 2;
            KullanIsi *yeni = (KullanIsi *)arena_ayir(a,
                sizeof(KullanIsi) * (size_t)yeni_kap);
            if (!yeni) return 0;
            if (*isler) {
                memcpy(yeni, *isler, sizeof(KullanIsi) * (size_t)*is_sayi);
            }
            *isler = yeni;
            *kap = yeni_kap;
        }
        (*isler)[*is_sayi].k = u;
        (*isler)[*is_sayi].ithalatci_yol = yol;
        (*isler)[*is_sayi].ithalatci_kaynak = kaynak;
        (*is_sayi)++;
    }
    return 1;
}

/* Faz-1: kesfet + parse + sentetik DUGUM_MODUL olarak prog'a ekle.
 * Hata sayisi doner (0 = temiz). prog mutate edilir. */
static int modulleri_yukle(Arena *a, Dugum *prog,
                           const char *giris_yolu, const char *giris_kaynak) {
    if (!prog || prog->tip != DUGUM_PROGRAM) return 0;

    KullanIsi *isler = NULL;
    int is_sayi = 0, is_kap = 0;
    YukluAd *yuklu = NULL;
    Dugum **moduller = NULL;
    int modul_sayi = 0, modul_kap = 0;
    int hata = 0;

    kullan_isleri_topla(a, prog->veri.program.uyeler,
                        prog->veri.program.sayi,
                        giris_yolu, giris_kaynak,
                        &isler, &is_sayi, &is_kap);

    for (int wi = 0; wi < is_sayi; wi++) {
        const Dugum *k = isler[wi].k;
        const char *mad = k->veri.kullan.yol;
        int muz = k->veri.kullan.yol_uzunluk;
        if (!mad || muz <= 0) continue;

        /* Ad bazli dedup (dongusel/elmas import sonlanir) */
        int zaten = 0;
        for (YukluAd *y = yuklu; y; y = y->sonraki) {
            if (y->uz == muz && memcmp(y->ad, mad, (size_t)muz) == 0) {
                zaten = 1;
                break;
            }
        }
        if (zaten) continue;

        /* Arama yolu: ithalatci dizini -> proje koku -> kütüphane/ */
        char aday[1024];
        char dizin[512];
        dizin_al(isler[wi].ithalatci_yol, dizin, sizeof(dizin));
        char *icerik = NULL;
        const char *bulunan_yol = NULL;
        char *yol_kalici = NULL;
        for (int deneme = 0; deneme < 3 && !icerik; deneme++) {
            if (deneme == 0) {
                snprintf(aday, sizeof(aday), "%s/%.*s.kem", dizin, muz, mad);
            } else if (deneme == 1) {
                snprintf(aday, sizeof(aday), "%.*s.kem", muz, mad);
            } else {
                snprintf(aday, sizeof(aday),
                         "k\xc3\xbct\xc3\xbcphane/%.*s.kem", muz, mad);
            }
            /* Giris dosyasinin kendisi modul olarak yuklenemez */
            if (strcmp(aday, giris_yolu) == 0) continue;
            icerik = dosya_icerik_oku(a, aday);
        }
        if (!icerik) {
            hata_raporla(isler[wi].ithalatci_yol, isler[wi].ithalatci_kaynak,
                         k->satir, k->sutun, "T040",
                         "kullan: mod\xc3\xbcl dosyas\xc4\xb1 bulunamad\xc4\xb1",
                         "arama yolu: dosya dizini, proje k\xc3\xb6k\xc3\xbc, "
                         "k\xc3\xbct\xc3\xbcphane/");
            hata++;
            continue;
        }
        bulunan_yol = aday;

        /* Yol kalici kopya (parser hata mesajlari + ithalatci dizini) */
        {
            size_t yuz = strlen(bulunan_yol);
            yol_kalici = (char *)arena_ayir(a, yuz + 1);
            if (!yol_kalici) { hata++; continue; }
            memcpy(yol_kalici, bulunan_yol, yuz + 1);
        }

        /* Parse */
        Lexer ml;
        lexer_baslat(&ml, icerik, yol_kalici);
        Parser mp;
        parser_baslat(&mp, &ml, a, yol_kalici, icerik);
        Dugum *fprog = parser_calistir(&mp);
        if (!fprog || mp.hata_sayisi > 0) {
            hata += mp.hata_sayisi > 0 ? mp.hata_sayisi : 1;
            continue;
        }

        /* Sentetik dosya-modul dugumu */
        Dugum *md = dugum_olustur(a, DUGUM_MODUL, k->satir, k->sutun);
        if (!md) { hata++; continue; }
        md->veri.modul.ad = mad;   /* kullan.yol arena'da null-terminated */
        md->veri.modul.ad_uzunluk = muz;
        md->veri.modul.uyeler = fprog->veri.program.uyeler;
        md->veri.modul.sayi = fprog->veri.program.sayi;
        md->veri.modul.dosya_modulu = 1;

        if (modul_sayi == modul_kap) {
            int yeni_kap = modul_kap == 0 ? 8 : modul_kap * 2;
            Dugum **yeni = (Dugum **)arena_ayir(a,
                sizeof(Dugum *) * (size_t)yeni_kap);
            if (!yeni) { hata++; continue; }
            if (moduller) {
                memcpy(yeni, moduller, sizeof(Dugum *) * (size_t)modul_sayi);
            }
            moduller = yeni;
            modul_kap = yeni_kap;
        }
        moduller[modul_sayi++] = md;

        /* Yuklu olarak isaretle */
        YukluAd *ya = (YukluAd *)arena_ayir_sifir(a, sizeof(YukluAd));
        if (ya) {
            ya->ad = mad;
            ya->uz = muz;
            ya->sonraki = yuklu;
            yuklu = ya;
        }

        /* Transitif: yuklenen dosyanin kendi kullan'lari */
        kullan_isleri_topla(a, fprog->veri.program.uyeler,
                            fprog->veri.program.sayi,
                            yol_kalici, icerik,
                            &isler, &is_sayi, &is_kap);
    }

    /* Splice: [dosya-moduller..., orijinal uyeler...] */
    if (modul_sayi > 0) {
        int eski_sayi = prog->veri.program.sayi;
        Dugum **yeni = (Dugum **)arena_ayir(a,
            sizeof(Dugum *) * (size_t)(modul_sayi + eski_sayi));
        if (yeni) {
            memcpy(yeni, moduller, sizeof(Dugum *) * (size_t)modul_sayi);
            memcpy(yeni + modul_sayi, prog->veri.program.uyeler,
                   sizeof(Dugum *) * (size_t)eski_sayi);
            prog->veri.program.uyeler = yeni;
            prog->veri.program.sayi = modul_sayi + eski_sayi;
        }
    }
    return hata;
}

static int mode_check(const char *kaynak, const char *dosya_adi) {
    Arena *a = arena_olustur(0);
    if (!a) {
        fprintf(stderr, "Arena olusturulamadi\n");
        return 1;
    }

    Lexer l;
    lexer_baslat(&l, kaynak, dosya_adi);
    Parser p;
    parser_baslat(&p, &l, a, dosya_adi, kaynak);
    Dugum *prog = parser_calistir(&p);

    int parser_hata = p.hata_sayisi;
    int yukleme_hata = 0;
    int tk_hata = 0;
    int rt_hata = 0;

    if (parser_hata == 0 && prog) {
        /* A: cok-dosya modul yukleme (kesfet + parse + splice) —
         * tip kontrolu tum modulleri tek tabloda gorur. */
        yukleme_hata = modulleri_yukle(a, prog, dosya_adi, kaynak);

        Scope *g = scope_olustur(a, SCOPE_GLOBAL, NULL);
        TipKontrol tk;
        tip_kontrol_baslat(&tk, a, g, dosya_adi, kaynak);
        tip_kontrol_program(&tk, prog);
        tk_hata = tk.hata_sayisi;

        /* Realtime Spec V1: gerçekzamanlı işlevler icin RT001-RT005
         * denetimi + WCET hesabı (sembol tablosu hazır olunca). */
        WcetKontrol wk;
        wcet_kontrol_baslat(&wk, a, g, dosya_adi, kaynak);
        wcet_kontrol_program(&wk, prog);
        rt_hata = wk.hata_sayisi;
    }

    int toplam = parser_hata + yukleme_hata + tk_hata + rt_hata;
    if (toplam == 0) {
        fprintf(stdout, "OK: %s — tip kontrolu basarili.\n", dosya_adi);
    } else {
        fprintf(stdout,
                "HATA: %s — parser %d, yukleme %d, tip kontrol %d, "
                "realtime %d (toplam %d hata).\n",
                dosya_adi, parser_hata, yukleme_hata, tk_hata, rt_hata,
                toplam);
    }

    arena_serbest(a);
    return toplam > 0 ? 1 : 0;
}

/* --checkdump: SELF-HOST tip denetleyici diff-oracle (Aşama 2, D-051).
 * Tip-kontrol hatalarını DÜZ formatta dök: <KOD>\t<satır>\t<sütün> (callback
 * sırasıyla), hata yoksa "OK". KEMGU-checker aynı çıktıyı üretecek → accept/reject
 * + tanı paritesi. (Parser/yükleme/wcet hataları da toplanır; TC korpusu temiz
 * parse eder → yalnız T/L/M kodları.) */
typedef struct { char kod[24]; int satir; int sutun; } CheckHata;
static CheckHata g_check_hatalar[8192];
static int g_check_hata_sayi = 0;
static void check_topla_cb(int satir, int sutun, const char *kod,
                           const char *mesaj, const char *ipucu, void *ctx) {
    (void)mesaj; (void)ipucu; (void)ctx;
    if (g_check_hata_sayi < 8192) {
        CheckHata *h = &g_check_hatalar[g_check_hata_sayi++];
        snprintf(h->kod, sizeof(h->kod), "%s", kod ? kod : "?");
        h->satir = satir; h->sutun = sutun;
    }
}
static int mode_checkdump(const char *kaynak, const char *dosya_adi) {
    Arena *a = arena_olustur(0);
    if (!a) { fprintf(stderr, "Arena olusturulamadi\n"); return 1; }
    g_check_hata_sayi = 0;
    hata_callback_ayarla(check_topla_cb, NULL);
    Lexer l;
    lexer_baslat(&l, kaynak, dosya_adi);
    Parser p;
    parser_baslat(&p, &l, a, dosya_adi, kaynak);
    Dugum *prog = parser_calistir(&p);
    if (p.hata_sayisi == 0 && prog) {
        modulleri_yukle(a, prog, dosya_adi, kaynak);
        Scope *g = scope_olustur(a, SCOPE_GLOBAL, NULL);
        TipKontrol tk;
        tip_kontrol_baslat(&tk, a, g, dosya_adi, kaynak);
        tip_kontrol_program(&tk, prog);
        WcetKontrol wk;
        wcet_kontrol_baslat(&wk, a, g, dosya_adi, kaynak);
        wcet_kontrol_program(&wk, prog);
    }
    hata_callback_ayarla(NULL, NULL);
    if (g_check_hata_sayi == 0) {
        fputs("OK\n", stdout);
    } else {
        for (int i = 0; i < g_check_hata_sayi; i++) {
            fprintf(stdout, "%s\t%d\t%d\n", g_check_hatalar[i].kod,
                    g_check_hatalar[i].satir, g_check_hatalar[i].sutun);
        }
    }
    arena_serbest(a);
    return 0;
}

/* C2: IR-verifier kapisi varsayilan ACIK; --no-verify ile kapatilir
 * (benchmark kacis yolu). */
static int g_llvm_dogrula = 1;

/* D-337: `--llvm` artik TIP KONTROLUNU BAGLAR. Onceki hal olculdu ve
 * SESSIZ-YANLIS-CEVAP ureticiydi: `--check` REDDETTIGI program `--llvm` ile
 * derlenip CALISAN ikili verebiliyordu (`f(40, 99)` fazla argumanla exit 42).
 * Derleme yolu `kemgu --llvm | clang` oldugu icin bu, tip sisteminin pratikte
 * BAGLAYICI OLMAMASI demekti.
 *
 * KACIS KAPISI `--tip-atla`: bilinen borclu yapilar icin (ol. kem_os birlesik
 * kaynagi 60 tip hatasi veriyor; kasitli codegen korpuslari E002/E004/T001).
 * Kacis ACIK olmali — sessiz gecis degil, bayrakla BEYAN. */
static int g_tip_atla = 0;

/* Tek-gecis ad cozumu: resolver gecisinde hatalar susturulur (yalniz
 * --tip-atla modunda; aksi halde hatalar KULLANICIYA gosterilir). */
static void sessiz_hata_cb(int satir, int sutun, const char *kod,
                           const char *mesaj, const char *ipucu, void *ctx) {
    (void)satir; (void)sutun; (void)kod;
    (void)mesaj; (void)ipucu; (void)ctx;
}

static int mode_llvm(const char *kaynak, const char *dosya_adi) {
    Arena *a = arena_olustur(0);
    if (!a) {
        fprintf(stderr, "Arena olusturulamadi\n");
        return 1;
    }
    Lexer l;
    lexer_baslat(&l, kaynak, dosya_adi);
    Parser p;
    parser_baslat(&p, &l, a, dosya_adi, kaynak);
    Dugum *prog = parser_calistir(&p);
    if (p.hata_sayisi > 0) {
        fprintf(stderr, "Parser hatalari: %d (LLVM IR uretilmedi)\n",
                p.hata_sayisi);
        arena_serbest(a);
        return 1;
    }

    /* A: cok-dosya modul yukleme — codegen'den ONCE; sentetik moduller
     * program AST'sine eklenir, resolver binding'leri capraz-dosya
     * cozer, codegen @modul.ad olarak emit eder. Yukleme hatasi
     * (dosya yok / parse hatasi) IR uretimini durdurur — eksik modul
     * zaten link edilemezdi. */
    if (prog) {
        int yukleme_hata = modulleri_yukle(a, prog, dosya_adi, kaynak);
        if (yukleme_hata > 0) {
            fprintf(stderr,
                    "kullan yukleme hatalari: %d (LLVM IR uretilmedi)\n",
                    yukleme_hata);
            arena_serbest(a);
            return 1;
        }
    }

    /* Tek-gecis ad cozumu: resolver (tip_kontrol) binding'leri AST'ye
     * yazar, codegen string'le yeniden cozmek yerine bunlari tuketir
     * (bkz. ast.h CozumKategorisi). Scope/Sembol'ler ayni arena'da —
     * binding pointer'lari llvm_ir_uret boyunca gecerli. */
    if (prog) {
        Scope *g = scope_olustur(a, SCOPE_GLOBAL, NULL);
        if (g) {
            TipKontrol tk;
            tip_kontrol_baslat(&tk, a, g, dosya_adi, kaynak);
            /* D-337: --tip-atla YOKSA hatalar KULLANICIYA yazilir ve emit
             * ENGELLENIR. Tip hatali programdan ikili uretmek, tip sistemini
             * pratikte baglayici olmaktan cikariyordu. */
            if (g_tip_atla) hata_callback_ayarla(sessiz_hata_cb, NULL);
            tip_kontrol_program(&tk, prog);
            if (g_tip_atla) hata_callback_ayarla(NULL, NULL);
            if (!g_tip_atla && tk.hata_sayisi > 0) {
                fprintf(stderr,
                    "\nHATA: %s — tip kontrol %d hata (LLVM IR URETILMEDI).\n"
                    "  Bilerek tip hatali kod derliyorsaniz: --tip-atla\n",
                    dosya_adi, tk.hata_sayisi);
                arena_serbest(a);
                return 1;
            }
        }
    }

    /* --no-verify: kapi kapali, dogrudan emit.
     * C5 AS001: olumcul codegen hatasi varsa hata koduyla bit. */
    if (!g_llvm_dogrula) {
        int n = llvm_ir_uret(prog, stdout);
        arena_serbest(a);
        return n > 0 ? 1 : 0;
    }

    /* C2 kapisi: IR'i once tampona uret, opt pass'lerinden ONCE dogrula,
     * sonra stdout'a yaz. Text backend oldugu icin LLVMVerifyModule yerine
     * llvm_ir_dogrula (terminator-tamlik tarayici) kullanilir.
     * tmpfile acilamazsa guvenli taraf: dogrulamayi atla, yine de emit et. */
    FILE *tf = tmpfile();
    if (!tf) {
        int n = llvm_ir_uret(prog, stdout);
        arena_serbest(a);
        return n > 0 ? 1 : 0;
    }
    int codegen_hata = llvm_ir_uret(prog, tf);
    if (codegen_hata > 0) {
        /* C5 AS001: hata mesaji stderr'e zaten yazildi; IR yayinlanmaz. */
        fclose(tf);
        arena_serbest(a);
        return 1;
    }

    long boyut = ftell(tf);
    if (boyut < 0) {
        rewind(tf);
        char tampon[4096];
        size_t r;
        while ((r = fread(tampon, 1, sizeof(tampon), tf)) > 0) {
            fwrite(tampon, 1, r, stdout);
        }
        fclose(tf);
        arena_serbest(a);
        return 0;  /* ftell desteklenmiyor — dogrulamasiz akit */
    }
    rewind(tf);
    char *ir = (char *)malloc((size_t)boyut + 1);
    if (!ir) {
        fclose(tf);
        int n = llvm_ir_uret(prog, stdout);  /* bellek yok — fallback */
        arena_serbest(a);
        return n > 0 ? 1 : 0;
    }
    size_t okunan = fread(ir, 1, (size_t)boyut, tf);
    ir[okunan] = '\0';
    fclose(tf);

    char hata[256];
    if (llvm_ir_dogrula(ir, hata, sizeof(hata)) != 0) {
        fprintf(stderr, "kemgu: internal-codegen-error: %s\n", hata);
        free(ir);
        arena_serbest(a);
        return 1;
    }
    fwrite(ir, 1, okunan, stdout);
    free(ir);
    arena_serbest(a);
    return 0;
}

static void kullanim_yazdir(const char *prog_adi) {
    fprintf(stderr,
        "Kullanim: %s [--token | --parse | --check | --llvm | --lsp] [dosya]\n"
        "  --token   Lexer akisini yazdir\n"
        "  --parse   Parser calistir + AST yazdir\n"
        "  --check   Parser + tip kontrol (varsayilan)\n"
        "  --llvm    LLVM IR text yazdir (clang -x ir - ile derlenebilir)\n"
        "  --lsp     Language Server (stdio JSON-RPC)\n"
        "  --no-verify  LLVM IR dogrulama kapisini kapat (sadece --llvm)\n"
        "  --mimari M   Hedef mimari: arm64|x86_64 (satirici_asm arch-gate + triple; varsayilan x86_64)\n"
        "  dosya     Kaynak dosya yolu (yoksa stdin'den okur)\n",
        prog_adi);
}

int main(int argc, char *argv[]) {
    Mod mod = MOD_CHECK;  /* default */
    int arg_idx = 1;

    while (arg_idx < argc && argv[arg_idx][0] == '-') {
        if (strcmp(argv[arg_idx], "--token") == 0) {
            mod = MOD_TOKEN;
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "--parse") == 0) {
            mod = MOD_PARSE;
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "--ast") == 0) {
            mod = MOD_AST;
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "--check") == 0) {
            mod = MOD_CHECK;
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "--checkdump") == 0) {
            mod = MOD_CHECKDUMP;
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "--llvm") == 0) {
            mod = MOD_LLVM;
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "--lsp") == 0) {
            mod = MOD_LSP;
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "--no-verify") == 0) {
            g_llvm_dogrula = 0;  /* C2 kapisini kapat (benchmark kacisi) */
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "--tip-atla") == 0) {
            /* D-337: tip hatalarina RAGMEN emit et (bilinen borclu yapilar).
             * ACIK BEYAN — sessiz gecis degil. */
            g_tip_atla = 1;
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "--mimari") == 0) {
            /* D-269 (P1): hedef mimari sec (satirici_asm arch-gate + emit triple).
             * Varsayilan x86_64; arm64 aarch64 sysreg/bariyer asm'i acar. */
            if (arg_idx + 1 >= argc) {
                fprintf(stderr, "--mimari: mimari argumani gerekli (arm64|x86_64)\n");
                return 2;
            }
            const char *m = argv[arg_idx + 1];
            if (strcmp(m, "arm64") == 0 || strcmp(m, "aarch64") == 0) {
                llvm_hedef_ayarla("arm64", "aarch64-unknown-none-elf");
            } else if (strcmp(m, "x86_64") == 0) {
                llvm_hedef_ayarla("x86_64", "x86_64-pc-windows-gnu");
            } else {
                fprintf(stderr, "--mimari: bilinmeyen mimari '%s' (arm64|x86_64)\n", m);
                return 2;
            }
            arg_idx += 2;
        } else if (strcmp(argv[arg_idx], "--help") == 0 ||
                   strcmp(argv[arg_idx], "-h") == 0) {
            kullanim_yazdir(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Bilinmeyen bayrak: %s\n", argv[arg_idx]);
            kullanim_yazdir(argv[0]);
            return 2;
        }
    }

    /* LSP modu dosya/kaynak okumaz — stdio loop'una gecer */
    if (mod == MOD_LSP) {
        return lsp_server_calistir(stdin, stdout);
    }

    char *kaynak;
    const char *dosya_adi;

    if (arg_idx < argc) {
        dosya_adi = argv[arg_idx];
        kaynak = dosya_oku(dosya_adi);
    } else {
        dosya_adi = "<stdin>";
        kaynak = stdin_oku();
    }

    if (!kaynak) {
        fprintf(stderr, "Kaynak okunamadi\n");
        return 1;
    }

    int rc;
    switch (mod) {
        case MOD_TOKEN: rc = mode_token(kaynak, dosya_adi); break;
        case MOD_PARSE: rc = mode_parse(kaynak, dosya_adi); break;
        case MOD_AST:   rc = mode_ast(kaynak, dosya_adi); break;
        case MOD_CHECK: rc = mode_check(kaynak, dosya_adi); break;
        case MOD_CHECKDUMP: rc = mode_checkdump(kaynak, dosya_adi); break;
        case MOD_LLVM:  rc = mode_llvm(kaynak, dosya_adi); break;
        default:        rc = 2; break;
    }

    free(kaynak);
    return rc;
}
