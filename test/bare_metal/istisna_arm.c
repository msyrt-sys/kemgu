/*
 * C3a deliberate-fault testi (aarch64) — exception vektör mekanizmasını kanıtla.
 *
 * Boot (start_aarch64.S) VBAR'ı kurar. Bu main UART'a marker basar, sonra
 * eşlenmemiş (>1GB, identity-harita dışı) adrese erişip translation-fault
 * tetikler → kdl_exc_ortak → kdl_istisna_isle "ISTISNA ..." basar + halt.
 * "GORUNMEMELI" ASLA basılmamalı (fault yakalandı, ileri gidilmedi).
 *
 * Bu C kernel'i KEMGU-codegen değil — yalnız vektör mekanizmasının testi.
 */
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_satir(void);

int main(void) {
    kdl_yazdir_metin("FAULT TETIKLE");
    kdl_yazdir_satir();

    volatile unsigned long *bad = (volatile unsigned long *)0xFFFFFFFF00000000UL;
    unsigned long v = *bad;             /* eşlenmemiş → translation fault */
    (void)v;

    kdl_yazdir_metin("GORUNMEMELI");    /* fault yakalandıysa erişilmez */
    kdl_yazdir_satir();
    return 0;
}
