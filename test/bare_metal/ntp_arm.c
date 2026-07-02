/*
 * NTP (SNTP) İSTEMCİSİ testi (aarch64) — internetten gerçek zaman senkronizasyonu
 * (Faz G/H — AĞ/ZAMAN milestone).
 *
 * OS açılışta internetten doğru saati öğrenir. Bu, dns_resolver_arm.c (isim→IP
 * çözümleme) + udp_arm.c (IP/UDP inşa) üzerine kurulu tam bir uygulama-katmanı
 * protokol turudur: SNTP request gönder → response al → Transmit Timestamp'i
 * çıkar → Unix zamanına çevir.
 *
 * Akış:
 *   1) virtio-net kur; ARP ile gateway/DNS'in (SLIRP 10.0.2.3) MAC'ini çöz.
 *   2) DNS A-kaydı çözümle: "time.google.com" → NTP sunucu IPv4. SLIRP sorguyu
 *      host resolver'a forward eder → gerçek bir A kaydı döner (isim sıkıştırma
 *      0xC0 pointer dâhil). Çözülemezse bilinen sabit IP'ye (Google 216.239.35.0)
 *      düş.
 *   3) NTP request inşa+gönder: Ethernet + IPv4(proto=17) + UDP(src 123, dst 123)
 *      + NTP payload (48 byte): ilk byte LI/VN/Mode = 0x1B (LI=0, VN=3, Mode=3
 *      client), geri kalan 47 byte 0. UDP checksum 0 (opsiyonel).
 *   4) NTP response'u virtio-net RX ile al (poll): UDP src port 123. NTP
 *      payload'ta Transmit Timestamp offset 40'ta: ilk 4 byte = 1900'den beri
 *      saniye (big-endian u32). Bunu çıkar.
 *   5) Unix zamanına çevir: unix = ntp_saniye - 2208988800. NTP saniye + Unix
 *      zamanını bas (onaltılık — büyük sayı).
 *
 * Kanıt (RX): geçerli response (transmit timestamp non-zero) alındıysa → "NTP OK".
 * Kanıt (FALLBACK, internet yoksa): NTP request pcap'te (dst port 123 + LI/VN/Mode
 * 0x1B) yakalandıysa → "NTP SENT OK".
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
static uint8_t dns_mac[6];

/* Google NTP anycast (time.google.com) — DNS çözümü başarısız olursa yedek. */
static const uint8_t yedek_ntp_ip[4] = { 216, 239, 35, 0 };

static _Noreturn void halt(void) { for (;;) { __asm__ volatile("wfe"); } }

/* IPv4 header checksum: 16-bit tümleyen toplamı (RFC 1071). */
static uint16_t ip_checksum(const uint8_t *veri, int uzun) {
    uint32_t t = 0;
    for (int i = 0; i + 1 < uzun; i += 2) t += ((uint32_t)veri[i] << 8) | veri[i + 1];
    if (uzun & 1) t += (uint32_t)veri[uzun - 1] << 8;
    while (t >> 16) t = (t & 0xffff) + (t >> 16);
    return (uint16_t)(~t);
}

/*
 * DNS isim alanını (QNAME veya answer NAME) atla. off = payload içinde isme
 * giriş offseti. Döner: isimden sonraki offset. Compression pointer (0xC0) = 2
 * byte + biter; düz label dizisi 0 byte'a kadar. Sınır aşımında -1.
 */
static int isim_atla(const uint8_t *dns, int dns_uzun, int off) {
    while (off < dns_uzun) {
        uint8_t len = dns[off];
        if ((len & 0xC0) == 0xC0) {
            return (off + 2 <= dns_uzun) ? off + 2 : -1;
        }
        if (len == 0) {
            return off + 1;
        }
        off += 1 + len;
    }
    return -1;
}

/*
 * DNS A-kaydı çözümle: qname_buf içindeki hazır QNAME dizisini kullanarak
 * sorgu gönder + yanıtı parse et. Başarılıysa ip[0..3]'e non-zero A kaydını
 * yazar ve 1 döner; aksi 0.
 */
static int dns_coz(uint64_t base, const uint8_t *qname, int qname_uzun, uint8_t ip[4]) {
    const uint16_t dns_id = 0x4B7E;
    /* DNS payload: header(12) + qname + qtype(2) + qclass(2). */
    int dl = 12 + qname_uzun + 4;
    if (dl > 64) return 0;
    uint8_t dns[80];
    for (int i = 0; i < 80; i++) dns[i] = 0;
    dns[0] = (uint8_t)(dns_id >> 8); dns[1] = (uint8_t)dns_id;
    dns[2] = 0x01; dns[3] = 0x00;        /* flags: RD */
    dns[4] = 0x00; dns[5] = 0x01;        /* qdcount = 1 */
    for (int i = 0; i < qname_uzun; i++) dns[12 + i] = qname[i];
    dns[12 + qname_uzun + 0] = 0x00; dns[12 + qname_uzun + 1] = 0x01;   /* qtype = A */
    dns[12 + qname_uzun + 2] = 0x00; dns[12 + qname_uzun + 3] = 0x01;   /* qclass = IN */

    /* eth + IP + UDP + DNS çerçevesi inşa et. */
    for (int i = 0; i < 128; i++) frame[i] = 0;
    for (int i = 0; i < 6; i++) frame[i] = dns_mac[i];
    for (int i = 0; i < 6; i++) frame[6 + i] = bizim_mac[i];
    frame[12] = 0x08; frame[13] = 0x00;
    int ip_total = 20 + 8 + dl;
    frame[14] = 0x45; frame[16] = (uint8_t)(ip_total >> 8); frame[17] = (uint8_t)ip_total;
    frame[20] = 0x40; frame[22] = 64; frame[23] = 17;
    frame[26] = 10; frame[27] = 0; frame[28] = 2; frame[29] = 15;
    frame[30] = 10; frame[31] = 0; frame[32] = 2; frame[33] = 3;
    uint16_t s = ip_checksum(&frame[14], 20); frame[24] = (uint8_t)(s >> 8); frame[25] = (uint8_t)s;
    int udp_len = 8 + dl;
    frame[34] = 0x13; frame[35] = 0x88;                        /* src port 5000 */
    frame[36] = 0x00; frame[37] = 53;                        /* dst port 53 */
    frame[38] = (uint8_t)(udp_len >> 8); frame[39] = (uint8_t)udp_len;
    for (int i = 0; i < dl; i++) frame[42 + i] = dns[i];
    kdl_virtio_net_gonder(base, frame, 42 + dl);

    /* Yanıtı al + header doğrula. */
    int dns_baz = 42;
    int dns_uzun = 0;
    int yanit_ok = 0;
    for (int d = 0; d < 40 && !yanit_ok; d++) {
        int n = kdl_virtio_net_al(base, rx, 2048, 30000000);
        if (n >= 54 && rx[12] == 0x08 && rx[13] == 0x00 && rx[23] == 17 &&
            rx[26] == 10 && rx[27] == 0 && rx[28] == 2 && rx[29] == 3 &&
            rx[34] == 0x00 && rx[35] == 53) {
            int udp_toplam = ((int)rx[38] << 8) | rx[39];
            dns_uzun = udp_toplam - 8;
            if (dns_uzun >= 12 && dns_baz + dns_uzun <= n) {
                const uint8_t *dp = &rx[dns_baz];
                uint16_t r_id = ((uint16_t)dp[0] << 8) | dp[1];
                int qr = (dp[2] & 0x80) != 0;
                if (r_id == dns_id && qr) yanit_ok = 1;
            }
        }
    }
    if (!yanit_ok) return 0;

    const uint8_t *dp = &rx[dns_baz];
    uint16_t qdcount = ((uint16_t)dp[4] << 8) | dp[5];
    uint16_t ancount = ((uint16_t)dp[6] << 8) | dp[7];
    if (ancount == 0) return 0;

    int off = 12;
    for (int q = 0; q < qdcount; q++) {
        off = isim_atla(dp, dns_uzun, off);
        if (off < 0 || off + 4 > dns_uzun) return 0;
        off += 4;
    }
    for (int a = 0; a < ancount; a++) {
        off = isim_atla(dp, dns_uzun, off);
        if (off < 0 || off + 10 > dns_uzun) break;
        uint16_t type = ((uint16_t)dp[off] << 8) | dp[off + 1];
        uint16_t rdlength = ((uint16_t)dp[off + 8] << 8) | dp[off + 9];
        int rdata_off = off + 10;
        if (rdata_off + rdlength > dns_uzun) break;
        if (type == 1 && rdlength == 4) {
            ip[0] = dp[rdata_off + 0]; ip[1] = dp[rdata_off + 1];
            ip[2] = dp[rdata_off + 2]; ip[3] = dp[rdata_off + 3];
            if (!(ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0)) return 1;
        }
        off = rdata_off + rdlength;
    }
    return 0;
}

int main(void) {
    kdl_yazdir_metin("NTP BASLA");
    kdl_yazdir_satir();

    uint64_t base = kdl_virtio_net_bul();
    if (!base) { kdl_yazdir_metin("NET YOK"); kdl_yazdir_satir(); halt(); }
    if (kdl_virtio_net_kur(base) != 0) { kdl_yazdir_metin("NET KUR HATA"); kdl_yazdir_satir(); halt(); }

    /* --- 1) ARP: 10.0.2.3 (DNS/gateway) MAC'ini çöz --- */
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
            for (int i = 0; i < 6; i++) dns_mac[i] = rx[22 + i];
            arp_ok = 1;
        }
    }
    if (!arp_ok) {
        /* ARP çözülemedi → SLIRP default MAC'e düş (gateway 52:55:0a:00:02:03). */
        dns_mac[0] = 0x52; dns_mac[1] = 0x55; dns_mac[2] = 0x0a;
        dns_mac[3] = 0x00; dns_mac[4] = 0x02; dns_mac[5] = 0x03;
        kdl_yazdir_metin("ARP COZULEMEDI (yedek MAC)");
        kdl_yazdir_satir();
    }

    /* --- 2) DNS: "time.google.com" A-kaydı → NTP sunucu IP --- */
    /* QNAME: "\x04time\x06google\x03com\x00" = 17 byte. */
    uint8_t qname[17];
    qname[0] = 4;  qname[1] = 't'; qname[2] = 'i'; qname[3] = 'm'; qname[4] = 'e';
    qname[5] = 6;  qname[6] = 'g'; qname[7] = 'o'; qname[8] = 'o'; qname[9] = 'g';
    qname[10] = 'l'; qname[11] = 'e';
    qname[12] = 3; qname[13] = 'c'; qname[14] = 'o'; qname[15] = 'm'; qname[16] = 0;

    uint8_t ntp_ip[4] = { 0, 0, 0, 0 };
    int dns_ok = dns_coz(base, qname, 17, ntp_ip);
    if (!dns_ok) {
        for (int i = 0; i < 4; i++) ntp_ip[i] = yedek_ntp_ip[i];
        kdl_yazdir_metin("DNS COZULEMEDI (yedek IP)");
        kdl_yazdir_satir();
    }

    kdl_yazdir_metin("NTP IP=");
    kdl_yaz_onaltilik(ntp_ip[0]); kdl_yazdir_metin(".");
    kdl_yaz_onaltilik(ntp_ip[1]); kdl_yazdir_metin(".");
    kdl_yaz_onaltilik(ntp_ip[2]); kdl_yazdir_metin(".");
    kdl_yaz_onaltilik(ntp_ip[3]);
    kdl_yazdir_satir();

    /* --- 3) NTP request inşa + gönder --- */
    /* NTP payload: 48 byte. ilk byte LI/VN/Mode = 0x1B (LI=0, VN=3, Mode=3). */
    uint8_t ntp[48];
    for (int i = 0; i < 48; i++) ntp[i] = 0;
    ntp[0] = 0x1B;
    int nl = 48;

    for (int i = 0; i < 128; i++) frame[i] = 0;
    for (int i = 0; i < 6; i++) frame[i] = dns_mac[i];         /* dst = gateway MAC */
    for (int i = 0; i < 6; i++) frame[6 + i] = bizim_mac[i];
    frame[12] = 0x08; frame[13] = 0x00;                        /* IPv4 */
    int ip_total = 20 + 8 + nl;
    frame[14] = 0x45; frame[16] = (uint8_t)(ip_total >> 8); frame[17] = (uint8_t)ip_total;
    frame[20] = 0x40; frame[22] = 64; frame[23] = 17;         /* DF, TTL, UDP */
    frame[26] = 10; frame[27] = 0; frame[28] = 2; frame[29] = 15;             /* src 10.0.2.15 */
    frame[30] = ntp_ip[0]; frame[31] = ntp_ip[1]; frame[32] = ntp_ip[2]; frame[33] = ntp_ip[3];
    uint16_t ipsum = ip_checksum(&frame[14], 20);
    frame[24] = (uint8_t)(ipsum >> 8); frame[25] = (uint8_t)ipsum;
    int udp_len = 8 + nl;
    frame[34] = 0x00; frame[35] = 123;                        /* src port 123 */
    frame[36] = 0x00; frame[37] = 123;                        /* dst port 123 */
    frame[38] = (uint8_t)(udp_len >> 8); frame[39] = (uint8_t)udp_len;
    frame[40] = 0; frame[41] = 0;                            /* UDP checksum 0 (opsiyonel) */
    for (int i = 0; i < nl; i++) frame[42 + i] = ntp[i];
    int toplam = 42 + nl;                                     /* 90 byte */
    kdl_virtio_net_gonder(base, frame, toplam);

    kdl_yazdir_metin("NTP SENT OK");
    kdl_yazdir_satir();

    /* --- 4) NTP response'u al (poll) + Transmit Timestamp çıkar --- */
    /* Yanıt: eth(14) + IP(20) + UDP(8) + NTP(48). NTP payload rx[42]'de.        */
    /* Transmit Timestamp offset 40 (NTP içinde) → rx[42+40]=rx[82], 4 byte sn.  */
    uint32_t ntp_saniye = 0;
    int yanit_ok = 0;
    for (int d = 0; d < 60 && !yanit_ok; d++) {
        int n = kdl_virtio_net_al(base, rx, 2048, 30000000);
        if (n >= 90 && rx[12] == 0x08 && rx[13] == 0x00 && rx[23] == 17 &&   /* IPv4 + UDP */
            rx[34] == 0x00 && rx[35] == 123) {                               /* src port 123 */
            int ntp_baz = 42;
            uint32_t sec = ((uint32_t)rx[ntp_baz + 40] << 24) |
                           ((uint32_t)rx[ntp_baz + 41] << 16) |
                           ((uint32_t)rx[ntp_baz + 42] << 8) |
                           ((uint32_t)rx[ntp_baz + 43]);
            if (sec != 0) {
                ntp_saniye = sec;
                yanit_ok = 1;
            }
        }
    }

    if (!yanit_ok) {
        /* İnternet yok / SLIRP dış-UDP yanıt vermedi → FALLBACK: request emisyonu
         * kanıtı zaten "NTP SENT OK" ile serilendi + pcap'te dst port 123 +
         * LI/VN/Mode 0x1B yakalanacak. */
        kdl_yazdir_metin("NTP YANIT YOK (fallback: request emisyonu)");
        kdl_yazdir_satir();
        halt();
    }

    /* --- 5) Unix zamanına çevir + bas --- */
    /* unix = ntp_saniye - 2208988800 (1900→1970 farkı). */
    uint64_t unix_zaman = (uint64_t)ntp_saniye - 2208988800ULL;

    kdl_yazdir_metin("NTP SN=");
    kdl_yaz_onaltilik((uint64_t)ntp_saniye);
    kdl_yazdir_satir();

    kdl_yazdir_metin("UNIX=");
    kdl_yaz_onaltilik(unix_zaman);
    kdl_yazdir_satir();

    kdl_yazdir_metin("NTP OK");
    kdl_yazdir_satir();
    halt();
    return 0;
}
