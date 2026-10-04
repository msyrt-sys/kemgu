/*
 * test_kdl_bolge_eszamanli.c — [D-665] bölge sayaçlarının EŞ ZAMANLI doğruluğu
 *
 * NEDEN VAR: `kdl_bolge_olustur_sayisi` / `kdl_bolge_serbest_sayisi` sızıntıyı
 * ölçmek için kullanılan TANIK sayaçlardır (`kdl_bolge_bakiye`). Düz
 * `uint64_t ++` idiler; görevler gerçek OS thread'leri olduğu için bölgelerini
 * EŞ ZAMANLI açıp kapatınca artırmalar KAYBOLUYORDU. Dış bir inceleme bunu
 * izole testle ölçtü (8 × 25.000 → oluştur 149.717–188.010, beklenen 200.000).
 *
 * ⚠ BU TEST İZOLE BİR KOPYAYI DEĞİL GERÇEK `runtime/kdl_bolge.c`yi derler —
 * yani runtime gerilerse kapı kırmızıya döner. Her thread yalnız KENDİ
 * bölgelerini kullanır: yarış bölgelerde değil ORTAK sayaçlardadır.
 *
 * ⚠⚠ SAYIM TEK BAŞINA ŞANSA BAĞLIDIR: yarış her koşumda manifest olmayabilir.
 * Bu yüzden aynı kaynak AYRICA ThreadSanitizer ile derlenip koşulur
 * (Makefile `calistir_bolge_sayac_yarisi`) — TSan yarışı DETERMİNİSTİK
 * yakalar. ASan bu sınıfı GÖRMEZ; sayaç yarışının bugüne dek fark edilmemesinin
 * sebebi tam olarak buydu.
 *
 * Çıkış: 0 = geçti, 1 = sayaç kaybı.
 */
#include "kdl_bolge.h"

#include <stdint.h>
#include <stdio.h>

#define THREAD_SAYISI  8
#define TUR            25000

#ifdef _WIN32
#  include <windows.h>
static DWORD WINAPI isci(LPVOID arg) {
#else
#  include <pthread.h>
static void *isci(void *arg) {
#endif
    (void)arg;
    for (int i = 0; i < TUR; i++) {
        KdlBolge *b = kdl_bolge_olustur();
        if (b) {
            /* Bölgeyi gerçekten kullan: boş aç-kapa derleyicinin elemesine
             * ve gerçek tahsis yolunun hiç koşmamasına izin verirdi. */
            void *p = kdl_bolge_ayir(b, 16);
            if (p) ((volatile unsigned char *)p)[0] = 1;
            kdl_bolge_serbest(b);
        }
    }
    return 0;
}

int main(void) {
    uint64_t beklenen = (uint64_t)THREAD_SAYISI * TUR;
    uint64_t o0 = kdl_bolge_olustur_sayisi;
    uint64_t s0 = kdl_bolge_serbest_sayisi;

#ifdef _WIN32
    HANDLE t[THREAD_SAYISI];
    for (int i = 0; i < THREAD_SAYISI; i++)
        t[i] = CreateThread(NULL, 0, isci, NULL, 0, NULL);
    WaitForMultipleObjects(THREAD_SAYISI, t, TRUE, INFINITE);
    for (int i = 0; i < THREAD_SAYISI; i++) CloseHandle(t[i]);
#else
    pthread_t t[THREAD_SAYISI];
    for (int i = 0; i < THREAD_SAYISI; i++)
        pthread_create(&t[i], NULL, isci, NULL);
    for (int i = 0; i < THREAD_SAYISI; i++)
        pthread_join(t[i], NULL);
#endif

    uint64_t olustur = kdl_bolge_olustur_sayisi - o0;
    uint64_t serbest = kdl_bolge_serbest_sayisi - s0;
    int bakiye = kdl_bolge_bakiye();

    printf("beklenen=%llu olustur=%llu serbest=%llu bakiye=%d\n",
           (unsigned long long)beklenen, (unsigned long long)olustur,
           (unsigned long long)serbest, bakiye);

    if (olustur != beklenen || serbest != beklenen || bakiye != 0) {
        printf("FAIL: bolge sayaclari eszamanli kullanimda KAYIP veriyor\n");
        return 1;
    }
    printf("OK: %d thread x %d tur, sayaclar tam\n", THREAD_SAYISI, TUR);
    return 0;
}
