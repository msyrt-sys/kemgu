# KEMGU — Proje Bağlamı (Claude Code İçin)

Bu dosya, KEMGU projesinde çalışan Claude Code instanslarına bağlam sağlar.
Her oturumda otomatik okunur. Güncel tutulmalıdır.

---

## Proje Nedir?

KEMGU, **Türkçe syntax'lı bir sistem programlama dili** ve bu dille yazılacak bir **işletim sistemidir**.

Ana hedefler:
- **Güvenlik:** Buffer overflow, null pointer, use-after-free dil tasarımı seviyesinde imkansız
- **Çökmezlik:** Exception yok (`sonuç<T,H>`), null yok (`seçimlik<T>`)
- **Hız:** GC yok, bölge tabanlı bellek modeli (region-based, zero-pause)
- **Taşınabilirlik:** x86_64 ve ARM64 (DGX Spark, Android) hedefleri

---

## Derleme ve Test

```bash
# Derleme (mingw32-make Windows'ta, make Linux'ta)
make

# Tüm testleri çalıştır
make test_tumu

# Sadece lexer testleri
make calistir_lexer_test

# Sadece arena testleri (YAPILACAK — parser fazında eklenecek)
make calistir_arena_test

# Sadece AST testleri (YAPILACAK — parser fazında eklenecek)
make calistir_ast_test

# Sadece parser testleri (YAPILACAK — parser fazında eklenecek)
make calistir_parser_test

# Bellek kontrolü (AddressSanitizer)
# Test binary'leri ASan ile derlenir, runtime'da otomatik tarar.
# Manuel komut yok — `make calistir_*_test` çalıştırınca aktif olur.
# Sızıntı/hata varsa: "ERROR: AddressSanitizer: ..." mesajı stderr'e çıkar.
# Valgrind YOK (Windows native ortam) — Dr. Memory ikincil seçenek.
```

**Derleyici:** `gcc -Wall -Wextra -Wpedantic -std=c11` (MinGW-w64 GCC, UCRT64)
**Hedef platformlar:** Windows x86_64 (birincil — geliştirme), x86_64/ARM64 Linux (ikincil — gelecekte port: DGX Spark, Android NDK)

---

## Dosya Yapısı

```
kemgu/
├── CLAUDE.md                          ← BU DOSYA
├── Makefile
├── README.md
├── belgeler/
│   ├── KEMGU_Lexer_Spesifikasyonu.md  — Lexer spec (tamamlandı)
│   ├── KEMGU_Grammar_EBNF.md          — EBNF grammar tanımı
│   ├── KEMGU_Bellek_Modeli.md         — Formalizasyon (3 katman)
│   └── KEMGU_Makale_Tasari.md         — TOPLAS makale planı
├── src/
│   ├── lexer.h / lexer.c             — Tokenizer (TAMAMLANDI ✓)
│   ├── utf8.h / utf8.c               — Türkçe UTF-8 karakter tanıma (TAMAMLANDI ✓)
│   ├── anahtar_kelime.c              — 30 anahtar kelime tablosu (TAMAMLANDI ✓)
│   ├── hata.h / hata.c               — Hata raporlama (TAMAMLANDI ✓, genişletilecek)
│   ├── arena.h / arena.c             — Arena allocator (TAMAMLANDI ✓)
│   ├── ast.h / ast.c                 — AST düğüm yapıları (TAMAMLANDI ✓)
│   ├── ast_yazdir.h / ast_yazdir.c   — AST debug çıktısı (TAMAMLANDI ✓)
│   ├── parser.h / parser.c           — Recursive descent parser (TAMAMLANDI ✓ — çekirdek, kalan deyimler ADIM 10'da)
│   ├── ifade.c                        — Pratt parser: ifadeler (TAMAMLANDI ✓ — tam öncelik tablosu + sonek + yapı/dizi/lambda)
│   ├── tip.h / tip.c                 — Tip temsili (TipBilgisi, equality, yazdırma) (TAMAMLANDI ✓)
│   ├── sembol.h / sembol.c           — Symbol table + scope hierarchy (TAMAMLANDI ✓)
│   ├── tip_kontrol.h / tip_kontrol.c — İfade tip kontrolü (TAMAMLANDI ✓ — ADIM 11.3)
│   └── ana.c                          — Ana giriş noktası (lexer + parser, --token/--parse) (TAMAMLANDI ✓)
├── test/
│   ├── test_lexer.c                   — 103 birim testi (103/103 ✓)
│   ├── test_arena.c                   — Arena testleri (TAMAMLANDI ✓ — 19/19, ASan temiz)
│   ├── test_ast.c                     — AST testleri (TAMAMLANDI ✓ — 31/31, ASan temiz)
│   ├── test_parser.c                  — Parser testleri (TAMAMLANDI ✓ — 78/78 (29 çekirdek + 24 ifade + 10 deyim + 11 tip + 4 örnek), ASan temiz)
│   ├── test_tip.c                     — Tip sistemi testleri (TAMAMLANDI ✓ — 26/26, ASan temiz)
│   ├── test_sembol.c                  — Sembol tablosu testleri (TAMAMLANDI ✓ — 18/18, ASan temiz)
│   ├── test_tip_kontrol.c             — İfade tip kontrolü testleri (TAMAMLANDI ✓ — 46/46, ASan temiz)
│   └── ornekler/
│       ├── hasta.kem                  — Mevcut örnek (TAMAMLANDI ✓)
│       ├── fibonacci.kem              — Özyinelemeli fibonacci (TAMAMLANDI ✓)
│       ├── yapilar.kem                — Generic yapılar + referans (TAMAMLANDI ✓)
│       └── eslesme.kem                — Pattern matching + döngü (TAMAMLANDI ✓)
```

---

## Kodlama Kuralları

### Genel
- **Dil:** C11 standardı, platforma özel uzantı YASAK
- **Kodlama:** Zorunlu UTF-8 (kaynak dosyalar ve KEMGU kaynak kodu)
- **Derleyici uyarıları:** `-Wall -Wextra -Wpedantic` — sıfır uyarı hedefi
- **Sabit boyutlu tipler:** `int64_t`, `uint32_t` vb. kullan, çıplak `int`/`long` YASAK (boyut belirsizliği)
  - İstisna: döngü sayaçları, boolean dönüşler, dizi indeksleri için `int` kabul edilir

### İsimlendirme (Türkçe)
- Fonksiyonlar: `snake_case` Türkçe → `arena_ayir`, `dugum_olustur`, `token_oku`
- Yapılar/Enum: `PascalCase` Türkçe → `DugumTipi`, `ArenaBlok`, `Operator`
- Sabitler/Enum değerleri: `BÜYÜK_HARF` → `DUGUM_IKILI`, `OP_ARTI`, `TIP_TAM32`
- Makrolar: `BÜYÜK_HARF` → `ARENA_VARSAYILAN_BLOK_BOYUTU`
- Dosya isimleri: küçük harf ASCII → `arena.c`, `ast_yazdir.c` (dosya sistemleri Türkçe karakter sorunlu olabilir)

### Türkçe UTF-8 Dikkat Noktası (KRİTİK)
C'de Türkçe karakter içeren string literalleri yazarken, UTF-8 hex escape'den sonra
hex rakam (a-f, A-F, 0-9) geliyorsa string concatenation ile ayır:

```c
// YANLIŞ — "değer" yazmak istiyoruz ama \xc4\x9f'den sonra "er" geliyor
//          ve 'e' hex rakam olduğu için \x9fe olarak okunur
"de\xc4\x9fer"

// DOĞRU — string concatenation ile ayır
"de\xc4\x9f" "er"
```

**Sebep (C11 §6.4.4.4):** `\x` ardışık olarak en uzun hex rakam dizisini yutar. Yani
`"de\xc4\x9fer"` içinde `\x9fe` 3 hex rakam olarak okunur (0x9fe = 2558,
`unsigned char` aralığı dışı) → derleme hatası/uyarısı.

**Hex rakam = 0-9 + a-f + A-F** — sadece harf değil, rakamlar da dahil. Yani
`"hata\xc5\x9f0"` de aynı şekilde bozulur (`\x9f0` = 0x9f0).

Tüm Türkçe karakterlerin ikinci UTF-8 byte'ı zaten hex aralığında (\x87–\xbc), bu
yüzden sonrasında 0-9, a-f veya A-F geliyorsa **her zaman** concatenation gerekir:

| Karakter | UTF-8 | İkinci byte |
|----------|-------|-------------|
| ç / Ç | `\xc3\xa7` / `\xc3\x87` | a7 / 87 |
| ğ / Ğ | `\xc4\x9f` / `\xc4\x9e` | 9f / 9e |
| ı / İ | `\xc4\xb1` / `\xc4\xb0` | b1 / b0 |
| ö / Ö | `\xc3\xb6` / `\xc3\x96` | b6 / 96 |
| ş / Ş | `\xc5\x9f` / `\xc5\x9e` | 9f / 9e |
| ü / Ü | `\xc3\xbc` / `\xc3\x9c` | bc / 9c |

Güvenli devam karakterleri: `g-z` (a-f hariç), `G-Z` (A-F hariç), boşluk, noktalama,
diğer Türkçe karakter (yeni `\x` escape ile başladığı için zincirleme kırılır).

**Pratik:** `-Wall` aktifken `\x` overflow için uyarı çıkar — sıfır uyarı hedefi her
Türkçe içerikli literali otomatik denetler.

### Bellek Yönetimi
- Parser ve AST: **Arena allocator** kullan, doğrudan malloc/free YASAK
- Lexer: Mevcut haliyle malloc kullanıyor (yeniden düzenleme ileride yapılabilir)
- Testlerde: Arena oluştur → test → arena_serbest(). ASan ile doğrula.
- `arena_ayir_sifir` tercih et (başlatılmamış bellek hataları önlenir)

### Test Yazım Kuralları
- Her modülün kendi test dosyası var: `test/test_<modül>.c`
- Test makrosu: `TEST("açıklama", { ... })` ve `ASSERT(koşul)`
- Çıktı formatı: `[numara] açıklama ... ✓` veya `✗`
- Son satır: `=== X/Y test geçti ===`
- Her test kendi arena'sını oluşturup serbest bırakır (izolasyon)
- UTF-8 Türkçe string testleri her modülde olmalı

---

## Dil Temelleri (KEMGU Syntax Özeti)

```
Dosya uzantısı:     .kem
Kodlama:            Zorunlu UTF-8
İfade sonlandırıcı: ;
Yorum:              // satır, /* blok */ (iç içe destekli)
Sayı ayracı:        1_000_000
Raw string:         r#"..."#
```

### 31 Anahtar Kelime
```
eğer, değilse, için, iken, eşleş, ver, işlev, yapı, özellik, modül,
değişken, sabit, doğru, yanlış, boş, ve, veya, değil, kullan, dışa,
tamam, hata, bölge, uygula, kendin, seçimlik, sonuç, değer, hiç,
güvensiz
```

### Tip Sistemi
```
Basit:    tam8-64, dtam8-64, kesirli32-64, mantıksal, karakter, metin, boş
Bileşik:  yapı{}, Dizi<T>, seçimlik<T>, sonuç<T,H>, &T, &değişken T, *T
Null:     YOK — seçimlik<T> ile değer/hiç
Exception: YOK — sonuç<T,H> ile tamam/hata
```

### Bellek Modeli
```
- Bölge tabanlı (region-based), GC yok
- Compiler bölgeleri otomatik çıkarsar (programcı annotation yazmaz)
- & güvenli referans (her yerde), * ham pointer (sadece güvensiz blokta)
- Bileşik tipler otomatik referansla aktarılır
- Concurrency: bölge sahipliği modeli (her bölge tek thread'e ait)
```

---

## Parser Tasarımı

### Strateji
- **Hibrit:** Recursive descent (deyimler/tanımlar) + Pratt parser (ifadeler)
- **AST:** Tagged union + arena allocator
- **Hata kurtarma:** Panik modu (`;` ve `}` senkronizasyon noktaları)

### Pratt Parser Öncelik Tablosu (düşükten yükseğe)
```
Seviye  Operatörler           Birleşme
  1     veya                  Sol
  2     ve                    Sol
  3     == !=                 Sol
  4     < > <= >=             Sol
  5     + -                   Sol
  6     * / %                 Sol
  7     değil - & *           Önek (sağ)
  8     . [] () ::            Sol (sonek)
```

### AST Düğüm Tipleri (tam liste ast.h'de)
Üst düzey: PROGRAM, MODUL, KULLAN, DISA
Tanımlar:  ISLEV, YAPI, OZELLIK, UYGULA, SABIT, PARAMETRE, ALAN
Deyimler:  DEGISKEN, ATAMA, VER, EGER, IKEN, ICIN, ESLES, GUVENSIZ, BLOK
İfadeler:  IKILI, TEKLI, CAGRI, ERISIM, INDEKS, LAMBDA, YAPI_OLUSTUR, DIZI_OLUSTUR
Literaller: TAM, KESIRLI, METIN, KARAKTER, MANTIKSAL, BOS, TANIMLAYICI
Tipler:    BASIT, REFERANS, POINTER, DIZI, SECIMLIK, SONUC, ISLEV, KULLANICI
Desenler:  LITERAL, TANIMLAYICI, YAPICI, JOKER
Özel:      HATA (error recovery placeholder)

---

## Operatör Kodları (ast.h — Operator enum)
```
İkili:  OP_ARTI, OP_EKSI, OP_CARPI, OP_BOLU, OP_MOD,
        OP_ESIT, OP_ESIT_DEGIL, OP_KUCUK, OP_BUYUK, OP_KUCUK_ESIT, OP_BUYUK_ESIT,
        OP_VE, OP_VEYA
Tekli:  OP_NEG (-x), OP_DEGIL (değil x), OP_REF (&x),
        OP_REF_DEGISKEN (&değişken x), OP_DEREFERANS (*x)
```

---

## Mevcut Durum ve Yapılacaklar

### Tamamlanan ✓
1. Lexer spesifikasyonu ve C implementasyonu (103/103 test)
2. Bellek modeli formalizasyonu (3 katman taslak)
3. EBNF grammar tanımı
4. AST düğüm tasarımı
5. Parser tasarım kararları (hibrit, arena, panik modu)
6. Arena allocator (`arena.h/c`) — 19/19 test, ASan temiz, Clang64 ile doğrulandı
7. AST düğüm yapıları (`ast.h/c`) + AST yazıcısı (`ast_yazdir.h/c`) — 31/31 test, ASan temiz
8. Parser çekirdeği (`parser.h/c` + `ifade.c` minimal) — 29/29 test, ASan temiz
   - Token akışı (1+lazy 2 lookahead), panik modu (sync points: `;`, `}`, üst düzey keywords)
   - Üst düzey: işlev, yapı, kullan, dışa, modül, sabit
   - Temel deyimler: değişken, ver, blok, atama, ifade_deyimi
   - Çocuk listesi: arena'da linked list → array kopyalama (malloc YOK)
9. Pratt ifade parser (`ifade.c`) — 53/53 parser test (29 çekirdek + 24 ifade), ASan temiz
   - Tam öncelik tablosu (8 seviye, sol birleşmeli ikili, sağ birleşmeli önek)
   - Önek: `-`, `değil`, `&`, `&değişken`, `*`
   - Sonek (zincirleme): `.alan`, `[indeks]`, `(args)`, `::yol`
   - Yapı oluşturma: `Tip { alan: değer, ... }` (trailing comma destekli)
   - Dizi oluşturma: `[e1, e2, ...]` (boş dizi destekli)
   - Lambda: `|param: tip, ...| ifade` veya `|...| { blok }`
10. Kalan deyimler + desenler (`parser.c` ekleme) — 63/63 parser test, ASan temiz
    - `eğer`/`değilse` zinciri (else if), `iken`, `için`
    - `eşleş` + desenler: literal, tanımlayıcı, joker (`_`), yapıcı
    - `güvensiz` blok (opsiyonel `[etiket: "açıklama"]` ile)
    - **Kondisyonel ifadelerde `{` blok başı** (`yapi_olusturma_izni` flag)
    - `hiç` ve `değer` anahtar kelimeleri pattern matching'de tanımlayıcı/yapıcı
11. Karmaşık tipler + generic yapı (`ifade.c parse_tip` + `parser.c`) — 74/74 parser test
    - `&T`, `&değişken T` (referans), `*T` (pointer)
    - `seçimlik<T>`, `sonuç<T,H>`, `Dizi<T>` (özel düğümler)
    - `işlev(T1, T2) -> T` (işlev tipi)
    - `Tip<T1, T2>` (kullanıcı generic tipi — `DUGUM_TIP_KULLANICI`)
    - Generic yapı tanımı: `yapı Kutu<T> { ... }`
    - **`>>` ayırma (parser_buyuk_ayir):** `Dizi<seçimlik<T>>` için lexer'ın
      `>>` (TOK_SAGA_KAYDIR) tokenını generic kapanışta iki `>` olarak böl
12. Örnek `.kem` dosyaları + `ana.c` parser entegrasyonu — 78/78 parser test
    - `fibonacci.kem`, `yapilar.kem`, `eslesme.kem` (yeni) + `hasta.kem` (mevcut)
    - `ana.c` `--token` / `--parse` (default) bayrakları — Lexer→Parser→AST yazdırma
    - 4 dosya parse testi (test_parser.c file I/O)
    - `hiç`/`değer`/`tamam`/`hata` keyword'leri ifade context'inde tanımlayıcı

### 🎉 PARSER FAZI TAMAMLANDI (231/231 test)

13. Tip temsili (`tip.h/c`) — 26/26 tip test, ASan temiz
    - 14 basit tip (tam8-64, dtam8-64, kesirli32-64, mantıksal, karakter, metin, boş)
    - 7 bileşik tip (referans, pointer, dizi, seçimlik, sonuç, işlev, yapı)
    - Generic parametre (TIP_GENERIC_PARAM)
    - **Nominal eşitlik** (recursive, ad + args)
    - Yazdırma (KEMGU sözdizimine yakın: `Dizi<seçimlik<tam32>>`)
    - Yardımcılar: `tip_sayisal_mi`, `tip_tamsayi_mi`, `tip_mantiksal_mi`

14. Symbol table (`sembol.h/c`) — 18/18 sembol test, ASan temiz
    - 8 sembol kategorisi (DEGISKEN, SABIT, PARAMETRE, ISLEV, YAPI, OZELLIK, MODUL, GENERIC_PARAM)
    - 5 scope kategorisi (GLOBAL, MODUL, ISLEV, BLOK, YAPI)
    - Parent pointer'lı linked list — `sembol_bul` parent zincirinde arar
    - `sembol_bul_yerel` parent'a bakmaz (shadowing için)
    - `sembol_yapi_alani` yapı sembolünün yapı_scope'unda arar
    - `sembol_modul_scope` modül üyelerine erişim
    - Çift tanım algılama (sembol_ekle -1 döner)

15. İfade tip kontrolü (`tip_kontrol.h/c` 1) — 46 ifade testi, ASan temiz
    - AST visitor pattern (her ifade düğümü için tip belirleme)
    - 17 hata kodu (T001-T017): tip uyumsuzluğu, tanımsız sembol, sayısal/mantıksal/tamsayı bekleniyor, vs.
    - Literaller (default tipler — context ADIM 11.5'te)
    - İkili op (sayısal aritmetik, eşitlik, karşılaştırma, mantıksal)
    - Tekli op (-, değil, &, &değişken, *)
    - Çağrı (parametre sayısı + tip eşleşmesi)
    - Erişim (yapı.alan — referans otomatik dereference)
    - İndeks (Dizi<T>[i] — i tamsayı kontrolü)
    - Yapı oluşturma (alan tipi + eksik/fazla kontrol)
    - Dizi oluşturma (homojen kontrolü; boş dizi T014 — ADIM 11.5'te context)
    - Yol (modul::ad çözümleme)
    - `ast_tip_to_bilgi`: AST tip düğümü → TipBilgisi (built-in + kullanıcı)

16. Deyim/tanım tip kontrolü (`tip_kontrol.c` 2) — 28 program testi (toplam 74), ASan temiz
    - **İki geçişli `pre_populate`:** önce yapılar (alanlar dahil), sonra işlevler/sabitler — forward references çalışır
    - Generic params yapı_scope'a eklenir (alan tipleri için resolve)
    - Deyim visitor: değişken (annot vs değer eşleşmesi), atama (lvalue + tip), ver (aktif_donus_tipi ile uyum), eğer/iken (koşul mantıksal), için (Dizi<T> + eleman tipi), eşleş (kol gövdesi), güvensiz, blok (yeni scope), ifade_deyimi
    - Tanım visitor: işlev gövdesi (parametre scope + aktif_donus_tipi context), sabit (annot vs değer), yapı (pre-populate yeterli), modül (üye recursive), dışa (iç tanım recursive)
    - Yeni hata kodları: T020 (ver tipi), T021 (kondisyon mantıksal), T022 (lvalue), T023 (ver dış), T024 (çift tanım), T026 (yapı çakışması), T027 (için dizi)

### TİP SİSTEMİ FAZI (yapım sırası)
1. ~~tip.h/c (temsil + equality)~~ ✓ ADIM 11.1
2. ~~sembol.h/c (symbol table + scope)~~ ✓ ADIM 11.2
3. ~~tip_kontrol.h/c (1) — ifade tip kontrolü~~ ✓ ADIM 11.3
4. ~~tip_kontrol.c (2) — deyim/tanım tip kontrolü~~ ✓ ADIM 11.4
5. **tip_kontrol.c (3) — tip çıkarsama (bidirectional) — ADIM 11.5 sıradaki**
6. tip_kontrol.c (4) — generic instantiation
7. ana.c `--check` modu + örnek dosyalar üzerinde tip kontrolü

### İlerideki Fazlar
- Tip sistemi (tip çıkarsama, tip kontrolü)
- Bölge çözümleyici (escape analizi, bölge atama)
- LLVM backend (IR üretimi)
- Bootstrapping (KEMGU ile KEMGU derleyicisi)

---

## Taşınabilirlik Notları

- `sizeof(void *)` ile hizalama (ARM64 uyumlu)
- Sabit boyutlu tipler zorunlu (`int64_t`, `uint32_t`)
- Locale bağımsız UTF-8 işleme (byte seviyesinde)
- FAM (Flexible Array Member) yerine manual offset (MSVC uyumu)
- Endianness varsayımı yok (gerekirse açık dönüşüm)

---

## Lexer → Parser Entegrasyon Notu

Mevcut lexer API'si:
```c
void lexer_baslat(Lexer *lexer, const char *kaynak, const char *dosya_adi);
Token lexer_sonraki_token(Lexer *lexer);
const char *token_tipi_adi(TokenTipi tip);
```

Parser, lexer'dan token akışı alır. Parser kendi token tamponunu tutar (1-2 token lookahead).
Lexer dosyadan veya stdin'den UTF-8 metin okur, parser Token yapılarını tüketir.

---

## İletişim Dili

Kod içi yorumlar, değişken isimleri, hata mesajları, test açıklamaları: **Türkçe**.
Commit mesajları: Türkçe veya İngilizce (geliştirici tercihi).
Belge dosyaları: Türkçe.

---

## Geliştirme Ortamı (Windows Native — Dual-Compiler)

- **Shell:** Git Bash (MSYS / MinTTY)
- **Prod derleyici:** MinGW-w64 GCC 16.1.0 (UCRT64) — `kemgu.exe`, lexer testi
- **Test + ASan derleyicisi:** Clang 22.1.4 (Clang64) — arena / AST / parser testleri
  (Win11'de ASan runtime için zorunlu — UCRT64 GCC `libasan` içermez)
- **Standart:** C11 (her iki derleyici de)
- **Build:** mingw32-make 4.4.1 (UCRT64'ten)
- **PATH (her Bash tool çağrısında set edilmeli — kalıcı PATH'e yazılmadı):**
  ```bash
  export PATH=/c/msys64/clang64/bin:/c/msys64/ucrt64/bin:$PATH
  ```
  Sebep: `clang` Clang64'ten (ASan testleri), `gcc` UCRT64'ten (prod), `mingw32-make` UCRT64'ten.
- **Bellek kontrolü:** AddressSanitizer + UndefinedBehaviorSanitizer (Clang64 dynamic runtime).
  `libclang_rt.asan_dynamic-x86_64.dll` Clang64/bin içinde — PATH'teyse otomatik bulunur.
- **Test binary'leri:** `.exe` uzantılı (`build/test_lexer.exe`, `build/test_arena.exe`, `build/kemgu.exe`)

### Win11 26200 — ASan / Dr. Memory Notu

- **MinGW-w64 UCRT64 GCC**, ASan/UBSan **runtime** kütüphanelerini içermez (sadece compiler header'ları). Bu MinGW-w64'ün bilinen kısıtlaması.
- **Dr. Memory** Win11 26200'de DynamoRIO uyumsuzluğu nedeniyle çöküyor (GitHub issues [#2456](https://github.com/DynamoRIO/drmemory/issues/2456), [#2489](https://github.com/DynamoRIO/drmemory/issues/2489) — 3+ yıldır açık, fix yok).
- **Çözüm:** Bellek alan modül testleri için **Clang64 + ASan** kullan. Makefile'da `CC_ASAN = clang` yapısı bunu yapar — prod tarafı GCC kalır, sadece test ASan ile derlenir.

---

## Aktif Görev

- **Faz:** Tip sistemi (7 alt-adımdan 4.'ü tamam)
- **Sıra:** ~~tip.h/c (11.1)~~ ✓ → ~~sembol.h/c (11.2)~~ ✓ → ~~tip_kontrol ifadeler (11.3)~~ ✓ → ~~deyimler/tanımlar (11.4)~~ ✓ → **çıkarsama (11.5 sıradaki)** → generic (11.6) → entegrasyon (11.7)
- **Tip sistemi tasarım kararları (kullanıcı onayladı):**
  - Çıkarsama: Lokal + Bidirectional (Rust/Swift tarzı)
  - Generic: Monomorphization (Rust gibi)
  - Eşitlik: Nominal (Rust/Java gibi)
  - Constraint: ŞIMDILIK YOK (ileride)
  - Sayı literal: Context-dependent, default tam32
  - Bölge: Önce tip, sonra ADIM 12'de bölge
- **Sıradaki hedef:** Tip çıkarsama — sayı literal context-dependent (`tam8`/`tam64`), boş dizi `[]` context'ten tip al, `değişken x = ...` annot olmadan değer tipinden çıkarsama (zaten var), bidirectional propagation (`f(arg)` arg tipi parametre tipinden zorlama)

---

## Bu Oturumun Çalışma Kuralları

- Her dosya yazıldıktan sonra `make` çalıştır, sıfır uyarı hedefi (`-Wall -Wextra -Wpedantic`)
- Test binary'leri için ASan derleme bayrakları: `-fsanitize=address,undefined -g -fno-omit-frame-pointer`
- Bellek alan modüller için her test çalıştırmasında ASan aktif olmalı; `ERROR: AddressSanitizer` çıkarsa adım onaylanmaz
- Her mantıksal birim bitince Türkçe küçük git commit
- Türkçe UTF-8 hex escape kuralı: ayrıntı için yukarıdaki **"Türkçe UTF-8 Dikkat Noktası"** bölümüne bak (kural: her Türkçe karakter escape'inden sonra 0-9 / a-f / A-F geliyorsa concatenation şart)
