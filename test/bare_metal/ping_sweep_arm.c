/*
 * ICMP ping-sweep testi (aarch64) — nmap-tarzı HOST KEŞFİ (L3 canlı-host bulma).
 *
 * D-156 tek-ping (icmp_arm.c) + D-158 subnet-iterasyon (arp_scan_arm.c) BİRLEŞİMİ.
 * Bir "pentest OS"nin L3 keşif primitifi: subnet'teki her IP'ye ICMP Echo Request
 * yolla → echo reply gelen = CANLI host (ARP L2 keşfin üstünde IP-seviyesi keşif).
 *
 * 10.0.2.1'den 10.0.2.5'e kadar her IP için:
 *   1) Ethernet+IPv4+ICMP Echo Request (type=8, payload "KEMGU") inşa et + gönder.
 *      (SLIRP tüm dahili IP'leri geçit MAC'inden yönlendirir → ARP bir kez çözülür.)
 *   2) KISA per-IP timeout ile echo reply (type=0, id/seq eşleşir) bekle.
 *   3) Reply gelen → canlı host; gelmeyen → ölü/yanıtsız.
 *
 * QEMU SLIRP ağ geçidi (10.0.2.2) VE DNS (10.0.2.3) ICMP echo'ya dahili yanıt
 * verir → en az 2 canlı host DETERMİNİSTİK keşfedilir (internetsiz, host ayrıcalığı
 * gerekmez).
 *
 * Kanıt: ">=2 canlı host" + "PING SWEEP OK N".
 *
 * ICMP checksum: pseudo-header YOK — sadece ICMP başlığı + veri üzerinde RFC1071.
 *
 * DERS (D-158, KRİTİK net yük): net_al busy-wait → per-IP KÜÇÜK tik (~2.5M),
 * erken-çıkışlı. Toplam (5 IP × birkaç poll × tik) küçük tutulur (<300M) yoksa
 * yüklü makinede QEMU timeout flake olur.
 */
#include <stdint.h>
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_satir(void);
extern void kdl_yazdir_onaltilik(uint64_t);
extern uint64_t kdl_virtio_net_bul(void);
extern int kdl_virtio_net_kur(uint64_t base);
extern int kdl_virtio_net_gonder(uint64_t base, const uint8_t *cerceve, int uzun);
extern int kdl_virtio_net_al(uint64_t base, uint8_t *hedef, int max, long tikler);

static const uint8_t bizim_mac[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
static uint8_t frame[128];
static uint8_t rx[2048];
static uint8_t gw_mac[6];

/* Taranacak aralık: 10.0.2.1 .. 10.0.2.5 (5 IP). */
#define TARAMA_ILK   1
#define TARAMA_SON   5

/* ICMP payload işaretçisi (pcap fallback'te grep ile aranır). */
static const char payload[5] = { 'K', 'E', 'M', 'G', 'U' };

static _Noreturn void halt(void) { for (;;) { __asm__ volatile("wfe"); } }

/* RFC1071 internet checksum — hem IPv4 başlığı hem ICMP mesajı için. */
static uint16_t ip_checksum(const uint8_t *veri, int uzun) {
    uint32_t t = 0;
    for (int i = 0; i + 1 < uzun; i += 2) t += ((uint32_t)veri[i] << 8) | veri[i + 1];
    if (uzun & 1) t += (uint32_t)veri[uzun - 1] << 8;
    while (t >> 16) t = (t & 0xffff) + (t >> 16);
    return (uint16_t)(~t);
}

/* Tek hedef IP (10.0.2.hedef_son) için ICMP Echo Request inşa et + gönder.
 * seq: her hedef için farklı → reply eşleşmesinde hedefi ayırt eder. */
static void ping_gonder(uint64_t base, uint8_t hedef_son, uint16_t icmp_id, uint16_t icmp_seq) {
    /* --- ICMP mesajı: header(8) + payload(5) = 13 bayt --- */
    int icmp_len = 8 + (int)sizeof(payload);
    uint8_t icmp[13];
    for (int i = 0; i < icmp_len; i++) icmp[i] = 0;
    icmp[0] = 8;                                        /* type = 8 (echo request) */
    icmp[1] = 0;                                        /* code = 0 */
    icmp[2] = 0; icmp[3] = 0;                           /* checksum = 0 (hesap öncesi) */
    icmp[4] = (uint8_t)(icmp_id >> 8);  icmp[5] = (uint8_t)icmp_id;
    icmp[6] = (uint8_t)(icmp_seq >> 8); icmp[7] = (uint8_t)icmp_seq;
    for (int i = 0; i < (int)sizeof(payload); i++) icmp[8 + i] = (uint8_t)payload[i];
    uint16_t ics = ip_checksum(icmp, icmp_len);        /* pseudo-header YOK — sade */
    icmp[2] = (uint8_t)(ics >> 8); icmp[3] = (uint8_t)ics;

    /* --- Ethernet + IPv4 çerçevesini kur --- */
    for (int i = 0; i < 128; i++) frame[i] = 0;
    for (int i = 0; i < 6; i++) frame[i] = gw_mac[i];          /* dst = gateway MAC */
    for (int i = 0; i < 6; i++) frame[6 + i] = bizim_mac[i];   /* src */
    frame[12] = 0x08; frame[13] = 0x00;                        /* ethertype = IPv4 */
    int ip_total = 20 + icmp_len;
    frame[14] = 0x45;                                          /* ver=4, IHL=5 */
    frame[16] = (uint8_t)(ip_total >> 8); frame[17] = (uint8_t)ip_total;
    frame[20] = 0x40;                                          /* flags = DF */
    frame[22] = 64;                                            /* TTL */
    frame[23] = 1;                                             /* proto = 1 (ICMP) */
    frame[26] = 10; frame[27] = 0; frame[28] = 2; frame[29] = 15;         /* src = 10.0.2.15 */
    frame[30] = 10; frame[31] = 0; frame[32] = 2; frame[33] = hedef_son;  /* dst = hedef */
    uint16_t ihs = ip_checksum(&frame[14], 20);
    frame[24] = (uint8_t)(ihs >> 8); frame[25] = (uint8_t)ihs;
    for (int i = 0; i < icmp_len; i++) frame[34 + i] = icmp[i]; /* ICMP mesajı IP başlığından sonra */
    kdl_virtio_net_gonder(base, frame, 34 + icmp_len);
}

int main(void) {
    kdl_yazdir_metin("PING SWEEP BASLA");
    kdl_yazdir_satir();

    uint64_t base = kdl_virtio_net_bul();
    if (!base) { kdl_yazdir_metin("NET YOK"); kdl_yazdir_satir(); halt(); }
    if (kdl_virtio_net_kur(base) != 0) { kdl_yazdir_metin("NET KUR HATA"); kdl_yazdir_satir(); halt(); }

    /* --- 1) ARP: geçit (10.0.2.2) MAC'ini bir kez çöz.
     *     SLIRP tüm dahili IP'leri geçit MAC'inden yönlendirir → tek ARP yeter. --- */
    for (int i = 0; i < 64; i++) frame[i] = 0;
    for (int i = 0; i < 6; i++) frame[i] = 0xff;               /* dst broadcast */
    for (int i = 0; i < 6; i++) frame[6 + i] = bizim_mac[i];   /* src */
    frame[12] = 0x08; frame[13] = 0x06;                        /* ethertype = ARP */
    frame[14] = 0x00; frame[15] = 0x01; frame[16] = 0x08; frame[17] = 0x00;
    frame[18] = 6; frame[19] = 4; frame[20] = 0x00; frame[21] = 0x01;   /* oper = request */
    for (int i = 0; i < 6; i++) frame[22 + i] = bizim_mac[i];  /* sha */
    frame[28] = 10; frame[29] = 0; frame[30] = 2; frame[31] = 15;   /* spa = 10.0.2.15 */
    frame[38] = 10; frame[39] = 0; frame[40] = 2; frame[41] = 2;    /* tpa = 10.0.2.2 (gateway) */
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

    /* --- 2) Ping-sweep: 10.0.2.1..5 aralığındaki her IP'yi tara. --- */
    uint16_t icmp_id  = 0xBEEF;
    int icmp_len = 8 + (int)sizeof(payload);
    int canli_sayi = 0;

    for (int son = TARAMA_ILK; son <= TARAMA_SON; son++) {
        uint16_t icmp_seq = (uint16_t)son;   /* her hedef için ayrı seq */
        ping_gonder(base, (uint8_t)son, icmp_id, icmp_seq);

        /* KISA per-IP poll penceresi: her poll ~2.5M tik, en çok birkaç deneme,
         * reply gelince erken çık. Toplam yük <300M korunur (D-158 dersi). */
        int canli = 0;
        for (int d = 0; d < 8 && !canli; d++) {
            int n = kdl_virtio_net_al(base, rx, 2048, 2500000);
            if (n < 42) continue;
            if (rx[12] != 0x08 || rx[13] != 0x00) continue;            /* IPv4 mı */
            if (rx[23] != 1) continue;                                 /* proto = ICMP */
            /* src IP = hedef ettiğimiz IP mi (10.0.2.son) — yanıtı doğru hosta bağla */
            if (!(rx[26] == 10 && rx[27] == 0 && rx[28] == 2 && rx[29] == (uint8_t)son)) continue;
            int ihl = (rx[14] & 0x0f) * 4;                             /* IP başlık uzunluğu */
            int io = 14 + ihl;                                         /* ICMP başlangıç offset'i */
            if (n < io + 8) continue;
            if (rx[io] != 0 || rx[io + 1] != 0) continue;              /* type = 0 reply, code = 0 */
            uint16_t r_id  = ((uint16_t)rx[io + 4] << 8) | rx[io + 5];
            uint16_t r_seq = ((uint16_t)rx[io + 6] << 8) | rx[io + 7];
            if (r_id != icmp_id || r_seq != icmp_seq) continue;        /* id/seq eşleşmeli */
            /* payload geri döndü mü ("KEMGU") */
            int pl_ok = 1;
            if (n < io + icmp_len) pl_ok = 0;
            for (int i = 0; pl_ok && i < (int)sizeof(payload); i++)
                if (rx[io + 8 + i] != (uint8_t)payload[i]) pl_ok = 0;
            if (!pl_ok) continue;
            canli = 1;
        }

        if (canli) {
            canli_sayi++;
            kdl_yazdir_metin("CANLI HOST 10.0.2.");
            kdl_yazdir_onaltilik((uint64_t)son);   /* canlı host son okteti */
            kdl_yazdir_satir();
        } else {
            kdl_yazdir_metin("YANITSIZ 10.0.2.");
            kdl_yazdir_onaltilik((uint64_t)son);   /* ölü/yanıtsız son okteti */
            kdl_yazdir_satir();
        }
    }

    /* --- 3) Sonuç: keşfedilen canlı host sayısı. --- */
    kdl_yazdir_metin("CANLI HOST SAYISI");
    kdl_yazdir_satir();
    kdl_yazdir_onaltilik((uint64_t)canli_sayi);

    if (canli_sayi >= 2) {
        kdl_yazdir_metin("PING SWEEP OK ");
        kdl_yazdir_onaltilik((uint64_t)canli_sayi);   /* N: keşfedilen canlı host */
    } else {
        kdl_yazdir_metin("PING SWEEP FAIL");
    }
    kdl_yazdir_satir();
    halt();
}
