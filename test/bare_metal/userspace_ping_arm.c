/*
 * USERSPACE ICMP PING (aarch64) — EL0 süreç yalnız SYSCALL ile L3 protokol yapar.
 *
 * Bu, iki milestone'u BİRLEŞTİRİR: userspace ağ syscall'ları (D-176: net_gonder/net_al)
 * + tam ICMP echo (ping) round-trip mantığı (Faz G `icmp_arm.c` — ama orada KERNEL/EL1
 * kodundaydı). Burada mantık EL0'a (yetkisiz süreç) taşınır: EL0 süreç virtio-net'e
 * DOĞRUDAN dokunmaz — ARP çözümü, IPv4+ICMP Echo Request inşası, checksum ve yanıt
 * doğrulaması TAMAMEN userspace'te, çekirdek yalnız iki syscall ile aracılık eder:
 *   num=24 net_gonder(cerceve, uzun)  — kernel frame'i OKUR (user VA doğrulanır) + yollar
 *   num=25 net_al(buf, maxlen)        — kernel frame'i user buffer'a YAZAR (user VA doğrulanır)
 *
 * Senaryo (SLIRP ağ geçidi 10.0.2.2 — internetsiz DETERMİNİSTİK, ICMP echo'ya dahili yanıt):
 *   (a) EL0: ARP isteği ile gateway MAC'ini çöz (net_gonder + net_al poll). ARP çözülmezse
 *       SLIRP'in bilinen sabit gateway MAC'i (52:55:0a:00:02:02) fallback olarak kullanılır.
 *   (b) EL0: Ethernet + IPv4(proto=1) + ICMP Echo Request (type=8, id, seq, checksum,
 *       payload "KEMGU") inşa et → net_gonder ile yolla.
 *   (c) EL0: net_al poll ile echo reply al → doğrula (IPv4 proto=1, ICMP type=0/reply,
 *       id/seq eşleşir, "KEMGU" payload geri döner).
 *   Reply geldiyse: sys(5,"USERPING OK") + sys(7). main önce "USERPING BASLA" basar.
 *
 * Kanıt: bir userspace program, yalnız çekirdek-aracılı ağ syscall'larıyla tam bir L3
 * (ICMP) ping round-trip'i yaptı — protokol mantığı çekirdekte DEĞİL, EL0'da.
 *
 * ICMP checksum: pseudo-header YOK — sadece ICMP başlığı + veri üzerinde RFC1071
 * (icmp_arm.c ile aynı). IPv4 başlık checksum'ı da RFC1071.
 *
 * FALLBACK: SLIRP echo reply gelmezse, pcap filter-dump'ta gönderilen ICMP Echo
 * Request'in "KEMGU" payload'u grep ile aranır → "USERPING SENT OK" (TX kanıtı).
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

static uint64_t l1_p[512] __attribute__((aligned(4096)));
static uint64_t l2_p[512] __attribute__((aligned(4096)));
static unsigned char kstack_p[8192] __attribute__((aligned(16)));

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

/* RFC1071 internet checksum — hem IPv4 başlığı hem ICMP mesajı için (icmp_arm.c ile aynı).
 * EL0'da tamamen self-contained (kernel'e checksum syscall'ı YOK). */
__attribute__((section(".user"), noinline))
static uint16_t ip_checksum(const unsigned char *veri, int uzun) {
    uint32_t t = 0;
    for (int i = 0; i + 1 < uzun; i += 2) t += ((uint32_t)veri[i] << 8) | veri[i + 1];
    if (uzun & 1) t += (uint32_t)veri[uzun - 1] << 8;
    while (t >> 16) t = (t & 0xffff) + (t >> 16);
    return (uint16_t)(~t);
}

/* EL0 süreç: ARP → ICMP Echo Request → Echo Reply doğrula — hepsi syscall ile. */
__attribute__((section(".user"), noinline))
static void launcher(void) {
    const unsigned char bizim_mac[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
    /* SLIRP gateway'in bilinen sabit MAC'i — ARP çözülemezse fallback. */
    unsigned char gw_mac[6] = { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x02 };
    unsigned char frame[128];
    unsigned char rx[128];
    int i;

    /* --- (a) ARP: gateway 10.0.2.2 MAC'ini syscall ile çöz --- */
    for (i = 0; i < 128; i++) frame[i] = 0;
    for (i = 0; i < 6; i++) frame[i] = 0xff;               /* dst broadcast */
    for (i = 0; i < 6; i++) frame[6 + i] = bizim_mac[i];   /* src mac */
    frame[12] = 0x08; frame[13] = 0x06;                    /* ethertype ARP */
    frame[14] = 0x00; frame[15] = 0x01;                    /* htype ethernet */
    frame[16] = 0x08; frame[17] = 0x00;                    /* ptype IPv4 */
    frame[18] = 6; frame[19] = 4;                          /* hlen, plen */
    frame[20] = 0x00; frame[21] = 0x01;                    /* oper = request */
    for (i = 0; i < 6; i++) frame[22 + i] = bizim_mac[i];  /* sha */
    frame[28] = 10; frame[29] = 0; frame[30] = 2; frame[31] = 15;  /* spa 10.0.2.15 */
    frame[38] = 10; frame[39] = 0; frame[40] = 2; frame[41] = 2;   /* tpa 10.0.2.2 gateway */
    sys2(24, (unsigned long)(uintptr_t)frame, 42);

    int arp_ok = 0;
    for (int deneme = 0; deneme < 200 && !arp_ok; deneme++) {
        long n = (long)sys2(25, (unsigned long)(uintptr_t)rx, 128);
        if (n < 42) continue;
        if (rx[12] != 0x08 || rx[13] != 0x06) continue;    /* ethertype ARP */
        if (rx[20] != 0x00 || rx[21] != 0x02) continue;    /* oper = reply */
        if (rx[28] == 10 && rx[29] == 0 && rx[30] == 2 && rx[31] == 2) {
            for (i = 0; i < 6; i++) gw_mac[i] = rx[22 + i]; /* sha = gateway MAC */
            arp_ok = 1;
        }
    }
    /* arp_ok=0 olsa bile sabit gw_mac ile devam (SLIRP MAC deterministik). */

    /* --- (b) ICMP Echo Request inşa et: header(8) + payload(5) = 13 bayt --- */
    /* ICMP: type(1) code(1) checksum(2) id(2) seq(2) + payload "KEMGU". */
    const unsigned char payload[5] = { 'K', 'E', 'M', 'G', 'U' };
    uint16_t icmp_id  = 0xBEEF;
    uint16_t icmp_seq = 0x0001;
    int icmp_len = 8 + 5;                                  /* 8 header + 5 payload = 13 */
    unsigned char icmp[13];
    for (i = 0; i < icmp_len; i++) icmp[i] = 0;
    icmp[0] = 8;                                           /* type = 8 (echo request) */
    icmp[1] = 0;                                           /* code = 0 */
    icmp[2] = 0; icmp[3] = 0;                              /* checksum = 0 (hesap öncesi) */
    icmp[4] = (unsigned char)(icmp_id >> 8);  icmp[5] = (unsigned char)icmp_id;
    icmp[6] = (unsigned char)(icmp_seq >> 8); icmp[7] = (unsigned char)icmp_seq;
    for (i = 0; i < 5; i++) icmp[8 + i] = payload[i];
    uint16_t ics = ip_checksum(icmp, icmp_len);           /* pseudo-header YOK — sade */
    icmp[2] = (unsigned char)(ics >> 8); icmp[3] = (unsigned char)ics;

    /* --- Ethernet + IPv4 çerçevesini kur --- */
    for (i = 0; i < 128; i++) frame[i] = 0;
    for (i = 0; i < 6; i++) frame[i] = gw_mac[i];          /* dst = gateway MAC */
    for (i = 0; i < 6; i++) frame[6 + i] = bizim_mac[i];   /* src */
    frame[12] = 0x08; frame[13] = 0x00;                    /* ethertype = IPv4 */
    int ip_total = 20 + icmp_len;
    frame[14] = 0x45;                                      /* ver=4, IHL=5 */
    frame[16] = (unsigned char)(ip_total >> 8); frame[17] = (unsigned char)ip_total;
    frame[20] = 0x40;                                      /* flags = DF */
    frame[22] = 64;                                        /* TTL */
    frame[23] = 1;                                         /* proto = 1 (ICMP) */
    frame[26] = 10; frame[27] = 0; frame[28] = 2; frame[29] = 15;  /* src = 10.0.2.15 */
    frame[30] = 10; frame[31] = 0; frame[32] = 2; frame[33] = 2;   /* dst = 10.0.2.2 */
    uint16_t ihs = ip_checksum(&frame[14], 20);
    frame[24] = (unsigned char)(ihs >> 8); frame[25] = (unsigned char)ihs;
    for (i = 0; i < icmp_len; i++) frame[34 + i] = icmp[i]; /* ICMP mesajı IP başlığından sonra */
    int toplam = 34 + icmp_len;

    /* --- Echo Request'i SYSCALL ile yolla --- */
    long g = (long)sys2(24, (unsigned long)(uintptr_t)frame, (unsigned long)toplam);
    if (g < 0) { sys(5, (unsigned long)(uintptr_t)"USERPING GONDER HATA"); sys(7, 0); for (;;) {} }

    /* --- (c) ICMP Echo Reply'i SYSCALL ile al + doğrula --- */
    int ping_ok = 0;
    for (int deneme = 0; deneme < 400 && !ping_ok; deneme++) {
        long n = (long)sys2(25, (unsigned long)(uintptr_t)rx, 128);
        if (n < 42) continue;
        if (rx[12] != 0x08 || rx[13] != 0x00) continue;    /* IPv4 mı */
        if (rx[23] != 1) continue;                         /* proto = ICMP */
        if (!(rx[26] == 10 && rx[27] == 0 && rx[28] == 2 && rx[29] == 2)) continue; /* src = gateway */
        int ihl = (rx[14] & 0x0f) * 4;                     /* IP başlık uzunluğu */
        int io = 14 + ihl;                                 /* ICMP başlangıç offset'i */
        if (n < io + 8) continue;
        if (rx[io] != 0 || rx[io + 1] != 0) continue;      /* type = 0 reply, code = 0 */
        uint16_t r_id  = ((uint16_t)rx[io + 4] << 8) | rx[io + 5];
        uint16_t r_seq = ((uint16_t)rx[io + 6] << 8) | rx[io + 7];
        if (r_id != icmp_id || r_seq != icmp_seq) continue; /* id/seq eşleşmeli */
        /* payload geri döndü mü ("KEMGU") */
        int pl_ok = 1;
        if (n < io + icmp_len) pl_ok = 0;
        for (i = 0; pl_ok && i < 5; i++)
            if (rx[io + 8 + i] != payload[i]) pl_ok = 0;
        if (!pl_ok) continue;
        ping_ok = 1;
    }

    if (ping_ok) {
        sys(5, (unsigned long)(uintptr_t)"USERPING OK");
    } else {
        /* TX kanıtı gönderildi (pcap'te "KEMGU" grep'lenir) → Makefile fallback. */
        sys(5, (unsigned long)(uintptr_t)"USERPING YANIT YOK");
    }
    sys(7, 0);
    for (;;) { }
}

int main(void) {
    kdl_yazdir_metin("USERPING BASLA");
    kdl_yazdir_satir();

    /* Kernel (EL1) net sürücüsünü kurar — EL0 süreç yalnız syscall kullanır. */
    uint64_t nb = kdl_virtio_net_bul();
    if (!nb || kdl_virtio_net_kur(nb) != 0) {
        kdl_yazdir_metin("NET KUR HATA"); kdl_yazdir_satir();
        for (;;) { __asm__ volatile("wfe"); }
    }

    kdl_surec_kur_el0_veri(l1_p, l2_p, 0x42000000UL, 0x44000000UL);
    kdl_preempt_baslat();
    int tp = kdl_preempt_gorev_olustur_el0(launcher, kstack_p + sizeof(kstack_p),
                                           (void *)(uintptr_t)0x42380000UL);
    kdl_preempt_gorev_ttbr(tp, l1_p);
    kdl_kesme_kur();
    kdl_timer_baslat();
    kdl_preempt_ac();

    for (;;) { __asm__ volatile("wfe"); }
}
