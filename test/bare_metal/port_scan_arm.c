/*
 * PENTEST testi (aarch64) — TCP SYN PORT-TARAMASI (nmap-lite recon).
 *
 * Bir pentest OS'un temel araci: bir host'un hangi portlari acik? Bu test
 * tcp_connect_arm.c'nin (D-159) kanitlanmis TCP SYN insasini + pseudo-header
 * checksum'ini YENIDEN KULLANIR, ama tek bir SYN yerine bir PORT LISTESINE
 * SYN gonderir ve her portu open/closed/filtered olarak siniflandirir:
 *
 *   1) virtio-net kur; ARP ile gateway (SLIRP 10.0.2.2) MAC'ini coz.
 *   2) DNS ile "example.com" A-kaydini coz (dns_resolver_arm.c mantigi) -> hedef IPv4.
 *   3) Bir port listesini tara ({80, 443, 22, 8080, 65000}). Her port icin:
 *      hedef-IP:port'a TCP SYN gonder (dst port = taranan port, pseudo-header
 *      checksum dogru), RX ile kisa poll (~30 iter) yaparak yaniti sinifla:
 *        - SYN-ACK (flags & 0x12 == 0x12)          -> ACIK
 *        - RST     (flags & 0x04)  (RST veya RST+ACK) -> KAPALI
 *        - yanit yok (poll timeout)                 -> FILTRELI
 *      Her SYN'de farkli src port kullan (SRC_PORT_BAZ + indeks) — SLIRP'in
 *      farkli baglantilari ayirt etmesi + gecikmis yanitlarin dogru porta
 *      eslesmesi icin.
 *   4) Her portu + durumunu bas ("PORT 80: ACIK"). En az 1 ACIK port varsa
 *      (example.com:80/443 web acik) -> basari: acik-port sayisi + "PORT SCAN OK".
 *
 * Kanit (internet var): "PORT SCAN OK" (>=1 ACIK port; SYN-ACK RX).
 * Fallback (internet yok / SYN-ACK gelmez): pcap TX kaniti — SYN'ler FARKLI
 * dst-portlara insa+gonderildi (Makefile pcap: birden cok dst-port SYN) ->
 * "PORT SCAN SENT OK".
 *
 * TX-payload marker (pcap fallback icin): TCP seq = 0x4B454D47 ("KEMG").
 */
#include <stdint.h>
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_satir(void);
extern void kdl_yaz_metin(const char *);
extern void kdl_yaz_tam(int32_t);
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
#define SRC_PORT_BAZ 40000       /* her taranan port icin +indeks (farkli baglanti) */
#define TCP_SEQ  0x4B454D47UL    /* "KEMG" — pcap fallback marker + baslangic seq */

/* Taranacak portlar. example.com:80 (HTTP) + :443 (HTTPS) acik olmali. */
static const int taranan_portlar[] = { 80, 443, 22, 8080, 65000 };
#define PORT_SAYISI ((int)(sizeof(taranan_portlar) / sizeof(taranan_portlar[0])))

/* Port durum kodlari. */
#define DURUM_FILTRELI 0
#define DURUM_ACIK     1
#define DURUM_KAPALI   2

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
 * TCP SYN segmenti insa et (veri yok): eth + IPv4(proto=6) + TCP header (20 bayt).
 * tcp_connect_arm.c ile ayni mantik; src_port parametrik (her port taramasinda
 * farkli). frame[] tamponunu doldurur; toplam cerceve uzunlugunu (54) doner.
 */
static int tcp_syn_kur(const uint8_t *dst_ip, int src_port, int dst_port,
                       uint32_t seq, uint8_t flags) {
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
    frame[34] = (uint8_t)(src_port >> 8); frame[35] = (uint8_t)src_port;   /* src port */
    frame[36] = (uint8_t)(dst_port >> 8); frame[37] = (uint8_t)dst_port;   /* dst port */
    frame[38] = (uint8_t)(seq >> 24); frame[39] = (uint8_t)(seq >> 16);
    frame[40] = (uint8_t)(seq >> 8);  frame[41] = (uint8_t)seq;            /* seq */
    frame[42] = 0; frame[43] = 0; frame[44] = 0; frame[45] = 0;            /* ack = 0 */
    frame[46] = 0x50;                                          /* data offset = 5 (20 bayt) */
    frame[47] = flags;                                        /* TCP bayraklari (SYN=0x02) */
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

/* ARP request gonder (tpa = hedef 4-bayt IP), yaniti bekle -> mac_cikti (6 bayt). */
static int arp_coz(uint64_t base, uint8_t a, uint8_t b, uint8_t c, uint8_t d,
                   uint8_t *mac_cikti) {
    for (int i = 0; i < 64; i++) frame[i] = 0;
    for (int i = 0; i < 6; i++) frame[i] = 0xff;                /* dst broadcast */
    for (int i = 0; i < 6; i++) frame[6 + i] = bizim_mac[i];    /* src */
    frame[12] = 0x08; frame[13] = 0x06;                         /* ethertype = ARP */
    frame[14] = 0x00; frame[15] = 0x01; frame[16] = 0x08; frame[17] = 0x00;
    frame[18] = 6; frame[19] = 4; frame[20] = 0x00; frame[21] = 0x01;  /* request */
    for (int i = 0; i < 6; i++) frame[22 + i] = bizim_mac[i];   /* sha = bizim mac */
    frame[28] = 10; frame[29] = 0; frame[30] = 2; frame[31] = 15;   /* spa = 10.0.2.15 */
    frame[38] = a; frame[39] = b; frame[40] = c; frame[41] = d;    /* tpa = hedef */
    kdl_virtio_net_gonder(base, frame, 60);

    for (int t = 0; t < 30; t++) {
        int n = kdl_virtio_net_al(base, rx, 2048, 20000000);
        if (n >= 42 && rx[12] == 0x08 && rx[13] == 0x06 && rx[20] == 0x00 && rx[21] == 0x02 &&
            rx[28] == a && rx[29] == b && rx[30] == c && rx[31] == d) {
            for (int i = 0; i < 6; i++) mac_cikti[i] = rx[22 + i];   /* sha = hedef MAC */
            return 1;
        }
    }
    return 0;
}

int main(void) {
    kdl_yazdir_metin("PORT SCAN BASLA");
    kdl_yazdir_satir();

    uint64_t base = kdl_virtio_net_bul();
    if (!base) { kdl_yazdir_metin("NET YOK"); kdl_yazdir_satir(); halt(); }
    if (kdl_virtio_net_kur(base) != 0) { kdl_yazdir_metin("NET KUR HATA"); kdl_yazdir_satir(); halt(); }

    /* --- 1) ARP: 10.0.2.2 (gateway) MAC'ini coz --- */
    if (!arp_coz(base, 10, 0, 2, 2, gw_mac)) {
        kdl_yazdir_metin("ARP COZULEMEDI"); kdl_yazdir_satir(); halt();
    }

    /* --- 2) DNS: "example.com" A-kaydini coz -> hedef_ip --- */
    /* ARP: 10.0.2.3 (DNS) MAC'ini coz (gw_mac'i ezmez; ayri dns_mac). */
    uint8_t dns_mac[6];
    if (!arp_coz(base, 10, 0, 2, 3, dns_mac)) {
        kdl_yazdir_metin("DNS ARP COZULEMEDI"); kdl_yazdir_satir(); halt();
    }

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

    kdl_yaz_metin("HEDEF IP=");
    kdl_yaz_onaltilik(hedef_ip[0]); kdl_yaz_metin(".");
    kdl_yaz_onaltilik(hedef_ip[1]); kdl_yaz_metin(".");
    kdl_yaz_onaltilik(hedef_ip[2]); kdl_yaz_metin(".");
    kdl_yaz_onaltilik(hedef_ip[3]);
    kdl_yazdir_satir();

    /* --- 3) Port listesini tara --- */
    int acik_sayi = 0;
    for (int p = 0; p < PORT_SAYISI; p++) {
        int dst_port = taranan_portlar[p];
        int src_port = SRC_PORT_BAZ + p;   /* her port icin farkli src (baglanti ayirimi) */

        /* SYN gonder: hedef_ip:dst_port, seq="KEMG". */
        int toplam = tcp_syn_kur(hedef_ip, src_port, dst_port, TCP_SEQ, 0x02);   /* SYN */
        kdl_virtio_net_gonder(base, frame, toplam);

        /* Yaniti bekle (kisa poll ~30 iter): SYN-ACK / RST / timeout. */
        int durum = DURUM_FILTRELI;
        for (int d = 0; d < 30 && durum == DURUM_FILTRELI; d++) {
            int n = kdl_virtio_net_al(base, rx, 2048, 20000000);
            if (n < 54) continue;
            if (rx[12] != 0x08 || rx[13] != 0x00) continue;       /* IPv4 */
            if (rx[23] != 6) continue;                            /* proto = TCP */
            /* src IP = hedef (SLIRP proxy'de kaynak IP korunur) */
            if (!(rx[26] == hedef_ip[0] && rx[27] == hedef_ip[1] &&
                  rx[28] == hedef_ip[2] && rx[29] == hedef_ip[3])) continue;
            int rsrc = (rx[34] << 8) | rx[35];                    /* gelen src port */
            int rdst = (rx[36] << 8) | rx[37];                    /* gelen dst port */
            /* Bu yanit bu taramaya mi ait? (gelen src=taranan port, dst=bizim src) */
            if (rsrc != dst_port || rdst != src_port) continue;
            uint8_t flags = rx[47];
            if ((flags & 0x12) == 0x12) {                         /* SYN+ACK -> ACIK */
                durum = DURUM_ACIK;
                /* Nazik kapanis: RST gonder (yarim-acik baglantiyi hemen birak). */
                uint32_t onlarin_seq = ((uint32_t)rx[38] << 24) | ((uint32_t)rx[39] << 16) |
                                       ((uint32_t)rx[40] << 8)  | (uint32_t)rx[41];
                int rt = tcp_syn_kur(hedef_ip, src_port, dst_port,
                                     (uint32_t)(TCP_SEQ + 1), 0x04);   /* RST */
                (void)onlarin_seq;
                kdl_virtio_net_gonder(base, frame, rt);
            } else if (flags & 0x04) {                            /* RST (0x04/0x14) -> KAPALI */
                durum = DURUM_KAPALI;
            }
        }

        /* Port + durum bas: "PORT <n>: <DURUM>" tek satir. */
        kdl_yaz_metin("PORT ");
        kdl_yaz_tam((int32_t)dst_port);
        kdl_yaz_metin(": ");
        if (durum == DURUM_ACIK) { kdl_yaz_metin("ACIK"); acik_sayi++; }
        else if (durum == DURUM_KAPALI) kdl_yaz_metin("KAPALI");
        else kdl_yaz_metin("FILTRELI");
        kdl_yazdir_satir();
    }

    /* --- 4) Sonuc --- */
    if (acik_sayi > 0) {
        kdl_yaz_metin("ACIK PORT SAYISI=");
        kdl_yaz_tam((int32_t)acik_sayi);
        kdl_yazdir_satir();
        kdl_yazdir_metin("PORT SCAN OK");
        kdl_yazdir_satir();
    } else {
        /* Internet yok / SYN-ACK gelmedi -> pcap TX fallback devrede.
         * SYN'ler FARKLI dst-portlara insa+gonderildi (pcap'te dogrulanir). */
        kdl_yazdir_metin("ACIK PORT YOK (SYN-ACK GELMEDI)");
        kdl_yazdir_satir();
    }
    halt();
}
