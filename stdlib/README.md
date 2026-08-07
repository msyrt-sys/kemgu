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

### Veri yapıları ve algoritmalar (saf, uçtan uca doğrulanmış)

Hepsi saf KEMGU; `--check` + sentinel `main` sürücüsüyle
(`--llvm | clang | çalıştır -> exit 42`) uçtan uca ölçülmüştür.
Ortak sözleşmeler: koleksiyon = `Dizi<T>` + disiplinli API (sarmalayıcı
yapı yok); eleman SİLME primitifi olmadığı için küçülten işlemler yeni
dizi döner; çok-değerli dönüş olmadığı için "oku (tepe/on), sonra
küçült (cek/cikar)" iki adımı; yokluk `seçimlik<T>`.

- **`yigin.kem`** — LIFO yığın: `yigin_it`, `yigin_tepe`, `yigin_cek`,
  `yigin_bos_mu`, `yigin_boyut`, `yigin_temizle` + somut kurucular
- **`kuyruk.kem`** — FIFO kuyruk: `kuyruk_ekle`, `kuyruk_on`,
  `kuyruk_cikar`, `kuyruk_bos_mu`, `kuyruk_boyut` + somut kurucular
- **`obek.kem`** — ikili min-öbek (priority queue): `obek_kur` (O(n)
  heapify), `obek_ekle`, `obek_cek`, `obek_tepe`, `obek_gecerli_mi`
- **`arama.kem`** — `dogrusal_ara`, `ikili_ara`, `ilk_uygun`,
  `son_uygun`, `alt_sinir`, `ust_sinir` (bulunamama `seçimlik<tam32>`;
  sınırlar konum döner — C++ lower/upper_bound eşleniği)
- **`sirali.kem`** — `sirali_konum`, `sirali_ekle`, `birlestir_sirali`
  (kararlı merge), `hizli_sirala` (Lomuto quicksort — dizi.kem
  `sirala`yı tamamlar, yerine geçmez)
- **`eslem.kem`** — paralel-dizi anahtar->değer eşlemi:
  `eslem_bul`, `eslem_bul_ya_da`, `eslem_koy` (yerinde),
  `eslem_sil_anahtar`/`eslem_sil_deger`, `eslem_gecerli_mi`
- **`bit.kem`** — tamamen aritmetik bit işlemleri (alan: x >= 0,
  indeks 0..30): `bit_al/kur/temizle/cevir`, `bit_say`, `onde_sifir`,
  `guc_iki_mi`, `sola_dondur`/`saga_dondur` (açık genişlik parametreli)
- **`istatistik.kem`** — tam32 betimleyici istatistik: `ist_ortalama`,
  `ist_medyan` (girdiyi bozmaz), `ist_aralik`, `ist_varyans`,
  `ist_std_sapma`, `ist_en_sik` (kırpılma noktaları belgeli)
- **`zaman.kem`** — saf takvim aritmetiği (saat okumaz): `artik_yil_mi`,
  `ay_gun_sayisi`, `tarih_gecerli_mi`, `gun_numarasi` (Hinnant
  days_from_civil, epoch 1970-01-01), `gun_farki`, `haftanin_gunu`
  (0=Pazartesi), `yilin_gunu`; alan yıl >= 1
- **`matris.kem`** — düz-bellek (row-major) tam32 matris: `mat_al/yaz`,
  `mat_olustur`, `mat_birim`, `mat_topla`, `mat_skaler_carp`,
  `mat_carp`, `mat_transpoze`, `mat_iz`, `mat_gecerli_mi`
- **`kesir.kem`** — kesin rasyonel aritmetik (`[pay, payda]` çifti,
  kurucuda normalize; geçersizlik NaN-tarzı `[0,0]` zehir değeriyle
  yayılır — çökme yok, sessiz yanlış sayı yok): `kesir_olustur`,
  `kesir_topla/cikar/carp/bol`, `kesir_neg/ters`, `kesir_esit_mi`,
  `kesir_kucuk_mu`, `kesir_tam_kisim`, `kesir_gecerli_mi`
- **`rastgele.kem`** — deterministik MINSTD PRNG (Park-Miller, Schrage
  çarpanlamasıyla taşmasız; KRİPTO DEĞİL — o iş `kripto/rastgele.kem`):
  `rastgele_tohumla`, `rastgele_sonraki`, `rastgele_aralikta`,
  `rastgele_dizi`, `rastgele_karistir` (Fisher-Yates); global durum
  yok, durum parametreyle iplenir
- **`karma.kem`** — taşmasız polinom karma (Rabin-Karp ailesi, mod
  1e9+7; FNV/djb2 bilinçli değil — taşan çarpma + XOR ister):
  `karma_tam`, `karma_dizi_tam`, `karma_birlestir`, `karma_kova`
  (eslem karma-tablo V2 köprüsü), `karma_modcarp`, `karma_normalize`

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
| yigin | ✓ tam | Yok |
| kuyruk | ✓ tam | Yok |
| obek | ✓ tam | Yok |
| arama | ✓ tam | Yok |
| sirali | ✓ tam | Yok |
| eslem | ✓ tam | Yok (karma tablo V2: hash primitifi) |
| bit | ✓ tam | Yok (kaydırma op gelirse sadeleşir) |
| istatistik | ✓ tam | Yok (kesirli sürüm: float codegen) |
| zaman | ✓ tam | Yok (ters dönüşüm V2: yapı-dönüş) |
| matris | ✓ tam | Yok (Matris yapısı V2) |
| kesir | ✓ tam | Yok (tam64 iç aritmetik V2) |
| rastgele | ✓ tam | Yok (bias'sız aralık V2) |
| karma | ✓ tam | Yok (karma_metin: karakter erişimi) |
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
