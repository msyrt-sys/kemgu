/*
 * D-122 syscall ARGÜMAN geçişi testi (aarch64) — SVC arg0 (x0) korunuyor mu?
 *
 * SVC num=4 (x8), arg=42 (x0) → kernel kdl_syscall_isle(4, 42) → arg==42 ise
 * "SYSCALL ARG OK". D-121 vektör-stub x0-koruma onarımından ÖNCE EXC vektörü
 * x0'ı vektör-indeksiyle (4) eziyordu → arg=4 → "HATA". Onarımla gerçek arg
 * ulaşır → "OK". Bu, userspace syscall'larının (argüman geçen) ön-koşuludur.
 */
extern void kdl_yazdir_metin(const char *);

int main(void) {
    kdl_yazdir_metin("SYSCALL ARG BASLA");

    /* num=4 → x8, arg=42 → x0, sonra svc. eret ile buraya döner (kurtarılır). */
    __asm__ volatile(
        "mov x8, #4\n"
        "mov x0, #42\n"
        "svc #0\n"
        : : : "x0", "x8", "memory");

    kdl_yazdir_metin("SYSCALL ARG SONRA");   /* eret döndü → SVC kurtarma çalışıyor */
    for (;;) { __asm__ volatile("wfe"); }
}
