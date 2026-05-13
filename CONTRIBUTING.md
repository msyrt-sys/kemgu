# KEMGU'ya Katkı

> **Şu an proje kapalı geliştirme aşamasında.** Katkı süreci direktif eki
> Bölüm 2 onayı ile açılacak. Bu belge süreç açıldığında uygulanacak
> kuralları önceden tutar; *şimdiden* repoyu inceleyenler için referans.

Bu belgeyi okumadan önce [`belgeler/BASLAMAK.md`](belgeler/BASLAMAK.md)'ye
göz at — KEMGU'ya yeni giriyorsan orası ana onboarding'dir. Burada
yalnızca **katkı süreci** anlatılır.

---

## 1. Kapsam

Kabul edilen katkı türleri:

| Tür                       | Renk | İlk PR olarak uygun mu? |
|---------------------------|------|--------------------------|
| Belge düzeltme / iyileştirme | 🟢 | ✓ ideal                |
| Test ekleme                | 🟢   | ✓                       |
| Bug fix (test eşliğinde)   | 🟢   | ✓                       |
| Yeni tutorial örneği       | 🟢   | ✓                       |
| Performans iyileştirme     | 🟢   | ✓ (benchmark gerekli)   |
| Refactor (semantik değişmez) | 🟢 | ✓                       |
| Stdlib genişletme (runtime gerek yok) | 🟡 | △ küçük PR ise |
| Yeni dil özelliği          | 🔴   | ✗ önce KIRMIZI_QUEUE.md  |
| Tip sistemine yeni katman  | 🔴   | ✗                       |
| Breaking change            | 🔴   | ✗                       |

Renk kategorileri için: [`belgeler/KEMGU_Direktif_Ek_v1.1.md`](belgeler/KEMGU_Direktif_Ek_v1.1.md).

Kabul **edilmeyen** PR'lar:
- ASLA listesini ihlal eden (null, exception, GC, implicit conv, vb.)
- Türkçe kimliği bozan (İngilizce keyword alias'ı, karma sözdizimi)
- Test olmadan sunulan
- Sıfır uyarı hedefini bozan (`-Wall -Wextra -Wpedantic`)
- `--no-verify` ile hook bypass eden
- Performance regression getiren (ölçülmemiş olsa bile gerekçesi gerek)

---

## 2. Kod Stili

### 2.1 C kodu (`src/`, `test/`, `runtime/`)

| Konu                 | Kural                                              |
|----------------------|----------------------------------------------------|
| Standart             | C11 (`-std=c11`)                                   |
| Uyarılar             | `-Wall -Wextra -Wpedantic` — sıfır uyarı           |
| Tipler               | `int64_t`, `uint32_t` (sabit boyut zorunlu)        |
| Çıplak `int`/`long`  | YASAK (döngü sayacı/indeks istisna)                 |
| Bellek               | Parser/AST: arena; doğrudan `malloc/free` YASAK    |
| İsim — işlev         | snake_case Türkçe (`arena_ayir`, `dugum_olustur`)   |
| İsim — yapı/enum     | PascalCase Türkçe (`DugumTipi`, `ArenaBlok`)        |
| İsim — sabit / enum değeri | BÜYÜK_HARF (`DUGUM_IKILI`, `OP_ARTI`)         |
| Dosya adı            | Küçük harf ASCII (`arena.c`, `ast_yazdir.c`)        |
| UTF-8 string         | Türkçe karakter hex escape kuralı (aşağıda)         |
| Yorum dili           | Türkçe                                              |

### 2.2 UTF-8 Türkçe hex escape (KRİTİK)

```c
// YANLIŞ — \xc4\x9fer → derleyici 'e'yi hex rakam sayar
"de\xc4\x9fer"

// DOĞRU — string concatenation ile ayır
"de\xc4\x9f" "er"
```

Ayrıntı: [`CLAUDE.md`](CLAUDE.md) → "Türkçe UTF-8 Dikkat Noktası".

### 2.3 KEMGU kodu (`stdlib/`, `test/ornekler/`)

| Konu                 | Kural                                              |
|----------------------|----------------------------------------------------|
| Anahtar kelimeler    | Türkçe (`işlev`, `eğer`, `iken`, ...)              |
| Değişken / işlev adı | snake_case Türkçe                                  |
| Yapı / özellik adı   | PascalCase Türkçe                                  |
| Sabit                | BÜYÜK_HARF                                         |
| Sayı tipi            | Açık genişlik (`tam32`, `tam64`), çıplak değil     |
| Null                 | YASAK — `seçimlik<T>` kullan                       |
| Exception            | YASAK — `sonuç<T,H>` veya `KSonuc<T,E>` kullan     |
| `güvensiz`           | Yalnız ham `*T` / FFI / unrepresentable invariant  |
| Yorum                | Türkçe; iddialı satırların ALTINA neden            |

### 2.4 Format

KEMGU şu an otomatik formatter sağlamaz (planlanmakta). El ile:
- Girintiler 4 boşluk; sekme yok.
- Satır uzunluğu önerisi 100 sütun.
- İşlev tanımı arasında bir boş satır.
- Tek `{` aynı satırda; tek `}` kendi satırında.
- Dosya sonunda bir newline.

### 2.5 Yorumlar

- **Ne** yapıldığını anlatma — kod zaten anlatır.
- **Neden** yapıldığını yaz — gizli kısıt, geçmiş hata, sürpriz davranış.
- Tutorial örneklerinde ders niteliğinde uzun yorum kabul edilir.

---

## 3. Test Gereksinimi

**Her PR test eklemeli.** Test yoksa PR kabul edilmez.

### 3.1 Hangi test nereye

[`belgeler/BASLAMAK.md`](belgeler/BASLAMAK.md) "Test gereksinimi" bölümüne bak.

### 3.2 Bütünlük

Tek bir `mingw32-make test_tumu` koşusu PR'dan önce yeşil olmalı:

```bash
export PATH=/c/msys64/clang64/bin:/c/msys64/ucrt64/bin:$PATH
mingw32-make test_tumu
```

Test sayısı azalmamalı (eski test çürüdüyse ayrı PR ile silinir).

### 3.3 ASan

Bellek alan modüllerin testleri Clang64 + ASan + UBSan ile çalışır.
Herhangi `ERROR: AddressSanitizer: ...` çıkarsa root cause fix et — bypass yasak.

### 3.4 Benchmark (perf PR'ları)

Performans iddiası varsa karşılaştırma sayısı paylaş:

```
[Önceki]  X iter / Y ms
[Yeni]    X iter / Z ms
[Δ]       (Z-Y)/Y % daha hızlı
```

Mikro benchmark için `test/test_fuzz.c` etkenlik baz alınabilir.

---

## 4. Branch ve PR Akışı

### 4.1 Branch

```
feature/<konu>           ← yeni iş; main'den ayrılır
feature/<konu>-bugfix    ← varsa
konsolidasyon            ← spec entegrasyonu (maintainer)
main                     ← korumalı; force-push yasak
```

Adlandırma:
- Türkçe veya açıklayıcı İngilizce.
- Kısa ama anlamlı (`feature/bit-operatorleri`, mevcut `feature/stdlib-genisletme`).
- Çok uzun veya kript değil.

### 4.2 Commit

[`belgeler/BASLAMAK.md`](belgeler/BASLAMAK.md) "Commit mesajı kuralları" geçerli:
- Türkçe veya İngilizce (tutarlı).
- İlk satır ≤ 72 karakter.
- Atomik commit'ler (bir mantıksal birim = bir commit).
- `--no-verify` yasak.
- `--amend` ile yayınlanmış commit'i değiştirme.

### 4.3 PR

1. Branch'i push et.
2. GitHub'da PR aç. Şablon:

   ```markdown
   ## Özet
   - <bir cümle: ne değişti?>
   - <iki cümle: neden gerekti?>

   ## Test
   - <yeni test sayısı>
   - `make test_tumu` çıktısı X/X.
   - ASan: temiz / N hata (varsa açıkla)

   ## Renk (Direktif Ek)
   - 🟢 / 🟡 / 🔴 — kısa gerekçe

   ## Bağlantılı
   - KIRMIZI_QUEUE.md (varsa)
   - Issue #
   ```

3. Maintainer review — Mehmet veya delege.
4. Değişiklik istenirse aynı branch üzerinde yeni commit (squash merge'e
   bırakılır; gereksiz `fixup` history bırakma).
5. Yeşilden geçtiğinde merge — fast-forward veya squash. Merge commit yok.

### 4.4 Konuda dur

Bir PR bir konu yapmalı. "Build fix + yeni özellik + stdlib genişlet" gibi
karma PR'lar reddedilir, ayrılması istenir.

---

## 5. Issue Açma

İyi bir issue:

```markdown
## Ne oldu?
<1-3 cümle: yaşadığın durum>

## Ne bekliyordun?
<beklenen davranış>

## Tekrar üretim adımları
1. ...
2. ...

## Ortam
- OS: Windows 11 / Linux / macOS
- KEMGU branch + commit hash: `git rev-parse HEAD`
- Derleyici: gcc X.X.X / clang Y.Y.Y

## Ek
<log / stack trace / .kem dosyası örneği>
```

Soru issue'ları da hoş karşılanır — başka bir yer olmadığı için.

---

## 6. Güvenlik

KEMGU bir güvenlik dilidir. Eğer derleyicide veya stdlib'de bir güvenlik
zafiyeti bulduysan, **public issue açma**. Bunun yerine doğrudan
maintainer'a iletişim kanalı üzerinden bildir (e-posta veya iletişim
sayfası açılacak — şu an direkt Mehmet).

---

## 7. Lisans

KEMGU şu an "Tüm hakları saklıdır" lisansı altında. Yayın aşamasında MIT
veya Apache-2.0 değerlendirilecek (direktif eki Bölüm 8). Kabul edilen PR'lar
yayın lisansı uyarınca lisanslanmış sayılır.

---

## 8. Davranış Kuralları

- Saygılı iletişim — kod hakkında konuş, kişiye değil.
- Türkçe ve İngilizce iletişim ikisi de kabul (Türkçe tercih).
- Spekülatif "bunu daha sonra çıkarmalı" yorumlarını PR'a yazma — issue aç.
- Direktife uymadığını düşündüğün bir kararın gerekçesini sor; çevirme.

---

## 9. Teşekkür

Katkıda bulunan herkes [`AUTHORS.md`](AUTHORS.md) (yayında) listesine
eklenir. KEMGU'nun ilerlemesinin görünür olması önemli.

---

Sorun olursa bu belge yetersizdir — issue aç.
