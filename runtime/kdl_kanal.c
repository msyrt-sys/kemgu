/*
 * KEMGU Bare-Metal Kanal — SPSC halka tampon (kdl_kanal.c)
 * =======================================================
 * Tek-çekirdek + preemptive scheduler bağlamında tek-üretici / tek-tüketici
 * (SPSC) mesaj kanalı. İndeksler `volatile` — preemption sınırında (timer-IRQ
 * ile görev değişimi) görünürlük tek çekirdekte program-sırasıyla garanti.
 * (SMP DEĞİL → bellek-bariyeri gerekmez; çok-çekirdekte burada DMB eklenir.)
 *
 * Bloklama = busy-wait: dolu/boş iken döngüde bekle. Preemptive scheduler (C7b)
 * timer-IRQ'da diğer göreve geçirir → karşı taraf ilerler → koşul çözülür.
 * Tek slot rezerve edilir (klasik halka): en çok KAP-1 öğe tutulur → `dolu`/
 * `boş` ayrımı indeks eşitliğiyle yapılır.
 *
 * Düşük kapasite (KAP=4) bilinçli: üretici sık dolu-bloklar, tüketici sık
 * boş-bloklar → ÇİFT YÖNLÜ akış denetimi (flow control / ping-pong) gerçekten
 * sınanır. (D-119 not: küçük KAP ile gözlenen bozulma, IRQ vektör stub'unun
 * preempt edilen görevin x0'ını ezmesiydi — D-121'de kök-neden onarıldı, cap=4
 * artık sağlam.)
 */
#include <stdint.h>

#include "kdl_kanal.h"

#define KDL_KANAL_KAP    4   /* halka kapasitesi (en çok KAP-1=3 öğe dolu) */
#define KDL_KANAL_HAVUZ  4   /* eşzamanlı kanal sayısı */

struct KdlKanal {
    volatile int buf[KDL_KANAL_KAP];
    volatile int bas;   /* okuma (tüketici) indeksi */
    volatile int son;   /* yazma (üretici) indeksi */
};

static struct KdlKanal kdl_kanal_havuz[KDL_KANAL_HAVUZ];
static int kdl_kanal_sayi = 0;

KdlKanal *kdl_kanal_olustur(void) {
    if (kdl_kanal_sayi >= KDL_KANAL_HAVUZ) return 0;
    struct KdlKanal *k = &kdl_kanal_havuz[kdl_kanal_sayi++];
    k->bas = 0;
    k->son = 0;
    return k;
}

static int kdl_kanal_dolu(struct KdlKanal *k) {
    return ((k->son + 1) % KDL_KANAL_KAP) == k->bas;
}

static int kdl_kanal_bos(struct KdlKanal *k) {
    return k->son == k->bas;
}

void kdl_kanal_gonder(KdlKanal *k, int deger) {
    while (kdl_kanal_dolu(k)) { __asm__ volatile("" ::: "memory"); }
    k->buf[k->son] = deger;
    k->son = (k->son + 1) % KDL_KANAL_KAP;
}

int kdl_kanal_al(KdlKanal *k) {
    while (kdl_kanal_bos(k)) { __asm__ volatile("" ::: "memory"); }
    int v = k->buf[k->bas];
    k->bas = (k->bas + 1) % KDL_KANAL_KAP;
    return v;
}
