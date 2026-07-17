/*
 * KEMGU Bare-Metal Kanal (SPSC mesaj kanalı) — kdl_kanal.h
 * ========================================================
 * KEMGU `kanal` ilkelinin çekirdek-düzeyi gerçeklemesi. Tek-üretici /
 * tek-tüketici (SPSC) halka tampon. Üretici dolu kanala, tüketici boş kanala
 * preemptive scheduler (C7b) altında bloklanır (çift yönlü akış denetimi):
 * timer-IRQ karşı göreve geçer → karşı taraf ilerler → koşul eninde sonunda
 * çözülür (tek çekirdekte kilitlenme yok). Gönüllü yield GEREKTİRMEZ.
 *
 * KEMGU bağı: R-KANAL bölge aksiyomu + `görev`/`kanal` keyword'leri (DRF V1) —
 * dilin eşzamanlılık ilkesinin gerçek çekirdek karşılığı.
 */
#ifndef KDL_KANAL_H
#define KDL_KANAL_H

typedef struct KdlKanal KdlKanal;

/* Havuzdan yeni kanal ayır (havuz tükendiyse 0 döner).
 *
 * `kapasite`: istenen kanal kapasitesi. Halka DERLEME-ZAMANI sabit boyutludur
 * (bkz. kdl_kanal.c: KDL_KANAL_KAP); istenen kapasite bunu aşarsa 0 döner —
 * SESSİZCE KIRPILMAZ (çağıran, istemediği bir kapasiteyle çalıştığını
 * sanmasın). kapasite <= 0 → sabit halka kullanılır.
 *
 * İmza, host sürümüyle (kdl_runtime.c: kdl_kanal_olustur(int32_t)) BİLEREK
 * AYNI: codegen `kanal_oluştur(n)` için tek bir @kdl_kanal_olustur(i32) çağırır
 * ve bu çağrı host'ta da bare-metal'de de aynı ABI'yle bağlanır. Eskiden bu
 * sürüm (void) idi → bare-metal hedefte kapasite argümanı sessizce yutulurdu
 * (latent ABI tuzağı). */
KdlKanal *kdl_kanal_olustur(int kapasite);

/* Değer gönder — kanal doluysa preemption altında bekle (akış denetimi). */
void kdl_kanal_gonder(KdlKanal *k, int deger);

/* Değer al — kanal boşsa preemption altında bekle. FIFO sıra korunur. */
int kdl_kanal_al(KdlKanal *k);

#endif /* KDL_KANAL_H */
