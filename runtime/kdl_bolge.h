/*
 * KEMGU Bölge (Region) Arena Allokatörü — Runtime Arabirimi (kdl_bolge.h)
 * =======================================================================
 *
 * V2 F4 FAZ 0 (D-099). Bölge tabanlı bellek modelinin (KEMGU_Bellek_Modeli.md,
 * Katman 1) runtime tabanı: bir BÖLGE = bir ARENA (malloc'lu blok-listesi +
 * bump pointer). Tahsisler O(1) bump; bölge kapanışında TÜM bloklar tek seferde
 * serbest (O(blok)). GC yok — deterministik serbest.
 *
 * Bu faz SADECE runtime'dır: codegen/checker/IR DEĞİŞMEZ; bu fonksiyonları
 * henüz kimse çağırmaz (F4.1'de lambda env + dizi/metin tahsisi buraya bağlanır;
 * F4.2'de region-passing ABI — bölge `ptr` parametresi olarak geçer).
 *
 *   KdlBolge *b = kdl_bolge_olustur();     // bölge aç
 *   void *p = kdl_bolge_ayir(b, n);        // n bayt (16-hizalı bump)
 *   kdl_bolge_serbest(b);                  // tüm blokları + handle'ı free
 *
 * Handle OPAK (`ptr`): F4.2 region-passing'de gizli bölge parametresi olarak
 * `ptr` taşınır — alan düzeni codegen'e sızmaz.
 *
 * NOT (bare-metal): host'ta malloc/free üstüne kuruludur. KEMGU_BARE_METAL
 * altında ileride kdl page-allocator'a bağlanacak (kdl_bolge.c TODO) — F4.0
 * host hedefini bloklamaz.
 *
 * İPLİK (thread): F4.0 tek-thread host. Sızıntı-tanığı sayaçları atomik DEĞİL;
 * bölge-sahipliği/concurrency Katman 2 işi (gerekince atomik'e taşınır).
 */
#ifndef KDL_BOLGE_H
#define KDL_BOLGE_H

#include <stdint.h>

/* Opak bölge handle'ı (tanım kdl_bolge.c'de). F4.2'de `ptr` param. */
typedef struct KdlBolge KdlBolge;

/* Yeni bölge aç: handle + ilk blok (64 KB) malloc'lanır. Bellek yoksa NULL. */
KdlBolge *kdl_bolge_olustur(void);

/* Bölgeden n bayt ayır (16-bayt hizalı bump-tahsis). Aktif blokta yer yoksa
 * yeni blok malloc'lanır (boyut = max(64KB, n) → oversized'a adanmış blok).
 * Hizalı işaretçi döner; bellek/taşma hatasında NULL. Bölge serbest edilene
 * kadar geçerli — TEK TEK serbest EDİLMEZ (bölge kapanışında topluca). */
void *kdl_bolge_ayir(KdlBolge *b, uint64_t n);

/* Bölgenin TÜM bloklarını + handle'ı free et (O(blok)). NULL güvenli. */
void kdl_bolge_serbest(KdlBolge *b);

/* === Sızıntı-tanığı (T2/T3 runtime kontrolü — Windows'ta LSan yok) === */
extern uint64_t kdl_bolge_olustur_sayisi;   /* kdl_bolge_olustur çağrı sayısı */
extern uint64_t kdl_bolge_serbest_sayisi;   /* kdl_bolge_serbest çağrı sayısı */

/* oluştur − serbest. 0 = tüm bölgeler serbest (sızıntı yok). Test/teşhis. */
int kdl_bolge_bakiye(void);

/* Bölgedeki blok adedi (teşhis/test — büyüme doğrulaması). NULL → 0. */
int kdl_bolge_blok_sayisi(const KdlBolge *b);

#endif /* KDL_BOLGE_H */
