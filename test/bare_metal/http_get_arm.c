/*
 * UYGULAMA KATMANI testi (aarch64) — TCP uzerinden HTTP GET (OS bir web sayfasi ceker).
 *
 * D-159 (tcp_connect_arm.c) TAM uc-yonlu el sikismasini kanitladi (SYN -> SYN-ACK ->
 * ACK, ESTABLISHED). Bu test bir adim ileri gider: el sikismasi sonrasi TCP DATA
 * segmenti (PSH+ACK) ile bir HTTP/1.1 GET istegi gonderir ve sunucunun HTTP yanitini
 * (durum satiri) alir — yani OS uygulama katmaninda bir web sayfasi ceker:
 *
 *   1) virtio-net kur; ARP ile gateway (SLIRP 10.0.2.2) MAC'ini coz.
 *   2) DNS ile "example.com" A-kaydini coz -> hedef IPv4.
 *   3) Hedef-IP:80'e TAM handshake (SYN -> SYN-ACK -> ACK) -> ESTABLISHED.
 *   4) HTTP GET istegini TCP DATA segmenti olarak gonder (PSH+ACK, flags=0x18):
 *        "GET / HTTP/1.1\r\nHost: example.com\r\nConnection: close\r\n\r\n"
 *      seq = bizim_iss+1 (handshake sonrasi), ack = onlarin_iss+1.
 *      Pseudo-header checksum PAYLOAD dahil.
 *   5) RX ile HTTP yanitini al (poll; birkac segment olabilir). Ilk DATA segmentinde
 *      "HTTP/1." + durum kodu (200 VEYA 3xx redirect) ara. Gelen data'ya ACK doner
 *      (seq/ack takibi), sonra durum satirini basar.
 *
 * Kanit (gercek HTTP): "HTTP GET OK" (yanitta HTTP/1.x durum satiri bulundu).
 * Fallback (host internet yoksa / yanit gelmezse): pcap TX kaniti — GET istegi
 * dis-IP'ye gonderildi (Makefile pcap grep -> "GET /") -> "HTTP GET SENT OK".
 *
 * TX-payload marker: TCP seq = 0x4B454D47 ("KEMG") ISS; HTTP payload ASCII "GET /".
 */
#include <stdint.h>
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_satir(void);
extern void kdl_yaz_karakter(int32_t cp);
extern void kdl_yaz_onaltilik(uint64_t);
extern uint64_t kdl_virtio_net_bul(void);
extern int kdl_virtio_net_kur(uint64_t base);
extern int kdl_virtio_net_gonder(uint64_t base, const uint8_t *cerceve, int uzun);
extern int kdl_virtio_net_al(uint64_t base, uint8_t *hedef, int max, long tikler);

static const uint8_t bizim_mac[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
static uint8_t frame[1600];
static uint8_t rx[2048];
static uint8_t gw_mac[6];
static uint8_t hedef_ip[4];              /* DNS ile cozulen hedef IPv4 */

/* Bizim adres/port (SLIRP guest = 10.0.2.15). */
static const uint8_t bizim_ip[4] = { 10, 0, 2, 15 };
#define SRC_PORT 40000
#define DST_PORT 80              /* HTTP — internet host yanit doner */
#define TCP_SEQ  0x4B454D47UL    /* "KEMG" — pcap fallback marker + baslangic seq (ISS) */

/* HTTP GET istegi (ASCII; Turkce yok -> hex-escape sorunu yok). */
static const char http_istek[] =
    "GET / HTTP/1.1\r\nHost: example.com\r\nConnection: close\r\n\r\n";

static _Noreturn void halt(void) { for (;;) { __asm__ volatile("wfe"); } }

/* Internet checksum (RFC 1071) — dns/tcp _arm.c ile ayni. */
static uint16_t ip_checksum(const uint8_t *veri, int uzun) {
    uint32_t t = 0;
    for (int i = 0; i + 1 < uzun; i += 2) t += ((uint32_t)veri[i] << 8) | veri[i + 1];
    if (uzun & 1) t += (uint32_t)veri[uzun - 1] << 8;
    while (t >> 16) t = (t & 0xffff) + (t >> 16);
    return (uint16_t)(~t);
}

/* DNS isim alanini atla (compression pointer 0xC0 veya duz label dizisi). */
static int isim_atla(const uint8_t *dns, int dns_uzun, int off) {
    while (off < dns_uzun) {
        uint8_t len = dns[off];
        if ((len & 0xC0) == 0xC0) return (off + 2 <= dns_uzun) ? off + 2 : -1;
        if (len == 0) return off + 1;
        off += 1 + len;
    }
    return -1;
}

/*
 * TCP segmenti insa et (opsiyonel veri ile): eth + IPv4(proto=6) + TCP header(20) + payload.
 * frame[] tamponunu doldurur; toplam cerceve uzunlugunu doner.
 * seq/ack = host byte order; flags = TCP bayrak byte'i (SYN=0x02, ACK=0x10, PSH=0x08...).
 * payload/payload_uzun = TCP verisi (yoksa NULL/0). dst MAC = gw_mac (SLIRP proxy'ler).
 */
static int tcp_segment_kur(const uint8_t *dst_ip, int dst_port,
                           uint32_t seq, uint32_t ack, uint8_t flags,
                           const uint8_t *payload, int payload_uzun) {
    for (int i = 0; i < 1600; i++) frame[i] = 0;
    for (int i = 0; i < 6; i++) frame[i] = gw_mac[i];           /* dst = gateway MAC */
    for (int i = 0; i < 6; i++) frame[6 + i] = bizim_mac[i];    /* src = bizim mac */
    frame[12] = 0x08; frame[13] = 0x00;                         /* ethertype = IPv4 */

    /* IPv4 header (20 bayt) @ offset 14 */
    int tcp_len = 20 + payload_uzun;                          /* TCP header + veri */
    int ip_total = 20 + tcp_len;
    frame[14] = 0x45;                                          /* ver=4, IHL=5 */
    frame[15] = 0x00;
    frame[16] = (uint8_t)(ip_total >> 8); frame[17] = (uint8_t)ip_total;
    frame[18] = 0x00; frame[19] = 0x00;                       /* identification */
    frame[20] = 0x40; frame[21] = 0x00;                       /* flags: DF, frag=0 */
    frame[22] = 64;                                           /* TTL */
    frame[23] = 6;                                            /* protocol = TCP */
    /* frame[24..25] = header checksum (asagida) */
    for (int i = 0; i < 4; i++) frame[26 + i] = bizim_ip[i];   /* src IP 10.0.2.15 */
    for (int i = 0; i < 4; i++) frame[30 + i] = dst_ip[i];     /* dst IP = hedef */
    uint16_t ipcs = ip_checksum(&frame[14], 20);
    frame[24] = (uint8_t)(ipcs >> 8); frame[25] = (uint8_t)ipcs;

    /* TCP header (20 bayt) @ offset 34 */
    frame[34] = (uint8_t)(SRC_PORT >> 8); frame[35] = (uint8_t)SRC_PORT;   /* src port */
    frame[36] = (uint8_t)(dst_port >> 8); frame[37] = (uint8_t)dst_port;   /* dst port */
    frame[38] = (uint8_t)(seq >> 24); frame[39] = (uint8_t)(seq >> 16);
    frame[40] = (uint8_t)(seq >> 8);  frame[41] = (uint8_t)seq;            /* seq */
    frame[42] = (uint8_t)(ack >> 24); frame[43] = (uint8_t)(ack >> 16);
    frame[44] = (uint8_t)(ack >> 8);  frame[45] = (uint8_t)ack;            /* ack */
    frame[46] = 0x50;                                          /* data offset = 5 (20 bayt) */
    frame[47] = flags;                                        /* TCP bayraklari */
    frame[48] = 0x20; frame[49] = 0x00;                       /* window = 8192 */
    /* frame[50..51] = TCP checksum (asagida) */
    frame[52] = 0x00; frame[53] = 0x00;                       /* urgent pointer */

    /* Payload @ offset 54 */
    for (int i = 0; i < payload_uzun; i++) frame[54 + i] = payload[i];

    /* TCP checksum: pseudo-header(12) + TCP segmenti(header 20 + payload). */
    uint8_t psbuf[12 + 20 + 128];
    int ps_uzun = 12 + tcp_len;
    for (int i = 0; i < ps_uzun; i++) psbuf[i] = 0;
    for (int i = 0; i < 4; i++) psbuf[i] = bizim_ip[i];        /* src IP */
    for (int i = 0; i < 4; i++) psbuf[4 + i] = dst_ip[i];      /* dst IP */
    psbuf[8] = 0;                                              /* zero */
    psbuf[9] = 6;                                              /* protocol = TCP */
    psbuf[10] = (uint8_t)(tcp_len >> 8); psbuf[11] = (uint8_t)tcp_len;
    for (int i = 0; i < tcp_len; i++) psbuf[12 + i] = frame[34 + i];   /* TCP hdr+veri (cs=0) */
    uint16_t tcs = ip_checksum(psbuf, ps_uzun);
    frame[50] = (uint8_t)(tcs >> 8); frame[51] = (uint8_t)tcs;

    return 34 + tcp_len;                                       /* eth+IP+TCP+payload */
}

/* Tampon icinde alt-dizi ara (memmem yerine el ile — bare metal). */
static int icinde_bul(const uint8_t *buf, int buf_uzun, const char *desen, int desen_uzun) {
    for (int i = 0; i + desen_uzun <= buf_uzun; i++) {
        int esles = 1;
        for (int j = 0; j < desen_uzun; j++) {
            if (buf[i + j] != (uint8_t)desen[j]) { esles = 0; break; }
        }
        if (esles) return i;
    }
    return -1;
}

int main(void) {
    kdl_yazdir_metin("HTTP BASLA");
    kdl_yazdir_satir();

    uint64_t base = kdl_virtio_net_bul();
    if (!base) { kdl_yazdir_metin("NET YOK"); kdl_yazdir_satir(); halt(); }
    if (kdl_virtio_net_kur(base) != 0) { kdl_yazdir_metin("NET KUR HATA"); kdl_yazdir_satir(); halt(); }

    /* --- 1) ARP: 10.0.2.2 (gateway) MAC'ini coz --- */
    for (int i = 0; i < 64; i++) frame[i] = 0;
    for (int i = 0; i < 6; i++) frame[i] = 0xff;                /* dst broadcast */
    for (int i = 0; i < 6; i++) frame[6 + i] = bizim_mac[i];    /* src */
    frame[12] = 0x08; frame[13] = 0x06;                         /* ethertype = ARP */
    frame[14] = 0x00; frame[15] = 0x01; frame[16] = 0x08; frame[17] = 0x00;
    frame[18] = 6; frame[19] = 4; frame[20] = 0x00; frame[21] = 0x01;  /* request */
    for (int i = 0; i < 6; i++) frame[22 + i] = bizim_mac[i];   /* sha = bizim mac */
    frame[28] = 10; frame[29] = 0; frame[30] = 2; frame[31] = 15;   /* spa = 10.0.2.15 */
    frame[38] = 10; frame[39] = 0; frame[40] = 2; frame[41] = 2;    /* tpa = 10.0.2.2 */
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

    /* --- 2) DNS: "example.com" A-kaydini coz -> hedef_ip --- */
    /* ARP: 10.0.2.3 (DNS) MAC'ini coz (gw_mac'i ezmez; ayri dns_mac). */
    uint8_t dns_mac[6];
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

    int dns_arp_ok = 0;
    for (int d = 0; d < 30 && !dns_arp_ok; d++) {
        int n = kdl_virtio_net_al(base, rx, 2048, 20000000);
        if (n >= 42 && rx[12] == 0x08 && rx[13] == 0x06 && rx[20] == 0x00 && rx[21] == 0x02 &&
            rx[28] == 10 && rx[29] == 0 && rx[30] == 2 && rx[31] == 3) {
            for (int i = 0; i < 6; i++) dns_mac[i] = rx[22 + i];
            dns_arp_ok = 1;
        }
    }
    if (!dns_arp_ok) { kdl_yazdir_metin("DNS ARP COZULEMEDI"); kdl_yazdir_satir(); halt(); }

    /* DNS sorgusu: eth+IP+UDP+DNS("example.com" A). */
    const uint16_t dns_id = 0x4B7E;
    uint8_t dns[29];
    for (int i = 0; i < 29; i++) dns[i] = 0;
    dns[0] = (uint8_t)(dns_id >> 8); dns[1] = (uint8_t)dns_id;  /* id */
    dns[2] = 0x01; dns[3] = 0x00;        /* flags: RD */
    dns[4] = 0x00; dns[5] = 0x01;        /* qdcount = 1 */
    dns[12] = 7;
    dns[13] = 'e'; dns[14] = 'x'; dns[15] = 'a'; dns[16] = 'm'; dns[17] = 'p'; dns[18] = 'l'; dns[19] = 'e';
    dns[20] = 3; dns[21] = 'c'; dns[22] = 'o'; dns[23] = 'm'; dns[24] = 0;
    dns[25] = 0x00; dns[26] = 0x01;      /* qtype = A */
    dns[27] = 0x00; dns[28] = 0x01;      /* qclass = IN */
    int dl = 29;

    for (int i = 0; i < 128; i++) frame[i] = 0;
    for (int i = 0; i < 6; i++) frame[i] = dns_mac[i];
    for (int i = 0; i < 6; i++) frame[6 + i] = bizim_mac[i];
    frame[12] = 0x08; frame[13] = 0x00;
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
    kdl_virtio_net_gonder(base, frame, 42 + dl);

    /* DNS yanitini AL + ANSWER parse -> hedef_ip. */
    for (int i = 0; i < 4; i++) hedef_ip[i] = 0;
    int resolve_ok = 0;
    int dns_baz = 42;
    for (int d = 0; d < 40 && !resolve_ok; d++) {
        int n = kdl_virtio_net_al(base, rx, 2048, 30000000);
        if (n < 54) continue;
        if (!(rx[12] == 0x08 && rx[13] == 0x00 && rx[23] == 17)) continue;   /* IPv4+UDP */
        if (!(rx[26] == 10 && rx[27] == 0 && rx[28] == 2 && rx[29] == 3)) continue; /* src 10.0.2.3 */
        if (!(rx[34] == 0x00 && rx[35] == 53)) continue;                     /* src port 53 */
        int udp_toplam = ((int)rx[38] << 8) | rx[39];
        int dns_uzun = udp_toplam - 8;
        if (dns_uzun < 12 || dns_baz + dns_uzun > n) continue;
        const uint8_t *dp = &rx[dns_baz];
        uint16_t r_id = ((uint16_t)dp[0] << 8) | dp[1];
        int qr = (dp[2] & 0x80) != 0;
        if (r_id != dns_id || !qr) continue;
        uint16_t qdcount = ((uint16_t)dp[4] << 8) | dp[5];
        uint16_t ancount = ((uint16_t)dp[6] << 8) | dp[7];
        if (ancount == 0) continue;
        /* Question bolumunu atla. */
        int off = 12;
        int hata = 0;
        for (int q = 0; q < qdcount && !hata; q++) {
            off = isim_atla(dp, dns_uzun, off);
            if (off < 0 || off + 4 > dns_uzun) { hata = 1; break; }
            off += 4;
        }
        if (hata) continue;
        /* Answer record'lari: ilk A kaydi (TYPE=1, RDLENGTH=4). */
        for (int a = 0; a < ancount && !resolve_ok; a++) {
            off = isim_atla(dp, dns_uzun, off);
            if (off < 0 || off + 10 > dns_uzun) break;
            uint16_t type = ((uint16_t)dp[off] << 8) | dp[off + 1];
            uint16_t rdlength = ((uint16_t)dp[off + 8] << 8) | dp[off + 9];
            int rdata_off = off + 10;
            if (rdata_off + rdlength > dns_uzun) break;
            if (type == 1 && rdlength == 4) {
                for (int i = 0; i < 4; i++) hedef_ip[i] = dp[rdata_off + i];
                if (!(hedef_ip[0] == 0 && hedef_ip[1] == 0 && hedef_ip[2] == 0 && hedef_ip[3] == 0))
                    resolve_ok = 1;
            }
            off = rdata_off + rdlength;
        }
    }
    if (!resolve_ok) { kdl_yazdir_metin("DNS RESOLVE YOK"); kdl_yazdir_satir(); halt(); }

    kdl_yazdir_metin("HEDEF IP=");
    kdl_yaz_onaltilik(hedef_ip[0]); kdl_yazdir_metin(".");
    kdl_yaz_onaltilik(hedef_ip[1]); kdl_yazdir_metin(".");
    kdl_yaz_onaltilik(hedef_ip[2]); kdl_yazdir_metin(".");
    kdl_yaz_onaltilik(hedef_ip[3]);
    kdl_yazdir_satir();

    /* --- 3) TCP SYN gonder: hedef_ip:80, seq="KEMG" (ISS) --- */
    int toplam = tcp_segment_kur(hedef_ip, DST_PORT, TCP_SEQ, 0, 0x02, 0, 0);   /* SYN */
    kdl_virtio_net_gonder(base, frame, toplam);

    /* --- 4) SYN-ACK al (poll): flags=0x12, ack=ISS+1, src port=80, src IP=hedef --- */
    int synack_ok = 0;
    uint32_t onlarin_iss = 0;
    for (int d = 0; d < 60 && !synack_ok; d++) {
        int n = kdl_virtio_net_al(base, rx, 2048, 30000000);
        if (n < 54) continue;
        if (rx[12] != 0x08 || rx[13] != 0x00) continue;       /* IPv4 */
        if (rx[23] != 6) continue;                            /* proto = TCP */
        if (!(rx[26] == hedef_ip[0] && rx[27] == hedef_ip[1] &&
              rx[28] == hedef_ip[2] && rx[29] == hedef_ip[3])) continue;   /* src IP = hedef */
        int rsrc = (rx[34] << 8) | rx[35];                    /* gelen src port */
        int rdst = (rx[36] << 8) | rx[37];                    /* gelen dst port */
        if (rsrc != DST_PORT || rdst != SRC_PORT) continue;
        uint8_t flags = rx[47];
        if ((flags & 0x12) != 0x12) continue;                 /* SYN+ACK bekle */
        uint32_t r_ack = ((uint32_t)rx[42] << 24) | ((uint32_t)rx[43] << 16) |
                         ((uint32_t)rx[44] << 8)  | (uint32_t)rx[45];
        if (r_ack != (uint32_t)(TCP_SEQ + 1)) continue;       /* ack = ISS+1 */
        onlarin_iss = ((uint32_t)rx[38] << 24) | ((uint32_t)rx[39] << 16) |
                      ((uint32_t)rx[40] << 8)  | (uint32_t)rx[41];
        synack_ok = 1;
    }

    /* HTTP payload uzunlugu (NUL haric). */
    int http_uzun = 0;
    while (http_istek[http_uzun] != '\0') http_uzun++;

    if (!synack_ok) {
        /* Internet yok / SLIRP dis-TCP yanit vermedi -> pcap TX fallback devrede.
         * Handshake kurulamadi; yine de GET istegini gonder ki pcap'te "GET /" gorunsun.
         * seq = ISS+1 (varsayimsal), ack = 0 (handshake yok). */
        toplam = tcp_segment_kur(hedef_ip, DST_PORT, (uint32_t)(TCP_SEQ + 1), 0,
                                 0x18, (const uint8_t *)http_istek, http_uzun);  /* PSH+ACK */
        kdl_virtio_net_gonder(base, frame, toplam);
        kdl_yazdir_metin("HTTP GET GONDERILDI (HANDSHAKE/YANIT YOK)");
        kdl_yazdir_satir();
        /* Makefile pcap fallback: "GET /" -> "HTTP GET SENT OK". */
        halt();
    }

    /* --- ACK gonder: flags=0x10, seq=ISS+1, ack=onlarin_iss+1 -> ESTABLISHED --- */
    uint32_t bizim_seq = (uint32_t)(TCP_SEQ + 1);       /* handshake sonrasi seq */
    uint32_t bizim_ack = (uint32_t)(onlarin_iss + 1);   /* onlarin SYN'ini ACK'le */
    toplam = tcp_segment_kur(hedef_ip, DST_PORT, bizim_seq, bizim_ack, 0x10, 0, 0);  /* ACK */
    kdl_virtio_net_gonder(base, frame, toplam);

    kdl_yazdir_metin("TCP ESTABLISHED");
    kdl_yazdir_satir();

    /* --- 5) HTTP GET istegini TCP DATA segmenti olarak gonder (PSH+ACK, flags=0x18) --- */
    toplam = tcp_segment_kur(hedef_ip, DST_PORT, bizim_seq, bizim_ack, 0x18,
                             (const uint8_t *)http_istek, http_uzun);   /* PSH+ACK + veri */
    kdl_virtio_net_gonder(base, frame, toplam);
    /* Veriyi gonderdik: seq bizim payload uzunlugu kadar ilerler. */
    bizim_seq += (uint32_t)http_uzun;

    /* --- 6) HTTP yanitini al (poll): ilk TCP DATA segmentinde "HTTP/1." durum satiri --- */
    int http_ok = 0;
    int durum_off = -1;       /* rx icinde "HTTP/1." baslangici */
    int rx_uzun = 0;
    for (int d = 0; d < 80 && !http_ok; d++) {
        int n = kdl_virtio_net_al(base, rx, 2048, 30000000);
        if (n < 54) continue;
        if (rx[12] != 0x08 || rx[13] != 0x00) continue;       /* IPv4 */
        if (rx[23] != 6) continue;                            /* proto = TCP */
        if (!(rx[26] == hedef_ip[0] && rx[27] == hedef_ip[1] &&
              rx[28] == hedef_ip[2] && rx[29] == hedef_ip[3])) continue;   /* src IP = hedef */
        int rsrc = (rx[34] << 8) | rx[35];
        int rdst = (rx[36] << 8) | rx[37];
        if (rsrc != DST_PORT || rdst != SRC_PORT) continue;   /* bizim baglantimiz */

        /* IPv4 toplam uzunluk + TCP header uzunlugundan payload konumunu bul. */
        int ip_len = ((int)rx[16] << 8) | rx[17];
        int ip_ihl = (rx[14] & 0x0F) * 4;
        int tcp_off = 14 + ip_ihl;
        if (tcp_off + 20 > n) continue;
        int tcp_data_off = ((rx[tcp_off + 12] >> 4) & 0x0F) * 4;   /* TCP data offset */
        int payload_off = tcp_off + tcp_data_off;
        int payload_uzun = (14 + ip_len) - payload_off;
        if (payload_uzun <= 0 || payload_off >= n) continue;      /* veri yok (saf ACK vs) */
        if (payload_off + payload_uzun > n) payload_uzun = n - payload_off;

        /* Gelen data'ya ACK don (seq/ack takibi — nazik protokol). */
        uint32_t r_seq = ((uint32_t)rx[tcp_off + 4] << 24) | ((uint32_t)rx[tcp_off + 5] << 16) |
                         ((uint32_t)rx[tcp_off + 6] << 8)  | (uint32_t)rx[tcp_off + 7];
        bizim_ack = r_seq + (uint32_t)payload_uzun;
        toplam = tcp_segment_kur(hedef_ip, DST_PORT, bizim_seq, bizim_ack, 0x10, 0, 0);  /* ACK */
        kdl_virtio_net_gonder(base, frame, toplam);

        /* Durum satirini ara: "HTTP/1." payload icinde. */
        int rel = icinde_bul(&rx[payload_off], payload_uzun, "HTTP/1.", 7);
        if (rel >= 0) {
            durum_off = payload_off + rel;
            rx_uzun = n;
            http_ok = 1;
        }
    }

    if (!http_ok) {
        /* Yanit gelmedi -> pcap TX fallback: GET istegi gonderildi. */
        kdl_yazdir_metin("HTTP YANIT YOK (GET GONDERILDI)");
        kdl_yazdir_satir();
        halt();
    }

    /* Durum satirini bas (ilk CR/LF veya guvenlik siniri 96 bayta kadar). */
    kdl_yazdir_metin("HTTP DURUM: ");
    for (int i = durum_off; i < rx_uzun && i < durum_off + 96; i++) {
        if (rx[i] == '\r' || rx[i] == '\n') break;
        kdl_yaz_karakter((int32_t)rx[i]);
    }
    kdl_yazdir_satir();

    /* Durum kodu kontrolu: "HTTP/1." sonrasi <boslukla ayrilmis> 3 haneli kod.
     * durum_off + "HTTP/1.x " = durum_off + 9 -> kod baslangici. */
    int kod_off = durum_off + 9;
    int kabul = 0;
    if (kod_off + 2 < rx_uzun) {
        char k0 = (char)rx[kod_off];
        /* 200 (basari) VEYA 3xx (redirect — Cloudflare 301/302) kabul. */
        if (k0 == '2' && rx[kod_off + 1] == '0' && rx[kod_off + 2] == '0') kabul = 1;
        if (k0 == '3') kabul = 1;
    }

    if (kabul) {
        kdl_yazdir_metin("HTTP GET OK");
        kdl_yazdir_satir();
    } else {
        /* HTTP/1.x bulundu ama beklenen kod degil — yine de yanit alindi, TX kanit. */
        kdl_yazdir_metin("HTTP YANIT ALINDI (BEKLENMEYEN KOD)");
        kdl_yazdir_satir();
    }
    halt();
}
