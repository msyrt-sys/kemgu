/*
 * Faz H testi (aarch64) — TCP UC-YONLU EL SIKISMASI (SYN -> SYN-ACK/RST).
 *
 * Tum ag yigini bir arada: ARP ile gateway'in (SLIRP 10.0.2.2) MAC'ini coz ->
 * eth+IPv4(proto=6)+TCP SYN segmenti insa et (TCP checksum PSEUDO-HEADER dahil) +
 * gonder -> yaniti virtio-net RX ile AL + dogrula (IPv4+TCP, src-port=bizim dst,
 * dst-port=bizim src, SYN+ACK VEYA RST bayragi). Gercek TCP el sikismasi round-trip'i.
 *
 * Deterministik hedef: kapali bir port (10.0.2.2:9999) -> SLIRP RST doner.
 * Acik port yaniti (SYN-ACK) da kabul edilir. Ikisi de TCP RX kaniti.
 *
 * Kanit (RX round-trip): "TCP HANDSHAKE OK".
 * TX-payload marker (pcap fallback icin): TCP seq = 0x4B454D47 ("KEMG").
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
static uint8_t gw_mac[6];

/* Bizim adres/port (SLIRP guest = 10.0.2.15). */
#define SRC_PORT 40000
#define DST_PORT 9999            /* kapali port -> SLIRP RST (deterministik) */
#define TCP_SEQ  0x4B454D47UL    /* "KEMG" — pcap fallback marker */

static _Noreturn void halt(void) { for (;;) { __asm__ volatile("wfe"); } }

/* Internet checksum (RFC 1071) — dns_arm.c ile ayni; TCP icin pseudo-header
 * onune eklenmis gecici tampon uzerinde cagrilir. */
static uint16_t ip_checksum(const uint8_t *veri, int uzun) {
    uint32_t t = 0;
    for (int i = 0; i + 1 < uzun; i += 2) t += ((uint32_t)veri[i] << 8) | veri[i + 1];
    if (uzun & 1) t += (uint32_t)veri[uzun - 1] << 8;
    while (t >> 16) t = (t & 0xffff) + (t >> 16);
    return (uint16_t)(~t);
}

int main(void) {
    kdl_yazdir_metin("TCP BASLA");
    kdl_yazdir_satir();

    uint64_t base = kdl_virtio_net_bul();
    if (!base) { kdl_yazdir_metin("NET YOK"); kdl_yazdir_satir(); halt(); }
    if (kdl_virtio_net_kur(base) != 0) { kdl_yazdir_metin("NET KUR HATA"); kdl_yazdir_satir(); halt(); }

    /* --- 1) ARP: 10.0.2.2 (gateway) MAC'ini coz --- */
    for (int i = 0; i < 64; i++) frame[i] = 0;
    for (int i = 0; i < 6; i++) frame[i] = 0xff;                /* dst broadcast */
    for (int i = 0; i < 6; i++) frame[6 + i] = bizim_mac[i];    /* src */
    frame[12] = 0x08; frame[13] = 0x06;                         /* ethertype = ARP */
    frame[14] = 0x00; frame[15] = 0x01;                         /* htype = ethernet */
    frame[16] = 0x08; frame[17] = 0x00;                         /* ptype = IPv4 */
    frame[18] = 6; frame[19] = 4;                              /* hlen, plen */
    frame[20] = 0x00; frame[21] = 0x01;                        /* oper = request */
    for (int i = 0; i < 6; i++) frame[22 + i] = bizim_mac[i];  /* sha = bizim mac */
    frame[28] = 10; frame[29] = 0; frame[30] = 2; frame[31] = 15;  /* spa = 10.0.2.15 */
    frame[38] = 10; frame[39] = 0; frame[40] = 2; frame[41] = 2;   /* tpa = 10.0.2.2 */
    kdl_virtio_net_gonder(base, frame, 60);

    int arp_ok = 0;
    for (int d = 0; d < 30 && !arp_ok; d++) {
        int n = kdl_virtio_net_al(base, rx, 2048, 20000000);
        if (n >= 42 && rx[12] == 0x08 && rx[13] == 0x06 && rx[20] == 0x00 && rx[21] == 0x02 &&
            rx[28] == 10 && rx[29] == 0 && rx[30] == 2 && rx[31] == 2) {
            for (int i = 0; i < 6; i++) gw_mac[i] = rx[22 + i];   /* sha = gateway MAC */
            arp_ok = 1;
        }
    }
    if (!arp_ok) { kdl_yazdir_metin("ARP COZULEMEDI"); kdl_yazdir_satir(); halt(); }

    /* --- 2) TCP SYN segmenti: eth + IPv4(proto=6) + TCP header (20 bayt, veri yok) --- */
    for (int i = 0; i < 128; i++) frame[i] = 0;
    for (int i = 0; i < 6; i++) frame[i] = gw_mac[i];           /* dst = gateway MAC */
    for (int i = 0; i < 6; i++) frame[6 + i] = bizim_mac[i];    /* src = bizim mac */
    frame[12] = 0x08; frame[13] = 0x00;                         /* ethertype = IPv4 */

    /* IPv4 header (20 bayt) @ offset 14 */
    int tcp_len = 20;                                          /* TCP header, veri yok */
    int ip_total = 20 + tcp_len;                              /* IP toplam uzunlugu */
    frame[14] = 0x45;                                          /* ver=4, IHL=5 */
    frame[15] = 0x00;                                          /* DSCP/ECN */
    frame[16] = (uint8_t)(ip_total >> 8); frame[17] = (uint8_t)ip_total;
    frame[18] = 0x00; frame[19] = 0x00;                       /* identification */
    frame[20] = 0x40; frame[21] = 0x00;                       /* flags: DF, frag=0 */
    frame[22] = 64;                                           /* TTL */
    frame[23] = 6;                                            /* protocol = TCP */
    /* frame[24..25] = header checksum (asagida) */
    frame[26] = 10; frame[27] = 0; frame[28] = 2; frame[29] = 15;   /* src IP 10.0.2.15 */
    frame[30] = 10; frame[31] = 0; frame[32] = 2; frame[33] = 2;    /* dst IP 10.0.2.2 */
    uint16_t ipcs = ip_checksum(&frame[14], 20);
    frame[24] = (uint8_t)(ipcs >> 8); frame[25] = (uint8_t)ipcs;

    /* TCP header (20 bayt) @ offset 34 */
    frame[34] = (uint8_t)(SRC_PORT >> 8); frame[35] = (uint8_t)SRC_PORT;   /* src port */
    frame[36] = (uint8_t)(DST_PORT >> 8); frame[37] = (uint8_t)DST_PORT;   /* dst port */
    frame[38] = (uint8_t)(TCP_SEQ >> 24); frame[39] = (uint8_t)(TCP_SEQ >> 16);
    frame[40] = (uint8_t)(TCP_SEQ >> 8);  frame[41] = (uint8_t)TCP_SEQ;    /* seq = "KEMG" */
    frame[42] = 0; frame[43] = 0; frame[44] = 0; frame[45] = 0;           /* ack = 0 */
    frame[46] = 0x50;                                          /* data offset = 5 (20 bayt), rezerve=0 */
    frame[47] = 0x02;                                          /* flags: SYN */
    frame[48] = 0x20; frame[49] = 0x00;                       /* window = 8192 */
    /* frame[50..51] = TCP checksum (asagida) */
    frame[52] = 0x00; frame[53] = 0x00;                       /* urgent pointer */

    /* TCP checksum: pseudo-header (12 bayt) + TCP segmenti (20 bayt) = 32 bayt.
     * Pseudo-header: src IP(4) + dst IP(4) + zero(1) + proto(1) + TCP-len(2). */
    uint8_t psbuf[12 + 20];
    for (int i = 0; i < 32; i++) psbuf[i] = 0;
    psbuf[0] = 10; psbuf[1] = 0; psbuf[2] = 2; psbuf[3] = 15;   /* src IP */
    psbuf[4] = 10; psbuf[5] = 0; psbuf[6] = 2; psbuf[7] = 2;    /* dst IP */
    psbuf[8] = 0;                                               /* zero */
    psbuf[9] = 6;                                               /* protocol = TCP */
    psbuf[10] = (uint8_t)(tcp_len >> 8); psbuf[11] = (uint8_t)tcp_len;   /* TCP length */
    for (int i = 0; i < tcp_len; i++) psbuf[12 + i] = frame[34 + i];     /* TCP header (cs=0) */
    uint16_t tcs = ip_checksum(psbuf, 12 + tcp_len);
    frame[50] = (uint8_t)(tcs >> 8); frame[51] = (uint8_t)tcs;

    int toplam = 34 + tcp_len;                                /* eth(14) + IP(20) + TCP(20) = 54 */
    kdl_virtio_net_gonder(base, frame, toplam);

    /* --- 3) TCP yanitini AL: IPv4+TCP, src=10.0.2.2, src-port=DST_PORT,
     *        dst-port=SRC_PORT, ve (SYN+ACK bayragi VEYA RST bayragi). --- */
    int tcp_ok = 0;
    for (int d = 0; d < 40 && !tcp_ok; d++) {
        int n = kdl_virtio_net_al(base, rx, 2048, 30000000);
        if (n < 54) continue;
        if (rx[12] != 0x08 || rx[13] != 0x00) continue;       /* IPv4 */
        if (rx[23] != 6) continue;                            /* proto = TCP */
        /* src IP = 10.0.2.2 (gateway) */
        if (!(rx[26] == 10 && rx[27] == 0 && rx[28] == 2 && rx[29] == 2)) continue;
        int rsrc = (rx[34] << 8) | rx[35];                    /* gelen src port */
        int rdst = (rx[36] << 8) | rx[37];                    /* gelen dst port */
        if (rsrc != DST_PORT || rdst != SRC_PORT) continue;
        uint8_t flags = rx[47];
        int synack = (flags & 0x12) == 0x12;                  /* SYN+ACK */
        int rst    = (flags & 0x04) != 0;                     /* RST */
        if (synack || rst) tcp_ok = 1;
    }

    kdl_yazdir_metin(tcp_ok ? "TCP HANDSHAKE OK" : "TCP HANDSHAKE YOK");
    kdl_yazdir_satir();
    halt();
}
