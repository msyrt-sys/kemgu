/*
 * D-145 testi (aarch64) — ARP ROUND-TRIP (2-yönlü ağ: gönder + AL).
 *
 * Kernel ağ geçidi için (QEMU SLIRP: 10.0.2.2) bir ARP İSTEĞİ yollar (broadcast).
 * SLIRP ARP YANITI ile karşılık verir (ağ geçidi MAC'i). Kernel yanıtı virtio-net
 * RX ile ALIR + ARP-reply olduğunu doğrular → gerçek ağ round-trip'i (TX+RX).
 *
 * Kanıt: "ARP REPLY OK" (ağ geçidinden ARP yanıtı alındı → 2-yönlü ağ çalışıyor).
 */
#include <stdint.h>
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_satir(void);
extern uint64_t kdl_virtio_net_bul(void);
extern int kdl_virtio_net_kur(uint64_t base);
extern int kdl_virtio_net_gonder(uint64_t base, const uint8_t *cerceve, int uzun);
extern int kdl_virtio_net_al(uint64_t base, uint8_t *hedef, int max, long tikler);

static const uint8_t bizim_mac[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
static uint8_t frame[64];
static uint8_t rx[2048];

static void halt(void) { for (;;) { __asm__ volatile("wfe"); } }

int main(void) {
    kdl_yazdir_metin("ARP BASLA");
    kdl_yazdir_satir();

    uint64_t base = kdl_virtio_net_bul();
    if (!base) { kdl_yazdir_metin("NET YOK"); kdl_yazdir_satir(); halt(); }
    if (kdl_virtio_net_kur(base) != 0) { kdl_yazdir_metin("NET KUR HATA"); kdl_yazdir_satir(); halt(); }

    /* ARP isteği: eth(broadcast, bizim mac, 0x0806) + arp(request, tpa=10.0.2.2). */
    for (int i = 0; i < 64; i++) frame[i] = 0;
    for (int i = 0; i < 6; i++) frame[i] = 0xff;               /* dst broadcast */
    for (int i = 0; i < 6; i++) frame[6 + i] = bizim_mac[i];   /* src */
    frame[12] = 0x08; frame[13] = 0x06;                        /* ethertype = ARP */
    frame[14] = 0x00; frame[15] = 0x01;                        /* htype = ethernet */
    frame[16] = 0x08; frame[17] = 0x00;                        /* ptype = IPv4 */
    frame[18] = 6; frame[19] = 4;                              /* hlen, plen */
    frame[20] = 0x00; frame[21] = 0x01;                        /* oper = request */
    for (int i = 0; i < 6; i++) frame[22 + i] = bizim_mac[i];  /* sha = bizim mac */
    frame[28] = 10; frame[29] = 0; frame[30] = 2; frame[31] = 15;  /* spa = 10.0.2.15 */
    /* tha = 0 (bilinmiyor) */
    frame[38] = 10; frame[39] = 0; frame[40] = 2; frame[41] = 2;   /* tpa = 10.0.2.2 (gateway) */

    kdl_virtio_net_gonder(base, frame, 60);

    /* Yanıtı AL: ARP-reply (oper=2) ve spa=10.0.2.2 arayan birkaç paket dinle. */
    int bulundu = 0;
    for (int deneme = 0; deneme < 30 && !bulundu; deneme++) {
        int n = kdl_virtio_net_al(base, rx, 2048, 20000000);
        if (n >= 42 && rx[12] == 0x08 && rx[13] == 0x06 &&        /* ARP */
            rx[20] == 0x00 && rx[21] == 0x02 &&                   /* oper = reply */
            rx[28] == 10 && rx[29] == 0 && rx[30] == 2 && rx[31] == 2) {   /* spa = gateway */
            bulundu = 1;
        }
    }

    kdl_yazdir_metin(bulundu ? "ARP REPLY OK" : "ARP REPLY YOK");
    kdl_yazdir_satir();
    halt();
}
