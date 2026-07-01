/*
 * KEMGU Bare-Metal Yetki (Capability) Runtime — kdl_yetki_bare.c (D-148).
 * =====================================================================
 *
 * `yetki<R>` (object-capability) intrinsic'lerinin bare-metal karşılığı. Host
 * kdl_runtime.c'nin kdl_yetki_* fonksiyonları libc/PRNG'ye dokunur; bu freestanding
 * sürüm self-host .kem sürücülerinin (virtio_selfhost.kem) bare-metal link'i için.
 *
 * KdlYetki layout codegen `%kdl_yetki = { i64, i16, i16, i8, [3 x i8] }` (16 bayt)
 * ile birebir. yetki<R> DERLEME-ZAMANI ispatıdır (WCET için sıfır runtime yükü) —
 * mmio_oku32/yaz32 yetkiyi kullanmaz, yalnız adresi geçer; bu yüzden runtime
 * değeri işlevsel olarak önemsizdir (ABI şeffaf). Freestanding (libc yok).
 */
#include <stdint.h>

typedef struct {
    uint64_t id;
    uint16_t kaynak_tipi;
    uint16_t izin;
    uint8_t  iptal;
    uint8_t  rezerv[3];
} KdlYetki;

static uint64_t kdl_yetki_sayac = 1;   /* PRNG yerine basit sayaç (id!=0 yeter) */

KdlYetki kdl_yetki_olustur(uint16_t kt, uint16_t izin) {
    KdlYetki y;
    y.id = kdl_yetki_sayac++;
    y.kaynak_tipi = kt;
    y.izin = izin;
    y.iptal = 0;
    y.rezerv[0] = y.rezerv[1] = y.rezerv[2] = 0;
    return y;
}

KdlYetki kdl_yetki_delege(KdlYetki y, uint16_t yeni_izin) {
    KdlYetki y2 = y;
    y2.id = kdl_yetki_sayac++;
    y2.izin = (uint16_t)(yeni_izin & y.izin);
    y2.iptal = 0;
    return y2;
}

void kdl_yetki_geri_al(KdlYetki *y) {
    if (y) y->iptal = 1;
}

int32_t kdl_yetki_kontrol(KdlYetki y, uint16_t gerekli) {
    if (y.iptal) return -2;
    if ((uint16_t)(gerekli & ~y.izin) != 0) return -3;
    return 0;
}
