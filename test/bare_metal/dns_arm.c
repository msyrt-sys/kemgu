/*
 * D-147 testi (aarch64) — DNS ROUND-TRIP (UDP request-response, "OS internet'le konuşuyor").
 *
 * Tüm ağ yığını bir arada: ARP ile DNS sunucusunun (SLIRP 10.0.2.3) MAC'ini çöz →
 * IP/UDP DNS sorgusu inşa et + gönder → yanıtı virtio-net RX ile AL + doğrula
 * (IPv4+UDP, src=10.0.2.3, src-port=53). Gerçek istek-yanıt döngüsü.
 *
 * Kanıt: "DNS REPLY OK" (DNS sunucusundan UDP yanıtı alındı → internet-katmanı round-trip).
 */
#include <stdint.h>
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_satir(void);
extern uint64_t kdl_virtio_net_bul(void);
extern int kdl_virtio_net_kur(uint64_t base);
extern int kdl_virtio_net_gonder(uint64_t base, const uint8_t *cerceve, int uzun);
extern int kdl_virtio_net_al(uint64_t base, uint8_t *hedef, int max, long tikler);

static const uint8_t bizim_mac[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
static uint8_t frame[128];
static uint8_t rx[2048];
static uint8_t dns_mac[6];

static void halt(void) { for (;;) { __asm__ volatile("wfe"); } }

static uint16_t ip_checksum(const uint8_t *veri, int uzun) {
    uint32_t t = 0;
    for (int i = 0; i + 1 < uzun; i += 2) t += ((uint32_t)veri[i] << 8) | veri[i + 1];
    if (uzun & 1) t += (uint32_t)veri[uzun - 1] << 8;
    while (t >> 16) t = (t & 0xffff) + (t >> 16);
    return (uint16_t)(~t);
}

int main(void) {
    kdl_yazdir_metin("DNS BASLA");
    kdl_yazdir_satir();

    uint64_t base = kdl_virtio_net_bul();
    if (!base) { kdl_yazdir_metin("NET YOK"); kdl_yazdir_satir(); halt(); }
    if (kdl_virtio_net_kur(base) != 0) { kdl_yazdir_metin("NET KUR HATA"); kdl_yazdir_satir(); halt(); }

    /* --- 1) ARP: 10.0.2.3 (DNS) MAC'ini çöz --- */
    for (int i = 0; i < 64; i++) frame[i] = 0;
    for (int i = 0; i < 6; i++) frame[i] = 0xff;
    for (int i = 0; i < 6; i++) frame[6 + i] = bizim_mac[i];
    frame[12] = 0x08; frame[13] = 0x06;
    frame[14] = 0x00; frame[15] = 0x01; frame[16] = 0x08; frame[17] = 0x00;
    frame[18] = 6; frame[19] = 4; frame[20] = 0x00; frame[21] = 0x01;
    for (int i = 0; i < 6; i++) frame[22 + i] = bizim_mac[i];
    frame[28] = 10; frame[29] = 0; frame[30] = 2; frame[31] = 15;
    frame[38] = 10; frame[39] = 0; frame[40] = 2; frame[41] = 3;   /* tpa = 10.0.2.3 */
    kdl_virtio_net_gonder(base, frame, 60);

    int arp_ok = 0;
    for (int d = 0; d < 30 && !arp_ok; d++) {
        int n = kdl_virtio_net_al(base, rx, 2048, 20000000);
        if (n >= 42 && rx[12] == 0x08 && rx[13] == 0x06 && rx[20] == 0x00 && rx[21] == 0x02 &&
            rx[28] == 10 && rx[29] == 0 && rx[30] == 2 && rx[31] == 3) {
            for (int i = 0; i < 6; i++) dns_mac[i] = rx[22 + i];   /* sha = DNS MAC */
            arp_ok = 1;
        }
    }
    if (!arp_ok) { kdl_yazdir_metin("ARP COZULEMEDI"); kdl_yazdir_satir(); halt(); }

    /* --- 2) DNS sorgusu: eth+IP+UDP+DNS("a.com" A) --- */
    /* DNS payload: header(12) + qname("\x01a\x03com\x00"=7) + qtype(2) + qclass(2) = 23 */
    uint8_t dns[23];
    for (int i = 0; i < 23; i++) dns[i] = 0;
    dns[0] = 0x12; dns[1] = 0x34;        /* id */
    dns[2] = 0x01; dns[3] = 0x00;        /* flags: RD */
    dns[4] = 0x00; dns[5] = 0x01;        /* qdcount = 1 */
    dns[12] = 1; dns[13] = 'a'; dns[14] = 3; dns[15] = 'c'; dns[16] = 'o'; dns[17] = 'm'; dns[18] = 0;
    dns[19] = 0x00; dns[20] = 0x01;      /* qtype = A */
    dns[21] = 0x00; dns[22] = 0x01;      /* qclass = IN */
    int dl = 23;

    for (int i = 0; i < 128; i++) frame[i] = 0;
    for (int i = 0; i < 6; i++) frame[i] = dns_mac[i];         /* dst = DNS MAC */
    for (int i = 0; i < 6; i++) frame[6 + i] = bizim_mac[i];
    frame[12] = 0x08; frame[13] = 0x00;                        /* IPv4 */
    int ip_total = 20 + 8 + dl;
    frame[14] = 0x45; frame[16] = (uint8_t)(ip_total >> 8); frame[17] = (uint8_t)ip_total;
    frame[20] = 0x40; frame[22] = 64; frame[23] = 17;         /* DF, TTL, UDP */
    frame[26] = 10; frame[27] = 0; frame[28] = 2; frame[29] = 15;
    frame[30] = 10; frame[31] = 0; frame[32] = 2; frame[33] = 3;
    uint16_t s = ip_checksum(&frame[14], 20); frame[24] = (uint8_t)(s >> 8); frame[25] = (uint8_t)s;
    int udp_len = 8 + dl;
    frame[34] = 0x13; frame[35] = 0x88;                        /* src port 5000 */
    frame[36] = 0x00; frame[37] = 53;                        /* dst port 53 */
    frame[38] = (uint8_t)(udp_len >> 8); frame[39] = (uint8_t)udp_len;
    for (int i = 0; i < dl; i++) frame[42 + i] = dns[i];
    int toplam = 42 + dl;
    kdl_virtio_net_gonder(base, frame, toplam);

    /* --- 3) DNS yanıtını AL (IPv4+UDP, src=10.0.2.3, src-port=53) --- */
    int dns_ok = 0;
    for (int d = 0; d < 40 && !dns_ok; d++) {
        int n = kdl_virtio_net_al(base, rx, 2048, 30000000);
        if (n >= 42 && rx[12] == 0x08 && rx[13] == 0x00 && rx[23] == 17 &&   /* IPv4 + UDP */
            rx[26] == 10 && rx[27] == 0 && rx[28] == 2 && rx[29] == 3 &&     /* src = 10.0.2.3 */
            rx[34] == 0x00 && rx[35] == 53) {                                /* src port = 53 */
            dns_ok = 1;
        }
    }

    kdl_yazdir_metin(dns_ok ? "DNS REPLY OK" : "DNS REPLY YOK");
    kdl_yazdir_satir();
    halt();
}
