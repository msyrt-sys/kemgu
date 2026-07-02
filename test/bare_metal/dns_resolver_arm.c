/*
 * DNS A-KAYDI ÇÖZÜMLEME testi (aarch64) — isim → IPv4 (Faz G derinleşme).
 *
 * dns_arm.c (D-147) DNS round-trip'i kanıtladı: sorgu gönder → yanıt AL.
 * Bu test bir adım öteye gider: yanıtın ANSWER bölümünü PARSE eder ve
 * çözümlenen IPv4 A-kaydını çıkarır (isim sıkıştırma 0xC0 pointer dâhil).
 *
 * Akış:
 *   1) ARP ile DNS sunucusunun (SLIRP 10.0.2.3) MAC'ini çöz.
 *   2) IP/UDP DNS sorgusu inşa et + gönder ("example.com" A kaydı). SLIRP
 *      sorguyu host resolver'a forward eder → gerçek bir A kaydı döner.
 *   3) Yanıtı virtio-net RX ile al; DNS header'ı doğrula (ID eşleşir, QR=1,
 *      ANCOUNT >= 1).
 *   4) Question bölümünü atla (QNAME + QTYPE + QCLASS). Her answer record'u
 *      parse et: NAME (0xC0 compression pointer = 2 byte ya da düz label
 *      dizisi), TYPE (2), CLASS (2), TTL (4), RDLENGTH (2), RDATA. TYPE=1 (A)
 *      + RDLENGTH=4 olan ilk record'un 4 byte RDATA'sını IPv4 olarak çıkar.
 *
 * Kanıt: 4 byte'ı hepsi sıfır OLMAYAN bir A-kaydı çıkarıldıysa "RESOLVE OK".
 * (Çözümlenen IP host DNS'ine bağlı → belirli bir IP beklenmez; geçerli bir
 *  non-zero A kaydı çıkarımı yeterli kanıttır.)
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

static void halt(void) { for (;;) { __asm__ volatile("wfe"); } }

static uint16_t ip_checksum(const uint8_t *veri, int uzun) {
    uint32_t t = 0;
    for (int i = 0; i + 1 < uzun; i += 2) t += ((uint32_t)veri[i] << 8) | veri[i + 1];
    if (uzun & 1) t += (uint32_t)veri[uzun - 1] << 8;
    while (t >> 16) t = (t & 0xffff) + (t >> 16);
    return (uint16_t)(~t);
}

/*
 * DNS isim alanını (QNAME veya answer NAME) atla. off = payload içinde isme
 * giriş offseti. Döner: isimden sonraki offset. İki durum:
 *   - Compression pointer: ilk byte üst iki biti set (0xC0) → 2 byte, biter.
 *   - Düz label dizisi: uzunluk-öneki byte'lar; 0 byte'a kadar oku. Bir label
 *     ortasında compression pointer da gelebilir (o zaman 2 byte + biter).
 * Sınır aşımında -1 döner (bozuk paket koruması).
 */
static int isim_atla(const uint8_t *dns, int dns_uzun, int off) {
    while (off < dns_uzun) {
        uint8_t len = dns[off];
        if ((len & 0xC0) == 0xC0) {
            /* Compression pointer: 2 byte (pointer + hedef), isim burada biter. */
            return (off + 2 <= dns_uzun) ? off + 2 : -1;
        }
        if (len == 0) {
            /* Kök label (null terminator). */
            return off + 1;
        }
        /* Düz label: 1 uzunluk byte'ı + len karakter. */
        off += 1 + len;
    }
    return -1;
}

int main(void) {
    kdl_yazdir_metin("RESOLVE BASLA");
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

    /* --- 2) DNS sorgusu: eth+IP+UDP+DNS("example.com" A) --- */
    /* QNAME: "\x07example\x03com\x00" = 13 byte.                              */
    /* DNS payload: header(12) + qname(13) + qtype(2) + qclass(2) = 29 byte.   */
    const uint16_t dns_id = 0x4B7E;   /* sorgu ID — yanıtta eşleşmesi gerekir */
    uint8_t dns[29];
    for (int i = 0; i < 29; i++) dns[i] = 0;
    dns[0] = (uint8_t)(dns_id >> 8); dns[1] = (uint8_t)dns_id;  /* id */
    dns[2] = 0x01; dns[3] = 0x00;        /* flags: RD (recursion desired) */
    dns[4] = 0x00; dns[5] = 0x01;        /* qdcount = 1 */
    /* qname = 7 'example' 3 'com' 0 */
    dns[12] = 7;
    dns[13] = 'e'; dns[14] = 'x'; dns[15] = 'a'; dns[16] = 'm'; dns[17] = 'p'; dns[18] = 'l'; dns[19] = 'e';
    dns[20] = 3; dns[21] = 'c'; dns[22] = 'o'; dns[23] = 'm'; dns[24] = 0;
    dns[25] = 0x00; dns[26] = 0x01;      /* qtype = A */
    dns[27] = 0x00; dns[28] = 0x01;      /* qclass = IN */
    int dl = 29;

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

    /* --- 3) DNS yanıtını AL + header doğrula --- */
    /* Yanıt: eth(14) + IP(20) + UDP(8) + DNS payload. DNS payload rx[42]'de. */
    int yanit_ok = 0;
    int dns_baz = 42;          /* DNS payload'un rx içindeki başlangıcı */
    int dns_uzun = 0;          /* DNS payload uzunluğu */
    for (int d = 0; d < 40 && !yanit_ok; d++) {
        int n = kdl_virtio_net_al(base, rx, 2048, 30000000);
        if (n >= 54 && rx[12] == 0x08 && rx[13] == 0x00 && rx[23] == 17 &&   /* IPv4 + UDP */
            rx[26] == 10 && rx[27] == 0 && rx[28] == 2 && rx[29] == 3 &&     /* src = 10.0.2.3 */
            rx[34] == 0x00 && rx[35] == 53) {                                /* src port = 53 */
            /* UDP uzunluğu (rx[38..39]) - 8 = DNS payload uzunluğu. */
            int udp_toplam = ((int)rx[38] << 8) | rx[39];
            dns_uzun = udp_toplam - 8;
            if (dns_uzun >= 12 && dns_baz + dns_uzun <= n) {
                const uint8_t *dp = &rx[dns_baz];
                /* Header: ID eşleşir + QR bit (flags byte, üst bit) set. */
                uint16_t r_id = ((uint16_t)dp[0] << 8) | dp[1];
                int qr = (dp[2] & 0x80) != 0;
                if (r_id == dns_id && qr) yanit_ok = 1;
            }
        }
    }

    if (!yanit_ok) {
        kdl_yazdir_metin("DNS YANIT YOK");
        kdl_yazdir_satir();
        halt();
    }

    /* --- 4) ANSWER bölümünü parse et → A kaydı IPv4 --- */
    const uint8_t *dp = &rx[dns_baz];
    uint16_t qdcount = ((uint16_t)dp[4] << 8) | dp[5];
    uint16_t ancount = ((uint16_t)dp[6] << 8) | dp[7];

    kdl_yazdir_metin("ANCOUNT=");
    kdl_yaz_onaltilik(ancount);
    kdl_yazdir_satir();

    if (ancount == 0) {
        kdl_yazdir_metin("A-KAYDI YOK (ANCOUNT=0)");
        kdl_yazdir_satir();
        halt();
    }

    /* Question bölümünü atla (qdcount adet: QNAME + QTYPE(2) + QCLASS(2)). */
    int off = 12;   /* header sonrası */
    for (int q = 0; q < qdcount; q++) {
        off = isim_atla(dp, dns_uzun, off);
        if (off < 0 || off + 4 > dns_uzun) {
            kdl_yazdir_metin("QUESTION PARSE HATA");
            kdl_yazdir_satir();
            halt();
        }
        off += 4;   /* QTYPE + QCLASS */
    }

    /* Answer record'ları tara; ilk A kaydını (TYPE=1, RDLENGTH=4) çıkar. */
    uint8_t ip[4] = { 0, 0, 0, 0 };
    int bulundu = 0;
    for (int a = 0; a < ancount && !bulundu; a++) {
        off = isim_atla(dp, dns_uzun, off);           /* NAME (genelde 0xC0 pointer) */
        if (off < 0 || off + 10 > dns_uzun) break;     /* TYPE+CLASS+TTL+RDLENGTH = 10 */
        uint16_t type = ((uint16_t)dp[off] << 8) | dp[off + 1];
        /* dp[off+2..3] = CLASS, dp[off+4..7] = TTL */
        uint16_t rdlength = ((uint16_t)dp[off + 8] << 8) | dp[off + 9];
        int rdata_off = off + 10;
        if (rdata_off + rdlength > dns_uzun) break;    /* bozuk paket koruması */
        if (type == 1 && rdlength == 4) {              /* A kaydı = IPv4 */
            ip[0] = dp[rdata_off + 0];
            ip[1] = dp[rdata_off + 1];
            ip[2] = dp[rdata_off + 2];
            ip[3] = dp[rdata_off + 3];
            bulundu = 1;
        }
        off = rdata_off + rdlength;                    /* sonraki record'a geç */
    }

    if (!bulundu) {
        kdl_yazdir_metin("A-KAYDI COZULEMEDI");
        kdl_yazdir_satir();
        halt();
    }

    /* Non-zero A kaydı = geçerli çözümleme kanıtı. */
    int hepsi_sifir = (ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0);

    kdl_yazdir_metin("DNS IP=");
    kdl_yaz_onaltilik(ip[0]); kdl_yazdir_metin(".");
    kdl_yaz_onaltilik(ip[1]); kdl_yazdir_metin(".");
    kdl_yaz_onaltilik(ip[2]); kdl_yazdir_metin(".");
    kdl_yaz_onaltilik(ip[3]);
    kdl_yazdir_satir();

    if (hepsi_sifir) {
        kdl_yazdir_metin("A-KAYDI SIFIR (gecersiz)");
        kdl_yazdir_satir();
        halt();
    }

    kdl_yazdir_metin("RESOLVE OK");
    kdl_yazdir_satir();
    halt();
    return 0;
}
