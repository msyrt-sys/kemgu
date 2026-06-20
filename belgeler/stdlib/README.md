# KEMGU Standart Kütüphane — Belgeler

Bu dizin, `stdlib/` altındaki saf-KEMGU modüllerinin işlev-bazlı
belgelerini içerir. Her belge, ilgili `.kem` kaynağındaki **her** üst düzey
işlev için imza + ne yaptığı + örnek + kenar durum verir.

> Not: KEMGU'da henüz içe-aktarma (import) yoktur — modüller tek-dosya
> derlenir. `make calistir_stdlib_check`, her kütüphane modülünü karşılık
> gelen `test/stdlib/test_<modul>.kem` ile birleştirip `--check`'ten geçirir.

## Modüller

| Belge | Kaynak | İçerik |
|-------|--------|--------|
| [dizi.md](dizi.md) | `stdlib/dizi.kem` | `Dizi<T>` sorgu/erişim/arama, sıralama, fold, harita/filtre, küme işlemleri (kesisim/birlesim/fark), konum/sayım |
| [metin.md](metin.md) | `stdlib/metin.kem` | Metin uzunluk/birleştir/kes, böl/birleştir-liste, arama (indeks/say/bul_tum), değiştir, sınıflama (sadece_rakam/harf), UTF-8 codepoint farkındalığı |
| [matematik.md](matematik.md) | `stdlib/temel/matematik.kem` | mutlak/min/maks/kare/küp/işaret, kuvvet/kök/mod, asal/fibonacci/faktöriyel, dizi OBEB/EKOK, modüler üs, kombinatorik, sayı palindromu |
| [sayisal.md](sayisal.md) | `stdlib/temel/sayisal.kem` | ortalama, üs, OBE (GCD), EKOK (LCM) |
| [karsilastir.md](karsilastir.md) | `stdlib/temel/karsilastir.kem` | eşitlik/karşılaştırma yardımcıları, sıralı_mı, dizi karşılaştırma |
| [opsiyonel.md](opsiyonel.md) | `stdlib/opsiyonel.kem` | `seçimlik<T>` üzerinde harita/çöz/varsayılan vb. |
| [sonuc.md](sonuc.md) | `stdlib/sonuc.kem` | `sonuç<T,H>` üzerinde harita/çöz, seçimlik ↔ sonuç dönüşümleri |
| [dosya.md](dosya.md) | `stdlib/dosya.kem` | Dosya aç/oku/yaz/kapat sarmalayıcıları |
| [kripto.md](kripto.md) | `stdlib/kripto.kem` + `stdlib/kripto/*.kem` | Karma, şifre, rastgele, anahtar (bundle) |

## Doğrulama deseni

Yeni stdlib işlevleri, kütüphane + sürücü birleştirilip
`kemgu --llvm | clang | run` ile çalıştırılarak doğrulanır; sürücü `main`'i
tüm kontroller geçerse `42` döndürür. Fonksiyon-dönüşü diziler **çağıran
scope'ta indekslenmez** — `uzunluk`/`toplam` gibi bir işlevin parametresine
geçirilerek skalere indirgenir (codegen F4 sınırı).
