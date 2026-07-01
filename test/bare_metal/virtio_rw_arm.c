/*
 * D-142 testi (aarch64) — VİRTİO-BLK YAZ+OKU round-trip (gerçek kalıcı depolama).
 *
 * Diske bir blok YAZ, sonra aynı bloğu OKU, içeriğin eşleştiğini doğrula → disk
 * gerçekten veri saklıyor (kalıcılık). RAM dosya sistemini disk-backed yapmanın
 * (ileride) temeli.
 *
 *   yaz(blok 7, "KEMGU-YAZDI-42") → oku(blok 7) → eşleşme.
 *
 * Kanıt: "DISK RW OK" (yazılan blok geri okunduğunda aynı).
 */
#include <stdint.h>
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_satir(void);
extern void kdl_yazdir_onaltilik(uint64_t);
extern uint64_t kdl_virtio_blk_bul(void);
extern int kdl_virtio_blk_kur(uint64_t base);
extern int kdl_virtio_blk_oku(uint64_t base, uint64_t sektor, uint8_t *hedef);
extern int kdl_virtio_blk_yaz(uint64_t base, uint64_t sektor, const uint8_t *kaynak);

static uint8_t yaz_veri[512];
static uint8_t oku_veri[512];

static void halt(void) { for (;;) { __asm__ volatile("wfe"); } }

int main(void) {
    kdl_yazdir_metin("VIRTIORW BASLA");
    kdl_yazdir_satir();

    uint64_t base = kdl_virtio_blk_bul();
    if (!base) { kdl_yazdir_metin("DISK YOK"); kdl_yazdir_satir(); halt(); }
    if (kdl_virtio_blk_kur(base) != 0) { kdl_yazdir_metin("KUR HATA"); kdl_yazdir_satir(); halt(); }

    /* Yazılacak desen: "KEMGU-YAZDI-42" (ASCII) + sıfır dolgu. */
    const char *msg = "KEMGU-YAZDI-42";
    for (int i = 0; i < 512; i++) yaz_veri[i] = 0;
    for (int i = 0; msg[i]; i++) yaz_veri[i] = (uint8_t)msg[i];

    if (kdl_virtio_blk_yaz(base, 7, yaz_veri) != 0) { kdl_yazdir_metin("YAZ HATA"); kdl_yazdir_satir(); halt(); }
    for (int i = 0; i < 512; i++) oku_veri[i] = 0xEE;   /* okumadan önce farklı doldur */
    if (kdl_virtio_blk_oku(base, 7, oku_veri) != 0) { kdl_yazdir_metin("OKU HATA"); kdl_yazdir_satir(); halt(); }

    int ok = 1;
    for (int i = 0; i < 32; i++) if (oku_veri[i] != yaz_veri[i]) { ok = 0; break; }
    kdl_yazdir_metin(ok ? "DISK RW OK" : "DISK RW HATA");
    kdl_yazdir_satir();
    halt();
}
