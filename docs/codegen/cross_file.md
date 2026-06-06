# Cross-file Sembol Çözümü (`kullan`) — Codegen Modeli

Bu belge, KEMGU codegen'inin (`src/llvm.c`) `kullan` ile import edilen
sembolleri (sabit + fonksiyon + yapı) nasıl çözdüğünü açıklar.

## Derleme modeli: SINGLE-MODULE / INLINE

KEMGU şu an **tek modül** derler: `kemgu --llvm A.kem`, A'nın `kullan`'larını
**çözüp tüm sembolleri tek bir LLVM modülüne inline eder**. Ayrı `.o` + link
zamanı çözüm YOKTUR (bu bilinçli; bare-metal / WCET-dostu, link-zamanı
sürprizi yok). Çok-dosyalı kontrol için bazı yerlerde **bundle/concat**
(`cat a.kem b.kem | kemgu`) de kullanılır (örn. `stdlib/kripto`).

## `kullan` pre-pass (fixed-point — C2.6)

`llvm_ir_uret` başında, AST üyeleri bir **worklist** ile genişletilir:

1. Worklist orijinal program üyeleriyle başlatılır.
2. Bir `DUGUM_KULLAN` görülünce: `a::b::c` yolu `a/b/c.kem`'e çevrilir, dosya
   yüklenip parse edilir, ve üyeleri **worklist'in sonuna** eklenir.
3. Non-`kullan` üyeler çıktı listesine gider; çıktı `program`'ın üye listesine
   yazılır.
4. Sonraki pre-pass'ler (yapı kayıt, **işlev imza kayıt**, sabit kayıt) ve emit
   döngüsü, **import edilenler dahil** tüm üyeleri işler → fonksiyon tanımları
   aynı modülde emit edilir, çağrılar ada göre çözülür.

### Transitif import (A ← B ← C)

İmport edilen dosyanın **kendi `kullan`'ları da worklist'e eklendiği için**
işlenir. Yani `transitif kullan lib_islem`, `lib_islem kullan lib_sayi` →
`lib_sayi` de yüklenir. (C2.6 öncesi pre-pass tek geçişliydi; transitif
bağımlılıklar `undefined @<fn>` veriyordu.)

### Dedup + terminasyon

`YuklenmisDosya` listesi her dosyayı **bir kez** yükler:
- **Diamond** (A→{B,C}→D): D bir kez yüklenir (duplicate `define` yok).
- **Cycle** (A↔B): worklist + dedup ile sonlanır (sonsuz döngü yok).

## Sabit vs Fonksiyon

| Sembol     | Çözüm                                                      |
|------------|------------------------------------------------------------|
| `sabit`    | `SabitKayit` tablosu + referans yerinde **inline** edilir.  |
| `işlev`    | Pre-pass tanımı aynı modüle dahil eder; **çağrı ada göre** çözülür (`define @<fn>` emit edilir). |
| `yapı`     | `YapiKayit` + `%Ad = type {...}` emisyonu.                  |

C2.5'in tagged-union ABI'si (`sonuç`/`seçimlik` → `{i8, ...}` by-value)
cross-file çağrılarda korunur — import edilen `sonuç`-dönüşlü fonksiyon aynı
aggregate imzayla emit + çağrılır.

## Bilinen sınırlamalar

- **`kullan`'sız çağrı çözülmez:** Bir dosya, import etmediği bir dosyadaki
  fonksiyonu çağırırsa (örn. `drivers/virtio/mock_transport.kem` →
  `virtio_feature_kabul_maskesi`, ki o `features.kem`'de), o sembol modülde
  yoktur → `undefined`. Çözüm: çağıran dosyaya uygun `kullan` eklenir **veya**
  bundle derlenir. (Bu codegen boşluğu değil; eksik import.)
- **Yol çözümü CWD'ye görelidir** (`a/b/c.kem`, importing dosyaya göre değil).
  Çağrı dizininden çözülür.

## Kaynak / test

`src/llvm.c`: `llvm_ir_uret` `kullan` pre-pass (worklist fixed-point).
Testler: `test/crossfile/transitif.kem` (transitif), `sonuc_cagri.kem`
(sonuç-dönüşlü cross-file, C2.5 ABI).
