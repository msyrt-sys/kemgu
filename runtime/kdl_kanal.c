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
    volatile int64_t buf[KDL_KANAL_KAP];   /* D-295: host ile AYNI genislik */
    volatile int bas;   /* okuma (tüketici) indeksi */
    volatile int son;   /* yazma (üretici) indeksi */
};

static struct KdlKanal kdl_kanal_havuz[KDL_KANAL_HAVUZ];
static int kdl_kanal_sayi = 0;

KdlKanal *kdl_kanal_olustur(int kapasite) {
    /* Halka derleme-zamani sabit (en cok KAP-1 ogo). Istenen kapasite bunu
     * asiyorsa SESSIZCE kirpmak yerine BASARISIZ don — cagiran, istedigi
     * kapasiteyle calistigini sanmasin. kapasite<=0 -> sabit halka. */
    if (kapasite > KDL_KANAL_KAP - 1) return 0;
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

void kdl_kanal_gonder(KdlKanal *k, int64_t deger) {
    while (kdl_kanal_dolu(k)) { __asm__ volatile("" ::: "memory"); }
    k->buf[k->son] = deger;
    k->son = (k->son + 1) % KDL_KANAL_KAP;
}

int64_t kdl_kanal_al(KdlKanal *k) {
    while (kdl_kanal_bos(k)) { __asm__ volatile("" ::: "memory"); }
    int64_t v = k->buf[k->bas];
    k->bas = (k->bas + 1) % KDL_KANAL_KAP;
    return v;
}
