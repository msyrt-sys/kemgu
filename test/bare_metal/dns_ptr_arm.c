/*
 * REVERSE DNS (PTR kaydı) çözümleme testi (aarch64) — IP → hostname (recon).
 *
 * dns_resolver_arm.c (D-157) ileri yönlü A-kaydı çözümledi (isim → IPv4).
 * Bu test tersini yapar: bir IPv4 adresinin hangi isme ait olduğunu bulur
 * (pentest recon: hedef IP tanıma). 8.8.8.8 → "dns.google" beklenir.
 *
 * PTR sorgusu kuralı:
 *   IP a.b.c.d için QNAME = "d.c.b.a.in-addr.arpa" (oktetler TERS sırada,
 *   her biri ayrı label). QTYPE = 12 (PTR), QCLASS = 1 (IN).
 *   Örn 8.8.8.8 → "8.8.8.8.in-addr.arpa" (tersi de 8.8.8.8, simetrik).
 *
 * Akış:
 *   1) ARP ile DNS sunucusunun (SLIRP 10.0.2.3) MAC'ini çöz.
 *   2) IP/UDP DNS sorgusu inşa et + gönder (PTR, QNAME = ters-oktet + arpa).
 *      SLIRP sorguyu host resolver'a forward eder → gerçek PTR kaydı döner.
 *   3) Yanıtı virtio-net RX ile al; DNS header'ı doğrula (ID eşleşir, QR=1,
 *      ANCOUNT >= 1).
 *   4) Question bölümünü atla. Her answer record'u tara: NAME (compression
 *      pointer atla), TYPE (2), CLASS (2), TTL (4), RDLENGTH (2), RDATA.
 *      TYPE = 12 (PTR) olan ilk record'un RDATA'sındaki domain-name'i PARSE et:
 *      label dizisi (uzunluk-öneki byte + o kadar karakter, 0'a kadar);
 *      compression pointer (0xC0) gelirse takip et. Label'ları '.' ile birleştir.
 *
 * Kanıt: en az 1 karakterlik geçerli bir PTR ismi çıkarıldıysa → "PTR OK".
 * (Çözülen isim host DNS'ine bağlı; 8.8.8.8 genelde "dns.google" döner.)
 *
 * FALLBACK: host DNS PTR döndürmezse (ortam-bağımlı), yanıt-alındı + header
 * parse (QR=1, ANCOUNT) kısmi kanıtı raporlanır; "PTR OK" YALNIZ gerçek isim
 * çıktığında basılır.
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
static char hostname[256];   /* çözülen PTR ismi (null-terminated) */

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

/*
 * DNS domain-name'i OKU (isim_atla'nın tersi — atlamak yerine biriktir).
 * RDATA'daki PTR ismini çözer: label'ları '.' ile birleştirir. Compression
 * pointer (0xC0) gelirse hedefe atlayıp okumaya devam eder (isim aynı payload
 * içinde başka yeri işaret edebilir). Sonuç `cikti`ya null-terminated yazılır.
 *
 *   dns       — DNS payload başlangıcı
 *   dns_uzun  — payload uzunluğu
 *   off       — ismin başladığı offset
 *   cikti     — hedef tampon (null-terminated yazılır)
 *   cikti_max — tampon boyutu (null dâhil)
 * Döner: yazılan karakter sayısı (>= 0) ya da bozuk paketse -1.
 *
 * Compression döngüsüne karşı korumak için sınırlı sayıda pointer takip edilir.
 */
static int isim_oku(const uint8_t *dns, int dns_uzun, int off,
                    char *cikti, int cikti_max) {
    int yazilan = 0;
    int atlama_sayisi = 0;      /* takip edilen compression pointer sayısı */
    int ilk_label = 1;          /* label'lar arasına '.' koymak için */

    while (off >= 0 && off < dns_uzun) {
        uint8_t len = dns[off];
        if ((len & 0xC0) == 0xC0) {
            /* Compression pointer: 14-bit offset'e atla. */
            if (off + 1 >= dns_uzun) return -1;
            if (++atlama_sayisi > 32) return -1;   /* döngü koruması */
            int hedef = (((int)(len & 0x3F)) << 8) | dns[off + 1];
            if (hedef < 0 || hedef >= dns_uzun) return -1;
            off = hedef;
            continue;
        }
        if (len == 0) {
            /* Kök label — isim bitti. */
            break;
        }
        if ((len & 0xC0) != 0) return -1;   /* geçersiz üst bitler */
        /* Düz label: len karakter. */
        if (off + 1 + len > dns_uzun) return -1;   /* bozuk paket koruması */
        if (!ilk_label) {
            if (yazilan + 1 >= cikti_max) return -1;
            cikti[yazilan++] = '.';
        }
        ilk_label = 0;
        for (int i = 0; i < len; i++) {
            if (yazilan + 1 >= cikti_max) return -1;
            cikti[yazilan++] = (char)dns[off + 1 + i];
        }
        off += 1 + len;
    }

    if (yazilan >= cikti_max) return -1;
    cikti[yazilan] = '\0';
    return yazilan;
}

/* Bir oktet değerini (0-255) ondalık ASCII'ye çevir; yazılan byte sayısını
 * döner. En fazla 3 hane. dns tamponuna offset'ten itibaren yazar. */
static int oktet_yaz(uint8_t *dst, int off, uint8_t deger) {
    char tmp[3];
    int n = 0;
    if (deger == 0) {
        dst[off] = '0';
        return 1;
    }
    while (deger > 0 && n < 3) {
        tmp[n++] = (char)('0' + (deger % 10));
        deger /= 10;
    }
    /* tmp ters sırada; düzelt. */
    for (int i = 0; i < n; i++) dst[off + i] = (uint8_t)tmp[n - 1 - i];
    return n;
}

int main(void) {
    kdl_yazdir_metin("PTR BASLA");
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

    /* --- 2) PTR sorgusu inşa et: QNAME = ters-oktet + "in-addr.arpa" --- */
    /* Çözülecek IP: 8.8.8.8 (Google Public DNS). PTR = "8.8.8.8.in-addr.arpa".*/
    const uint8_t hedef_ip[4] = { 8, 8, 8, 8 };
    const uint16_t dns_id = 0x5C3D;   /* sorgu ID — yanıtta eşleşmesi gerekir */

    uint8_t dns[128];
    for (int i = 0; i < 128; i++) dns[i] = 0;
    dns[0] = (uint8_t)(dns_id >> 8); dns[1] = (uint8_t)dns_id;  /* id */
    dns[2] = 0x01; dns[3] = 0x00;        /* flags: RD (recursion desired) */
    dns[4] = 0x00; dns[5] = 0x01;        /* qdcount = 1 */

    /* QNAME: oktetler TERS sırada (d.c.b.a), her biri ayrı label.            */
    /* Sonra "in-addr" (7) + "arpa" (4) + kök (0).                            */
    int p = 12;   /* header sonrası QNAME başlangıcı */
    for (int k = 3; k >= 0; k--) {
        int len_off = p++;                 /* label uzunluk byte'ı yeri */
        int yaz = oktet_yaz(dns, p, hedef_ip[k]);
        dns[len_off] = (uint8_t)yaz;       /* label uzunluğu */
        p += yaz;
    }
    /* "in-addr" label */
    dns[p++] = 7;
    dns[p++] = 'i'; dns[p++] = 'n'; dns[p++] = '-';
    dns[p++] = 'a'; dns[p++] = 'd'; dns[p++] = 'd'; dns[p++] = 'r';
    /* "arpa" label */
    dns[p++] = 4;
    dns[p++] = 'a'; dns[p++] = 'r'; dns[p++] = 'p'; dns[p++] = 'a';
    dns[p++] = 0;                          /* kök label */
    dns[p++] = 0x00; dns[p++] = 12;        /* qtype = PTR (12) */
    dns[p++] = 0x00; dns[p++] = 0x01;      /* qclass = IN */
    int dl = p;

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
    int yanit_ok = 0;
    int dns_baz = 42;          /* DNS payload'un rx içindeki başlangıcı */
    int dns_uzun = 0;          /* DNS payload uzunluğu */
    for (int d = 0; d < 40 && !yanit_ok; d++) {
        int n = kdl_virtio_net_al(base, rx, 2048, 30000000);
        if (n >= 54 && rx[12] == 0x08 && rx[13] == 0x00 && rx[23] == 17 &&   /* IPv4 + UDP */
            rx[26] == 10 && rx[27] == 0 && rx[28] == 2 && rx[29] == 3 &&     /* src = 10.0.2.3 */
            rx[34] == 0x00 && rx[35] == 53) {                                /* src port = 53 */
            int udp_toplam = ((int)rx[38] << 8) | rx[39];
            dns_uzun = udp_toplam - 8;
            if (dns_uzun >= 12 && dns_baz + dns_uzun <= n) {
                const uint8_t *dp0 = &rx[dns_baz];
                uint16_t r_id = ((uint16_t)dp0[0] << 8) | dp0[1];
                int qr = (dp0[2] & 0x80) != 0;
                if (r_id == dns_id && qr) yanit_ok = 1;
            }
        }
    }

    if (!yanit_ok) {
        kdl_yazdir_metin("DNS YANIT YOK");
        kdl_yazdir_satir();
        halt();
    }

    /* --- 4) ANSWER bölümünü parse et → PTR domain-name --- */
    const uint8_t *dp = &rx[dns_baz];
    uint16_t qdcount = ((uint16_t)dp[4] << 8) | dp[5];
    uint16_t ancount = ((uint16_t)dp[6] << 8) | dp[7];

    kdl_yazdir_metin("QR=1 YANIT ALINDI");
    kdl_yazdir_satir();
    kdl_yazdir_metin("ANCOUNT=");
    kdl_yaz_onaltilik(ancount);
    kdl_yazdir_satir();

    if (ancount == 0) {
        /* FALLBACK: yanıt alındı ama PTR kaydı yok (host DNS PTR döndürmedi). */
        kdl_yazdir_metin("PTR KAYDI YOK (ANCOUNT=0)");
        kdl_yazdir_satir();
        kdl_yazdir_metin("KISMI: sorgu-gonderildi + yanit-alindi (QR=1); host DNS PTR dondurmedi");
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

    /* Answer record'ları tara; ilk PTR kaydını (TYPE=12) çıkar. */
    int bulundu = 0;
    int isim_uzun = 0;
    for (int a = 0; a < ancount && !bulundu; a++) {
        off = isim_atla(dp, dns_uzun, off);           /* NAME (genelde 0xC0 pointer) */
        if (off < 0 || off + 10 > dns_uzun) break;     /* TYPE+CLASS+TTL+RDLENGTH = 10 */
        uint16_t type = ((uint16_t)dp[off] << 8) | dp[off + 1];
        uint16_t rdlength = ((uint16_t)dp[off + 8] << 8) | dp[off + 9];
        int rdata_off = off + 10;
        if (rdata_off + rdlength > dns_uzun) break;    /* bozuk paket koruması */
        if (type == 12) {                              /* PTR kaydı */
            /* RDATA = domain-name; label dizisini oku (compression dâhil). */
            isim_uzun = isim_oku(dp, dns_uzun, rdata_off, hostname, (int)sizeof(hostname));
            if (isim_uzun >= 1) bulundu = 1;
        }
        off = rdata_off + rdlength;                    /* sonraki record'a geç */
    }

    if (!bulundu || isim_uzun < 1) {
        /* FALLBACK: answer var ama PTR ismi çıkarılamadı / boş. */
        kdl_yazdir_metin("PTR ISMI COZULEMEDI");
        kdl_yazdir_satir();
        kdl_yazdir_metin("KISMI: yanit-alindi + ANCOUNT parse; PTR RDATA ismi bos/gecersiz");
        kdl_yazdir_satir();
        halt();
    }

    /* Geçerli PTR ismi çıkarıldı — bas. */
    kdl_yazdir_metin("HOSTNAME=");
    kdl_yazdir_metin(hostname);

    kdl_yazdir_metin("PTR OK");
    kdl_yazdir_satir();
    halt();
    return 0;
}
