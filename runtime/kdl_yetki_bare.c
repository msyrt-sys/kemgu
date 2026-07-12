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
 *
 * D-268 OUT-PTR ABI: olustur/delege AÇIK out-pointer ile döner (`void(ptr out, ...)`),
 * sret DEĞİL. Sebep: %kdl_yetki 16B (≤ AAPCS64/SysV register-return eşiği) → clang
 * bare-metal'de struct'ı x0:x1'de register-return ederdi; codegen ise call-site'ta
 * out-pointer (x0) bekliyor. Out-ptr imzası ikisini birebir eşler + saf-.kem sağlayıcı
 * (kem_heap.kem) ile aynı konvansiyonu paylaşır. Bu dosya artık YALNIZ virtio bare-metal
 * link'i içindir — kem_os olustur/geri_al'ı saf-.kem'den alır (Yasa-4, D-268).
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

/* D-268 OUT-PTR: dönüş açık out-pointer üzerinden (sret yerine) — bkz. dosya başı. */
void kdl_yetki_olustur(KdlYetki *out, uint16_t kt, uint16_t izin) {
    out->id = kdl_yetki_sayac++;
    out->kaynak_tipi = kt;
    out->izin = izin;
    out->iptal = 0;
    out->rezerv[0] = out->rezerv[1] = out->rezerv[2] = 0;
}

void kdl_yetki_delege(KdlYetki *out, KdlYetki *y, uint16_t yeni_izin) {
    KdlYetki y2 = *y;
    y2.id = kdl_yetki_sayac++;
    y2.izin = (uint16_t)(yeni_izin & y->izin);
    y2.iptal = 0;
    *out = y2;
}

void kdl_yetki_geri_al(KdlYetki *y) {
    if (y) y->iptal = 1;
}

int32_t kdl_yetki_kontrol(KdlYetki y, uint16_t gerekli) {
    if (y.iptal) return -2;
    if ((uint16_t)(gerekli & ~y.izin) != 0) return -3;
    return 0;
}
