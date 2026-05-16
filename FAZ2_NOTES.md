# Faz 2 — Altyapı Bootstrap: Yetki-gated Dosya I/O

**Tarih:** 2026-05-15
**Branch:** `feature/altyapi-bootstrap`
**Önceki:** Faz 1 (test_modul_import 17/17, commit `1e1d59d`)
**Bu Faz Çıktı:** 22/22 yeni test, ASan temiz, sıfır regression

---

## Hedef

Capability Spec V1 (`yetki<R>`) + Linear V1 (`tip_lineer_mi`) + runtime'ın
hazır yetki-gated dosya primitifleri (`kdl_dosya_ac_yetkili` vs.)
birleştirilerek **derleme-zamanı capability-disiplinli dosya I/O** sağlamak.

Direktif §B Hedef 1 (Kırılamaz Güvenlik): her syscall capability token
gerektirir; ambient authority + confused-deputy + handle leak compile-time
yasak.

---

## Karar Defteri (Mehmet onaylı, 2026-05-15)

### Karar 1 — Stdlib `dosya.kem` ezme stratejisi: (a) Eklemeli

Mevcut V1 metin-handle API (`ac/oku_metin/yaz_metin/kapat/var_mi/sil/...`)
**KORUNDU**. Capability variant'ları aynı dosyaya **eklemeli** — geriye
uyumlu, mevcut `test/stdlib/test_dosya.kem` `--check`'ten geçer.

**Eklenen bölüm** (`stdlib/dosya.kem` sonu):
- İzin bit-field sabitleri (`IZIN_OKU=1, IZIN_YAZ=2, IZIN_OKU_YAZ=3,
  IZIN_CALISTIR=4, IZIN_SIL=8, IZIN_DEVRET=16`) — `runtime/kdl_runtime.c`
  `KDL_IZIN_*` ile birebir
- IOHata kodları (`IO_OK=0, IO_DOSYA_YOK=1, IO_ERISIM_REDDEDILDI=2,
  IO_GIRIS_CIKIS=3, IO_KAYNAK_TUKENDI=4, IO_YETKI_GECERSIZ=5, IO_BOS_YOL=6`)
- Dokümantasyon: built-in çağrılar nasıl kullanılır

**Stdlib wrapper işlevleri YAZILMADI** çünkü Linear V1'de `işlev fn(y: yetki<Dosya>)`
parametresi y'yi by-value alır → caller'da y tüketilmiş olarak işaretlenir.
Bu CP-IO semantiği (y tüketilmez) ile çelişir. **V1 pragmatik:** kullanıcı
doğrudan built-in çağırır; wrapper işlevler V2'ye saklı (ref-based
parametre + linear referans istisnası gerekir — Linear V1 §LR-2 sınırı).

### Karar 2 — Handle tipi: Direkt `yetki<Dosya>` (tekkez sarmalama YOK)

Capability Spec V1 CP.1.1 (Linear Integration) zaten `yetki<R>`'yi linear
olarak takip eder. **`tekkez<yetki<Dosya>>` çift sarmalama gereksiz** —
`tip_lineer_mi` doğrudan `TIP_YETKI` için 1 döner.

### Karar 3 — IOHata gösterimi: `tam32` hata kodu + sabitler

KEMGU'da enum tipi yok. Pragmatik V1: `tam32` + global sabitler. Kullanıcı
`eğer sonuc != IO_OK { ... }` ile kontrol eder. V2'de sonuç enum (variant
ile) önerilebilir.

### Karar 4 — Branch ismi

Mevcut `feature/altyapi-bootstrap` korunur (Faz 1 zinciri burada). Direktif
`feature/altyapi-bootstrap-faz2` istemişti — bilgi notu, manuel rename
Mehmet tercihi.

---

## Implementasyon Özeti

### `src/tip_kontrol.c` ekleme (~150 satır)

`geri_al` handler'ı sonrasına, `görev_başlat` öncesine 4 built-in handler:

| Built-in çağrı | Arity | Donüş | Linear etki | Hata kodu |
|----------------|-------|-------|-------------|-----------|
| `dosya_ac_yetkili(yol: metin, izin: tam16)` | 2 | `yetki<Dosya>` | üretici | CP004 |
| `dosya_oku_yetkili(y: yetki<Dosya>)` | 1 | `metin` | CP-IO (tüketmez, kontrol eder) | CP004 / CP005 |
| `dosya_yaz_yetkili(y: yetki<Dosya>, icerik: metin)` | 2 | `tam32` | CP-IO (tüketmez, kontrol eder) | CP004 / CP005 |
| `dosya_kapat_yetkili(y: yetki<Dosya>)` | 1 | `bos` | `geri_al` semantik (tüketir) | CP004 |

**Yeni helper:** `lineer_kullanim_kontrolu(tk, d)` — `lineer_tuketildi >= 1`
ise hata, artırmadan kontrol. CP-IO için kritik (`dosya_oku_yetkili(y)` y
zaten kapatılmışsa CP005 verir).

### `stdlib/dosya.kem` ekleme

Capability section: sabit + dokümantasyon. Helper işlev yok (Linear param
sorunu).

### `test/test_dosya.c` (22 test, F1-F22)

| Grup | Kapsam | Test |
|------|--------|------|
| F1-F6 | Pozitif round-trip + IZIN sabit + inference | 6 |
| F7-F12 | Linear tüketim (L001/L002/L004) | 6 |
| F13-F16 | Arity + tip kontrol (CP004) | 4 |
| F17-F20 | Capability kaynak tipi + delege + geri_al | 4 |
| F21-F22 | Koşullu (V1 KNOWN-LIMIT) + LR-2 yapı yasak | 2 |

**Toplam:** 22/22 ASan temiz.

### `Makefile`
- `$(BUILD)/test_dosya$(EXE)` target eklendi (Clang64 + ASan, paralel diğer
  spec test'leriyle aynı pattern)
- `calistir_dosya_test` hedef + `test_tumu` listesine eklendi
- Faz 1'in `calistir_modul_import_test`'i de `test_tumu`'na eklendi
  (önceki commit'te eklenmemiş bulgu)

---

## Parking Lot — V2'ye saklı sınırlar

### S1 — Stdlib wrapper işlevleri (Linear param sorunu)

```kemgu
işlev oku_yetkili_metin(y: yetki<Dosya>) -> metin {
    ver dosya_oku_yetkili(y);
}
```

Bu işlev `y`'yi parametre olarak alır → caller'da `y` tüketilir → CP-IO
(y tüketilmez) ile çelişir. **V2 gerek:** referans parametre + linear
referans istisnası (Linear V1 LR-2 sınırı genişletme), veya `ödünç ver`
(borrow) ABI'si.

**Workaround V1:** kullanıcı doğrudan built-in `dosya_oku_yetkili(y)`
çağırır.

### S2 — L-COND enforcement eksik (F21 V1 KNOWN-LIMIT)

Linear Spec V1 B.3 L-COND: "İki dallı koşulda her dal aynı bağlamayı
tüketmeli (ya ikisi ya hiçbiri). Aksi L005 LINEAR_COND_INCONSISTENT."

Mevcut implementasyon `DUGUM_EGER` dalları için snapshot/merge yapmaz —
flag tek yönlü artar; her iki dal `lineer_tuket_eger_baglamaysa` çağırırsa
ikinci dal CP005 alır.

**V2 gerek:** `tip_kontrol_deyim` `DUGUM_EGER`'de scope snapshot al, her dal
sonrası merge et. Tutarlılık ihlali L005, tutarlıysa OK.

### S3 — Runtime entegrasyon testi yok

V1 test'leri tip-kontrol seviyesinde. Runtime fopen/fclose entegrasyonu
`test_runtime_link.c`'de zaten doğrulanıyor ama `kdl_dosya_ac_yetkili`
end-to-end (KEMGU kaynak → LLVM IR → clang link → çalıştırma) test yok.

**V2 gerek:** `test/ornekler/dosya_yetkili.kem` + `test/test_dosya_e2e.c`
(`kemgu --llvm | clang | run` zinciri).

### S4 — Sınır kontrolü: izin alt-küme runtime (compile-time delege)

`dosya_ac_yetkili(yol, IZIN_OKU)` çağrısında, kullanıcı `IZIN_OKU` yerine
literal `1` veya değişken kullanabilir. Runtime `kdl_dosya_ac_yetkili`
`fopen` mod string'i seçer; compile-time'da izin literal kontrolü yok.

**V2 gerek:** izin literal ise compile-time `dosya_ac_yetkili(yol, izin: literal)`
mod string'ini doğrula (yazma izni var ama dosya read-only filesystem...
runtime sorumluluğunda kalır).

### S5 — Cross-platform bare-metal guard

Direktif "Bare-metal hedefte derleme dışı: `#if defined(KEMGU_BARE_METAL)`
guard" istedi. `runtime/kdl_runtime.c` libc fopen/fread/fwrite kullanır —
bare-metal hedefte mevcut değil. Şu an guard YOK; Faz 5 (bare-metal)
geldiğinde `kdl_dosya_*` etrafına ifdef wrap edilir.

**V1 davranışı:** Bare-metal mevcut faz dışı — sorun yok.

### S6 — IOHata mapping (runtime → KEMGU sabit)

Runtime `kdl_dosya_ac_yetkili` başarısızsa `id=0` döndürür (errno
kaybedildi). Kullanıcı `yetki_id(y) == 0` kontrolü yapar ama hatanın
**hangi** olduğu (DosyaYok mu, ErişimReddedildi mi?) bilmiyor.

**V2 gerek:** Runtime `kdl_dosya_son_hata() -> tam32` global errno
köprüsü, KEMGU `IO_*` sabitlerine eşle. Veya yetki struct'a `hata`
alanı.

---

## Direktif §B Karar Tablosu

| Direktif §B Hedef 1 maddesi | Faz 2 Karşılık |
|------------------------------|----------------|
| Anonim FFI YOK — her syscall yetki gerektirir | ✓ `dosya_ac_yetkili` yetki<Dosya> döner; oku/yaz/kapat hep yetki ister |
| Lineer Dosya — implicit close yok | ✓ `yetki<Dosya>` linear (tip_lineer_mi); scope sonunda L001/CP005 |
| Bare-metal guard | ⏳ Faz 5 (parking lot S5) |
| IOHata varyantlar (DosyaYok/ErişimReddedildi/...) | ✓ tam32 sabitler (Karar 3); runtime mapping V2 (parking lot S6) |

---

## KIRMIZI_QUEUE durumu

**Madde G (stdlib::dosya syscall altyapısı):** Faz 2 ile **çözüldü** —
tip-kontrol built-in pariteliği eklendi. Runtime hâlâ libc wrap (G'de
istenen POSIX `open/close/read/write` syscall API'si değil; pragmatik
V1 fopen yeterli). Madde tarihsel olarak kalır; gelecek bare-metal Faz 5
direkt syscall'a geçer.

**Yeni KIRMIZI girişi YOK** — bu Faz spec-içi (Direktif §B Hedef 1 + onaylı
Capability/Linear V1 kapsamında).

---

## Test Disiplini

- **22/22 test** geçti, ASan temiz
- **Regression:** test_linear 57/57, test_capability 40/40, test_drf 39/39,
  test_modul_import 17/17, test_sabitsure 39/39, test_wcet 32/32,
  test_simd 30/30, stdlib --check yeşil
- **make test_tumu:** "Tum testler gecti!"

---

## Faz 2 Sonraki Adımlar

(V2 — bu commit'in dışı)

1. **L-COND enforcement** (parking S2) — `DUGUM_EGER` dal snapshot/merge
2. **Runtime e2e test** (parking S3) — `test_dosya_e2e.c` + örnek
3. **Stdlib wrapper'ları** (parking S1) — borrow ABI veya linear referans
4. **IOHata runtime mapping** (parking S6) — errno köprüsü
5. **Bare-metal guard** (parking S5) — Faz 5'te
6. **POSIX direkt syscall** (KIRMIZI G evrim) — fopen yerine open/close

---

**END FAZ2_NOTES.md**
