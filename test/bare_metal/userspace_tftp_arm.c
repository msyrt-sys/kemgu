/*
 * USERSPACE TFTP GET testi (aarch64) — EL0 süreç SYSCALL ile AĞDAN DOSYA ÇEKER.
 *
 * DOSYA-TRANSFERİ milestone: şimdiye kadar EL0 (yetkisiz) süreç ARP/DHCP/DNS/
 * ICMP/TCP/HTTP protokollerini D-176 raw-frame syscall'larıyla (net_gonder=24,
 * net_al=25) çalıştırdı. Bu test o zinciri UYGULAMA-KATMANI DOSYA TRANSFERİNE
 * taşır: bir EL0 süreç virtio-net'e DOĞRUDAN erişmeden, YALNIZ syscall ile
 * SLIRP'in dahili TFTP sunucusundan (10.0.2.2:69) bir dosya çeker.
 *
 * TFTP (RFC 1350) UDP tabanlı basit dosya transferi:
 *   İstemci -> sunucu:69 : RRQ (opcode 1) + "dosya\0octet\0"
 *   Sunucu  -> istemci   : DATA (opcode 3, block 1) + <=512 bayt dosya içeriği
 *                          (kaynak port = SLIRP'in seçtiği efemeral TID)
 *   İstemci -> sunucu:TID: ACK (opcode 4, block 1)
 *   Dosya <=512 bayt ise tek DATA bloğu → transfer biter.
 *
 * SLIRP'in DAHİLİ TFTP sunucusu (QEMU `-netdev user,tftp=<DIR>`) 10.0.2.2:69'da
 * dosyaları sunar. Makefile `<BUILD>/tftp/dosya.txt` içine "KEMGU-TFTP-DATA"
 * (15 bayt) yazar → tek DATA bloğu, DETERMİNİSTİK (internetsiz).
 *
 * Akış (EL0 launcher):
 *   1) Gateway MAC'i (52:55:0a:00:02:02 — SLIRP sabit) kullan; TFTP RRQ inşa et
 *      (Eth + IPv4 dst=10.0.2.2 + UDP src=rastgele dst=69 + TFTP opcode 1 +
 *      "dosya.txt\0octet\0") → sys2(24) ile yolla.
 *   2) sys2(25) poll ile TFTP DATA (opcode 3, block 1) al → src port = SLIRP TID,
 *      dst port = bizim RRQ src port. TID'i öğren + ACK (opcode 4, block 1) yolla.
 *   3) DATA payload'unu "KEMGU-TFTP-DATA" ile karşılaştır → eşleşirse çekilen
 *      içeriği bas (EL0-güvenli) + sys(5,"USERTFTP OK") + sys(7,0).
 *
 * Kanıt: bir userspace program, çekirdek-aracılı ham-frame syscall'larıyla
 * ağdan (TFTP) bir dosya ÇEKTİ; içeriği bellekte doğruladı.
 *
 * Güvenlik (D-150/151/177 dersi): launcher tüm frame/rx tamponlarını + basılan
 * string'leri EL0 user yığınında (0x42000000 VA sayfası) tutar → sys2(24)/
 * sys2(25)/sys(5) user-VA guard'ından geçer. EL0 kernel .rodata'yı (AP=00)
 * DEREF EDEMEZ: RRQ string'leri ("dosya.txt", "octet") user tamponuna ELLE
 * yazılır (D-177); karşılaştırma sabiti de user yığınına elle kurulur.
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

/* Bizim UDP kaynak portu (RRQ src). SLIRP DATA'yı bu porta döner (dst). */
#define TFTP_SRC_PORT 0xC3A5U   /* 50085 — efemeral aralıkta sabit */

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

/* IPv4 başlık sağlaması (RFC 1071). always_inline → .user launcher'a gömülür. */
__attribute__((always_inline)) static inline uint16_t ip_checksum(const unsigned char *veri, int uzun) {
    uint32_t t = 0;
    int i;
    for (i = 0; i + 1 < uzun; i += 2) t += ((uint32_t)veri[i] << 8) | veri[i + 1];
    if (uzun & 1) t += (uint32_t)veri[uzun - 1] << 8;
    while (t >> 16) t = (t & 0xffff) + (t >> 16);
    return (uint16_t)(~t);
}

/*
 * EL0 launcher: TFTP GET protokolü syscall üstünde.
 * Tüm tamponlar user yığınında (0x42000000 sayfası) → user-VA guard geçer.
 * RRQ string'leri (.rodata YASAK — D-177) user tamponuna elle yazılır.
 */
__attribute__((section(".user"), noinline))
static void launcher(void) {
    const unsigned char bizim_mac[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
    /* SLIRP gateway MAC sabittir: 52:55:0a:00:02:02 (D-176 dersi — ARP'siz de
     * çalışır; unicast IPv4 hedefi 10.0.2.2 zaten gateway). */
    const unsigned char gw_mac[6] = { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x02 };
    unsigned char frame[128];
    unsigned char rx[600];
    int i;

    /* --- TFTP RRQ payload'unu user tamponuna ELLE kur (D-177: .rodata deref YOK) ---
     * TFTP RRQ: [opcode=1 (2B)] "dosya.txt\0" "octet\0"
     * "dosya.txt" = 9 karakter; mod "octet" = 5 karakter. */
    unsigned char tftp[32];
    int tp = 0;
    tftp[tp++] = 0x00; tftp[tp++] = 0x01;   /* opcode = 1 (RRQ) */
    /* "dosya.txt" — her byte aritmetik/sabit (string literal deref etmeyiz). */
    tftp[tp++] = 'd'; tftp[tp++] = 'o'; tftp[tp++] = 's'; tftp[tp++] = 'y';
    tftp[tp++] = 'a'; tftp[tp++] = '.'; tftp[tp++] = 't'; tftp[tp++] = 'x';
    tftp[tp++] = 't'; tftp[tp++] = 0x00;    /* dosya adı + NUL */
    tftp[tp++] = 'o'; tftp[tp++] = 'c'; tftp[tp++] = 't'; tftp[tp++] = 'e';
    tftp[tp++] = 't'; tftp[tp++] = 0x00;    /* mod "octet" + NUL */
    int tftp_len = tp;                       /* = 18 */

    /* --- RRQ çerçevesi (Eth + IPv4 + UDP + TFTP) --- */
    for (i = 0; i < 128; i++) frame[i] = 0;

    /* Ethernet (14): dst = gateway MAC (unicast), src = bizim MAC */
    for (i = 0; i < 6; i++) frame[i] = gw_mac[i];
    for (i = 0; i < 6; i++) frame[6 + i] = bizim_mac[i];
    frame[12] = 0x08; frame[13] = 0x00;                    /* ethertype = IPv4 */

    /* IPv4 (offset 14, 20 bayt) */
    int ip_total = 20 + 8 + tftp_len;
    frame[14] = 0x45;                                      /* v4, IHL=5 */
    frame[15] = 0x00;                                      /* TOS */
    frame[16] = (unsigned char)(ip_total >> 8); frame[17] = (unsigned char)ip_total;
    frame[18] = 0; frame[19] = 0;                          /* id */
    frame[20] = 0x00; frame[21] = 0;                       /* flags/frag = 0 */
    frame[22] = 64;                                        /* TTL */
    frame[23] = 17;                                        /* protocol = UDP */
    frame[24] = 0; frame[25] = 0;                          /* checksum (önce 0) */
    frame[26] = 10; frame[27] = 0; frame[28] = 2; frame[29] = 15;   /* src = 10.0.2.15 */
    frame[30] = 10; frame[31] = 0; frame[32] = 2; frame[33] = 2;    /* dst = 10.0.2.2 (SLIRP TFTP) */
    uint16_t ipsum = ip_checksum(&frame[14], 20);
    frame[24] = (unsigned char)(ipsum >> 8); frame[25] = (unsigned char)ipsum;

    /* UDP (offset 34, 8 bayt) */
    int udp_len = 8 + tftp_len;
    frame[34] = (unsigned char)(TFTP_SRC_PORT >> 8);       /* src port (bizim, efemeral) */
    frame[35] = (unsigned char)(TFTP_SRC_PORT & 0xff);
    frame[36] = 0x00; frame[37] = 69;                      /* dst port = 69 (TFTP) */
    frame[38] = (unsigned char)(udp_len >> 8); frame[39] = (unsigned char)udp_len;
    frame[40] = 0; frame[41] = 0;                          /* UDP checksum = 0 (opsiyonel) */

    /* TFTP payload (offset 42) */
    for (i = 0; i < tftp_len; i++) frame[42 + i] = tftp[i];

    int toplam = 42 + tftp_len;                            /* = 60 */
    if (toplam < 60) toplam = 60;

    /* (1) SYSCALL ile TFTP RRQ'yu yolla. */
    long g = (long)sys2(24, (unsigned long)(uintptr_t)frame, (unsigned long)toplam);
    if (g < 0) {
        sys(5, (unsigned long)(uintptr_t)"USERTFTP GONDER HATA");
        sys(7, 0);
        for (;;) { }
    }

    /* --- Beklenen içerik "KEMGU-TFTP-DATA" (15 bayt) user tamponunda kurulur --- */
    unsigned char beklenen[16];
    int bp = 0;
    beklenen[bp++] = 'K'; beklenen[bp++] = 'E'; beklenen[bp++] = 'M'; beklenen[bp++] = 'G';
    beklenen[bp++] = 'U'; beklenen[bp++] = '-'; beklenen[bp++] = 'T'; beklenen[bp++] = 'F';
    beklenen[bp++] = 'T'; beklenen[bp++] = 'P'; beklenen[bp++] = '-'; beklenen[bp++] = 'D';
    beklenen[bp++] = 'A'; beklenen[bp++] = 'T'; beklenen[bp++] = 'A';
    int beklenen_len = bp;                                  /* = 15 */

    /* (2) SYSCALL ile TFTP DATA'yı al (poll — kısa per-çağrı timeout, EL0 döngüsü).
     *
     * KRİTİK (SLIRP dosya-transferi): SLIRP TFTP DATA'yı 10.0.2.15'e (BİZE) yollarken
     * önce ARP ile bizim MAC'imizi sorar ("who has 10.0.2.15?"). Bu ARP isteğine
     * YANIT vermezsek SLIRP DATA'yı iletemez → transfer takılır. Bu yüzden poll
     * döngüsünde gelen ARP request'i (tpa=10.0.2.15) yakalar + ARP reply yollarız
     * (bizim MAC'i SLIRP'e öğretir). UDP/DHCP tek-yönlüydü; TFTP çift-yönlü. */
    int data_ok = 0;
    int veri_len = 0;
    unsigned char veri[512];
    uint16_t tid = 0;                                      /* SLIRP DATA kaynak portu (TID) */
    for (int t = 0; t < 300 && !data_ok; t++) {
        long n = (long)sys2(25, (unsigned long)(uintptr_t)rx, 600);
        if (n < 42) continue;                              /* en az eth+ip+udp veya eth+arp */

        /* --- Gelen ARP request'e yanıt ver (SLIRP bizim MAC'i öğrenmeli) --- */
        if (rx[12] == 0x08 && rx[13] == 0x06 &&            /* ethertype ARP */
            rx[20] == 0x00 && rx[21] == 0x01 &&            /* oper = request */
            rx[38] == 10 && rx[39] == 0 &&                 /* tpa = 10.0.2.15 (BİZ) */
            rx[40] == 2 && rx[41] == 15) {
            unsigned char arp[64];
            for (i = 0; i < 64; i++) arp[i] = 0;
            /* Eth: dst = istekçinin MAC'i (rx sha), src = bizim MAC */
            for (i = 0; i < 6; i++) arp[i] = rx[22 + i];    /* dst = sender hw addr */
            for (i = 0; i < 6; i++) arp[6 + i] = bizim_mac[i];
            arp[12] = 0x08; arp[13] = 0x06;                 /* ethertype ARP */
            arp[14] = 0x00; arp[15] = 0x01;                 /* htype ethernet */
            arp[16] = 0x08; arp[17] = 0x00;                 /* ptype IPv4 */
            arp[18] = 6; arp[19] = 4;                       /* hlen, plen */
            arp[20] = 0x00; arp[21] = 0x02;                 /* oper = reply */
            for (i = 0; i < 6; i++) arp[22 + i] = bizim_mac[i];   /* sha = bizim MAC */
            arp[28] = 10; arp[29] = 0; arp[30] = 2; arp[31] = 15; /* spa = 10.0.2.15 (BİZ) */
            for (i = 0; i < 6; i++) arp[32 + i] = rx[22 + i];    /* tha = istekçi */
            for (i = 0; i < 4; i++) arp[38 + i] = rx[28 + i];    /* tpa = istekçi spa */
            int at = 42;
            if (at < 60) at = 60;
            (void)sys2(24, (unsigned long)(uintptr_t)arp, (unsigned long)at);
            continue;                                       /* ARP işlendi, DATA'yı bekle */
        }

        if (n < 42 + 4) continue;                          /* eth+ip+udp + tftp opcode/block */
        /* IPv4 + UDP kontrolü */
        if (!(rx[12] == 0x08 && rx[13] == 0x00 && rx[23] == 17)) continue;
        /* Kaynak IP = 10.0.2.2 (SLIRP TFTP sunucusu) */
        if (!(rx[26] == 10 && rx[27] == 0 && rx[28] == 2 && rx[29] == 2)) continue;
        /* UDP dst port = bizim RRQ src port (TID takibi) */
        uint16_t dstp = ((uint16_t)rx[36] << 8) | rx[37];
        if (dstp != TFTP_SRC_PORT) continue;
        /* TFTP başlangıcı offset 42: opcode (2B) */
        int tf = 42;
        uint16_t opcode = ((uint16_t)rx[tf + 0] << 8) | rx[tf + 1];
        if (opcode == 5) {                                 /* TFTP ERROR — dosya yok vb. */
            sys(5, (unsigned long)(uintptr_t)"USERTFTP SUNUCU HATA");
            sys(7, 0);
            for (;;) { }
        }
        if (opcode != 3) continue;                         /* DATA (opcode 3) şart */
        uint16_t block = ((uint16_t)rx[tf + 2] << 8) | rx[tf + 3];
        if (block != 1) continue;                          /* ilk blok */
        /* SLIRP'in DATA kaynak portu = transfer TID (ACK bu porta gider) */
        tid = ((uint16_t)rx[34] << 8) | rx[35];
        /* DATA payload: offset 42+4 .. n */
        veri_len = (int)n - (tf + 4);
        if (veri_len < 0) veri_len = 0;
        if (veri_len > 512) veri_len = 512;
        for (i = 0; i < veri_len; i++) veri[i] = rx[tf + 4 + i];
        data_ok = 1;
    }

    if (!data_ok) {
        sys(5, (unsigned long)(uintptr_t)"USERTFTP DATA YOK");
        sys(7, 0);
        for (;;) { }
    }

    /* (2b) TFTP ACK (opcode 4, block 1) yolla — TID'e (SLIRP efemeral portu).
     * Zorunlu değil (tek-blok transfer içeriği zaten aldık) ama protokol-doğru. */
    {
        unsigned char af[64];
        for (i = 0; i < 64; i++) af[i] = 0;
        for (i = 0; i < 6; i++) af[i] = gw_mac[i];
        for (i = 0; i < 6; i++) af[6 + i] = bizim_mac[i];
        af[12] = 0x08; af[13] = 0x00;
        int ack_tftp = 4;                                  /* opcode(2) + block(2) */
        int ait = 20 + 8 + ack_tftp;
        af[14] = 0x45; af[15] = 0x00;
        af[16] = (unsigned char)(ait >> 8); af[17] = (unsigned char)ait;
        af[22] = 64; af[23] = 17;
        af[26] = 10; af[27] = 0; af[28] = 2; af[29] = 15;
        af[30] = 10; af[31] = 0; af[32] = 2; af[33] = 2;
        uint16_t asum = ip_checksum(&af[14], 20);
        af[24] = (unsigned char)(asum >> 8); af[25] = (unsigned char)asum;
        int aul = 8 + ack_tftp;
        af[34] = (unsigned char)(TFTP_SRC_PORT >> 8);      /* src port = bizim (aynı TID) */
        af[35] = (unsigned char)(TFTP_SRC_PORT & 0xff);
        af[36] = (unsigned char)(tid >> 8); af[37] = (unsigned char)(tid & 0xff);  /* dst = SLIRP TID */
        af[38] = (unsigned char)(aul >> 8); af[39] = (unsigned char)aul;
        af[42] = 0x00; af[43] = 0x04;                      /* opcode = 4 (ACK) */
        af[44] = 0x00; af[45] = 0x01;                      /* block = 1 */
        int atop = 46;
        if (atop < 60) atop = 60;
        (void)sys2(24, (unsigned long)(uintptr_t)af, (unsigned long)atop);
    }

    /* (3) Çekilen içeriği doğrula: "KEMGU-TFTP-DATA" ile byte-byte karşılaştır. */
    int eslesti = (veri_len == beklenen_len);
    for (i = 0; i < beklenen_len && eslesti; i++) {
        if (veri[i] != beklenen[i]) eslesti = 0;
    }

    if (!eslesti) {
        sys(5, (unsigned long)(uintptr_t)"USERTFTP ICERIK UYUMSUZ");
        sys(7, 0);
        for (;;) { }
    }

    /* Çekilen içeriği bas (user tamponundan — NUL-sonlandırılmış). */
    {
        unsigned char ic[520];
        for (i = 0; i < veri_len && i < 519; i++) ic[i] = veri[i];
        ic[i] = 0;
        sys(5, (unsigned long)(uintptr_t)"USERTFTP ICERIK=");
        sys(5, (unsigned long)(uintptr_t)ic);
        sys(7, 0);
    }

    sys(5, (unsigned long)(uintptr_t)"USERTFTP OK");
    sys(7, 0);
    for (;;) { }
}

int main(void) {
    kdl_yazdir_metin("USERTFTP BASLA");
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
