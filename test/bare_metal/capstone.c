/*
 * KEMGU-OS Capstone — tam yığın TEK boot'ta KOMPOZE olur (aarch64 + x86_64).
 *
 * Minimal gösterici parçaları (her biri ayrı testte kanıtlı) burada BİRLİKTE:
 *   1. Region dizi (frame allocator)        — kdl_dizi_* / kdl_bare_heap
 *   2. Timer + interrupt (IRQ açık)         — kdl_kesme_kur + kdl_timer_baslat
 *   3. Region tahsis IRQ AÇIKKEN            — entegrasyon: allocator IRQ-safe mi
 *   4. Sistem çağrısı                       — SVC / int 0x80
 *   5. idle — timer tik'leri arka planda     — kdl_idle
 *
 * Kanıt: "DIZI=55" + "POST-IRQ=99" + "CAPSTONE OK" + "TIMER OK" hepsi → tam OS
 * yığını tek boot'ta birlikte çalışıyor.
 */
#include <stdint.h>

extern void    kdl_yazdir_metin(const char *);
extern void    kdl_yazdir_tam(int32_t);
extern void    kdl_yazdir_satir(void);
extern void   *kdl_dizi_olustur(void *rho, int32_t eleman_byte);
extern void    kdl_dizi_ekle_tam(void *rho, void *d, int32_t v);
extern int32_t kdl_dizi_al_tam(void *d, int32_t i);
extern int32_t kdl_dizi_boyut(void *d);
extern void    kdl_kesme_kur(void);
extern void    kdl_timer_baslat(void);
extern void    kdl_idle(void);

static void sistem_cagrisi(unsigned long num, unsigned long arg) {
#if defined(__aarch64__)
    register unsigned long x8 __asm__("x8") = num;
    register unsigned long x0 __asm__("x0") = arg;
    __asm__ volatile("svc #0" : : "r"(x8), "r"(x0) : "memory");
#elif defined(__x86_64__)
    __asm__ volatile("int $0x80" : : "a"(num), "D"(arg) : "memory");
#endif
}

int main(void) {
    kdl_yazdir_metin("== KEMGU OS CAPSTONE ==");
    kdl_yazdir_satir();

    /* 1. Region dizi (frame allocator): 1..10 = 55 */
    void *d = kdl_dizi_olustur(0, 4);
    for (int32_t i = 0; i < 10; i++) kdl_dizi_ekle_tam(0, d, i + 1);
    int32_t s = 0;
    for (int32_t i = 0; i < kdl_dizi_boyut(d); i++) s += kdl_dizi_al_tam(d, i);
    kdl_yazdir_metin("DIZI=");
    kdl_yazdir_tam(s);
    kdl_yazdir_satir();

    /* 2. Timer + interrupt aç */
    kdl_kesme_kur();
    kdl_timer_baslat();

    /* 3. Region tahsis IRQ AÇIKKEN (entegrasyon: allocator IRQ ile bozulmuyor) */
    void *d2 = kdl_dizi_olustur(0, 4);
    kdl_dizi_ekle_tam(0, d2, 99);
    kdl_yazdir_metin("POST-IRQ=");
    kdl_yazdir_tam(kdl_dizi_al_tam(d2, 0));
    kdl_yazdir_satir();

    /* 4. Sistem çağrısı */
    sistem_cagrisi(1, 0);

    kdl_yazdir_metin("CAPSTONE OK");
    kdl_yazdir_satir();

    /* 5. idle — timer tik'leri → "TIMER OK tik=5" arka planda */
    for (;;) kdl_idle();
}
