/*
 * D-146 testi (aarch64) — IP/UDP PAKET GÖNDERME (Faz G — internet katmanı).
 *
 * Kernel geçerli bir IP/UDP paketi inşa eder (IP header checksum dâhil) ve virtio-net
 * ile gönderir → OS'un gerçek internet-katmanı (IPv4 + UDP) paketleri oluşturabildiğini
 * kanıtlar. Paket QEMU filter-dump ile pcap'e yakalanır; gate payload'u pcap'te arar.
 * ARP (D-145) üstüne UDP/IP → gerçek protokol yığını temeli.
 *
 * Kanıt: seri "UDP GONDERILDI" + pcap'te "KEMGU-UDP-DATA".
 */
#include <stdint.h>
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_satir(void);
extern uint64_t kdl_virtio_net_bul(void);
extern int kdl_virtio_net_kur(uint64_t base);
extern int kdl_virtio_net_gonder(uint64_t base, const uint8_t *cerceve, int uzun);

static const uint8_t bizim_mac[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
static uint8_t frame[128];

static void halt(void) { for (;;) { __asm__ volatile("wfe"); } }

/* IPv4 header checksum: 16-bit tümleyen toplamı (RFC 1071). */
static uint16_t ip_checksum(const uint8_t *veri, int uzun) {
    uint32_t toplam = 0;
    for (int i = 0; i + 1 < uzun; i += 2) toplam += ((uint32_t)veri[i] << 8) | veri[i + 1];
    if (uzun & 1) toplam += (uint32_t)veri[uzun - 1] << 8;
    while (toplam >> 16) toplam = (toplam & 0xffff) + (toplam >> 16);
    return (uint16_t)(~toplam);
}

int main(void) {
    kdl_yazdir_metin("UDP BASLA");
    kdl_yazdir_satir();

    uint64_t base = kdl_virtio_net_bul();
    if (!base) { kdl_yazdir_metin("NET YOK"); kdl_yazdir_satir(); halt(); }
    if (kdl_virtio_net_kur(base) != 0) { kdl_yazdir_metin("NET KUR HATA"); kdl_yazdir_satir(); halt(); }

    const char *payload = "KEMGU-UDP-DATA";
    int pl = 14;

    for (int i = 0; i < 128; i++) frame[i] = 0;
    /* --- Ethernet (14) --- */
    for (int i = 0; i < 6; i++) frame[i] = 0xff;               /* dst broadcast */
    for (int i = 0; i < 6; i++) frame[6 + i] = bizim_mac[i];   /* src */
    frame[12] = 0x08; frame[13] = 0x00;                        /* ethertype = IPv4 */

    /* --- IPv4 (offset 14, 20 bayt) --- */
    int ip_total = 20 + 8 + pl;
    frame[14] = 0x45;                                          /* v4, IHL=5 */
    frame[15] = 0x00;                                          /* TOS */
    frame[16] = (uint8_t)(ip_total >> 8); frame[17] = (uint8_t)ip_total;   /* total length */
    frame[18] = 0; frame[19] = 0;                             /* id */
    frame[20] = 0x40; frame[21] = 0;                          /* flags: DF */
    frame[22] = 64;                                           /* TTL */
    frame[23] = 17;                                           /* protocol = UDP */
    frame[24] = 0; frame[25] = 0;                            /* checksum (önce 0) */
    frame[26] = 10; frame[27] = 0; frame[28] = 2; frame[29] = 15;   /* src = 10.0.2.15 */
    frame[30] = 10; frame[31] = 0; frame[32] = 2; frame[33] = 3;    /* dst = 10.0.2.3 (DNS) */
    uint16_t ipsum = ip_checksum(&frame[14], 20);
    frame[24] = (uint8_t)(ipsum >> 8); frame[25] = (uint8_t)ipsum;

    /* --- UDP (offset 34, 8 bayt) --- */
    int udp_len = 8 + pl;
    frame[34] = 0x13; frame[35] = 0x88;                       /* src port = 5000 */
    frame[36] = 0x00; frame[37] = 53;                        /* dst port = 53 (DNS) */
    frame[38] = (uint8_t)(udp_len >> 8); frame[39] = (uint8_t)udp_len;
    frame[40] = 0; frame[41] = 0;                            /* UDP checksum = 0 (opsiyonel) */

    /* --- Payload (offset 42) --- */
    for (int i = 0; i < pl; i++) frame[42 + i] = (uint8_t)payload[i];

    int toplam = 42 + pl;                                     /* 56 bayt */
    if (toplam < 60) toplam = 60;                            /* min ethernet çerçeve */
    kdl_virtio_net_gonder(base, frame, toplam);

    kdl_yazdir_metin("UDP GONDERILDI");
    kdl_yazdir_satir();
    halt();
}
