/*
 * DHCP DISCOVER/OFFER testi (aarch64) — AĞ OTO-KONFİGÜRASYON (bir OS'un ilk açılış adımı).
 *
 * Bir bare-metal OS açıldığında IP adresini bilmez. İlk iş: DHCP ile ağ config al.
 * Kernel bir DHCP DISCOVER (broadcast, src=0.0.0.0, UDP 68->67) yayınlar; SLIRP'in
 * dahili DHCP sunucusu (10.0.2.2:67) deterministik bir OFFER döner (yiaddr=10.0.2.15).
 * Kernel OFFER'ı RX ile alır, xid eşleşmesini + yiaddr'ı + option 53=OFFER'ı doğrular
 * ve önerilen IP'yi ekrana basar. Internet GEREKMEZ — SLIRP DHCP tamamen deterministik.
 *
 * Kanıt: seri "DHCP OK" + yiaddr oktetleri (10.0.2.15).
 */
#include <stdint.h>
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_satir(void);
extern void kdl_yazdir_isaretsiz_tam(uint32_t);
extern uint64_t kdl_virtio_net_bul(void);
extern int kdl_virtio_net_kur(uint64_t base);
extern int kdl_virtio_net_gonder(uint64_t base, const uint8_t *cerceve, int uzun);
extern int kdl_virtio_net_al(uint64_t base, uint8_t *hedef, int max, long tikler);

static const uint8_t bizim_mac[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
static uint8_t frame[400];
static uint8_t rx[2048];

/* Bilinen xid — OFFER'da eşleşmeli. */
#define DHCP_XID 0x12345678U

_Noreturn static void halt(void) { for (;;) { __asm__ volatile("wfe"); } }

/* IPv4 header checksum: 16-bit tümleyen toplamı (RFC 1071). */
static uint16_t ip_checksum(const uint8_t *veri, int uzun) {
    uint32_t t = 0;
    for (int i = 0; i + 1 < uzun; i += 2) t += ((uint32_t)veri[i] << 8) | veri[i + 1];
    if (uzun & 1) t += (uint32_t)veri[uzun - 1] << 8;
    while (t >> 16) t = (t & 0xffff) + (t >> 16);
    return (uint16_t)(~t);
}

int main(void) {
    kdl_yazdir_metin("DHCP BASLA");
    kdl_yazdir_satir();

    uint64_t base = kdl_virtio_net_bul();
    if (!base) { kdl_yazdir_metin("NET YOK"); kdl_yazdir_satir(); halt(); }
    if (kdl_virtio_net_kur(base) != 0) { kdl_yazdir_metin("NET KUR HATA"); kdl_yazdir_satir(); halt(); }

    /* --- DHCP/BOOTP payload inşası (offset 42'den itibaren frame içinde) --- */
    /* BOOTP sabit kısmı 236 bayt + magic cookie(4) + option 53(3) + option 55(4) + end(1).
     * Basit tutuyoruz: magic + op53(DISCOVER) + op55(param request) + op255(end). */
    for (int i = 0; i < 400; i++) frame[i] = 0;

    /* --- Ethernet (14) --- */
    for (int i = 0; i < 6; i++) frame[i] = 0xff;               /* dst = broadcast */
    for (int i = 0; i < 6; i++) frame[6 + i] = bizim_mac[i];   /* src = bizim MAC */
    frame[12] = 0x08; frame[13] = 0x00;                        /* ethertype = IPv4 */

    /* --- DHCP payload uzunluğu hesabı (BOOTP 236 + cookie 4 + options) --- */
    /* options: 53,1,1 (3) + 55,4,1,3,6,15 (6) + 255 (1) = 10 bayt. */
    int dhcp_len = 236 + 4 + 10;                              /* = 250 */

    /* --- IPv4 (offset 14, 20 bayt) --- */
    int ip_total = 20 + 8 + dhcp_len;
    frame[14] = 0x45;                                         /* v4, IHL=5 */
    frame[15] = 0x00;                                         /* TOS */
    frame[16] = (uint8_t)(ip_total >> 8); frame[17] = (uint8_t)ip_total;
    frame[18] = 0; frame[19] = 0;                            /* id */
    frame[20] = 0x00; frame[21] = 0;                         /* flags/frag = 0 */
    frame[22] = 64;                                          /* TTL */
    frame[23] = 17;                                          /* protocol = UDP */
    frame[24] = 0; frame[25] = 0;                           /* checksum (önce 0) */
    frame[26] = 0; frame[27] = 0; frame[28] = 0; frame[29] = 0;         /* src = 0.0.0.0 */
    frame[30] = 255; frame[31] = 255; frame[32] = 255; frame[33] = 255; /* dst = 255.255.255.255 */
    uint16_t ipsum = ip_checksum(&frame[14], 20);
    frame[24] = (uint8_t)(ipsum >> 8); frame[25] = (uint8_t)ipsum;

    /* --- UDP (offset 34, 8 bayt) --- */
    int udp_len = 8 + dhcp_len;
    frame[34] = 0x00; frame[35] = 68;                        /* src port = 68 (DHCP client) */
    frame[36] = 0x00; frame[37] = 67;                        /* dst port = 67 (DHCP server) */
    frame[38] = (uint8_t)(udp_len >> 8); frame[39] = (uint8_t)udp_len;
    frame[40] = 0; frame[41] = 0;                           /* UDP checksum = 0 (opsiyonel, geçerli) */

    /* --- BOOTP/DHCP payload (offset 42) --- */
    int d = 42;                                              /* payload başlangıcı */
    frame[d + 0] = 1;                                        /* op = BOOTREQUEST */
    frame[d + 1] = 1;                                        /* htype = Ethernet */
    frame[d + 2] = 6;                                        /* hlen = 6 */
    frame[d + 3] = 0;                                        /* hops = 0 */
    frame[d + 4] = (uint8_t)(DHCP_XID >> 24);               /* xid (big-endian) */
    frame[d + 5] = (uint8_t)(DHCP_XID >> 16);
    frame[d + 6] = (uint8_t)(DHCP_XID >> 8);
    frame[d + 7] = (uint8_t)(DHCP_XID);
    /* secs(8-9)=0, flags(10-11)=0, ciaddr(12-15)=0, yiaddr(16-19)=0, siaddr(20-23)=0,
     * giaddr(24-27)=0 — hepsi zaten sıfır. */
    for (int i = 0; i < 6; i++) frame[d + 28 + i] = bizim_mac[i];   /* chaddr = bizim MAC */
    /* chaddr kalanı(6..15), sname(64), file(128) = sıfır. */

    /* magic cookie @ offset 236 */
    int mc = d + 236;
    frame[mc + 0] = 0x63; frame[mc + 1] = 0x82; frame[mc + 2] = 0x53; frame[mc + 3] = 0x63;

    /* options @ offset 240 */
    int op = mc + 4;
    frame[op + 0] = 53; frame[op + 1] = 1; frame[op + 2] = 1;   /* option 53: DHCP message type = DISCOVER */
    frame[op + 3] = 55; frame[op + 4] = 4;                      /* option 55: parameter request list */
    frame[op + 5] = 1;                                          /*   subnet mask */
    frame[op + 6] = 3;                                          /*   router */
    frame[op + 7] = 6;                                          /*   DNS */
    frame[op + 8] = 15;                                         /*   domain name */
    frame[op + 9] = 255;                                        /* option 255: end */

    int toplam = 42 + dhcp_len;                              /* = 292 */
    if (toplam < 60) toplam = 60;
    kdl_virtio_net_gonder(base, frame, toplam);

    /* --- DHCP OFFER'ı AL (poll): UDP src 67 -> dst 68, BOOTREPLY, xid eşleşir --- */
    int offer_ok = 0;
    uint8_t yi0 = 0, yi1 = 0, yi2 = 0, yi3 = 0;
    for (int t = 0; t < 40 && !offer_ok; t++) {
        int n = kdl_virtio_net_al(base, rx, 2048, 30000000);
        if (n < 240 + 42) continue;                          /* eth+ip+udp+bootp+cookie+en az 1 opt */
        /* IPv4 + UDP kontrolü */
        if (!(rx[12] == 0x08 && rx[13] == 0x00 && rx[23] == 17)) continue;
        /* UDP port: src 67 -> dst 68 */
        if (!(rx[34] == 0x00 && rx[35] == 67 && rx[36] == 0x00 && rx[37] == 68)) continue;
        /* BOOTP başlangıcı offset 42 */
        int b = 42;
        if (rx[b + 0] != 2) continue;                        /* op = BOOTREPLY */
        /* xid eşleşmesi (offset 4..7) */
        uint32_t xid = ((uint32_t)rx[b + 4] << 24) | ((uint32_t)rx[b + 5] << 16) |
                       ((uint32_t)rx[b + 6] << 8) | (uint32_t)rx[b + 7];
        if (xid != DHCP_XID) continue;
        /* yiaddr (offset 16..19) */
        yi0 = rx[b + 16]; yi1 = rx[b + 17]; yi2 = rx[b + 18]; yi3 = rx[b + 19];
        if (yi0 == 0 && yi1 == 0 && yi2 == 0 && yi3 == 0) continue;   /* non-zero yiaddr şart */
        /* magic cookie (offset 236..239) */
        int mco = b + 236;
        if (!(rx[mco + 0] == 0x63 && rx[mco + 1] == 0x82 &&
              rx[mco + 2] == 0x53 && rx[mco + 3] == 0x63)) continue;
        /* option 53 = 2 (OFFER) arama (240'tan itibaren TLV yürü) */
        int o = mco + 4;
        int msg_tipi = 0;
        while (o < n) {
            uint8_t kod = rx[o];
            if (kod == 255) break;                           /* end */
            if (kod == 0) { o++; continue; }                 /* pad */
            if (o + 1 >= n) break;
            uint8_t ln = rx[o + 1];
            if (kod == 53 && ln >= 1 && o + 2 < n) msg_tipi = rx[o + 2];
            o += 2 + ln;
        }
        if (msg_tipi != 2) continue;                         /* option 53 = OFFER (2) şart */
        offer_ok = 1;
    }

    if (offer_ok) {
        kdl_yazdir_metin("YIADDR ");
        kdl_yazdir_isaretsiz_tam(yi0); kdl_yazdir_metin(".");
        kdl_yazdir_isaretsiz_tam(yi1); kdl_yazdir_metin(".");
        kdl_yazdir_isaretsiz_tam(yi2); kdl_yazdir_metin(".");
        kdl_yazdir_isaretsiz_tam(yi3);
        kdl_yazdir_satir();
        kdl_yazdir_metin("DHCP OK");
        kdl_yazdir_satir();
    } else {
        kdl_yazdir_metin("DHCP OFFER YOK");
        kdl_yazdir_satir();
    }
    halt();
}
