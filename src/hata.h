#ifndef KEMGU_HATA_H
#define KEMGU_HATA_H

/*
 * KEMGU Hata Raporlama
 *
 * Format:
 *   hata[LXXX]: mesaj
 *     --> dosya:satir:sutun
 *      |
 *   NN | kaynak satırı
 *      |        ^ işaretçi
 *      |
 *      = ipucu: ipucu metni (varsa)
 *
 * LSP entegrasyonu icin opsiyonel callback. Set edilirse stderr'e yazmaz,
 * sadece callback'i cagirir (LSP diagnostic toplama).
 */

typedef void (*HataCallback)(
    int satir, int sutun,
    const char *kod, const char *mesaj, const char *ipucu,
    void *ctx);

void hata_callback_ayarla(HataCallback cb, void *ctx);

void hata_raporla(
    const char *dosya_adi,
    const char *kaynak,
    int satir,
    int sutun,
    const char *kod,
    const char *mesaj,
    const char *ipucu
);

#endif /* KEMGU_HATA_H */
