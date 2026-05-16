/*
 * KEMGU Bare-Metal Runtime — kdl_yazdir_* Konsol Cikti Arayuzu
 * ============================================================
 *
 * Host runtime (runtime/kdl_runtime.c) ile AYNI isimleri verir
 * (kdl_yazdir_metin, kdl_yazdir_tam, vb.) — fakat altyapida libc
 * yerine UART surucusu cagrir. Final binary'de sadece BIR taraf
 * link edilir:
 *
 *   host:        runtime/kdl_runtime.c     (libc fputs/printf yolu)
 *   bare-metal:  runtime/kdl_runtime_yazdir_bare.c + UART surucusu
 *
 * LLVM IR `declare` deklarasyonlari her iki dunyada esit kalir.
 */

#ifndef KDL_YAZDIR_BARE_H
#define KDL_YAZDIR_BARE_H

#include <stdint.h>

void kdl_yazdir_metin(const char *s);
void kdl_yazdir_satir(void);
void kdl_yaz_metin(const char *s);
void kdl_yazdir_tam(int32_t n);
void kdl_yazdir_tam64(int64_t n);
void kdl_yaz_tam(int32_t n);
void kdl_yazdir_mantiksal(int b);

/* Continuation C1: isaretsiz + onaltilik formatlar */
void kdl_yazdir_isaretsiz_tam(uint32_t n);
void kdl_yazdir_isaretsiz_tam64(uint64_t n);
void kdl_yazdir_onaltilik(uint64_t n);
void kdl_yaz_onaltilik(uint64_t n);

/* Ic-icin kullanim (test dogrulamasi). Stack tampona biciminin
 * uretilen byte sayisini doner; NUL ile sonlanir. */
int32_t kdl_format_tam64(int64_t n, char *cikti, int32_t kapasite);
int32_t kdl_format_isaretsiz64(uint64_t n, char *cikti, int32_t kapasite);
int32_t kdl_format_onaltilik64(uint64_t n, char *cikti, int32_t kapasite);

#endif
