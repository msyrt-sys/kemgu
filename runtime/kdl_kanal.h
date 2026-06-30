/*
 * KEMGU Bare-Metal Kanal (SPSC mesaj kanalı) — kdl_kanal.h
 * ========================================================
 * KEMGU `kanal` ilkelinin çekirdek-düzeyi gerçeklemesi. Tek-üretici /
 * tek-tüketici (SPSC) halka tampon. Tüketici boş kanala (alım bekleme),
 * üretici dolu kanala preemptive scheduler (C7b) altında bloklanır: timer-IRQ
 * karşı göreve geçer → karşı taraf ilerler → koşul eninde sonunda çözülür (tek
 * çekirdekte kilitlenme yok). Gönüllü yield GEREKTİRMEZ (cooperative değil).
 *
 * KEMGU bağı: R-KANAL bölge aksiyomu + `görev`/`kanal` keyword'leri (DRF V1) —
 * dilin eşzamanlılık ilkesinin gerçek çekirdek karşılığı.
 */
#ifndef KDL_KANAL_H
#define KDL_KANAL_H

typedef struct KdlKanal KdlKanal;

/* Havuzdan yeni kanal ayır (havuz tükendiyse 0 döner). */
KdlKanal *kdl_kanal_olustur(void);

/* Değer gönder — kanal doluysa preemption altında bekle (akış denetimi). */
void kdl_kanal_gonder(KdlKanal *k, int deger);

/* Değer al — kanal boşsa preemption altında bekle. FIFO sıra korunur. */
int kdl_kanal_al(KdlKanal *k);

#endif /* KDL_KANAL_H */
