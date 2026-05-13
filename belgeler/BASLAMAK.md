# KEMGU Geliştirici Onboarding'i

Bu belge KEMGU'da çalışmaya yeni başlayan geliştiriciler içindir. Hem ortak
olarak katkı vermek isteyenler hem de dilin iç işleyişini öğrenmek isteyenler
için.

---

## 1. Önce: Direktif Eki

KEMGU **kapalı bir proje** olarak başladı; geliştirme kuralları direktif
belgelerinde tutulur:

- [`KEMGU_Direktif_Ek_v1.1.md`](KEMGU_Direktif_Ek_v1.1.md) — Operasyonel kurallar
- (Ana direktif henüz repoda değil; Mehmet ile çalışırken referans alınır.)

**Direktif eki ne diyor — özetle:**

| Renk | Anlam | Bildirim |
|------|-------|----------|
| 🟢 Yeşil | Test/codegen/refactor/doc/bug fix/perf/spec içi genişletme | Direkt yap |
| 🟡 Sarı | Söz dizimine küçük ek, yeni stdlib, yeni flag, yeni intrinsic | Yap + raporda etiketle |
| 🔴 Kırmızı | Tip sistemine yeni katman, formal teorem etkisi, breaking, yeni keyword | Karar bekle |

Eğer ortak olarak geliştirmek istiyorsan ana direktife erişim Mehmet'e
sorulur. Tek başına forking yapacaksan en azından **ASLA listesini**
([`KEMGU_Direktif_Ek_v1.1.md`](KEMGU_Direktif_Ek_v1.1.md) Bölüm G) tanı:

- Null pointer (yerine `seçimlik<T>`)
- Exception (yerine `sonuç<T,H>`)
- Garbage collector (region tabanlı)
- Implicit type conversion
- `güvensiz` normalleştirme
- C macro tarzı tekstüel substitution
- Cryptic hata mesajı
- Performance regression
- Akademik saflık için ergonomi feda

Bu maddelere uyumsuz bir PR doğrudan reddedilir — ne kadar küçük olursa olsun.

---

## 2. İlk Saat: Repoyu Tanı

1. Repoyu klonla (HTTPS — TR ağında SSH çoğu zaman bloklu):
   ```bash
   git clone https://github.com/msyrt-sys/kemgu.git
   cd kemgu
   ```

2. [`README.md`](../README.md) — proje özet + kurulum + ilk derleyişin.

3. [`CLAUDE.md`](../CLAUDE.md) — geliştirici notları. Yapılan tüm adımların
   tarihçesi, mevcut durum, yapılacak listesi burada. Mehmet'in dahili
   referans dosyası — okuması ağır ama bağlam buradan gelir.

4. [`KILAVUZ.md`](KILAVUZ.md) — dil rehberi (KEMGU'yu *kullanmak* için).

5. [`MIMARI.md`](MIMARI.md) — derleyici iç yapısı (KEMGU'yu *geliştirmek* için).

6. [`KIRMIZI_QUEUE.md`](../KIRMIZI_QUEUE.md) — bekleyen büyük kararlar.

Bir öğleden sonra okuma süresi.

---

## 3. Derleme + Test

[`README.md`](../README.md) "Kurulum" bölümünü takip ettiğini varsayıyorum.
MSYS2'de UCRT64 + Clang64 hazır, PATH set:

```bash
export PATH=/c/msys64/clang64/bin:/c/msys64/ucrt64/bin:$PATH

mingw32-make                  # build/kemgu.exe
mingw32-make test_tumu        # tüm modül + e2e test
```

Tek bir modüle odaklıysan:

```bash
mingw32-make calistir_parser_test       # 90/90 — parser dokunduysan
mingw32-make calistir_tip_kontrol_test  # 97/97 — tip kontrol dokunduysan
mingw32-make calistir_linear_test       # 54/54 — lineer kuralları
mingw32-make calistir_arm64_test        # cross-compile sanity
```

---

## 4. Branch Disiplini

```
main                        ← korumalı; direkt commit yasak
  └── feature/<konu>        ← her özellik/değişiklik
  └── feature/<konu>-bugfix
  └── konsolidasyon         ← spec entegrasyonu (özel)
```

**Kurallar:**
- `main` üzerinde commit yasak — yalnız PR/merge ile değişir.
- Her yeni iş için `feature/<konu>` aç (örn. `feature/bit-operatorleri`).
- Branch ismi Türkçe veya açıklayıcı İngilizce — tutarlı ol.
- Bir feature branch'in kapsamı net olsun: küçük, atomik, gözden geçirilebilir.

> Mevcut feature branch'lerin örnekleri:
> `feature/runtime-primitifler`, `feature/sabitsure-qualifier`,
> `feature/stdlib-genisletme`, `feature/test-altyapi`.

### 4.1 Force-push ve hard reset

`main`'e veya paylaşılan feature branch'lere `force-push` ve
`reset --hard` **yasak**. Kendi branch'inde rebase yapacaksan bile önce
yedek branch al.

### 4.2 Worktree

Repo aynı anda birden fazla iş için kullanılırken `git worktree add`
tercih edilir. Claude Code da `.claude/worktrees/` altında bunları otomatik
yönetir.

---

## 5. Test Gereksinimi

**Her PR test eklemeli.** Yeni özellik yeni test demektir.

### 5.1 Hangi test nereye

| Değişiklik              | Test dosyası                       |
|-------------------------|------------------------------------|
| Lexer kuralı            | [`test/test_lexer.c`](../test/test_lexer.c) |
| Parser kuralı           | [`test/test_parser.c`](../test/test_parser.c) |
| Tip kontrol kuralı      | [`test/test_tip_kontrol.c`](../test/test_tip_kontrol.c) |
| Bölge / escape aksiyomu | [`test/test_bolge_atama.c`](../test/test_bolge_atama.c), [`test/test_escape.c`](../test/test_escape.c) |
| Linear kuralı           | [`test/test_linear.c`](../test/test_linear.c) |
| LLVM emisyonu           | [`test/test_llvm.c`](../test/test_llvm.c) (`.kem` örneği + exit code) |
| Stdlib modül            | [`test/stdlib/test_<modül>.kem`](../test/stdlib/) — `--check` ile |
| Snapshot regression     | [`test/snapshots/`](../test/snapshots/) — referansı güncelle |

### 5.2 Test format örneği (birim)

```c
TEST("toplama: tam32 + tam32 → tam32", {
    /* Arena ve sembol tablosu kur */
    Arena *a = arena_olustur();
    /* ... ifadeyi kur, tip kontrol et ... */
    ASSERT(t.kategori == TIP_TAM32);
    arena_serbest(a);
});
```

Çıktı: `[N] açıklama ... ✓`. Sonunda `=== X/Y test geçti ===` görmek istiyoruz.

### 5.3 ASan zorunluluğu

Bellek alan modüllerin testleri Clang64 + AddressSanitizer + UBSan ile
derlenir. Bir testin koşması sırasında:

```
ERROR: AddressSanitizer: heap-use-after-free / leak / ...
```

görürsen adım onaylanmaz. **Çözüm:** root cause bul, fix et, yeniden çalıştır.
ASan'ı bypass etme.

### 5.4 Test sayısı azalmamalı

`make test_tumu` çıktısındaki toplam test sayısı PR sonrası düşmemeli.
Eski test eskidiyse:
1. Yerine yenisini ekle.
2. Eskisini silmek için ayrı PR (review ile).

---

## 6. Commit Mesajı Kuralları

### 6.1 Dil

Türkçe veya İngilizce — tutarlı ol (bir branch içinde karıştırma).
Türkçe tercih edilir (Türkçe DNA).

### 6.2 Yapı

```
<konu>: <kısa özet>             # ilk satır ≤ 72 karakter

<gövde, opsiyonel>              # neden bu değişiklik, ne riski var
```

İyi örnekler (mevcut repo log'undan):
- `Stdlib genişletme: matematik+opsiyonel+dizi+metin+sonuc+dosya`
- `ADIM 33: runtime/kdl_runtime.c Makefile entegrasyonu`
- `ADIM 32: Snapshot test framework + parser fuzzer (10000 iter temiz)`
- `stdlib: dosya modulu iskeleti (tum stub, runtime syscall bekliyor)`

Kötü örnekler:
- `fix` — neyi düzelttiğin belli değil
- `WIP` — niye merge ediyorsun?
- `Update`, `Update 2` — anlamsız

### 6.3 İmza

Eğer Claude Code ile birlikte çalışıyorsan:
```
Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
```
İnsan-AI ortak çalışmasını açıkça işaretle.

### 6.4 Pre-commit hook'u

`pre-commit` ile derleme + lint çalıştırma planlı (henüz hooks dizini boş).
Hook eklenince `--no-verify` ile bypass **yasak**.

---

## 7. PR Akışı

> PR akışı henüz açık değil; aşağıdaki süreç hazırlık.

1. **Branch aç:** `git checkout -b feature/<konu>`.
2. **Çalış + test ekle + commit** (atomik, açıklayıcı).
3. **Lint:** sıfır uyarı (`-Wall -Wextra -Wpedantic`).
4. **Test:** `mingw32-make test_tumu` yeşil.
5. **Push:** `git push -u origin feature/<konu>`.
6. **PR aç:** GitHub web arayüzünden veya `gh pr create`.
7. **PR şablonu** (henüz yok — burada öneri):

   ```
   ## Özet
   - <bir cümle: ne değişti?>
   - <iki cümle: neden gerekti?>

   ## Test
   - <yeni testler / etkilenen testler>
   - `make test_tumu` çıktısı X/X.

   ## Renk (Direktif Ek)
   - 🟢/🟡/🔴 — neden.

   ## Bağlantılı
   - KIRMIZI_QUEUE.md (varsa)
   ```

8. **Code review** — Mehmet veya başka maintainer.
9. **Merge** — fast-forward veya squash; merge commit yok (linear history tercihi).

---

## 8. Türkçe Kimlik

KEMGU'nun kimliği Türkçedir. Bu *teknik kolaylıktan önce* gelir:

- Anahtar kelimeler Türkçe (`işlev`, `eğer`, `iken`, `eşleş`, `tekkez`, vb.).
- Kod içi yorumlar Türkçe.
- Değişken isimleri Türkçe (snake_case Türkçe).
- Yapı/enum isimleri PascalCase Türkçe (`DugumTipi`, `TipBilgisi`).
- Hata mesajları Türkçe.
- Test açıklamaları Türkçe.
- Commit mesajları Türkçe veya İngilizce (tercih Türkçe).
- Belge dosyaları Türkçe.

Karma dil önerisi yapma — örn. `if`/`return` alias'ları ya da
`int`/`bool` "kolaylık" tipleri **kabul edilmez**. KEMGU Türkçe'dir,
Türkçe kalır.

İngilizce gerektiğinde (örn. dosya isimleri — Windows/Linux UTF-8
patolojileri için): küçük harf ASCII (`arena.c`, `ast_yazdir.c`).

---

## 9. UTF-8 Türkçe Hex Escape (KRİTİK)

C kodunda Türkçe karakter içeren string literali yazarken:

```c
// YANLIŞ — \xc4\x9f'den sonra "er" geliyor, 'e' hex rakam → derleme uyarısı
"de\xc4\x9fer"

// DOĞRU — string concatenation ile ayır
"de\xc4\x9f" "er"
```

Sebep: C11 §6.4.4.4 — `\x` ardışık en uzun hex rakam dizisini yutar.
Ayrıntı: [`../CLAUDE.md`](../CLAUDE.md) → "Türkçe UTF-8 Dikkat Noktası".

`-Wall` aktifken `\x` overflow için uyarı çıkar — sıfır uyarı hedefi
seni otomatik koruyor.

---

## 10. Yardım Kaynakları

| Konu                                    | Kaynak                                        |
|-----------------------------------------|-----------------------------------------------|
| Dil kullanımı                           | [`KILAVUZ.md`](KILAVUZ.md)                    |
| Derleyici iç yapısı                     | [`MIMARI.md`](MIMARI.md)                      |
| Grammar (tam EBNF)                      | [`KEMGU_Grammar_EBNF.md`](KEMGU_Grammar_EBNF.md) |
| Lexer detayı                            | [`KEMGU_Lexer_Spesifikasyonu.md`](KEMGU_Lexer_Spesifikasyonu.md) |
| Bellek modeli formal                    | [`KEMGU_Bellek_Modeli.md`](KEMGU_Bellek_Modeli.md) |
| Linear types semantik                   | [`KEMGU_Linear_Types_Spec_V1.md`](KEMGU_Linear_Types_Spec_V1.md) |
| Operasyonel kurallar / ASLA listesi     | [`KEMGU_Direktif_Ek_v1.1.md`](KEMGU_Direktif_Ek_v1.1.md) |
| Mevcut durum + yapılacak listesi        | [`../CLAUDE.md`](../CLAUDE.md)                |
| Bekleyen büyük kararlar                 | [`../KIRMIZI_QUEUE.md`](../KIRMIZI_QUEUE.md)  |
| Katkı süreci                            | [`../CONTRIBUTING.md`](../CONTRIBUTING.md)    |
| Tutorial örnekler                       | [`../test/ornekler/`](../test/ornekler/) — `01_merhaba.kem`'den başla |

---

## 11. İlk Katkı Önerisi

Eğer kendine güveniyorsan, küçük başla:

1. **Yorum eksiği bulup tamamla** (örn. `src/escape.c`'de DFA mantığının
   bir bölümü). Doc PR'ı — kabul edilmesi en kolay.
2. **Yeni tutorial örneği ekle** — `test/ornekler/10_*.kem`. `--check`'ten
   geçmeli, anlatımlı yorum içermeli.
3. **Stdlib modülünde eksik fonksiyon doldur** — `stdlib/dizi.kem`'deki
   "SINIR" notları için runtime gerekmeyen ekleme yap.
4. **Test eksiği kapat** — Mevcut bir modülde eksik kenar durumu bulup test ekle.

Daha büyükleri:
- LLVM v4 (dizi param/return, dizi length).
- LSP v3 (incremental sync, semanticTokens).
- Concurrency lang syntax (`görev`/`kanal`).

> Büyük işler [`KIRMIZI_QUEUE.md`](../KIRMIZI_QUEUE.md) listesindekiler — bunlar
> Mehmet'in onay verdiği planlardır; direkt başlamadan eki oku.

---

## Hoş geldin

Sorun veya tıkanma olursa GitHub issue aç. Bir sayfa içinde anlat —
neyi yapmaya çalışıyordun, ne oldu, ne bekliyordun.
