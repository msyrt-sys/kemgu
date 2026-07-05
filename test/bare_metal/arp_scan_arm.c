/*
 * ARP host-keşfi testi (aarch64) — SUBNET TARAMASI (L2 canlı-host bulma).
 *
 * Bir "pentest OS"nin temel keşif primitifi: ağdaki canlı host'ları ARP ile bul.
 * TEK-hedef ARP round-trip'ini (D-145, arp_arm.c) SUBNET TARAMASINA genişletir.
 *
 * 10.0.2.1'den 10.0.2.15'e KADAR her IP için bir ARP İSTEĞİ (tpa=hedef IP)
 * broadcast gönderilir, sonra gelen ARP-reply'ler virtio-net RX ile toplanır.
 * Her reply'den sender IP (spa) + sender MAC (sha) çıkarılır → canlı host.
 *
 * QEMU SLIRP ağ geçidi (10.0.2.2) ARP'a HER ZAMAN yanıt verir → en az 1 host
 * deterministik keşfedilir. DNS (10.0.2.3) de genelde yanıtlar (tipik 2 host).
 *
 * Kanıt: ">=1 canlı host keşfedildi" + "ARP SCAN OK".
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
static uint8_t frame[64];
static uint8_t rx[2048];

/* Keşfedilen host tablosu (spa'ları paketlenmiş uint32 olarak sakla). */
#define TARAMA_ILK   1    /* 10.0.2.1 */
#define TARAMA_SON   15   /* 10.0.2.15 */
static uint32_t bulunan_ip[TARAMA_SON - TARAMA_ILK + 1];
static uint8_t  bulunan_mac[TARAMA_SON - TARAMA_ILK + 1][6];
static int      bulunan_sayi = 0;

static _Noreturn void halt(void) { for (;;) { __asm__ volatile("wfe"); } }

/* Bir hedef IP (10.0.2.son_oktet) için ARP isteği (broadcast) inşa et + gönder. */
static void arp_istegi_gonder(uint64_t base, uint8_t hedef_son) {
    for (int i = 0; i < 64; i++) frame[i] = 0;
    for (int i = 0; i < 6; i++) frame[i] = 0xff;               /* dst broadcast */
    for (int i = 0; i < 6; i++) frame[6 + i] = bizim_mac[i];   /* src */
    frame[12] = 0x08; frame[13] = 0x06;                        /* ethertype = ARP */
    frame[14] = 0x00; frame[15] = 0x01;                        /* htype = ethernet */
    frame[16] = 0x08; frame[17] = 0x00;                        /* ptype = IPv4 */
    frame[18] = 6; frame[19] = 4;                              /* hlen, plen */
    frame[20] = 0x00; frame[21] = 0x01;                        /* oper = request */
    for (int i = 0; i < 6; i++) frame[22 + i] = bizim_mac[i];  /* sha = bizim mac */
    frame[28] = 10; frame[29] = 0; frame[30] = 2; frame[31] = 15;  /* spa = 10.0.2.15 */
    /* tha = 0 (bilinmiyor) */
    frame[38] = 10; frame[39] = 0; frame[40] = 2; frame[41] = hedef_son; /* tpa = hedef */
    kdl_virtio_net_gonder(base, frame, 60);
}

/* spa (paketlenmiş uint32) zaten tabloda mı? */
static int zaten_var(uint32_t spa) {
    for (int i = 0; i < bulunan_sayi; i++) {
        if (bulunan_ip[i] == spa) return 1;
    }
    return 0;
}

int main(void) {
    kdl_yazdir_metin("ARP SCAN BASLA");
    kdl_yazdir_satir();

    uint64_t base = kdl_virtio_net_bul();
    if (!base) { kdl_yazdir_metin("NET YOK"); kdl_yazdir_satir(); halt(); }
    if (kdl_virtio_net_kur(base) != 0) { kdl_yazdir_metin("NET KUR HATA"); kdl_yazdir_satir(); halt(); }

    /* --- 1) Tüm subnet'e ARP istekleri yayınla (10.0.2.1 .. 10.0.2.15). --- */
    for (int son = TARAMA_ILK; son <= TARAMA_SON; son++) {
        arp_istegi_gonder(base, (uint8_t)son);
    }

    /* --- 2) Gelen ARP-reply'leri topla (poll). --- */
    /* Yük-duyarlı timeout onarımı: kısa per-poll timeout (500K tik) + reply'ler
     * toplandıktan sonra ardışık-boş erken-çıkış. Eski 60×20M tik busy-wait
     * (~1.2 milyar dsb) yüklü makinede QEMU 12s timeout'unu aşıyordu. */
    int bos_ardisik = 0;
    for (int deneme = 0; deneme < 120; deneme++) {
        int n = kdl_virtio_net_al(base, rx, 2048, 500000);
        if (n < 42) {
            bos_ardisik++;
            if (bulunan_sayi >= 1 && bos_ardisik > 12) break;  /* yanıtlar toplandı */
            continue;
        }
        bos_ardisik = 0;
        /* ARP + oper=reply mı? */
        if (rx[12] != 0x08 || rx[13] != 0x06) continue;   /* ethertype ARP */
        if (rx[20] != 0x00 || rx[21] != 0x02) continue;   /* oper = reply */
        /* sender IP (spa @28) + sender MAC (sha @22). */
        uint32_t spa = ((uint32_t)rx[28] << 24) | ((uint32_t)rx[29] << 16) |
                       ((uint32_t)rx[30] << 8)  | (uint32_t)rx[31];
        if (zaten_var(spa)) continue;
        if (bulunan_sayi < (int)(sizeof(bulunan_ip) / sizeof(bulunan_ip[0]))) {
            bulunan_ip[bulunan_sayi] = spa;
            for (int i = 0; i < 6; i++) bulunan_mac[bulunan_sayi][i] = rx[22 + i];
            bulunan_sayi++;
        }
    }

    /* --- 3) Sonuçları bas. --- */
    kdl_yazdir_metin("CANLI HOST SAYISI");
    kdl_yazdir_satir();
    kdl_yazdir_onaltilik((uint64_t)bulunan_sayi);   /* keşfedilen host sayısı */

    for (int i = 0; i < bulunan_sayi; i++) {
        kdl_yazdir_metin("HOST IP");
        kdl_yazdir_satir();
        /* IP oktetleri paketlenmiş: 10.0.2.2 -> 0x0a000202. */
        kdl_yazdir_onaltilik((uint64_t)bulunan_ip[i]);
    }

    if (bulunan_sayi >= 1) {
        kdl_yazdir_metin("ARP SCAN OK");
    } else {
        kdl_yazdir_metin("ARP SCAN FAIL");
    }
    kdl_yazdir_satir();
    halt();
}
