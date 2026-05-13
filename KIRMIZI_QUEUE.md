# Kırmızı Queue — KEMGU

Direktif Ek v1.1 Bölüm A uyarınca: spec dışı 🔴 Kırmızı (tip sistemine yeni
katman, formal teorem etkisi, breaking change, yeni anahtar kelime, yeni
unsafe primitif, ABI değişikliği, concurrency modeli değişikliği) kararlar
buraya eklenir. Mehmet haftalık spec oturumunda toplu temizler.

Format:
```
## [tarih] — başlık
- Kategori: <yeni keyword | tip katmanı | teorem | breaking | unsafe | ABI | concurrency>
- Bağlam: <neden bu noktada gündeme geldi>
- Önerilen seçenekler:
  1. ...
  2. ...
- Engellediği iş: <ya da yok>
```

---

## (Şu anda kuyruk boş.)

Linear Types Spec V1 onaylı olduğu için tekkez/imha keyword eklemesi,
TIP_TEKKEZ kategori, L001–LC001 hata kodları, region/linear entegrasyonu
ve closure-itself-linear **spec içi** sayılır → otomatik onaylı, queue'ya
eklenmez.

---

## [2026-05-13] — Dizi<T> stack vs heap dualizmi

- Kategori: tip katmanı + ABI
- Bağlam: ADIM B (Kırmızı queue v1, runtime-primitifler oturumu) dinamik
  dizi allocator getirdi. KEMGU'da `Dizi<T>` şu an iki farklı şeyi
  kapsıyor:
  1. Stack literal: `[1, 2, 3]` — `alloca [N x T]`, ptr-as-buffer
  2. Heap dinamik: `dizi_olustur(N)` — `KdlDizi*` struct (veri/boyut/kapasite)
- Sorun: `[i]` indeksleme heap variantinde çalışmıyor (struct.veri'ye
  ulaşamaz). v1'de `dizi_al(d, i)` explicit. Bu iki form aynı tipte
  görünür ama farklı LLVM temsiline sahip.
- Önerilen seçenekler:
  1. Heap variant için ayrı tip: `DinDizi<T>` veya `Vektor<T>`
  2. `Dizi<T>` her zaman heap-allocated (stack literal'ı da heap'e taşı —
     performans kaybı, GC-less zinciri kırılır)
  3. v1 status quo: aynı tip, [i] sadece stack literal'da çalışsın
- Engellediği iş: Daha kullanışlı stdlib (Liste, Sözlük vs runtime tipleri)

## [2026-05-13] — dizi_olustur generic return-type inference

- Kategori: tip katmanı
- Bağlam: `dizi_olustur(N) -> Dizi<T>` çağrısı T'yi sadece beklenen
  tipten alabilir; arg listesinde T yok. v1'de eleman_byte sabit 4
  (tam32) — diğer tipler için yanlış byte_size.
- Önerilen seçenekler:
  1. Bidirectional inference T'yi beklenen `Dizi<T>` 'den çek (D ile
     birlikte çözülecek — Generic callback inference)
  2. Explicit type arg syntax: `dizi_olustur::<tam64>(100)`
  3. Per-type variants: `dizi_olustur_tam32`, `dizi_olustur_tam64`
- Engellediği iş: Çok-tipli stdlib (tam8/16/64 element diziler).
