/*
 * KEMGU — C2: IR-verifier kapisi (text backend)
 * ==============================================
 *
 * libLLVM linklenmedigi icin (llvm.c text uretici) LLVMVerifyModule
 * cagrilamaz. Bunun yerine emit edilen IR metnini tarayip her `define`'in
 * her temel blogunun gecerli bir terminator (ret/br/switch/.../unreachable)
 * ile bittigini dogrularuz — LangRef "her basic block bir terminator ile
 * biter" degismezi. Bu, C1 sinifi missing-terminator regresyonlarini
 * (or. esles kolunun bir sonraki bloga dusmesi) opt'a/clang'a varmadan
 * yakalar.
 *
 * Bilincli olarak AST/parser bagimliligi YOK: yalniz metin uzerinde calisir,
 * boylece ASan altinda izole birim testi mumkundur (bkz. test_llvm_dogrula.c).
 */

#include "llvm.h"

#include <stdio.h>
#include <string.h>

/* Trim'lenmis satirin ilk token'i bir LLVM terminator opcode'u mu?
 * LangRef terminator kumesi (ret/br/switch/.../unreachable). */
static int satir_terminator_mi(const char *t) {
    static const char *ops[] = {
        "ret", "br", "switch", "indirectbr", "invoke", "callbr",
        "resume", "catchswitch", "catchret", "cleanupret", "unreachable"
    };
    for (size_t i = 0; i < sizeof(ops) / sizeof(ops[0]); i++) {
        size_t n = strlen(ops[i]);
        if (strncmp(t, ops[i], n) == 0) {
            char c = t[n];  /* token siniri kontrolu */
            if (c == '\0' || c == ' ' || c == '\t' ||
                c == '\n' || c == '\r') {
                return 1;
            }
        }
    }
    return 0;
}

/* Ham satir (col-0 onemli) bir blok etiketi mi (`ad:`) ? Etiket adini
 * ad_buf'a kopyalar. Etiket: bastaki bosluk yok, [A-Za-z0-9._$]+ ardindan
 * ':' ve (bizim ciktida) sonrasinda sadece bosluk/yorum. */
static int satir_etiket_mi(const char *ham, char *ad_buf, size_t buf_n) {
    if (ham[0] == ' ' || ham[0] == '\t' || ham[0] == '\0') return 0;
    size_t i = 0;
    while (ham[i] && ham[i] != ':' && ham[i] != '\n' && ham[i] != '\r') {
        char c = ham[i];
        int ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                 (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '$';
        if (!ok) return 0;
        i++;
    }
    if (i == 0 || ham[i] != ':') return 0;
    size_t j = i + 1;
    while (ham[j] == ' ' || ham[j] == '\t') j++;
    if (ham[j] != '\0' && ham[j] != '\n' && ham[j] != '\r' && ham[j] != ';') {
        return 0;
    }
    size_t k = (i < buf_n - 1) ? i : buf_n - 1;
    memcpy(ad_buf, ham, k);
    ad_buf[k] = '\0';
    return 1;
}

int llvm_ir_dogrula(const char *ir_metni, char *hata, size_t hata_boyut) {
    if (hata && hata_boyut) hata[0] = '\0';
    if (!ir_metni) return 0;

    const char *p = ir_metni;
    int in_func = 0;       /* `define` ... `}` arasinda miyiz */
    int blok_acik = 0;     /* su an acik bir temel blok var mi */
    int blok_term = 0;     /* acik blok bir terminator gordu mu */
    char fn_ad[128] = "?";
    char blok_ad[128] = "?";
    int satir_no = 0;
    char satir[1024];

    while (*p) {
        size_t i = 0;
        while (*p && *p != '\n' && i < sizeof(satir) - 1) satir[i++] = *p++;
        satir[i] = '\0';
        if (*p == '\n') p++;
        satir_no++;

        const char *t = satir;            /* trim'lenmis baslangic */
        while (*t == ' ' || *t == '\t') t++;

        if (!in_func) {
            if (strncmp(t, "define", 6) == 0 &&
                (t[6] == ' ' || t[6] == '\t')) {
                in_func = 1; blok_acik = 0; blok_term = 0;
                const char *at = strchr(t, '@');
                size_t k = 0;
                if (at) {
                    at++;
                    while (at[k] && at[k] != '(' && at[k] != ' ' &&
                           k < sizeof(fn_ad) - 1) { fn_ad[k] = at[k]; k++; }
                }
                fn_ad[k] = '\0';
                if (k == 0) { fn_ad[0] = '?'; fn_ad[1] = '\0'; }
            }
            continue;
        }

        /* in_func: fonksiyon kapanisi */
        if (t[0] == '}') {
            if (blok_acik && !blok_term) {
                if (hata && hata_boyut) {
                    snprintf(hata, hata_boyut,
                        "islev @%s blok '%s' terminator'suz "
                        "(islev kapanisi, satir %d)",
                        fn_ad, blok_ad, satir_no);
                }
                return 1;
            }
            in_func = 0; blok_acik = 0; blok_term = 0;
            continue;
        }

        /* Yeni blok etiketi mi */
        char yeni_ad[128];
        if (satir_etiket_mi(satir, yeni_ad, sizeof(yeni_ad))) {
            if (blok_acik && !blok_term) {
                if (hata && hata_boyut) {
                    snprintf(hata, hata_boyut,
                        "islev @%s blok '%s' terminator'suz "
                        "(sonraki etiket '%s', satir %d)",
                        fn_ad, blok_ad, yeni_ad, satir_no);
                }
                return 1;
            }
            blok_acik = 1; blok_term = 0;
            strcpy(blok_ad, yeni_ad);
            continue;
        }

        /* Yorum veya bos satir — blok durumunu degistirmez */
        if (*t == '\0' || *t == ';') continue;

        /* Talimat satiri */
        if (!blok_acik) {
            /* Etiketsiz ilk (implicit entry) blok */
            blok_acik = 1; blok_term = 0;
            strcpy(blok_ad, "<entry>");
        }
        if (!blok_term && satir_terminator_mi(t)) blok_term = 1;
    }

    return 0;
}
