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
 */
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
