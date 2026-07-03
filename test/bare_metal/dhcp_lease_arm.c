/*
 * DHCP TAM LEASE (DORA) testi (aarch64) — 4-yönlü lease edinimi.
 *
 * D-162 yalnız DISCOVER->OFFER idi. Bu test onu TAM lease edinimine (DORA döngüsü)
 * tamamlar: bir bare-metal OS'un IP adresini gerçekten "kiralayabildiğini" kanıtlar.
 *
 *   (D)ISCOVER  — client broadcast (src=0.0.0.0, UDP 68->67, opt53=1)
 *   (O)FFER     — SLIRP yanıtı (yiaddr=10.0.2.15, opt53=2, server-id öğrenilir)
 *   (R)EQUEST   — client broadcast (opt53=3, opt50=istenen-IP=yiaddr, opt54=server-id)
 *   (A)CK       — SLIRP yanıtı (opt53=5, yiaddr=10.0.2.15) → lease EDİNİLDİ
 *
 * SLIRP'in dahili DHCP sunucusu tamamen deterministik → internet GEREKMEZ.
 * DISCOVER ve REQUEST aynı xid'i taşır (aynı işlem). Kanıt: ACK'te yiaddr=10.0.2.15
 * (SLIRP sabit) + opt53=5 (ACK) → seri "DHCP LEASE OK".
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

/* Bilinen xid — hem DISCOVER hem REQUEST bunu taşır, OFFER/ACK'te eşleşmeli. */
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

/*
 * DHCP/BOOTP broadcast çerçevesi inşa et (Ethernet+IPv4+UDP+BOOTP+options).
 * options tamponu (opt_veri, opt_uzun) offset 240'a kopyalanır ve 255 (end) eklenir.
 * Dönen değer gönderilecek toplam bayt sayısıdır (>=60 padded).
 */
static int dhcp_cerceve_kur(const uint8_t *opt_veri, int opt_uzun) {
    for (int i = 0; i < 400; i++) frame[i] = 0;

    /* --- Ethernet (14) --- */
    for (int i = 0; i < 6; i++) frame[i] = 0xff;               /* dst = broadcast */
    for (int i = 0; i < 6; i++) frame[6 + i] = bizim_mac[i];   /* src = bizim MAC */
    frame[12] = 0x08; frame[13] = 0x00;                        /* ethertype = IPv4 */

    /* DHCP payload: BOOTP 236 + cookie 4 + options + end(1). */
    int dhcp_len = 236 + 4 + opt_uzun + 1;

    /* --- IPv4 (offset 14, 20 bayt) --- */
    int ip_total = 20 + 8 + dhcp_len;
    frame[14] = 0x45;                                          /* v4, IHL=5 */
    frame[15] = 0x00;                                          /* TOS */
    frame[16] = (uint8_t)(ip_total >> 8); frame[17] = (uint8_t)ip_total;
    frame[18] = 0; frame[19] = 0;                             /* id */
    frame[20] = 0x00; frame[21] = 0;                          /* flags/frag = 0 */
    frame[22] = 64;                                           /* TTL */
    frame[23] = 17;                                           /* protocol = UDP */
    frame[24] = 0; frame[25] = 0;                            /* checksum (önce 0) */
    frame[26] = 0; frame[27] = 0; frame[28] = 0; frame[29] = 0;         /* src = 0.0.0.0 */
    frame[30] = 255; frame[31] = 255; frame[32] = 255; frame[33] = 255; /* dst = 255.255.255.255 */
    uint16_t ipsum = ip_checksum(&frame[14], 20);
    frame[24] = (uint8_t)(ipsum >> 8); frame[25] = (uint8_t)ipsum;

    /* --- UDP (offset 34, 8 bayt) --- */
    int udp_len = 8 + dhcp_len;
    frame[34] = 0x00; frame[35] = 68;                        /* src port = 68 (DHCP client) */
    frame[36] = 0x00; frame[37] = 67;                        /* dst port = 67 (DHCP server) */
    frame[38] = (uint8_t)(udp_len >> 8); frame[39] = (uint8_t)udp_len;
    frame[40] = 0; frame[41] = 0;                           /* UDP checksum = 0 (opsiyonel) */

    /* --- BOOTP/DHCP payload (offset 42) --- */
    int d = 42;
    frame[d + 0] = 1;                                        /* op = BOOTREQUEST */
    frame[d + 1] = 1;                                        /* htype = Ethernet */
    frame[d + 2] = 6;                                        /* hlen = 6 */
    frame[d + 3] = 0;                                        /* hops = 0 */
    frame[d + 4] = (uint8_t)(DHCP_XID >> 24);               /* xid (big-endian) */
    frame[d + 5] = (uint8_t)(DHCP_XID >> 16);
    frame[d + 6] = (uint8_t)(DHCP_XID >> 8);
    frame[d + 7] = (uint8_t)(DHCP_XID);
    /* secs/flags/ciaddr/yiaddr/siaddr/giaddr — hepsi sıfır. */
    for (int i = 0; i < 6; i++) frame[d + 28 + i] = bizim_mac[i];   /* chaddr = bizim MAC */

    /* magic cookie @ offset 236 */
    int mc = d + 236;
    frame[mc + 0] = 0x63; frame[mc + 1] = 0x82; frame[mc + 2] = 0x53; frame[mc + 3] = 0x63;

    /* options @ offset 240 */
    int op = mc + 4;
    for (int i = 0; i < opt_uzun; i++) frame[op + i] = opt_veri[i];
    frame[op + opt_uzun] = 255;                             /* option 255: end */

    int toplam = 42 + dhcp_len;
    if (toplam < 60) toplam = 60;
    return toplam;
}

/*
 * Belirli bir DHCP mesaj tipini (beklenen_tip) taşıyan BOOTREPLY'ı poll ile bekle.
 * Eşleşince yiaddr'ı yi[0..3]'e, server-id (opt54) varsa sid[0..3]'e yazar; 1 döner.
 * Bulamazsa 0 döner.
 */
static int dhcp_yanit_bekle(uint64_t base, int beklenen_tip,
                            uint8_t yi[4], uint8_t sid[4], int *sid_var) {
    *sid_var = 0;
    for (int t = 0; t < 40; t++) {
        int n = kdl_virtio_net_al(base, rx, 2048, 3000000);  /* ~3M tik, erken-çıkışlı */
        if (n < 240 + 42) continue;
        /* IPv4 + UDP */
        if (!(rx[12] == 0x08 && rx[13] == 0x00 && rx[23] == 17)) continue;
        /* UDP port: src 67 -> dst 68 */
        if (!(rx[34] == 0x00 && rx[35] == 67 && rx[36] == 0x00 && rx[37] == 68)) continue;
        int b = 42;
        if (rx[b + 0] != 2) continue;                        /* op = BOOTREPLY */
        uint32_t xid = ((uint32_t)rx[b + 4] << 24) | ((uint32_t)rx[b + 5] << 16) |
                       ((uint32_t)rx[b + 6] << 8) | (uint32_t)rx[b + 7];
        if (xid != DHCP_XID) continue;                       /* xid eşleşmeli */
        uint8_t y0 = rx[b + 16], y1 = rx[b + 17], y2 = rx[b + 18], y3 = rx[b + 19];
        if (y0 == 0 && y1 == 0 && y2 == 0 && y3 == 0) continue;   /* non-zero yiaddr */
        /* magic cookie */
        int mco = b + 236;
        if (!(rx[mco + 0] == 0x63 && rx[mco + 1] == 0x82 &&
              rx[mco + 2] == 0x53 && rx[mco + 3] == 0x63)) continue;
        /* option TLV yürü: opt53 (msg tipi) + opt54 (server-id) topla */
        int o = mco + 4;
        int msg_tipi = 0;
        int found_sid = 0;
        uint8_t s0 = 0, s1 = 0, s2 = 0, s3 = 0;
        while (o < n) {
            uint8_t kod = rx[o];
            if (kod == 255) break;                           /* end */
            if (kod == 0) { o++; continue; }                 /* pad */
            if (o + 1 >= n) break;
            uint8_t ln = rx[o + 1];
            if (kod == 53 && ln >= 1 && o + 2 < n) msg_tipi = rx[o + 2];
            if (kod == 54 && ln >= 4 && o + 5 < n) {
                s0 = rx[o + 2]; s1 = rx[o + 3]; s2 = rx[o + 4]; s3 = rx[o + 5];
                found_sid = 1;
            }
            o += 2 + ln;
        }
        if (msg_tipi != beklenen_tip) continue;              /* beklenen tip (OFFER/ACK) şart */
        yi[0] = y0; yi[1] = y1; yi[2] = y2; yi[3] = y3;
        if (found_sid) { sid[0] = s0; sid[1] = s1; sid[2] = s2; sid[3] = s3; *sid_var = 1; }
        return 1;
    }
    return 0;
}

static void yiaddr_bas(const uint8_t yi[4]) {
    kdl_yazdir_metin("YIADDR ");
    kdl_yazdir_isaretsiz_tam(yi[0]); kdl_yazdir_metin(".");
    kdl_yazdir_isaretsiz_tam(yi[1]); kdl_yazdir_metin(".");
    kdl_yazdir_isaretsiz_tam(yi[2]); kdl_yazdir_metin(".");
    kdl_yazdir_isaretsiz_tam(yi[3]);
    kdl_yazdir_satir();
}

int main(void) {
    kdl_yazdir_metin("DHCP LEASE BASLA");
    kdl_yazdir_satir();

    uint64_t base = kdl_virtio_net_bul();
    if (!base) { kdl_yazdir_metin("NET YOK"); kdl_yazdir_satir(); halt(); }
    if (kdl_virtio_net_kur(base) != 0) { kdl_yazdir_metin("NET KUR HATA"); kdl_yazdir_satir(); halt(); }

    /* ========== (D) DISCOVER ========== */
    /* options: 53,1,1 (DISCOVER) + 55,4,1,3,6,15 (param request) */
    uint8_t disc_opt[9] = { 53, 1, 1,  55, 4, 1, 3, 6, 15 };
    int len_d = dhcp_cerceve_kur(disc_opt, 9);
    kdl_virtio_net_gonder(base, frame, len_d);

    /* ========== (O) OFFER ========== */
    uint8_t yi[4] = { 0, 0, 0, 0 };
    uint8_t sid[4] = { 0, 0, 0, 0 };
    int sid_var = 0;
    if (!dhcp_yanit_bekle(base, 2 /* OFFER */, yi, sid, &sid_var)) {
        kdl_yazdir_metin("DHCP OFFER YOK");
        kdl_yazdir_satir();
        halt();
    }
    kdl_yazdir_metin("OFFER ALINDI");
    kdl_yazdir_satir();
    yiaddr_bas(yi);

    /* ========== (R) REQUEST ========== */
    /* options: 53,1,3 (REQUEST) + 50,4,<yiaddr> (requested IP)
     *          + [54,4,<server-id>] (server identifier — OFFER'da geldiyse) + 55 param list */
    uint8_t req_opt[32];
    int ro = 0;
    req_opt[ro++] = 53; req_opt[ro++] = 1; req_opt[ro++] = 3;   /* DHCP message type = REQUEST */
    req_opt[ro++] = 50; req_opt[ro++] = 4;                      /* option 50: requested IP = yiaddr */
    req_opt[ro++] = yi[0]; req_opt[ro++] = yi[1];
    req_opt[ro++] = yi[2]; req_opt[ro++] = yi[3];
    if (sid_var) {
        req_opt[ro++] = 54; req_opt[ro++] = 4;                 /* option 54: server identifier */
        req_opt[ro++] = sid[0]; req_opt[ro++] = sid[1];
        req_opt[ro++] = sid[2]; req_opt[ro++] = sid[3];
    }
    req_opt[ro++] = 55; req_opt[ro++] = 4;                     /* option 55: parameter request list */
    req_opt[ro++] = 1; req_opt[ro++] = 3; req_opt[ro++] = 6; req_opt[ro++] = 15;
    int len_r = dhcp_cerceve_kur(req_opt, ro);
    kdl_virtio_net_gonder(base, frame, len_r);
    kdl_yazdir_metin("REQUEST GONDERILDI");
    kdl_yazdir_satir();

    /* ========== (A) ACK ========== */
    uint8_t yi2[4] = { 0, 0, 0, 0 };
    uint8_t sid2[4] = { 0, 0, 0, 0 };
    int sid2_var = 0;
    if (!dhcp_yanit_bekle(base, 5 /* ACK */, yi2, sid2, &sid2_var)) {
        kdl_yazdir_metin("DHCP ACK YOK");
        kdl_yazdir_satir();
        halt();
    }

    /* ACK yiaddr, OFFER yiaddr ile aynı (10.0.2.15) olmalı → lease tutarlı. */
    if (!(yi2[0] == yi[0] && yi2[1] == yi[1] && yi2[2] == yi[2] && yi2[3] == yi[3])) {
        kdl_yazdir_metin("DHCP ACK YIADDR UYUMSUZ");
        kdl_yazdir_satir();
        halt();
    }

    kdl_yazdir_metin("ACK ALINDI");
    kdl_yazdir_satir();
    yiaddr_bas(yi2);

    /* 4-yönlü DORA tamamlandı → lease edinildi. */
    kdl_yazdir_metin("DHCP LEASE OK");
    kdl_yazdir_satir();
    halt();
}
