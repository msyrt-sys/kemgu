/*
 * C3a deliberate-fault testi (x86_64) — IDT/exception mekanizmasını kanıtla.
 *
 * Boot (start_x86_64.S) long_entry'de kdl_idt_kur() çağırır → IDT kurulu.
 * Bu main marker basar, sonra `ud2` (geçersiz opcode → vektör 6) çalıştırır →
 * isr6 → isr_ortak → kdl_istisna_isle "ISTISNA ..." basar + halt.
 * "GORUNMEMELI" ASLA basılmamalı.
 */
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_satir(void);

int main(void) {
    kdl_yazdir_metin("FAULT TETIKLE");
    kdl_yazdir_satir();

    __asm__ volatile("ud2");            /* geçersiz opcode → istisna 6 */

    kdl_yazdir_metin("GORUNMEMELI");    /* yakalandıysa erişilmez */
    kdl_yazdir_satir();
    return 0;
}
