/*
 * D-141 testi (aarch64) — VİRTİO-BLK GERÇEK DİSK OKUMA (C5, Faz E depolama).
 *
 * QEMU'ya bağlı gerçek bir diskten (build/disk.img, blok 0'a "KEMGU..." yazılı)
 * virtio-blk sürücüsüyle 512-baytlık blok okur + içeriği doğrular. RAM dosya
 * sistemini (D-131) KALICI depolamaya bağlamanın ilk adımı.
 *
 * Kanıt: "DISK OK KEMGU" (virtio-blk sürücüsü gerçek diskten blok 0'ı okudu).
 */
#include <stdint.h>
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_satir(void);
extern void kdl_yazdir_onaltilik(uint64_t);
extern uint64_t kdl_virtio_blk_bul(void);
extern int kdl_virtio_blk_kur(uint64_t base);
extern int kdl_virtio_blk_oku(uint64_t base, uint64_t sektor, uint8_t *hedef);

static uint8_t blok[512];

static void halt(void) { for (;;) { __asm__ volatile("wfe"); } }

int main(void) {
    kdl_yazdir_metin("VIRTIO BASLA");
    kdl_yazdir_satir();

    uint64_t base = kdl_virtio_blk_bul();
    if (!base) { kdl_yazdir_metin("DISK YOK (device bulunamadi)"); kdl_yazdir_satir(); halt(); }

    int r = kdl_virtio_blk_kur(base);
    if (r != 0) {
        kdl_yazdir_metin("DISK KUR HATA r=0x");
        kdl_yazdir_onaltilik((uint64_t)(int64_t)r);
        kdl_yazdir_satir();
        halt();
    }

    r = kdl_virtio_blk_oku(base, 0, blok);
    if (r != 0) {
        kdl_yazdir_metin("DISK OKU HATA r=0x");
        kdl_yazdir_onaltilik((uint64_t)(int64_t)r);
        kdl_yazdir_satir();
        halt();
    }

    if (blok[0] == 'K' && blok[1] == 'E' && blok[2] == 'M' && blok[3] == 'G' && blok[4] == 'U') {
        kdl_yazdir_metin("DISK OK KEMGU");
    } else {
        kdl_yazdir_metin("DISK VERI HATA");
    }
    kdl_yazdir_satir();
    halt();
}
