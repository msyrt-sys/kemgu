/*
 * D-143 testi (aarch64) — KALICI DOSYA SİSTEMİ (disk-backed persistence).
 *
 * Aynı kernel AYNI diskle İKİ KEZ boot edilir:
 *   Boot 1: diskte FS yok → dosya oluştur ("kalici"=777) + diske KAYDET → "FIRST BOOT".
 *   Boot 2: diskte FS var (magic) → diskten YÜKLE → "SECOND BOOT kalici=777".
 * Dosya BOOT'LAR ARASI yaşadı → GERÇEK KALICILIK (RAM-FS + virtio-blk disk).
 *
 * Kanıt (boot 2): "SECOND BOOT kalici=777" (ilk boot'ta yazılan dosya, kernel
 * yeniden başlayınca diskten geri geldi).
 */
#include <stdint.h>
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_satir(void);
extern void kdl_yaz_metin(const char *);
extern void kdl_yazdir_tam(int32_t);
extern uint64_t kdl_virtio_blk_bul(void);
extern int kdl_virtio_blk_kur(uint64_t base);
extern int kdl_dosya_yukle(uint64_t base);
extern int kdl_dosya_kaydet(uint64_t base);
extern int kdl_dosya_olustur_deger(const char *ad, int64_t deger);
extern int64_t kdl_dosya_deger(const char *ad);

static void halt(void) { for (;;) { __asm__ volatile("wfe"); } }

int main(void) {
    kdl_yazdir_metin("KALICI BASLA");
    kdl_yazdir_satir();

    uint64_t base = kdl_virtio_blk_bul();
    if (!base) { kdl_yazdir_metin("DISK YOK"); kdl_yazdir_satir(); halt(); }
    if (kdl_virtio_blk_kur(base) != 0) { kdl_yazdir_metin("KUR HATA"); kdl_yazdir_satir(); halt(); }

    if (kdl_dosya_yukle(base) == 0) {
        /* İkinci boot: dosya sistemi diskten yüklendi. */
        int64_t v = kdl_dosya_deger("kalici");
        kdl_yaz_metin("SECOND BOOT kalici=");
        kdl_yazdir_tam((int32_t)v);              /* → "SECOND BOOT kalici=777" */
    } else {
        /* İlk boot: dosya oluştur + diske kaydet. */
        kdl_dosya_olustur_deger("kalici", 777);
        kdl_dosya_kaydet(base);
        kdl_yazdir_metin("FIRST BOOT saved kalici=777");
    }
    kdl_yazdir_satir();
    halt();
}
