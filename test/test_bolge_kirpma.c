/* [D-783] E4q: bölge kapanışı glibc yığınını çekirdeğe İADE ETMEMELİ (brk gidiş-gelişi).
 *
 * Kök (ölçüldü): glibc varsayılanı, yığının tepesindeki 128 KB'ı aşan boş alanı her free()'de
 * kırpar (systrim -> sbrk). Bölge blokları 64 KB'tır ve bölgeler sık açılıp kapanır; her kapanış
 * bloğu iade eder, her açılış yeniden ister. Self-host derleyicide zamanın ~%70'i buradaydı.
 * kdl_bolge_olustur ilk çağrıda kırpma eşiğini yükseltir (yalnız glibc).
 *
 * Ölçüm DETERMİNİSTİKTİR: her turda kesme noktası bloklar ayrıldıktan sonra ve bölge kapandıktan
 * sonra okunur; kapanış bir kez bile GERİ ÇEKERSE kırmızı. ASan'sız derlenir (ASan kendi ayırıcısını kullanır, sbrk'e dokunmaz).
 * Her bölgeye 4 blok (4 x 60000 B) ayrılır: kapanışta tepe boş alanı 128 KB'ı aşar → onarım
 * yoksa glibc kırpar. Blok 64 KB < mmap eşiği (128 KB) → bloklar brk yığınından gelir. */
#define _DEFAULT_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include "kdl_bolge.h"

int main(void) {
#if !defined(__GLIBC__)
    printf("bolge_kirpma: glibc degil — kirpma esigi yalniz glibc'de ayarlanir, ATLANDI\n");
    return 0;
#else
    /* Isınma: ilk açılış ayarı kurar, yığın bir kez büyür. */
    KdlBolge *ilk = kdl_bolge_olustur();
    for (int k = 0; k < 4; k++) (void)kdl_bolge_ayir(ilk, 60000);
    kdl_bolge_serbest(ilk);
    /* ⚠ Ölçüm TUR İÇİNDE: glibc yığını aynı turda büyütüp kırpar; yalnız tur sonlarını okumak iki
     * durumda da eşit değer verir (ilk sürüm bu yüzden sabotaj S283'te SESSİZ kaldı). Bloklar
     * ayrıldıktan HEMEN sonra (ust) ve bölge kapandıktan sonra (alt) okunur; alt < ust = kırpma. */
    int geri = 0;
    for (int i = 0; i < 2000; i++) {
        KdlBolge *b = kdl_bolge_olustur();
        if (!b) { printf("  🔴 kdl_bolge_olustur NULL\n"); return 1; }
        for (int k = 0; k < 4; k++) {
            if (!kdl_bolge_ayir(b, 60000)) { printf("  🔴 kdl_bolge_ayir NULL\n"); return 1; }
        }
        uintptr_t ust = (uintptr_t)sbrk(0);
        kdl_bolge_serbest(b);
        uintptr_t alt = (uintptr_t)sbrk(0);
        if (alt < ust) geri++;
    }
    printf("bolge_kirpma: 2000 ac/kapa turu, kesme noktasi %d kez geri cekildi\n", geri);
    if (geri != 0) {
        printf("  🔴 bolge kapanisi yigini cekirdege iade ediyor (brk gidis-gelisi, D-783)\n");
        return 1;
    }
    printf("  ✅ yigin kirpilmadi\n");
    return 0;
#endif
}
