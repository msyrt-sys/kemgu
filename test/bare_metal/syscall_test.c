/*
 * C6 sistem çağrısı testi — SVC/int 0x80 → kernel dispatch → dönüş (aarch64 +
 * x86_64 ORTAK). Kullanıcı/kernel kodu sistem çağrısı yapar, kernel işler,
 * çağrı yerine DÖNER (eret/iretq).
 *
 * Kanıt: "BEFORE SYSCALL" → "SYSCALL OK num=1" (handler) → "AFTER SYSCALL"
 * (dönüş kanıtı). Üçü de görülürse syscall çalıştı VE düzgün döndü.
 */
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_satir(void);

static void sistem_cagrisi(unsigned long num, unsigned long arg) {
#if defined(__aarch64__)
    register unsigned long x8 __asm__("x8") = num;   /* çağrı no */
    register unsigned long x0 __asm__("x0") = arg;   /* arg0 */
    __asm__ volatile("svc #0" : : "r"(x8), "r"(x0) : "memory");
#elif defined(__x86_64__)
    __asm__ volatile("int $0x80" : : "a"(num), "D"(arg) : "memory");
#endif
}

int main(void) {
    kdl_yazdir_metin("BEFORE SYSCALL");
    kdl_yazdir_satir();

    sistem_cagrisi(1, 0);            /* çağrı #1 → kernel "SYSCALL OK" basar */

    kdl_yazdir_metin("AFTER SYSCALL");   /* buraya dönüldüyse eret/iretq çalıştı */
    kdl_yazdir_satir();
    return 0;
}
