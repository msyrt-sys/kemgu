/*
 * MILESTONE D testi (aarch64) — TCP ZARIF KAPANIS (FIN 4-yonlu teardown).
 *
 * D-159 (tcp_connect_arm.c) TCP FSM'in ACILISINI kanitladi (SYN -> SYN-ACK -> ACK,
 * ESTABLISHED). Bu test o yasam-dongusunu KAPANISLA tamamlar: ESTABLISHED sonrasi
 * zarif kapanis — bizim FIN -> peer ACK + peer FIN -> bizim ACK -> CLOSED. Boylece
 * TAM TCP yasam-dongusu (open + close) 4-yonlu teardown ile gosterilir:
 *
 *   1) virtio-net kur; ARP ile gateway (SLIRP 10.0.2.2) MAC'ini coz.
 *   2) DNS ile "example.com" A-kaydini coz -> hedef IPv4.
 *   3) Hedef-IP:80'e TAM handshake (SYN -> SYN-ACK -> ACK) -> ESTABLISHED.
 *   4) FIN yolla (flags = FIN|ACK = 0x11): seq = ISS+1, ack = onlarin_iss+1.
 *      -> FIN_WAIT_1. Kapanisi biz baslatiriz (active close).
 *   5) Peer'in FIN'imize ACK'ini AL (poll): flags ACK, ack = bizim_seq+1
 *      (yani ISS+2, cunku FIN 1 seq tuketir). -> FIN_WAIT_2.
 *   6) Peer'in FIN'ini AL (poll): flags FIN. -> son ACK gonder:
 *      ack = onlarin_fin_seq+1. -> TIME_WAIT / CLOSED.
 *   7) "TCP CLOSE OK".
 *
 * NOT: Peer bir web sunucusu (example.com:80). Handshake sonrasi hicbir veri
 * gondermeden dogrudan FIN yollariz; sunucu (Connection semantigi geregi) FIN'imize
 * ACK doner ve kendi FIN'ini yollar. Kimi sunucular FIN-ACK + FIN'i tek segmentte
 * (flags = FIN|ACK) birlestirir -> bu durumu da ele aliriz (birlesik teardown).
 *
 * Kanit (gercek 4-yonlu): "TCP CLOSE OK" (bizim FIN + peer ACK + peer FIN + son ACK).
 * Fallback (host internet yoksa / peer FIN gelmezse): en az bizim-FIN TX-pcap kaniti
 *   ("KEMG" seq + FIN bayragi) + ESTABLISHED + (varsa) FIN-ACK RX = yari-kapanis.
 *   Report'ta yari-kapanis olarak belirtilir.
 *
 * TX-payload marker (pcap fallback icin): TCP seq = 0x4B454D47 ("KEMG") ISS.
 */
#include <stdint.h>
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_satir(void);
extern void kdl_yaz_onaltilik(uint64_t);
extern uint64_t kdl_virtio_net_bul(void);
extern int kdl_virtio_net_kur(uint64_t base);
extern int kdl_virtio_net_gonder(uint64_t base, const uint8_t *cerceve, int uzun);
extern int kdl_virtio_net_al(uint64_t base, uint8_t *hedef, int max, long tikler);

static const uint8_t bizim_mac[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
static uint8_t frame[128];
static uint8_t rx[2048];
static uint8_t gw_mac[6];
static uint8_t hedef_ip[4];              /* DNS ile cozulen hedef IPv4 */

/* Bizim adres/port (SLIRP guest = 10.0.2.15). */
static const uint8_t bizim_ip[4] = { 10, 0, 2, 15 };
#define SRC_PORT 40000
#define DST_PORT 80              /* HTTP — internet host handshake+kapanisa yanit verir */
#define TCP_SEQ  0x4B454D47UL    /* "KEMG" — pcap fallback marker + baslangic seq (ISS) */

/* TCP bayraklari. */
#define TCP_FIN  0x01
#define TCP_ACK  0x10

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
 * TCP segmenti insa et (veri yok): eth + IPv4(proto=6) + TCP header (20 bayt).
 * frame[] tamponunu doldurur; toplam cerceve uzunlugunu (54) doner.
 * seq/ack = host byte order; flags = TCP bayrak byte'i (FIN=0x01, ACK=0x10, ...).
 * dst_ip = 4 bayt hedef IPv4; dst MAC = gw_mac (SLIRP proxy'ler).
 */
static int tcp_segment_kur(const uint8_t *dst_ip, int dst_port,
                           uint32_t seq, uint32_t ack, uint8_t flags) {
    for (int i = 0; i < 128; i++) frame[i] = 0;
    for (int i = 0; i < 6; i++) frame[i] = gw_mac[i];           /* dst = gateway MAC */
    for (int i = 0; i < 6; i++) frame[6 + i] = bizim_mac[i];    /* src = bizim mac */
    frame[12] = 0x08; frame[13] = 0x00;                         /* ethertype = IPv4 */

    /* IPv4 header (20 bayt) @ offset 14 */
    int tcp_len = 20;                                          /* TCP header, veri yok */
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

    /* TCP checksum: pseudo-header(12) + TCP segmenti(20) = 32 bayt. */
    uint8_t psbuf[12 + 20];
    for (int i = 0; i < 32; i++) psbuf[i] = 0;
    for (int i = 0; i < 4; i++) psbuf[i] = bizim_ip[i];        /* src IP */
    for (int i = 0; i < 4; i++) psbuf[4 + i] = dst_ip[i];      /* dst IP */
    psbuf[8] = 0;                                              /* zero */
    psbuf[9] = 6;                                              /* protocol = TCP */
    psbuf[10] = (uint8_t)(tcp_len >> 8); psbuf[11] = (uint8_t)tcp_len;
    for (int i = 0; i < tcp_len; i++) psbuf[12 + i] = frame[34 + i];   /* TCP header (cs=0) */
    uint16_t tcs = ip_checksum(psbuf, 12 + tcp_len);
    frame[50] = (uint8_t)(tcs >> 8); frame[51] = (uint8_t)tcs;

    return 34 + tcp_len;                                       /* eth+IP+TCP = 54 */
}

/*
 * Gelen cerceve bizim TCP baglantimiza ait mi? (hedef IP + port eslemesi)
 * Dogruysa 1 doner; TCP header ofsetini *tcp_off ile bildirir.
 */
static int bizim_tcp_mi(const uint8_t *r, int n, int *tcp_off) {
    if (n < 54) return 0;
    if (r[12] != 0x08 || r[13] != 0x00) return 0;             /* IPv4 */
    if (r[23] != 6) return 0;                                 /* proto = TCP */
    if (!(r[26] == hedef_ip[0] && r[27] == hedef_ip[1] &&
          r[28] == hedef_ip[2] && r[29] == hedef_ip[3])) return 0;   /* src IP = hedef */
    int ip_ihl = (r[14] & 0x0F) * 4;
    int t = 14 + ip_ihl;
    if (t + 20 > n) return 0;
    int rsrc = (r[t] << 8) | r[t + 1];                        /* gelen src port */
    int rdst = (r[t + 2] << 8) | r[t + 3];                    /* gelen dst port */
    if (rsrc != DST_PORT || rdst != SRC_PORT) return 0;       /* bizim baglantimiz */
    *tcp_off = t;
    return 1;
}

int main(void) {
    kdl_yazdir_metin("TCP CLOSE BASLA");
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
        int n = kdl_virtio_net_al(base, rx, 2048, 3000000);
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
        int n = kdl_virtio_net_al(base, rx, 2048, 3000000);
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
        int n = kdl_virtio_net_al(base, rx, 2048, 5000000);
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
    int toplam = tcp_segment_kur(hedef_ip, DST_PORT, TCP_SEQ, 0, 0x02);   /* SYN */
    kdl_virtio_net_gonder(base, frame, toplam);

    /* --- 4) SYN-ACK al (poll): flags=0x12, ack=ISS+1, src port=80, src IP=hedef --- */
    int synack_ok = 0;
    uint32_t onlarin_iss = 0;
    for (int d = 0; d < 60 && !synack_ok; d++) {
        int n = kdl_virtio_net_al(base, rx, 2048, 5000000);
        int t;
        if (!bizim_tcp_mi(rx, n, &t)) continue;
        uint8_t flags = rx[t + 13];
        if ((flags & 0x12) != 0x12) continue;                 /* SYN+ACK bekle */
        uint32_t r_ack = ((uint32_t)rx[t + 8] << 24) | ((uint32_t)rx[t + 9] << 16) |
                         ((uint32_t)rx[t + 10] << 8)  | (uint32_t)rx[t + 11];
        if (r_ack != (uint32_t)(TCP_SEQ + 1)) continue;       /* ack = ISS+1 */
        onlarin_iss = ((uint32_t)rx[t + 4] << 24) | ((uint32_t)rx[t + 5] << 16) |
                      ((uint32_t)rx[t + 6] << 8)  | (uint32_t)rx[t + 7];
        synack_ok = 1;
    }

    if (!synack_ok) {
        /* Internet yok / SLIRP dis-TCP yanit vermedi. ESTABLISHED kurulamadi;
         * yine de FIN'imizi gonder ki pcap'te bizim-FIN TX kaniti gorunsun
         * (seq="KEMG", flags=FIN|ACK). Makefile pcap fallback devrede. */
        toplam = tcp_segment_kur(hedef_ip, DST_PORT, (uint32_t)(TCP_SEQ + 1), 0,
                                 (uint8_t)(TCP_FIN | TCP_ACK));   /* FIN|ACK */
        kdl_virtio_net_gonder(base, frame, toplam);
        kdl_yazdir_metin("TCP FIN GONDERILDI (HANDSHAKE/YANIT YOK)");
        kdl_yazdir_satir();
        halt();
    }

    /* --- ACK gonder: flags=0x10, seq=ISS+1, ack=onlarin_iss+1 -> ESTABLISHED --- */
    uint32_t bizim_seq = (uint32_t)(TCP_SEQ + 1);       /* handshake sonrasi seq */
    uint32_t bizim_ack = (uint32_t)(onlarin_iss + 1);   /* onlarin SYN'ini ACK'le */
    toplam = tcp_segment_kur(hedef_ip, DST_PORT, bizim_seq, bizim_ack, TCP_ACK);
    kdl_virtio_net_gonder(base, frame, toplam);

    kdl_yazdir_metin("TCP ESTABLISHED");
    kdl_yazdir_satir();

    /* --- 5) ZARIF KAPANIS: FIN|ACK yolla (active close -> FIN_WAIT_1) --- */
    /* seq = ISS+1 (veri gondermedik), ack = onlarin_iss+1. FIN 1 seq tuketir. */
    toplam = tcp_segment_kur(hedef_ip, DST_PORT, bizim_seq, bizim_ack,
                             (uint8_t)(TCP_FIN | TCP_ACK));   /* FIN|ACK */
    kdl_virtio_net_gonder(base, frame, toplam);
    kdl_yazdir_metin("TCP FIN GONDERILDI");
    kdl_yazdir_satir();

    /* FIN 1 seq tuketti: bir sonraki bizim seq = ISS+2. */
    uint32_t bizim_fin_seq_ust = bizim_seq + 1;         /* = ISS+2; peer bunu ACK'lemeli */

    /*
     * --- 6) Peer'in yanitini AL (poll): iki olay bekleriz ---
     *   (a) FIN-ACK: peer bizim FIN'imizi ACK'ler (ack = ISS+2). -> FIN_WAIT_2.
     *   (b) FIN: peer kendi FIN'ini yollar. -> son ACK gondeririz -> CLOSED.
     * Bazi peer'lar (a)+(b)'yi tek segmentte birlestirir (flags = FIN|ACK).
     * Ayrica peer bizim gorulmemis oncesi HTTP/data segmenti gondermis olabilir;
     * onlari da ACK'leriz (bizim_ack ilerler) ama teardown mantigi FIN/ACK'e bakar.
     */
    int finack_rx = 0;       /* peer bizim FIN'imizi ACK'ledi (yari-kapanis) */
    int peer_fin_rx = 0;     /* peer kendi FIN'ini yolladi */
    int close_ok = 0;        /* son ACK gonderildi -> CLOSED */

    for (int d = 0; d < 80 && !close_ok; d++) {
        int n = kdl_virtio_net_al(base, rx, 2048, 5000000);
        int t;
        if (!bizim_tcp_mi(rx, n, &t)) continue;

        uint8_t flags = rx[t + 13];
        uint32_t r_seq = ((uint32_t)rx[t + 4] << 24) | ((uint32_t)rx[t + 5] << 16) |
                         ((uint32_t)rx[t + 6] << 8)  | (uint32_t)rx[t + 7];
        uint32_t r_ack = ((uint32_t)rx[t + 8] << 24) | ((uint32_t)rx[t + 9] << 16) |
                         ((uint32_t)rx[t + 10] << 8)  | (uint32_t)rx[t + 11];

        /* IPv4 toplam + TCP data-offset'ten payload uzunlugu (FIN de 1 seq tuketir). */
        int ip_len = ((int)rx[16] << 8) | rx[17];
        int tcp_data_off = ((rx[t + 12] >> 4) & 0x0F) * 4;
        int payload_off = t + tcp_data_off;
        int payload_uzun = (14 + ip_len) - payload_off;
        if (payload_uzun < 0) payload_uzun = 0;

        /* (a) Peer FIN'imizi ACK'ledi mi? ack = ISS+2 (bizim_fin_seq_ust). */
        if ((flags & TCP_ACK) && r_ack == bizim_fin_seq_ust && !finack_rx) {
            finack_rx = 1;
            kdl_yazdir_metin("FIN-ACK RX (FIN_WAIT_2)");
            kdl_yazdir_satir();
        }

        /* (b) Peer kendi FIN'ini yolladi mi? */
        if (flags & TCP_FIN) {
            peer_fin_rx = 1;
            /* Peer FIN'i 1 seq tuketir. Son ACK: ack = peer_fin_seq + payload + 1. */
            uint32_t son_ack = r_seq + (uint32_t)payload_uzun + 1;
            /* Bizim seq: FIN'i gonderdikten sonra bir sonraki = ISS+2. */
            toplam = tcp_segment_kur(hedef_ip, DST_PORT, bizim_fin_seq_ust, son_ack, TCP_ACK);
            kdl_virtio_net_gonder(base, frame, toplam);
            kdl_yazdir_metin("PEER FIN RX -> SON ACK");
            kdl_yazdir_satir();
            close_ok = 1;                                     /* 4-yonlu tamamlandi */
        } else if (payload_uzun > 0) {
            /* Peer henuz FIN yollamadan veri gonderdi (HTTP vs.) -> ACK'le, devam et. */
            uint32_t data_ack = r_seq + (uint32_t)payload_uzun;
            toplam = tcp_segment_kur(hedef_ip, DST_PORT, bizim_fin_seq_ust, data_ack, TCP_ACK);
            kdl_virtio_net_gonder(base, frame, toplam);
        }
    }

    if (close_ok) {
        /* Bizim FIN + peer ACK (varsa) + peer FIN + bizim son ACK = 4-yonlu teardown. */
        kdl_yazdir_metin("TCP CLOSE OK");
        kdl_yazdir_satir();
    } else if (finack_rx) {
        /* Yari-kapanis: FIN'imiz ACK'lendi ama peer FIN gelmedi (FIN_WAIT_2'de takildi). */
        kdl_yazdir_metin("TCP HALF CLOSE (FIN-ACK RX, PEER FIN YOK)");
        kdl_yazdir_satir();
    } else {
        /* Peer yanit vermedi: en az bizim-FIN TX-pcap kaniti + ESTABLISHED var. */
        kdl_yazdir_metin("TCP FIN GONDERILDI (PEER YANIT YOK)");
        kdl_yazdir_satir();
    }
    (void)peer_fin_rx;
    halt();
}
