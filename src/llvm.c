#include "llvm.h"

#include <stdio.h>
#include <inttypes.h>

/* === Yardimci: tek-deger ifade IR'ye yazdir ===
 * Su an sadece TAM literal + basit ikili op. SSA reg counter parametresi
 * ile recursive — her ifade icin yeni %N reg. */

static int ifade_uret(const Dugum *d, FILE *out, int *reg) {
    if (!d) {
        fputs("  ; (NULL ifade)\n", out);
        return -1;
    }

    switch (d->tip) {
        case DUGUM_TAM: {
            /* Constant — yeni register'a yukle: %N = add i32 0, K */
            int r = (*reg)++;
            fprintf(out, "  %%%d = add i32 0, %" PRId64 "\n",
                    r, d->veri.tam.deger);
            return r;
        }

        case DUGUM_IKILI: {
            int sol_r = ifade_uret(d->veri.ikili.sol, out, reg);
            int sag_r = ifade_uret(d->veri.ikili.sag, out, reg);
            if (sol_r < 0 || sag_r < 0) return -1;
            const char *op = NULL;
            switch (d->veri.ikili.op) {
                case OP_ARTI:  op = "add"; break;
                case OP_EKSI:  op = "sub"; break;
                case OP_CARPI: op = "mul"; break;
                case OP_BOLU:  op = "sdiv"; break;
                case OP_MOD:   op = "srem"; break;
                default:
                    fputs("  ; ikili op desteklenmiyor\n", out);
                    return -1;
            }
            int r = (*reg)++;
            fprintf(out, "  %%%d = %s i32 %%%d, %%%d\n",
                    r, op, sol_r, sag_r);
            return r;
        }

        case DUGUM_TEKLI: {
            int op_r = ifade_uret(d->veri.tekli.operand, out, reg);
            if (op_r < 0) return -1;
            if (d->veri.tekli.op == OP_NEG) {
                int r = (*reg)++;
                fprintf(out, "  %%%d = sub i32 0, %%%d\n", r, op_r);
                return r;
            }
            fputs("  ; tekli op desteklenmiyor\n", out);
            return -1;
        }

        default:
            fprintf(out, "  ; ifade tipi %d desteklenmiyor (yer tutucu)\n",
                    d->tip);
            int r = (*reg)++;
            fprintf(out, "  %%%d = add i32 0, 0\n", r);
            return r;
    }
}

/* === Islev IR === */

static void islev_uret(const Dugum *islev, FILE *out) {
    /* define i32 @<ad>() { ... } */
    fputs("define i32 @", out);
    fwrite(islev->veri.islev.ad, 1,
           (size_t)islev->veri.islev.ad_uzunluk, out);
    fputs("() {\nentry:\n", out);

    int reg = 0;  /* SSA register counter */

    if (islev->veri.islev.govde &&
        islev->veri.islev.govde->veri.blok.sayi > 0) {
        for (int i = 0; i < islev->veri.islev.govde->veri.blok.sayi; i++) {
            const Dugum *deyim = islev->veri.islev.govde->veri.blok.deyimler[i];
            if (deyim->tip == DUGUM_VER) {
                if (deyim->veri.ver.deger) {
                    int r = ifade_uret(deyim->veri.ver.deger, out, &reg);
                    if (r >= 0) {
                        fprintf(out, "  ret i32 %%%d\n", r);
                    } else {
                        fputs("  ret i32 0\n", out);
                    }
                } else {
                    fputs("  ret i32 0\n", out);
                }
                fputs("}\n\n", out);
                return;
            }
            /* Diger deyimler: simdilik atlanir */
        }
    }

    /* Govdesi yok veya ver yok — default 0 don */
    fputs("  ret i32 0\n}\n\n", out);
}

/* === Public API === */

void llvm_ir_uret(const Dugum *program, FILE *out) {
    if (!out) return;
    fputs("; KEMGU LLVM IR (text uretici, ADIM 13.1 minimum)\n", out);
    fputs("; Bu IR `clang -x ir - -o cikti.exe` ile derlenebilir.\n", out);
    fputs("target triple = \"x86_64-pc-windows-gnu\"\n\n", out);

    if (!program || program->tip != DUGUM_PROGRAM) {
        fputs("; (program AST'si yok)\n", out);
        return;
    }

    for (int i = 0; i < program->veri.program.sayi; i++) {
        const Dugum *uye = program->veri.program.uyeler[i];
        if (uye->tip == DUGUM_ISLEV) {
            islev_uret(uye, out);
        }
        /* DUGUM_DISA, DUGUM_YAPI vs ileride */
    }
}
