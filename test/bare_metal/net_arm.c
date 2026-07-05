/*
 * D-144 testi (aarch64) — VİRTİO-NET PAKET GÖNDERME (Faz G ağ başlangıcı).
 *
 * Kernel virtio-net cihazından bir Ethernet çerçevesi gönderir (broadcast, özel
 * ethertype 0x88b5, payload "KEMGUNET-PAKET"). Paket QEMU `-object filter-dump`
 * ile pcap'e yakalanır → gate payload'u pcap'te arar (gerçek ağ TX kanıtı).
 * virtio-blk (D-141) virtqueue makinesi yeniden kullanıldı (queue 1 = transmit).
 *
 * Kanıt: seri "NET GONDERILDI" + pcap'te "KEMGUNET-PAKET" baytları.
 */
#include <stdint.h>
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_satir(void);
extern void kdl_yazdir_onaltilik(uint64_t);
extern uint64_t kdl_virtio_net_bul(void);
extern int kdl_virtio_net_kur(uint64_t base);
extern int kdl_virtio_net_gonder(uint64_t base, const uint8_t *cerceve, int uzun);

static uint8_t frame[64];
static void halt(void) { for (;;) { __asm__ volatile("wfe"); } }

int main(void) {
    kdl_yazdir_metin("NET BASLA");
    kdl_yazdir_satir();

    uint64_t base = kdl_virtio_net_bul();
    if (!base) { kdl_yazdir_metin("NET YOK (device bulunamadi)"); kdl_yazdir_satir(); halt(); }
    if (kdl_virtio_net_kur(base) != 0) { kdl_yazdir_metin("NET KUR HATA"); kdl_yazdir_satir(); halt(); }

    /* Ethernet çerçevesi: dst=broadcast, src=52:54:00:12:34:56, ethertype=0x88b5. */
    for (int i = 0; i < 64; i++) frame[i] = 0;
    for (int i = 0; i < 6; i++) frame[i] = 0xff;
    frame[6] = 0x52; frame[7] = 0x54; frame[8] = 0x00; frame[9] = 0x12; frame[10] = 0x34; frame[11] = 0x56;
    frame[12] = 0x88; frame[13] = 0xb5;
    const char *pl = "KEMGUNET-PAKET";
    for (int j = 0; pl[j]; j++) frame[14 + j] = (uint8_t)pl[j];

    int r = kdl_virtio_net_gonder(base, frame, 60);   /* min ethernet çerçeve (dolgulu) */
    if (r != 0) {
        kdl_yazdir_metin("NET GONDER HATA r=0x");
        kdl_yazdir_onaltilik((uint64_t)(int64_t)r);
        kdl_yazdir_satir();
        halt();
    }
    kdl_yazdir_metin("NET GONDERILDI");
    kdl_yazdir_satir();
    halt();
}
