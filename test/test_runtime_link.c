/*
 * KEMGU Runtime Link Test (ADIM 33)
 *
 * runtime/kdl_runtime.c'nin compile + link calismasini dogrular.
 * Hicbir fonksiyon cagrilmadan sadece sembol erisilebilirligi
 * kontrol edilir — runtime entegrasyonu mevcut LLVM pipeline'da
 * link adimi olarak kullanilabilir.
 *
 * Calistirilince bir mesaj basar ve exit 0 doner.
 */

#include <stdio.h>

/* Forward declare (LLVM IR'da declare cikan tipik sembol) */
extern void kdl_yazdir_metin(const char *s);

int main(void) {
    /* Sembolun adresi alinabilir mi — link basarili mi? */
    void (*fn)(const char *) = &kdl_yazdir_metin;
    if (fn) {
        fputs("OK: runtime/kdl_runtime.c link basarili (kdl_yazdir_metin erisilebilir).\n",
              stdout);
        return 0;
    }
    fputs("FAIL: kdl_yazdir_metin sembolu bulunamadi.\n", stderr);
    return 1;
}
