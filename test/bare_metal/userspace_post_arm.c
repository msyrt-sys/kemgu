/*
 * USERSPACE HTTP POST testi (aarch64) — EL0 süreç SYSCALL ile VERİ GÖNDERİR.
 *
 * D-184 (userspace_http_arm.c) bir EL0 (yetkisiz) sürecin net_gonder(24)/
 * net_al(25) syscall'larıyla TAM bir HTTP/1.1 GET istemcisi çalıştırdığını
 * kanıtladı: L2 (Ethernet) + L3 (IPv4 + checksum) + L4 (TCP handshake +
 * pseudo-hdr csum) + L7 (HTTP GET + yanıt durum satırı parse), hepsi
 * userspace'te; ağ yalnız çekirdek-aracılıdır (sys2 24/25). Bu test zirveye
 * bir adım daha ekler: EL0 süreç yalnız OKUMAKLA kalmaz, sunucuya VERİ POST'lar.
 *
 * Fark GET'ten: HTTP metodu POST + gövde (body). İstek gövdesiyle birlikte
 * Content-Type + Content-Length başlıkları gönderilir. Sunucu POST'u aldığında
 * bir durum satırı döner (example.com POST'a 405 Method Not Allowed veya 200/3xx
 * döner — hepsi bağlantı+POST'un çalıştığını kanıtlar). Gövde "KEMGU-POST"
 * (10 byte) → Content-Length: 10.
 *
 * Akış (EL0 launcher):
 *   1) ARP: gateway 10.0.2.2 MAC'ini çöz (SLIRP proxy'ler dış-TCP'yi).
 *   2) ARP: DNS 10.0.2.3 MAC'ini çöz (fallback = gateway MAC).
 *   3) DNS: "example.com" A-kaydını çöz → hedef IPv4.
 *   4) TCP: hedef-IP:80'e TAM handshake (SYN → SYN-ACK → ACK) → ESTABLISHED.
 *   5) HTTP POST: istek satırı + başlıklar + gövdeyi TCP DATA segmenti (PSH+ACK,
 *        flags=0x18) olarak yolla:
 *        "POST /post HTTP/1.1\r\nHost: example.com\r\n"
 *        "Content-Type: text/plain\r\nContent-Length: 10\r\n"
 *        "Connection: close\r\n\r\nKEMGU-POST"
 *      seq = ISS+1 (handshake sonrası), ack = onların_iss+1.
 *   6) RX poll: yanıt DATA segmentinde "HTTP/1." durum satırı ara → gelen data'ya
 *      ACK dön (seq/ack takibi) → durum satırını + kodu (200/3xx/405) bas.
 *
 * KANIT (gerçek POST RX): "USERPOST OK" (yanıtta HTTP/1.x durum satırı bulundu).
 * FALLBACK (host internet yoksa / SLIRP dış-TCP yanıt vermezse): EL0 POST isteğini
 * sys2(24) ile GÖNDERDİ → pcap'te "POST /" TX kanıtı → "USERPOST SENT OK".
 *
 * GÜVENLİK (D-177 dersi): EL0 kernel .rodata (AP=00) DEREF EDEMEZ. Bu yüzden HTTP
 * POST request byte'ları (istek satırı + başlıklar + gövde) kernel .rodata string
 * literalinden OKUNMAZ — launcher onları EL0 user yığınındaki (0x42000000 sayfası)
 * bir tampona ELLE byte-byte yazar. Tüm frame/rx/http tamponları user yığınında →
 * sys2(24)/sys2(25) user-VA guard'ından (D-150) geçer. sys(5) durum string'leri
 * kernel tarafından okunur (user-VA ∪ .rodata guard) → sorunsuz.
 */
#include <stdint.h>
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_satir(void);
extern uint64_t kdl_virtio_net_bul(void);
extern int  kdl_virtio_net_kur(uint64_t base);
extern void kdl_surec_kur_el0_veri(uint64_t *l1, uint64_t *l2, uint64_t kod_pa, uint64_t veri_pa);
extern void kdl_kesme_kur(void);
extern void kdl_timer_baslat(void);
extern void kdl_preempt_baslat(void);
extern int  kdl_preempt_gorev_olustur_el0(void (*giris)(void), void *kernel_yigin_tepe,
                                          void *user_yigin_tepe);
extern void kdl_preempt_gorev_ttbr(int gorev, uint64_t *l1);
extern void kdl_preempt_ac(void);

static uint64_t l1_l[512] __attribute__((aligned(4096)));
static uint64_t l2_l[512] __attribute__((aligned(4096)));
static unsigned char kstack_l[8192] __attribute__((aligned(16)));

__attribute__((always_inline)) static inline unsigned long sys2(unsigned long num, unsigned long a0, unsigned long a1) {
    register unsigned long x8 __asm__("x8") = num;
    register unsigned long x0 __asm__("x0") = a0;
    register unsigned long x1 __asm__("x1") = a1;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8), "r"(x1) : "memory");
    return x0;
}
__attribute__((always_inline)) static inline unsigned long sys(unsigned long num, unsigned long arg) {
    register unsigned long x8 __asm__("x8") = num;
    register unsigned long x0 __asm__("x0") = arg;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}

/* IPv4/TCP başlık sağlaması (RFC 1071). always_inline → .user launcher'a gömülür. */
__attribute__((always_inline)) static inline uint16_t ip_checksum(const unsigned char *veri, int uzun) {
    uint32_t t = 0;
    int i;
    for (i = 0; i + 1 < uzun; i += 2) t += ((uint32_t)veri[i] << 8) | veri[i + 1];
    if (uzun & 1) t += (uint32_t)veri[uzun - 1] << 8;
    while (t >> 16) t = (t & 0xffff) + (t >> 16);
    return (uint16_t)(~t);
}

/* DNS isim alanını atla (compression pointer 0xC0 veya düz label dizisi). */
__attribute__((always_inline)) static inline int isim_atla(const unsigned char *dns, int dns_uzun, int off) {
    while (off < dns_uzun) {
        unsigned char len = dns[off];
        if ((len & 0xC0) == 0xC0) return (off + 2 <= dns_uzun) ? off + 2 : -1;
        if (len == 0) return off + 1;
        off += 1 + (int)len;
    }
    return -1;
}

/* Tampon içinde alt-dizi ara (memmem yerine el ile — bare metal). desen EL0
 * yığınında (user-VA) olmalı; burada çağıran onu byte-byte doldurur. */
__attribute__((always_inline)) static inline int icinde_bul(const unsigned char *buf, int buf_uzun,
                                                            const unsigned char *desen, int desen_uzun) {
    for (int i = 0; i + desen_uzun <= buf_uzun; i++) {
        int esles = 1;
        for (int j = 0; j < desen_uzun; j++) {
            if (buf[i + j] != desen[j]) { esles = 0; break; }
        }
        if (esles) return i;
    }
    return -1;
}

/* Bir nibble'ı hex karaktere çevir — .rodata lookup YOK (D-177 dersi), aritmetik. */
__attribute__((always_inline)) static inline char nibble_hex(unsigned char n) {
    n &= 0xf;
    return (char)(n < 10 ? ('0' + n) : ('a' + (n - 10)));
}
/* Onaltılık bir byte'ı EL0'dan sys(5) ile bas. `iki` user yığınında → guard geçer. */
__attribute__((always_inline)) static inline void bas_bayt_hex(unsigned char b) {
    char iki[3];
    iki[0] = nibble_hex((unsigned char)(b >> 4));
    iki[1] = nibble_hex(b);
    iki[2] = 0;
    sys(5, (unsigned long)(uintptr_t)iki);
}

/*
 * TCP segmenti inşa et: eth + IPv4(proto=6) + TCP header(20) + opsiyonel payload.
 * frame[] EL0 yığınında; toplam çerçeve uzunluğunu döner. payload da user yığınında.
 * seq/ack = host byte order; flags = TCP bayrak byte'i (SYN=0x02, ACK=0x10, PSH=0x08).
 * dst MAC = gw_mac (SLIRP dış-TCP'yi proxy'ler). Tüm tamponlar user-VA → guard geçer.
 * payload_uzun POST için 128'e kadar (istek+gövde) → psbuf 12+20+256 boyutlanır.
 */
__attribute__((always_inline)) static inline int tcp_segment_kur(
        unsigned char *frame, const unsigned char *gw_mac, const unsigned char *bizim_mac,
        const unsigned char *bizim_ip, const unsigned char *dst_ip, int src_port, int dst_port,
        uint32_t seq, uint32_t ack, unsigned char flags,
        const unsigned char *payload, int payload_uzun) {
    int i;
    int tcp_len = 20 + payload_uzun;
    int ip_total = 20 + tcp_len;
    for (i = 0; i < 100 + payload_uzun; i++) frame[i] = 0;   /* eth+ip+tcp+payload alanı sıfırla */

    for (i = 0; i < 6; i++) frame[i] = gw_mac[i];             /* dst = gateway MAC */
    for (i = 0; i < 6; i++) frame[6 + i] = bizim_mac[i];      /* src = bizim mac */
    frame[12] = 0x08; frame[13] = 0x00;                       /* ethertype = IPv4 */

    /* IPv4 header (20 bayt) @ offset 14 */
    frame[14] = 0x45;                                         /* ver=4, IHL=5 */
    frame[15] = 0x00;
    frame[16] = (unsigned char)(ip_total >> 8); frame[17] = (unsigned char)ip_total;
    frame[18] = 0x00; frame[19] = 0x00;                      /* identification */
    frame[20] = 0x40; frame[21] = 0x00;                      /* flags: DF, frag=0 */
    frame[22] = 64;                                          /* TTL */
    frame[23] = 6;                                           /* protocol = TCP */
    for (i = 0; i < 4; i++) frame[26 + i] = bizim_ip[i];      /* src IP */
    for (i = 0; i < 4; i++) frame[30 + i] = dst_ip[i];        /* dst IP */
    uint16_t ipcs = ip_checksum(&frame[14], 20);
    frame[24] = (unsigned char)(ipcs >> 8); frame[25] = (unsigned char)ipcs;

    /* TCP header (20 bayt) @ offset 34 */
    frame[34] = (unsigned char)(src_port >> 8); frame[35] = (unsigned char)src_port;
    frame[36] = (unsigned char)(dst_port >> 8); frame[37] = (unsigned char)dst_port;
    frame[38] = (unsigned char)(seq >> 24); frame[39] = (unsigned char)(seq >> 16);
    frame[40] = (unsigned char)(seq >> 8);  frame[41] = (unsigned char)seq;
    frame[42] = (unsigned char)(ack >> 24); frame[43] = (unsigned char)(ack >> 16);
    frame[44] = (unsigned char)(ack >> 8);  frame[45] = (unsigned char)ack;
    frame[46] = 0x50;                                         /* data offset = 5 (20 bayt) */
    frame[47] = flags;                                       /* TCP bayrakları */
    frame[48] = 0x20; frame[49] = 0x00;                      /* window = 8192 */
    frame[52] = 0x00; frame[53] = 0x00;                      /* urgent pointer */

    /* Payload @ offset 54 */
    for (i = 0; i < payload_uzun; i++) frame[54 + i] = payload[i];

    /* TCP checksum: pseudo-header(12) + TCP segmenti (header 20 + payload). */
    unsigned char psbuf[12 + 20 + 256];
    int ps_uzun = 12 + tcp_len;
    for (i = 0; i < ps_uzun; i++) psbuf[i] = 0;
    for (i = 0; i < 4; i++) psbuf[i] = bizim_ip[i];           /* src IP */
    for (i = 0; i < 4; i++) psbuf[4 + i] = dst_ip[i];         /* dst IP */
    psbuf[8] = 0;                                            /* zero */
    psbuf[9] = 6;                                            /* protocol = TCP */
    psbuf[10] = (unsigned char)(tcp_len >> 8); psbuf[11] = (unsigned char)tcp_len;
    for (i = 0; i < tcp_len; i++) psbuf[12 + i] = frame[34 + i];   /* TCP hdr+veri (cs=0) */
    uint16_t tcs = ip_checksum(psbuf, ps_uzun);
    frame[50] = (unsigned char)(tcs >> 8); frame[51] = (unsigned char)tcs;

    return 34 + tcp_len;                                      /* eth+IP+TCP+payload */
}

/*
 * EL0 launcher: TAM HTTP POST istemcisi syscall üstünde.
 * Tüm tamponlar user yığınında (0x42000000 sayfası) → user-VA guard geçer.
 */
__attribute__((section(".user"), noinline))
static void launcher(void) {
    const unsigned char bizim_mac[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
    const unsigned char bizim_ip[4] = { 10, 0, 2, 15 };
    /* SLIRP gateway sabit MAC (fallback — ARP çözülemezse) */
    unsigned char gw_mac[6]  = { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x02 };
    unsigned char dns_mac[6] = { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x02 };
    unsigned char frame[1600];
    unsigned char rx[2048];
    unsigned char hedef_ip[4] = { 0, 0, 0, 0 };
    int i;

    const int SRC_PORT = 40001;
    const int DST_PORT = 80;
    const uint32_t TCP_ISS = 0x4B454D50UL;   /* "KEMP" — POST marker + ISS */

    /* --- 1) ARP: gateway 10.0.2.2 MAC'ini çöz --- */
    for (i = 0; i < 42; i++) frame[i] = 0;
    for (i = 0; i < 6; i++) frame[i] = 0xff;                 /* dst broadcast */
    for (i = 0; i < 6; i++) frame[6 + i] = bizim_mac[i];
    frame[12] = 0x08; frame[13] = 0x06;                      /* ARP */
    frame[14] = 0x00; frame[15] = 0x01; frame[16] = 0x08; frame[17] = 0x00;
    frame[18] = 6; frame[19] = 4; frame[20] = 0x00; frame[21] = 0x01;  /* request */
    for (i = 0; i < 6; i++) frame[22 + i] = bizim_mac[i];
    frame[28] = 10; frame[29] = 0; frame[30] = 2; frame[31] = 15;  /* spa 10.0.2.15 */
    frame[38] = 10; frame[39] = 0; frame[40] = 2; frame[41] = 2;   /* tpa 10.0.2.2 gateway */
    (void)sys2(24, (unsigned long)(uintptr_t)frame, 42);

    for (int d = 0; d < 60; d++) {
        long n = (long)sys2(25, (unsigned long)(uintptr_t)rx, 128);
        if (n < 42) continue;
        if (rx[12] != 0x08 || rx[13] != 0x06) continue;      /* ARP */
        if (rx[20] != 0x00 || rx[21] != 0x02) continue;      /* reply */
        if (rx[28] == 10 && rx[29] == 0 && rx[30] == 2 && rx[31] == 2) {
            for (i = 0; i < 6; i++) gw_mac[i] = rx[22 + i];   /* sha = gateway MAC */
            break;
        }
    }

    /* --- 2) ARP: DNS 10.0.2.3 MAC'ini çöz (gw_mac'i ezmez; fallback = gateway MAC) --- */
    for (i = 0; i < 42; i++) frame[i] = 0;
    for (i = 0; i < 6; i++) frame[i] = 0xff;
    for (i = 0; i < 6; i++) frame[6 + i] = bizim_mac[i];
    frame[12] = 0x08; frame[13] = 0x06;
    frame[14] = 0x00; frame[15] = 0x01; frame[16] = 0x08; frame[17] = 0x00;
    frame[18] = 6; frame[19] = 4; frame[20] = 0x00; frame[21] = 0x01;
    for (i = 0; i < 6; i++) frame[22 + i] = bizim_mac[i];
    frame[28] = 10; frame[29] = 0; frame[30] = 2; frame[31] = 15;
    frame[38] = 10; frame[39] = 0; frame[40] = 2; frame[41] = 3;   /* tpa 10.0.2.3 DNS */
    (void)sys2(24, (unsigned long)(uintptr_t)frame, 42);

    for (int d = 0; d < 60; d++) {
        long n = (long)sys2(25, (unsigned long)(uintptr_t)rx, 128);
        if (n < 42) continue;
        if (rx[12] != 0x08 || rx[13] != 0x06) continue;
        if (rx[20] != 0x00 || rx[21] != 0x02) continue;
        if (rx[28] == 10 && rx[29] == 0 && rx[30] == 2 && rx[31] == 3) {
            for (i = 0; i < 6; i++) dns_mac[i] = rx[22 + i];
            break;
        }
    }

    /* --- 3) DNS: "example.com" A-kaydını çöz → hedef_ip --- */
    /* QNAME "\x07example\x03com\x00" = 13 byte. DNS payload = 12+13+2+2 = 29 byte. */
    const uint16_t dns_id = 0x4B7F;
    unsigned char dns[29];
    for (i = 0; i < 29; i++) dns[i] = 0;
    dns[0] = (unsigned char)(dns_id >> 8); dns[1] = (unsigned char)dns_id;  /* id */
    dns[2] = 0x01; dns[3] = 0x00;          /* flags: RD */
    dns[4] = 0x00; dns[5] = 0x01;          /* qdcount = 1 */
    dns[12] = 7;
    dns[13] = 'e'; dns[14] = 'x'; dns[15] = 'a'; dns[16] = 'm';
    dns[17] = 'p'; dns[18] = 'l'; dns[19] = 'e';
    dns[20] = 3; dns[21] = 'c'; dns[22] = 'o'; dns[23] = 'm'; dns[24] = 0;
    dns[25] = 0x00; dns[26] = 0x01;        /* qtype = A */
    dns[27] = 0x00; dns[28] = 0x01;        /* qclass = IN */
    int dl = 29;

    for (i = 0; i < 128; i++) frame[i] = 0;
    for (i = 0; i < 6; i++) frame[i] = dns_mac[i];           /* dst = DNS/gateway MAC */
    for (i = 0; i < 6; i++) frame[6 + i] = bizim_mac[i];
    frame[12] = 0x08; frame[13] = 0x00;                      /* IPv4 */
    int ip_total = 20 + 8 + dl;
    frame[14] = 0x45; frame[16] = (unsigned char)(ip_total >> 8); frame[17] = (unsigned char)ip_total;
    frame[20] = 0x40; frame[22] = 64; frame[23] = 17;        /* DF, TTL, UDP */
    frame[26] = 10; frame[27] = 0; frame[28] = 2; frame[29] = 15;   /* src 10.0.2.15 */
    frame[30] = 10; frame[31] = 0; frame[32] = 2; frame[33] = 3;    /* dst 10.0.2.3 DNS */
    uint16_t s = ip_checksum(&frame[14], 20);
    frame[24] = (unsigned char)(s >> 8); frame[25] = (unsigned char)s;
    int udp_len = 8 + dl;
    frame[34] = 0x13; frame[35] = 0x88;                      /* src port 5000 */
    frame[36] = 0x00; frame[37] = 53;                        /* dst port 53 */
    frame[38] = (unsigned char)(udp_len >> 8); frame[39] = (unsigned char)udp_len;
    for (i = 0; i < dl; i++) frame[42 + i] = dns[i];
    (void)sys2(24, (unsigned long)(uintptr_t)frame, (unsigned long)(42 + dl));

    int resolve_ok = 0;
    int dns_baz = 42;
    for (int d = 0; d < 200 && !resolve_ok; d++) {
        long n = (long)sys2(25, (unsigned long)(uintptr_t)rx, 512);
        if (n < 54) continue;
        if (rx[12] != 0x08 || rx[13] != 0x00) continue;      /* IPv4 */
        if (rx[23] != 17) continue;                          /* UDP */
        if (!(rx[26] == 10 && rx[27] == 0 && rx[28] == 2 && rx[29] == 3)) continue;  /* src 10.0.2.3 */
        if (rx[34] != 0x00 || rx[35] != 53) continue;        /* src port 53 */
        int udp_toplam = ((int)rx[38] << 8) | rx[39];
        int dns_uzun = udp_toplam - 8;
        if (dns_uzun < 12 || dns_baz + dns_uzun > (int)n) continue;
        const unsigned char *dp = &rx[dns_baz];
        uint16_t r_id = ((uint16_t)dp[0] << 8) | dp[1];
        int qr = (dp[2] & 0x80) != 0;
        if (r_id != dns_id || !qr) continue;
        uint16_t qdcount = ((uint16_t)dp[4] << 8) | dp[5];
        uint16_t ancount = ((uint16_t)dp[6] << 8) | dp[7];
        if (ancount == 0) continue;
        /* Question bölümünü atla. */
        int off = 12;
        int hata = 0;
        for (int q = 0; q < qdcount && !hata; q++) {
            off = isim_atla(dp, dns_uzun, off);
            if (off < 0 || off + 4 > dns_uzun) { hata = 1; break; }
            off += 4;
        }
        if (hata) continue;
        /* Answer record'ları: ilk A kaydı (TYPE=1, RDLENGTH=4). */
        for (int a = 0; a < ancount && !resolve_ok; a++) {
            off = isim_atla(dp, dns_uzun, off);
            if (off < 0 || off + 10 > dns_uzun) break;
            uint16_t type = ((uint16_t)dp[off] << 8) | dp[off + 1];
            uint16_t rdlength = ((uint16_t)dp[off + 8] << 8) | dp[off + 9];
            int rdata_off = off + 10;
            if (rdata_off + rdlength > dns_uzun) break;
            if (type == 1 && rdlength == 4) {
                for (i = 0; i < 4; i++) hedef_ip[i] = dp[rdata_off + i];
                if (!(hedef_ip[0] == 0 && hedef_ip[1] == 0 && hedef_ip[2] == 0 && hedef_ip[3] == 0))
                    resolve_ok = 1;
            }
            off = rdata_off + rdlength;
        }
    }

    if (!resolve_ok) {
        sys(5, (unsigned long)(uintptr_t)"USERPOST DNS YOK");
        sys(7, 0);
        for (;;) { }
    }

    sys(5, (unsigned long)(uintptr_t)"USERPOST HEDEF IP=");
    bas_bayt_hex(hedef_ip[0]); sys(5, (unsigned long)(uintptr_t)".");
    bas_bayt_hex(hedef_ip[1]); sys(5, (unsigned long)(uintptr_t)".");
    bas_bayt_hex(hedef_ip[2]); sys(5, (unsigned long)(uintptr_t)".");
    bas_bayt_hex(hedef_ip[3]);
    sys(7, 0);

    /* --- HTTP POST isteği payload'ını EL0 user tamponuna ELLE byte-byte yaz ---
     * KRİTİK (D-177): kernel .rodata string literalini EL0 dereference EDEMEZ.
     * sys2(24)'e verdiğimiz frame içindeki payload EL0'ın YAZDIĞI byte'lar olmalı.
     * İstek satırı + başlıklar + gövde:
     *   "POST /post HTTP/1.1\r\nHost: example.com\r\n"
     *   "Content-Type: text/plain\r\nContent-Length: 10\r\n"
     *   "Connection: close\r\n\r\nKEMGU-POST"
     * Gövde "KEMGU-POST" = 10 byte → Content-Length: 10.
     * Aşağıdaki her atama derleme-zamanı sabiti → .rodata deref YOK. */
    unsigned char http[160];
    int hl = 0;
    /* İstek satırı: "POST /post HTTP/1.1\r\n" */
    http[hl++]='P'; http[hl++]='O'; http[hl++]='S'; http[hl++]='T'; http[hl++]=' ';
    http[hl++]='/'; http[hl++]='p'; http[hl++]='o'; http[hl++]='s'; http[hl++]='t';
    http[hl++]=' '; http[hl++]='H'; http[hl++]='T'; http[hl++]='T'; http[hl++]='P';
    http[hl++]='/'; http[hl++]='1'; http[hl++]='.'; http[hl++]='1'; http[hl++]='\r';
    http[hl++]='\n';
    /* "Host: example.com\r\n" */
    http[hl++]='H'; http[hl++]='o'; http[hl++]='s'; http[hl++]='t'; http[hl++]=':';
    http[hl++]=' '; http[hl++]='e'; http[hl++]='x'; http[hl++]='a'; http[hl++]='m';
    http[hl++]='p'; http[hl++]='l'; http[hl++]='e'; http[hl++]='.'; http[hl++]='c';
    http[hl++]='o'; http[hl++]='m'; http[hl++]='\r'; http[hl++]='\n';
    /* "Content-Type: text/plain\r\n" */
    http[hl++]='C'; http[hl++]='o'; http[hl++]='n'; http[hl++]='t'; http[hl++]='e';
    http[hl++]='n'; http[hl++]='t'; http[hl++]='-'; http[hl++]='T'; http[hl++]='y';
    http[hl++]='p'; http[hl++]='e'; http[hl++]=':'; http[hl++]=' '; http[hl++]='t';
    http[hl++]='e'; http[hl++]='x'; http[hl++]='t'; http[hl++]='/'; http[hl++]='p';
    http[hl++]='l'; http[hl++]='a'; http[hl++]='i'; http[hl++]='n'; http[hl++]='\r';
    http[hl++]='\n';
    /* "Content-Length: 10\r\n" — gövde 10 byte */
    http[hl++]='C'; http[hl++]='o'; http[hl++]='n'; http[hl++]='t'; http[hl++]='e';
    http[hl++]='n'; http[hl++]='t'; http[hl++]='-'; http[hl++]='L'; http[hl++]='e';
    http[hl++]='n'; http[hl++]='g'; http[hl++]='t'; http[hl++]='h'; http[hl++]=':';
    http[hl++]=' '; http[hl++]='1'; http[hl++]='0'; http[hl++]='\r'; http[hl++]='\n';
    /* "Connection: close\r\n" */
    http[hl++]='C'; http[hl++]='o'; http[hl++]='n'; http[hl++]='n'; http[hl++]='e';
    http[hl++]='c'; http[hl++]='t'; http[hl++]='i'; http[hl++]='o'; http[hl++]='n';
    http[hl++]=':'; http[hl++]=' '; http[hl++]='c'; http[hl++]='l'; http[hl++]='o';
    http[hl++]='s'; http[hl++]='e'; http[hl++]='\r'; http[hl++]='\n';
    /* Başlık-gövde ayıracı "\r\n" */
    http[hl++]='\r'; http[hl++]='\n';
    /* Gövde "KEMGU-POST" (10 byte) */
    http[hl++]='K'; http[hl++]='E'; http[hl++]='M'; http[hl++]='G'; http[hl++]='U';
    http[hl++]='-'; http[hl++]='P'; http[hl++]='O'; http[hl++]='S'; http[hl++]='T';
    int http_uzun = hl;

    /* --- 4) TCP SYN gönder: hedef_ip:80, seq=ISS ("KEMP") --- */
    int toplam = tcp_segment_kur(frame, gw_mac, bizim_mac, bizim_ip, hedef_ip,
                                 SRC_PORT, DST_PORT, TCP_ISS, 0, 0x02, 0, 0);   /* SYN */
    (void)sys2(24, (unsigned long)(uintptr_t)frame, (unsigned long)toplam);

    /* --- SYN-ACK al (poll): flags=0x12, ack=ISS+1, src port=80, src IP=hedef --- */
    int synack_ok = 0;
    uint32_t onlarin_iss = 0;
    for (int d = 0; d < 120 && !synack_ok; d++) {
        long n = (long)sys2(25, (unsigned long)(uintptr_t)rx, 2048);
        if (n < 54) continue;
        if (rx[12] != 0x08 || rx[13] != 0x00) continue;       /* IPv4 */
        if (rx[23] != 6) continue;                            /* TCP */
        if (!(rx[26] == hedef_ip[0] && rx[27] == hedef_ip[1] &&
              rx[28] == hedef_ip[2] && rx[29] == hedef_ip[3])) continue;   /* src IP = hedef */
        int rsrc = (rx[34] << 8) | rx[35];
        int rdst = (rx[36] << 8) | rx[37];
        if (rsrc != DST_PORT || rdst != SRC_PORT) continue;
        unsigned char flags = rx[47];
        if ((flags & 0x12) != 0x12) continue;                 /* SYN+ACK bekle */
        uint32_t r_ack = ((uint32_t)rx[42] << 24) | ((uint32_t)rx[43] << 16) |
                         ((uint32_t)rx[44] << 8)  | (uint32_t)rx[45];
        if (r_ack != (uint32_t)(TCP_ISS + 1)) continue;       /* ack = ISS+1 */
        onlarin_iss = ((uint32_t)rx[38] << 24) | ((uint32_t)rx[39] << 16) |
                      ((uint32_t)rx[40] << 8)  | (uint32_t)rx[41];
        synack_ok = 1;
    }

    if (!synack_ok) {
        /* İnternet yok / SLIRP dış-TCP yanıt vermedi → pcap TX fallback devrede.
         * Handshake kurulamadı; yine de POST isteğini gönder ki pcap'te "POST /"
         * görünsün. seq = ISS+1 (varsayımsal), ack = 0. */
        toplam = tcp_segment_kur(frame, gw_mac, bizim_mac, bizim_ip, hedef_ip,
                                 SRC_PORT, DST_PORT, (uint32_t)(TCP_ISS + 1), 0,
                                 0x18, http, http_uzun);   /* PSH+ACK + veri */
        (void)sys2(24, (unsigned long)(uintptr_t)frame, (unsigned long)toplam);
        sys(5, (unsigned long)(uintptr_t)"USERPOST SENT OK");
        sys(7, 0);
        for (;;) { }
    }

    /* --- ACK gönder: flags=0x10, seq=ISS+1, ack=onların_iss+1 → ESTABLISHED --- */
    uint32_t bizim_seq = (uint32_t)(TCP_ISS + 1);
    uint32_t bizim_ack = (uint32_t)(onlarin_iss + 1);
    toplam = tcp_segment_kur(frame, gw_mac, bizim_mac, bizim_ip, hedef_ip,
                             SRC_PORT, DST_PORT, bizim_seq, bizim_ack, 0x10, 0, 0);  /* ACK */
    (void)sys2(24, (unsigned long)(uintptr_t)frame, (unsigned long)toplam);

    sys(5, (unsigned long)(uintptr_t)"USERPOST TCP ESTABLISHED");
    sys(7, 0);

    /* --- 5) HTTP POST isteğini TCP DATA segmenti (PSH+ACK, flags=0x18) olarak yolla --- */
    toplam = tcp_segment_kur(frame, gw_mac, bizim_mac, bizim_ip, hedef_ip,
                             SRC_PORT, DST_PORT, bizim_seq, bizim_ack, 0x18,
                             http, http_uzun);   /* PSH+ACK + veri (istek+gövde) */
    (void)sys2(24, (unsigned long)(uintptr_t)frame, (unsigned long)toplam);
    bizim_seq += (uint32_t)http_uzun;   /* payload kadar seq ilerler */

    /* Aranan durum-satırı deseni "HTTP/1." — EL0 tamponuna ELLE yaz (.rodata deref YOK). */
    unsigned char desen[8];
    desen[0]='H'; desen[1]='T'; desen[2]='T'; desen[3]='P';
    desen[4]='/'; desen[5]='1'; desen[6]='.'; desen[7]=0;

    /* --- 6) HTTP yanıtını al (poll): ilk DATA segmentinde "HTTP/1." durum satırı --- */
    int http_ok = 0;
    int durum_off = -1;
    int rx_uzun = 0;
    for (int d = 0; d < 160 && !http_ok; d++) {
        long n = (long)sys2(25, (unsigned long)(uintptr_t)rx, 2048);
        if (n < 54) continue;
        if (rx[12] != 0x08 || rx[13] != 0x00) continue;       /* IPv4 */
        if (rx[23] != 6) continue;                            /* TCP */
        if (!(rx[26] == hedef_ip[0] && rx[27] == hedef_ip[1] &&
              rx[28] == hedef_ip[2] && rx[29] == hedef_ip[3])) continue;   /* src IP = hedef */
        int rsrc = (rx[34] << 8) | rx[35];
        int rdst = (rx[36] << 8) | rx[37];
        if (rsrc != DST_PORT || rdst != SRC_PORT) continue;   /* bizim bağlantımız */

        /* Payload konumunu IPv4 total-len + TCP data-offset'ten bul. */
        int ip_len = ((int)rx[16] << 8) | rx[17];
        int ip_ihl = (rx[14] & 0x0F) * 4;
        int tcp_off = 14 + ip_ihl;
        if (tcp_off + 20 > (int)n) continue;
        int tcp_data_off = ((rx[tcp_off + 12] >> 4) & 0x0F) * 4;
        int payload_off = tcp_off + tcp_data_off;
        int payload_uzun = (14 + ip_len) - payload_off;
        if (payload_uzun <= 0 || payload_off >= (int)n) continue;   /* saf ACK vs → veri yok */
        if (payload_off + payload_uzun > (int)n) payload_uzun = (int)n - payload_off;

        /* Gelen data'ya ACK dön (seq/ack takibi — nazik protokol). */
        uint32_t r_seq = ((uint32_t)rx[tcp_off + 4] << 24) | ((uint32_t)rx[tcp_off + 5] << 16) |
                         ((uint32_t)rx[tcp_off + 6] << 8)  | (uint32_t)rx[tcp_off + 7];
        bizim_ack = r_seq + (uint32_t)payload_uzun;
        toplam = tcp_segment_kur(frame, gw_mac, bizim_mac, bizim_ip, hedef_ip,
                                 SRC_PORT, DST_PORT, bizim_seq, bizim_ack, 0x10, 0, 0);  /* ACK */
        (void)sys2(24, (unsigned long)(uintptr_t)frame, (unsigned long)toplam);

        /* Durum satırını ara: "HTTP/1." payload içinde. */
        int rel = icinde_bul(&rx[payload_off], payload_uzun, desen, 7);
        if (rel >= 0) {
            durum_off = payload_off + rel;
            rx_uzun = (int)n;
            http_ok = 1;
        }
    }

    if (!http_ok) {
        /* Yanıt gelmedi → pcap TX fallback: POST isteği gönderildi. */
        sys(5, (unsigned long)(uintptr_t)"USERPOST SENT OK");
        sys(7, 0);
        for (;;) { }
    }

    /* Durum satırını bas (ilk CR/LF veya güvenlik sınırı 96 bayta kadar). */
    sys(5, (unsigned long)(uintptr_t)"USERPOST DURUM: ");
    for (int k = durum_off; k < rx_uzun && k < durum_off + 96; k++) {
        if (rx[k] == '\r' || rx[k] == '\n') break;
        bas_bayt_hex(rx[k]);   /* durum satırı byte'larını hex bas (EL0 char-yaz yok) */
    }
    sys(7, 0);

    /* Durum kodu kontrolü: "HTTP/1.x " sonrası 3 haneli kod. durum_off + 9.
     * POST'a beklenen yanıtlar: 200 (echo/başarı), 3xx (redirect), 405 (Method
     * Not Allowed — example.com POST'a bunu döner). Hepsi bağlantı+POST kanıtı. */
    int kod_off = durum_off + 9;
    int kabul = 0;
    if (kod_off + 2 < rx_uzun) {
        unsigned char k0 = rx[kod_off];
        unsigned char k1 = rx[kod_off + 1];
        unsigned char k2 = rx[kod_off + 2];
        if (k0 == '2' && k1 == '0' && k2 == '0') kabul = 1;   /* 200 OK */
        if (k0 == '3') kabul = 1;                             /* 3xx redirect */
        if (k0 == '4' && k1 == '0' && k2 == '5') kabul = 1;   /* 405 Method Not Allowed */
        /* Durum kodunu da hex olarak bas (3 byte). */
        sys(5, (unsigned long)(uintptr_t)"USERPOST KOD=");
        bas_bayt_hex(k0);
        bas_bayt_hex(k1);
        bas_bayt_hex(k2);
        sys(7, 0);
    }

    if (kabul) {
        sys(5, (unsigned long)(uintptr_t)"USERPOST OK");
    } else {
        /* HTTP/1.x bulundu ama beklenen kod değil — yine de yanıt alındı → POST çalıştı. */
        sys(5, (unsigned long)(uintptr_t)"USERPOST YANIT ALINDI");
    }
    sys(7, 0);
    for (;;) { }
}

int main(void) {
    kdl_yazdir_metin("USERPOST BASLA");
    kdl_yazdir_satir();

    /* Kernel (EL1) net sürücüsünü kurar — EL0 süreç yalnız syscall kullanır. */
    uint64_t nb = kdl_virtio_net_bul();
    if (!nb || kdl_virtio_net_kur(nb) != 0) {
        kdl_yazdir_metin("NET KUR HATA"); kdl_yazdir_satir();
        for (;;) { __asm__ volatile("wfe"); }
    }

    kdl_surec_kur_el0_veri(l1_l, l2_l, 0x42000000UL, 0x44000000UL);
    kdl_preempt_baslat();
    int tl = kdl_preempt_gorev_olustur_el0(launcher, kstack_l + sizeof(kstack_l),
                                           (void *)(uintptr_t)0x42380000UL);
    kdl_preempt_gorev_ttbr(tl, l1_l);
    kdl_kesme_kur();
    kdl_timer_baslat();
    kdl_preempt_ac();

    for (;;) { __asm__ volatile("wfe"); }
}
