# KEMGU Standart Kütüphane

**Saf KEMGU.** Hiç bir runtime/FFI bağımlılığı yok. Her şey monomorphization
ile somut tipe derlenir.

## Felsefe

Diğer dillerin tuzaklarından kaçınma:

| Tuzak | Kaçınılan Pattern | KEMGU yaklaşımı |
|-------|--------------------|------------------|
| Java `equals`/`hashCode` | Method-zorunluluğu + heap object | Fonksiyonel `esit_mi<T>(a, b)` |
| C `int` taşması | Sessiz bozulma | Tipe göre kontrollü (`tam8`/`tam64`) |
| Python sayısal muğlaklık | Runtime check | Compile-time tip |
| Go yıllar boyunca generic yok | API patlaması | Gün 1'den generic |
| C++ STL karmaşası | Çoklu inheritance, iter chains | Düz fonksiyonlar |
| Rust `'a` lifetime yükü | Programcı annotation | Sıfır annotation |

## Mevcut Modüller

### `stdlib/temel/`

- **`matematik.kem`** — `mutlak`, `en_kucuk`, `en_buyuk`, `kare`, `kup`,
  `sinirla`, `isaret`, `kuvvet`, `kok_tam`, `mod`, `asal_mi`, `fibonacci`,
  `faktoriyel`, `cift_mi`, `tek_mi`, `negatif_mi`, `pozitif_mi`,
  `sifir_mi`, `iki_kat`, `yarisi`
- **`karsilastir.kem`** — `esit_mi`, `farkli_mi`, `karsilastir`,
  `en_kucuk_uc`, `en_buyuk_uc`
- **`sayisal.kem`** — `ortalama`, `us`, `obe` (GCD), `ekok` (LCM)

### `stdlib/` (yeşil katman, --check geçer)

- **`dizi.kem`** — `Dizi<T>` üzerinde generic yardımcılar
  - Generic: `uzunluk`, `bos_mu`, `dolu_mu`, `ilk`, `son`, `al`,
    `icerir`, `bul`, `indeks_bul`, `ters_cevir`, `sirala`, `esit_mi`
  - Concrete tam32: `indirgeme_tam` (fold), `toplam_tam`, `carpim_tam`,
    `min_tam`, `maks_tam`, `say_tam`, `tum_mu_tam`, `bir_mu_tam`,
    `harita_yerinde_tam`
  - SINIR: harita<T,U>/filtre/dilimle dinamik allocator gerek
    (KIRMIZI_QUEUE B)
- **`opsiyonel.kem`** — `seçimlik<T>` yardımcıları
  - Generic: `var_mi`, `yok_mu`, `bos_mu`, `ya_da_varsayilan`,
    `ilk_var`, `her_iki_var`, `esit_mi`, `sarmala`
  - Concrete (tam32, metin): `harita_*`, `filtre_*`, `bagla_*`,
    `ya_da_cagir_*`
- **`sonuc.kem`** — `sonuç<T,E>` + `KSonuc<T,E>` paralel API
  - Built-in wrapper: `tamam_yap`, `hata_yap`
  - KSonuc<T,E> (parser fix gelene kadar): `k_tamam`, `k_hata`,
    `k_basarili_mi`, `k_basarisiz_mi`, `k_ya_da`, `k_hata_ya_da`,
    `k_secimlik`, `k_hata_secimlik`, `k_alternatif`, `k_esit_mi`,
    `k_harita_tam`, `k_harita_hata_tam`, `k_bagla_tam`, `k_dene_tam`
  - SINIR: built-in sonuc pattern (KIRMIZI_QUEUE C)
- **`metin.kem`** — String manipülasyon (büyük kısım stub)
  - Çalışır: `esit_mi`, `farkli_mi`, `bos_mu`, `dolu_mu`, `sarmala`
  - Stub (runtime gerek): `uzunluk`, `birlestir`, `birlestir_uc`,
    `kes`, `bol`, `kucukharf`, `buyukharf`, `icerir`, `baslar_ile`,
    `biter_ile`, `kirp`, `yer_degistir`, `tekrarla`, `yansit`,
    `karakter_sayisi`
  - SINIR: runtime string primitif gerek (KIRMIZI_QUEUE A)
- **`dosya.kem`** — Dosya I/O API skeleton (tüm body stub)
  - `gecersiz_handle`, `handle_gecerli_mi`
  - `mod_okuma`, `mod_yazma`, `mod_ekleme` sabitleri
  - `ac`, `kapat`, `oku_metin`, `oku_satirlar`, `yaz_metin`, `ekle`,
    `ekle_satir`, `var_mi`, `sil`, `boyut`, `yeniden_adlandir`, `kopyala`
  - SINIR: syscall layer (KIRMIZI_QUEUE G)

## Kullanım

```kemgu
// İleride 'kullan' direktifi ile import edilebilir.
// Şu an: tek dosyada birleştirilerek derleniyor.

işlev main() -> tam32 {
    değişken x: tam32 = mutlak(0 - 42);   // -> 42
    değişken y: tam32 = en_buyuk(10, 32);  // -> 32
    ver x;
}
```

## Tip Kontrolünden Geçer

Tüm modüller `kemgu --check` ile doğrulanmıştır.
Hiçbir runtime gerektirmez — saf type-checked KEMGU.

## Roadmap

| Modül | Durum | Bağımlılık |
|-------|-------|------------|
| temel/matematik | ✓ tam | Yok |
| temel/karsilastir | ✓ tam | Yok |
| temel/sayisal | ✓ tam | Yok |
| dizi | ✓ kısmi | harita/filtre için allocator |
| opsiyonel | ✓ tam | Yok |
| sonuc | ◐ KSonuc | parser tamam/hata desen |
| metin | ◐ skeleton | runtime string primitif |
| dosya | ◐ skeleton | syscall layer |
| koleksiyon/Tablo | ⏳ | Allocator runtime |
| io/yazdir | ⏳ | Syscall layer |
| iş/Görev | ⏳ | Thread runtime |

Runtime gereksinimleri olmadan yazılabilecek modüller önce. Diğerleri
KEMGU runtime (allocator + syscall) eklendikten sonra gelecek.
Detay: `KIRMIZI_QUEUE.md` bölüm A–H.

## Test Yapısı

Her stdlib modülünün karşılık gelen test dosyası `test/stdlib/test_<modul>.kem`
altında. Makefile `calistir_stdlib_check` hedefi her modül için
kütüphane + test dosyasını birleştirip --check'ten geçirir (import yok,
tek dosya derleme şu an).

```bash
make calistir_stdlib_check
```
