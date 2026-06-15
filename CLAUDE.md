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
│   ├── bolge.h / bolge.c             — Bölge temsili (TAMAMLANDI ✓ — ADIM 12.1)
│   ├── bolge_atama.h / bolge_atama.c — Bölge atama R-* aksiyomları + escape entegrasyonu (TAMAMLANDI ✓ — ADIM 12.2 + 14.2)
│   ├── escape.h / escape.c           — DFA fixed-point escape analizi (TAMAMLANDI ✓ — ADIM 14.1)
│   ├── llvm.h / llvm.c               — LLVM IR text üretici (TAMAMLANDI ✓ — ADIM 13.1)
│   ├── json.h / json.c               — Minimal JSON parser + yazıcı (TAMAMLANDI ✓ — ADIM 16.1)
│   ├── lsp.h / lsp.c                 — LSP server MVP (TAMAMLANDI ✓ — ADIM 16.2-16.4)
│   └── ana.c                          — Ana giriş noktası (--token/--parse/--check/--llvm/--lsp) (TAMAMLANDI ✓)
├── test/
│   ├── test_lexer.c                   — 103 birim testi (103/103 ✓)
│   ├── test_arena.c                   — Arena testleri (TAMAMLANDI ✓ — 19/19, ASan temiz)
│   ├── test_ast.c                     — AST testleri (TAMAMLANDI ✓ — 31/31, ASan temiz)
│   ├── test_parser.c                  — Parser testleri (TAMAMLANDI ✓ — 90/90 (+12: özellik/uygula/bound), ASan temiz)
│   ├── test_tip.c                     — Tip sistemi testleri (TAMAMLANDI ✓ — 26/26, ASan temiz)
│   ├── test_sembol.c                  — Sembol tablosu testleri (TAMAMLANDI ✓ — 18/18, ASan temiz)
│   ├── test_tip_kontrol.c             — İfade tip kontrolü + constraint testleri (TAMAMLANDI ✓ — 97/97 (+7 constraint), ASan temiz)
│   ├── test_bolge.c                   — Bölge temsili testleri (TAMAMLANDI ✓ — 17/17, ASan temiz)
│   ├── test_bolge_atama.c             — Bölge atama R-* + escape entegrasyon testleri (TAMAMLANDI ✓ — 13/13, ASan temiz)
│   ├── test_escape.c                  — DFA escape analizi testleri (TAMAMLANDI ✓ — 17/17, ASan temiz)
│   ├── test_json.c                    — JSON parser + yazıcı testleri (TAMAMLANDI ✓ — 21/21, ASan temiz)
│   ├── test_lsp.c                     — LSP server MVP testleri (TAMAMLANDI ✓ — 6/6, ASan temiz)
│   ├── test_llvm.c                    — LLVM backend entegrasyon (derle + çalıştır + exit kodu) (TAMAMLANDI ✓ — 30/30, multi-int + metin + yapı + float/dizi/struct-by-value)
│   └── ornekler/
│       ├── hasta.kem                  — Mevcut örnek (TAMAMLANDI ✓)
│       ├── fibonacci.kem              — Özyinelemeli fibonacci (TAMAMLANDI ✓)
│       ├── yapilar.kem                — Generic yapılar + referans (TAMAMLANDI ✓)
│       ├── eslesme.kem                — Pattern matching + döngü (TAMAMLANDI ✓)
│       └── drone_kontrol.kem          — Realtime PID controller (TAMAMLANDI ✓)
├── src/wcet.h/c                       — Realtime Spec V1 WCET + RT001-RT005 (TAMAMLANDI ✓)
└── test/test_wcet.c                   — 32/32 test, ASan temiz
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

### 35 Anahtar Kelime
```
eğer, değilse, için, iken, eşleş, ver, işlev, yapı, özellik, modül,
değişken, sabit, doğru, yanlış, boş, ve, veya, değil, kullan, dışa,
tamam, hata, bölge, uygula, kendin, seçimlik, sonuç, değer, hiç,
güvensiz, tekkez, imha, görev, kanal
```
(`tekkez`, `imha` — Linear Types Spec V1; `kullan` ifade context'inde
linear consume olarak ikinci anlama sahip — `kullan(t)`. `görev`, `kanal`
— Concurrency / DRF V1 tip kurucuları; `görev_başlat`, `görev_birleştir`,
`kanal_gönder`, `kanal_al`, `dondur` built-in çağrılar.)

Diğer spec'lerle birlikte tam keyword tablosu: 35 yukarıda + `olarak`,
`sabitsüre`, `gerçekzamanlı`, `yetki`, `delege`, `geri_al`, `vektör`.

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

17. Bidirectional tip çıkarsama (`tip_kontrol.c` 3) — 10 ek test (toplam 84), ASan temiz
    - Yeni API: `tip_belirle_beklenen(tk, d, beklenen)` — context-aware
    - Sayı literal context-dependent: `değişken x: tam8 = 1` → 1 tam8'e çıkarsanır
    - Boş dizi context'ten: `değişken xs: Dizi<tam32> = []` → Dizi<tam32>
    - Çağrı arg context'ten: `f(5)` (f param tam8) → 5 tam8
    - Atama, ver, yapı_oluştur context'i yayılır
    - Zincirleme: `f(g(5))` — g param tam8 → 5 tam8 (recursive)

18. Generic instantiation (`tip_kontrol.c` 4) — 6 ek test (toplam 90), ASan temiz
    - `substitusyon` helper: TIP_GENERIC_PARAM → concrete tip (recursive)
    - Yapı oluşturma + erişimde substitusyon (Kutu<tam32>.eleman → tam32)
    - İç içe generic destekli (Kutu<Kutu<tam32>>)
    - Çoklu tip param: Çift<A, B> { ilk: A; ikinci: B; }

19. `ana.c` `--check` modu + örnek dosyalar (ADIM 11.7) — KEMGU CLI tip kontrolü
    - 3 mod: `--token`, `--parse`, `--check` (default)
    - `mode_check`: parser çalıştır + tip_kontrol_program → "OK" / "HATA"
    - **Test sonuçları:** `fibonacci.kem` ✓, `yapilar.kem` ✓ tip kontrolünden geçti
    - **Bilinen sınırlamalar (gelecek iyileştirmeler):**
      - `hiç`/`değer`/`tamam`/`hata` yapıcıları, beklenen tip (sonuç/seçimlik)
        bilindiğinde ifade context'inde çalışır (C2.5 codegen: `ver`, annotasyonlu
        `değişken`). Beklenen tipin bilinmediği çıplak context (annotasyonsuz)
        hâlâ çözülemez.
      - **(GÜNCEL — C2.5):** `eşleş` yapıcı-deseni binding (`tamam(v)`/`hata(e)`/
        `değer(s)`) hem tip kontrolünde (`tip_kontrol.c:3543-3577`) hem codegen'de
        (`llvm.c` DUGUM_ESLES destructuring) ÇALIŞIR. Eski "pattern binding eksik"
        notu geçersizdi — sorun yalnız codegen'deydi, o da C2.5'te kapandı.

### 🎉 TİP SİSTEMİ FAZI TAMAMLANDI (90 yeni test, toplam 365/365)

20. Bölge temsili (`bolge.h/c`) — 17/17 test, ASan temiz (ADIM 12.1)
    - 9 kategori: LIT, YEREL, CAGIRAN, ITERASYON, GLOBAL, SAHIP, KANAL, BILINMIYOR, HATA
    - Aksiyom: ITERASYON < YEREL < CAGIRAN < GLOBAL (ömür sırası)
    - LCA hesabı (R-KOŞUL için daha uzun ömürlü olanı döner)

21. Bölge atama (`bolge_atama.h/c`) — 10/10 test, ASan temiz (ADIM 12.2)
    - AST visitor: KEMGU_Bellek_Modeli.md Katman 1 R-* aksiyomları
    - R-LIT, R-YEREL, R-VER, R-İTERASYON, R-KOŞUL uygulanıyor
    - Context-tracking: ver_baglaminda, dongu_derinligi flag'leri

22. **LLVM IR backend (`llvm.h/c`) — END-TO-END ÇALIŞAN DERLEYİCİ!** (ADIM 13.1)
    - Text-based IR üretici (libLLVM yok, sadece string output)
    - Basit işlevler + tam sayı literal + ikili/tekli op + ver
    - SSA register counter ile recursive ifade
    - **Pipeline:** KEMGU kaynak → lexer → parser → AST → tip kontrol → LLVM IR → `clang -x ir -` → native `.exe` → çalıştırma
    - `ana.c` `--llvm` modu eklendi (4. CLI mod: token/parse/check/llvm)

23. **DFA fixed-point escape analizi (`escape.h/c`) — ADIM 14.1** (17/17 test, ASan temiz)
    - 3 kategori: `ESC_YEREL`, `ESC_ITERASYON`, `ESC_CAGIRAN`
    - **Per-AST-node escape kategorisi:** linear kayit haritasi
    - **Değişken bağlama izleme:** scope stack (sondan başa arama, append-only on assign)
    - **Forward DFA:** `ver e`'de `ifadeyi_yukselt` ile tüm alt-tahsis bölgelerini CAGIRAN'a terfi (tanımlayıcı zincirlerini takip eder)
    - **Fixed-point iterasyonu:** kategori değişikliği durana kadar tekrar (max 16 iter güvenlik)
    - **Transitif escape:** `değişken x = "hello"; ver x;` → `"hello"` CAGIRAN (eski sistemde YEREL idi)
    - **Loop iterasyonu:** döngü içinde tahsis → ESC_ITERASYON otomatik
    - **Koşullu dallanma:** `eğer { ver a } değilse { ver b }` → her iki tahsis CAGIRAN
    - **Erişim/indeks konservatif:** alt-nesne escape kabul edilir
    - **Sinirlamalar (v1):** interprocedural yok (call sonuçları konservatif), closure yakalama henuz yok, MAY-points-to konservatif

24. **Escape-aware bölge atama (`bolge_atama.c` ADIM 14.2)** (3 entegrasyon testi, 13/13 toplam)
    - Yeni API: `bolge_atama_escape_bagla(ba, &ea)` — opsiyonel DFA modu
    - **İki çalışma modu:**
      - Escape NULL: eski syntax tabanlı davranış korunur (geriye uyumlu)
      - Escape bağlı: AST düğümünden direkt escape kategori sorgusu, daha keskin sonuç
    - `ESC_CAGIRAN → BOLGE_CAGIRAN`, `ESC_ITERASYON → BOLGE_ITERASYON`, `ESC_YEREL → BOLGE_YEREL`
    - Güvenli taraf: escape YEREL diyorsa ama `ver_baglaminda` aktifse, CAGIRAN'a düşer

33. **Generic işlev + Stdlib seed (ADIM 23)** (4 yeni LLVM testi + 3 stdlib modülü)
    - **Generic işlev:** `işlev kimlik<T>(x: T) -> T { ver x; }`
      - AST: `islev` artık tip_paramlar + bound listesi alır
      - Parser: ad sonrası optional `<T, U: Bound>` (yapı ile aynı sözdizim)
      - Tip kontrol: pre_populate_islev'de generic params gp_scope'a;
        tip_kontrol_tanim'da gövde scope'una; tip_esit generic param için ad-bazlı + concrete-deferred
      - DUGUM_CAGRI: generic param parametresi her arg tipi kabul eder, donus tipi inferred T ile değişir
    - **LLVM monomorphization:**
      - Generic işlevler tek başına emit edilmez
      - Çağrı sırasında arg tipinden T çıkarsanır, mangled name (`kimlik$i32`) üretilir
      - Bekleyenler listesi + worklist sonrası emit (recursive instantiation desteği)
      - TipSubst context: generic param adı → IR string (`T` → `i32`)
      - MonoKayit ile duplicate emission engellenir
    - **`tip_sayisal_mi`/`tip_tamsayi_mi`/`tip_mantiksal_mi`:** Generic param için "deferred true" (instantiation'a ertelenir)
    - **`tip_esit`:** İki generic param → ad eşitliği; biri generic → uyumlu
    - **Stdlib seed (`stdlib/temel/`):**
      - `matematik.kem` — mutlak, en_kucuk, en_buyuk, kare, kup, sinirla, isaret
      - `karsilastir.kem` — esit_mi, farkli_mi, karsilastir, en_kucuk_uc, en_buyuk_uc
      - `sayisal.kem` — ortalama, us, obe (GCD), ekok (LCM)
      - Hepsi pure KEMGU, runtime/FFI bağımlılığı yok, --check geçer
      - Makefile `calistir_stdlib_check` hedefi
    - **Felsefe:**
      - Java equals/hashCode tuzağına düşme — fonksiyonel API
      - Go yıllarca generic yoktu — gün 1'den generic
      - Rust 'a lifetime yükü yok — sıfır annotation
    - **Sınırlamalar (v1):**
      - `kendin` (self) henüz parametre olarak parse edilmiyor — method-style API yok
      - Tip args çıkarsama yalnız param tiplerinden (return-type-driven inference yok)
      - Bound kontrolü generic işlevde henüz yok (yapıda var)
      - `kullan stdlib::matematik` import sistemi henüz yok (tek dosya derleme)

29. **Constraint v2 — uygula gövde tip kontrolü + method dispatch (ADIM 19)** (+5 test)
    - DUGUM_OZELLIK ve DUGUM_UYGULA artık gövdeleri tip-kontrol edilir
    - Generic param T uygula scope'una eklenir (`uygula<T> X<T>`)
    - Method dispatch: `x.method()` çağrısı `uygula_tablosu_method_bul` ile resolve
    - Inherent + trait impl methodları aynı yoldan
    - 5 yeni test (uygula gövde hatası, temiz, method dispatch, arg sayısı, trait method)

30. **Bölge Katman 2 — Concurrency aksiyomları (ADIM 20)** (+5 test)
    - `bolge_olustur_sahip(thread_id)` ve `bolge_olustur_kanal(kanal_id)` API'leri
    - `bolge_sahiplik_transfer(b, yeni_thread_id)` — R-GÖREV/R-BİRLEŞTİR scaffolding
    - `bolge_kanal_gonder(b, kanal_id)` — R-KANAL transferi
    - `bolge_donmus_mu(b)` — R-PAYLAŞ (v1: daima 0)
    - KEMGU_Bellek_Modeli.md Katman 2 aksiyomları (S1/S2/S3, R-GÖREV/R-BİRLEŞTİR/R-KANAL/R-PAYLAŞ)
    - Sınır: lang syntax (`görev`/`kanal` keyword'leri) henüz parse edilmiyor

31. **LSP v2 — Hover + Completion + Definition (ADIM 21)** (+3 test)
    - **Belge state'inde AST cache:** her didChange parse + sembol indeksleme
    - **textDocument/hover:** tanımlayıcı üzerine → markdown (ad + kategori)
    - **textDocument/definition:** tanımlayıcı → tanım konumu (uri + range)
    - **textDocument/completion:** keyword listesi + üst düzey semboller
    - Position lookup: `bul_tanimlayici_konum(prog, line, col)` — AST traversal
    - Üst düzey semboller (DUGUM_ISLEV/YAPI/OZELLIK/SABIT) toplama
    - Capabilities güncellemesi: hoverProvider + definitionProvider + completionProvider
    - Sınır: lokal değişken hover yok (yalnız üst düzey), incremental sync yok

32. **LLVM v3 — float/double + dizi + struct-by-value (ADIM 22)** (+6 test)
    - **Float/double:**
      - kesirli32 → float, kesirli64 → double
      - fadd/fsub/fmul/fdiv/frem, fcmp (oeq/one/olt/ogt/ole/oge)
      - KESIRLI literal güvenli format (decimal point garanti)
      - DUGUM_TAM beklenen kesirli ise fadd ile float literal üretir
    - **Dizi:**
      - `[e1, e2, ...]` → alloca [N x T] + GEP+store + return ptr
      - `arr[i]` → GEP (i64 index) + load T
      - Eleman tipi ilk elemanın tipinden çıkarsanır
    - **Struct-by-value:**
      - DUGUM_TIP_BASIT/DUGUM_TIP_KULLANICI artık yapı kayıtlı ise `%YapiAdi` döner (ptr değil)
      - `yapı_olustur`: alloca + store fields + LOAD struct value → akış struct-by-value
      - `erisim x.y`: struct value ise `extractvalue`, ptr ise GEP+load (eski yol)
      - Function params/returns artık gerçek `%Tip` ile çalışır
    - **Test örnekleri:**
      - `hesap(x: kesirli64) -> kesirli64 { ver x * 2.0 }` → çalışır
      - `[10, 20, 12] xs[0]+xs[1]+xs[2]` → 42
      - `iken i < 5 { s = s + xs[i]; }` → dizi indeksleme dongusu
      - `topla(c: Cift) -> tam32 { ver c.a + c.b; }` → struct param by value
      - `yap() -> N { ver N { x: 42 }; }` → struct dönüş by value
    - **Sınırlamalar:**
      - Dizi tipi parametre olarak (function arg) henüz yok
      - Dizi LENGTH bilgisi taşımıyor (sentinel veya ayrı uzunluk gerek)
      - Float karşılaştırma her zaman ordered (oeq/olt), unordered yok

28. **LLVM v2: yapılar + metin literali + multi-int (ADIM 18)** (8 yeni entegrasyon testi)
    - **Tip tracking:** her ifade artık `(reg, llvm_tip)` ikilisi döndürür
    - **Multi-int (annotation-driven):**
      - tam8/dtam8 → i8, tam16/dtam16 → i16, tam32/dtam32 → i32, tam64/dtam64 → i64
      - mantıksal → i1 (parametre/dönüş tipi olarak), arithmetic context'inde i32'ye genişler
      - karakter → i32, boş → void
      - Operandlar arasında otomatik int_donustur (sext/trunc) gereken yerde
      - Karşılaştırma operandları natural tipte (beklenen forward edilmez)
    - **Metin literali:**
      - Pre-pass: tüm DUGUM_METIN düğümleri toplanır, her benzersiz literal `@.str.N = private constant [K x i8] c"..."`
      - Reference: `getelementptr ptr @.str.N, i32 0, i32 0` → ptr
      - `metin` tipi → `ptr` (i8*)
      - Escape: özel karakterler `\HH` formatında
    - **Yapı:**
      - Pre-pass: tüm DUGUM_YAPI tanımları toplanır
      - Type emission: `%YapiAdi = type { tip1, tip2, ... }`
      - Yapı oluşturma: `alloca %YapiAdi` + her alan için `getelementptr + store`
      - Alan erişimi (`x.y`): `getelementptr + load`
      - Yapı değerleri pointer ile temsil (struct-by-value parametreler v3'te)
    - **Bonus: işlev imza tablosu** — çağrı dönüş tipi artık doğru
    - **Test örnekleri:**
      - tam8 + tam8 → 42 (i8 ops)
      - tam64 mul → 42 (i64 ops)
      - mantıksal param → 42 (i1)
      - `selam() -> metin` returning `"Merhaba"` → global string
      - `Nokta { x: 10, y: 32 }; ver n.x + n.y` → 42
      - Yapı 3 alan toplam → 42
    - **Sınırlamalar (v2'de hala):**
      - kesirli32/64 (float/double) yok
      - Yapılar by-pointer (struct-by-value parametre/dönüş yok)
      - Dizi yok
      - mantıksal `ve`/`veya` short-circuit yok

27. **LLVM backend genişletme (ADIM 17)** (16 entegrasyon testi — gerçek programlar derlenip çalıştırılıyor)
    - **Yeni desteklenen özellikler:**
      - İşlev parametreleri (i32, alloca + store ile mutable)
      - İşlev çağrısı (DUGUM_CAGRI) — özyinelemeli destek (fibonacci ✓)
      - Lokal değişken (alloca + store/load)
      - Atama (lvalue: tanimlayici → alloca'ya store)
      - if/else + nested if (br i1 + üç bb: then/else/end)
      - while loop (head + body + done bb'leri)
      - Karşılaştırma (==, !=, <, >, <=, >= → icmp + zext i1→i32)
      - Mantıksal ve/veya (and/or i32), değil (icmp eq 0 + zext)
      - Aritmetik +/-/*/sdiv/srem, tekli neg
    - **Mimari:** `LlvmGen` state + lineer isim tablosu + scope marker
    - **Test örnekleri (her biri --llvm | clang | exit code doğrulama):**
      - fib(10) → 55 ✓ (özyinelemeli + if)
      - faktöriyel(5) → 120 ✓ (while + lokal + atama)
      - gcd(48, 36) → 12 ✓ (while + parametre atama + %)
      - mutlak(-42) → 42 ✓ (if/else, iki dal da `ver`)
      - kup(3) + kare(3) → 36 ✓ (çoklu işlev)
    - **Sinirlamalar (v1):**
      - Sadece i32 (tam8/16/64, dtam*, kesirli* yok)
      - Yapılar/diziler/metin literali yok
      - Referans/pointer yok
      - mantıksal `ve`/`veya` short-circuit YOK (bitwise gibi)
    - **Test entegrasyonu:** `system()` ile `kemgu --llvm | clang -x ir | run` zinciri, exit code karşılaştırma

26. **LSP server MVP (ADIM 16)** (27 yeni test, kemgu --lsp ile çalıştırılır)
    - **16.1: `json.h/c`** — minimal JSON parser + yazıcı (~370 satır)
      - null/bool/sayı/metin/dizi/nesne
      - Escape: `\" \\ \n \r \t \b \f` + `\uXXXX` → UTF-8
      - Arena-tabanli, recursive descent
      - 21 test
    - **16.2: `lsp.h/c`** — LSP server (~310 satır)
      - JSON-RPC 2.0, Content-Length framing
      - `initialize` → capabilities response (textDocumentSync=1)
      - `textDocument/didOpen` + `didChange` + `didClose`
      - `shutdown` + `exit` (graceful)
      - `textDocument/publishDiagnostics` (otomatik gönderim)
      - Tek dosya hafıza (incremental sync henüz yok)
    - **16.3: Diagnostic toplama** (`hata.c` callback hook)
      - `hata_callback_ayarla(cb, ctx)` — set edilirse `hata_raporla` stderr yerine cb çağırır
      - LSP: parser + tip kontrol hataları → diagnostic listesi
      - 1-tabanli (KEMGU) → 0-tabanli (LSP) konum dönüşümü
      - 6 LSP test (initialize, shutdown, didOpen valid/parser/tip hata, didChange düzeltir)
    - **16.4: `ana.c --lsp`** — stdio loop'a geç, dosya okumadan
    - **Pratik kullanım:** `./build/kemgu --lsp` ile VSCode / Neovim LSP istemcisinden bağlanılabilir
    - **Sinirlamalar (v1):**
      - Hover, completion, definition, semanticTokens yok
      - workspace mesajları yok
      - Tek dosya hafıza (didChange tüm metni yeniler — incremental yok)
      - Çoklu workspace dosyası yok

25. **Constraint satisfaction — özellik/uygula sistemi (ADIM 15)** (19 yeni test)
    - **15.1: `parse_ozellik_tanimi`** — `özellik Ad<T> { işlev m() -> tip; ... }`
      - Method imzaları (`işlev m() -> T;`) ve default impl (`işlev m() { ... }`)
      - Generic params destekli: `özellik X<T> { ... }`
      - 4 parser testi
    - **15.2: `parse_uygula_tanimi`** — `uygula Trait için Tip { ... }` veya `uygula Tip { ... }` (inherent)
      - Generic uygula: `uygula<T> Tip<T> { ... }` (monomorphization sırasında resolve edilir)
      - 3 parser testi
    - **15.3: Bound sözdizimi** — `<T: Bound1 + Bound2, U: Bound3, V>`
      - Yapı/özellik/uygula generic params bound listesi alabilir
      - AST: `tip_param_boundlari` (Dugum***) + `tip_param_bound_sayilari` (int*) paralel diziler
      - 5 parser testi
    - **15.4: Sembol tablosu özellik desteği + uygula registry**
      - `SEMBOL_OZELLIK` zaten vardı; pre_populate_ozellik global scope'a ekler
      - Yeni: `UygulaTablosu` — (tip_adi, ozellik_adi) eşlemeleri için linked list
      - Sorgu: `uygula_tablosu_implementations_eder(tip, ozellik)` → 0/1
    - **15.5: Bound enforcement (T030, T031)**
      - `DUGUM_TIP_KULLANICI` resolve sırasında: her tip param için bound'ları kontrol et
      - T030: argüman tipi bound'u karşılamıyor (uygula bildirimi yok)
      - T031: bilinmeyen özellik (bound olarak verilen ad çözülemedi)
      - Generic param argümanları bound check'ten muaf (resolve sırasında enclosing scope'tan gelir)
      - 7 tip kontrol testi
    - **Pratik örnek:**
      ```
      özellik Sayilabilir {}
      yapı Tam { x: tam32; }
      uygula Sayilabilir için Tam {}   // <- gerek
      yapı Vektor<T: Sayilabilir> { ic: T; }
      sabit V: Vektor<Tam> = ...;  // OK; uygula olmadan T030 hatası
      ```
    - **Sınırlamalar (v1):**
      - Method tip kontrolü uygula gövdelerinde yok (sadece kayıt)
      - Default impl gövde tip kontrolü özellik içinde yok
      - Trait method dispatch yok (sadece bound enforcement)
      - Aynı bound iki kez raporlanabilir (annotation + constructor)
      - Makefile header dependency: `-MMD -MP` eklendi (artık `.h` değişikliklerinde otomatik rebuild)

### 🎉🎉 ADIM 12-23 TAMAMLANDI — GENERIC IŞLEV + STDLIB SEED + LLVM v3

```bash
echo 'işlev main() -> tam32 { ver 1 + 2 * 3 + 35; }' > x.kem
./build/kemgu --llvm x.kem | clang -x ir - -o x.exe
./x.exe; echo $?    # → 42 ✓
```

**Test sayısı:** 505/505 (önceki 501 + 4 generic işlev) + 3 stdlib --check geçti

### ADIM (Konsolidasyon) — Linear Types Spec V1 (`tekkez<T>` + `kullan` + `imha`)
Direktif Ek v1.1'de onaylı spec. Detay: `belgeler/KEMGU_Linear_Types_Spec_V1.md`.

- **Linear types altyapısı** — 54/54 yeni test, ASan temiz
    - **Lexer:** `tekkez`, `imha` keyword (toplam 33).
    - **AST:** `DUGUM_TIP_TEKKEZ`, `DUGUM_KULLAN_IFADE`, `DUGUM_IMHA_IFADE`.
    - **Parser:** `tekkez<T>` tip + `kullan(e)` / `imha(e)` ifade. `kullan`
      context-sensitive (üst düzey = import, ifade = consume).
    - **Tip sistemi:** `TIP_TEKKEZ` kategori, `tip_olustur_tekkez`,
      `tip_lineer_mi`, recursive nominal eşitlik, yazdırma.
    - **Sembol:** `lineer_tuketildi` ve `lineer_scope_seviyesi` flag'leri,
      `sembol_bul_yazilabilir` (mutable lookup, tüketim işaretleme için).
    - **Tip kontrol:** L001 (tüketilmedi), L002 (move sonrası), L004
      (referans yasağı), L007 (consume operandı tekkez değil), L008
      (`tekkez_olustur` arity), LR002 (yapı/dizi tekkez içeremez).
    - **Producer intrinsic:** `tekkez_olustur<T>(e: T) -> tekkez<T>` —
      özel built-in (sembol tablosu yok, CAGRI handler'ında özel-case).
    - **Tüketim noktaları:** `kullan`, `imha`, çağrı argümanı, `ver`,
      yapı alan değeri, değişken atama (move), `tekkez_olustur` iç wrap,
      lineer closure çağrısı (LC-3).
    - **Closure-itself-linear (LC-2):** Lambda gövdesi içinde lineer
      bağlama yakalandığında lambda tipi otomatik `tekkez<islev(...)>`.
    - **Region/Linear:** Bölge (blok/işlev/için/eşleş) kapanışında
      tüketilmemiş lineer bağlamalar L001 raporlanır.
    - **Örnek dosyalar:** `lineer_temel.kem` ✓, `lineer_closure.kem` ✓,
      `lineer_hata.kem` (4 hata sergiler — L001/L002/L004/LR002).

```bash
./build/kemgu --check test/ornekler/lineer_closure.kem
# → OK: lineer_closure.kem — tip kontrolu basarili.
```

### Sıradaki büyük seçenekler:
- **DRF V2 (operasyonel)** — Plan Karar B; runtime izler + C++11 weak memory model fence emit
- **Concurrency runtime** — `görev`/`kanal` lang syntax mevcut (DRF V1 statik tip kontrol);
  runtime thread/channel implementasyonu, LLVM codegen, semaforlar (Plan Karar F V2)
- **Lambda block-form gövde tip çıkarsama** (V1 sınır: lambda body ifade-form;
  block içindeki son `ver` deyimi tip dönüşü V2)
- **Inter-procedural escape analizi** (callee escape özetleri — escape.c v2)
- ~~**`hiç`/`değer` ifade desteği + pattern binding**~~ ✓ C2.5 (sonuç/seçimlik value codegen: yapıcılar + eşleş destructuring + binding). Kalan: custom ADT/enum + eşleş exhaustiveness (C2.7, syntax kararı).
- **LSP v3** (incremental sync, workspace, semanticTokens, references)
- **LLVM v4** (dizi param/return, dizi length, generic islev codegen)
- **Stdlib network/JSON/regex** (runtime altyapı sonra)
- **Linear V2:** lineer alanlı yapı (`yapı tekkez K { ... }`), L005 (koşullu tüketim tutarlılığı)
- **Linear stdlib:** `Dosya`, `OTP_Anahtar`, `Kilit` runtime tipleri (Spec B.6)
- **Self-host bootstrap** (uzun vade — KEMGU ile KEMGU)

### Self-host AŞAMA durumu (D-035..D-087)
- ~~AŞAMA 1: lexer self-host~~ ✓ (sıfır-diff bootstrap)
- ~~AŞAMA 2: parser + checker self-host~~ ✓ (`--ast`/`--checkdump` sıfır-diff)
- ~~AŞAMA 3: codegen self-host (CG1-CG9a)~~ ✓ (semantik exit-kod eşdeğerlik; CG8 dizi dâhil)
- ~~AŞAMA 5: codegen self-compile FIXPOINT~~ ✓ **D-085/D-087** (stage1==stage2 + 4 bileşen
  doğruluk: lexer46+parser46+**checker46**+codegen-fixpoint, `calistir_codegen_bootstrap`)
- ~~**AŞAMA 4: tek self-host kemgu binary (driver)**~~ ✓ **D-086** — `selfhost/codegen.kem`
  artık birleşik driver: checker + `--token/--parse/--check/--llvm` dispatch →
  `build/kemgu_self.exe` (no-flag→--llvm geri uyum). `make calistir_self_driver`:
  HEM C-derlenmiş HEM **self-host-derlenmiş** driver 4 modda C oracle ile eşleşir
  (TOKEN 22/22, PARSE 12/12, CHECK 48/48, LLVM 56/56) + driver kendini fixpoint olarak
  üretir (21728 satır IR kararlı).
- **SIRADA:** tek-kaynak konsolidasyon (checker.kem ↔ driver) — checker mantığı iki yerde
  (driver + Aşama 2 referans checker.kem); ileride driver tek-kaynak olabilir.

### İlerideki Fazlar
- ~~Tip sistemi (tip çıkarsama, tip kontrolü)~~ ✓ ADIM 11
- ~~Linear types (Spec V1)~~ ✓ (konsolidasyon)
- ~~Bölge çözümleyici (escape analizi, bölge atama)~~ ✓ ADIM 12 + 14
- LLVM backend genişletme (parametreler, kontrol akışı, yapılar, dizi, çağrı)
- Concurrency (R-GÖREV, R-BİRLEŞTİR, R-KANAL — Katman 2)
- Constraint satisfaction (uygula bound kontrolü)
- LSP server (IDE entegrasyonu)
- Stdlib (network/JSON/regex)
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

- **Faz:** **🎉🎉 KONSOLIDASYON — TİP + BÖLGE + LLVM + ESCAPE + CONSTRAINT + LSP + LİNEER FAZLARI TAMAMLANDI**
- **Tamamlanan:** Lexer → Parser → AST → Tip → Bölge (temel + DFA escape) → LLVM IR → native exe + LSP server + **Linear Types Spec V1 (`tekkez<T>` + `kullan` + `imha`)**
- **Sıra:** ~~11.1-11.7~~ ✓ → ~~12.1-12.2~~ ✓ → ~~13.1~~ ✓ → ~~14.1-14.2~~ ✓ → ~~15.1-15.5~~ ✓ → ~~16.1-16.5~~ ✓ → ~~Linear V1~~ ✓ → **(genişletme: Katman 2, LLVM v4, LSP v3, stdlib, Linear V2, bootstrap)**
- **Tip sistemi tasarım kararları (kullanıcı onayladı):**
  - Çıkarsama: Lokal + Bidirectional (Rust/Swift tarzı)
  - Generic: Monomorphization (Rust gibi)
  - Eşitlik: Nominal (Rust/Java gibi)
  - Constraint: ŞIMDILIK YOK (ileride)
  - Sayı literal: Context-dependent, default tam32
  - Bölge: Önce tip, sonra ADIM 12'de bölge
- **Escape analizi tasarım kararları:**
  - Forward DFA + fixed-point iterasyonu (max 16 pass)
  - Per-AST-node escape kategorisi (linear arama haritası)
  - Değişken bağlama: scope stack, append-only on assign (MAY-flow konservatif)
  - `bolge_atama` ile opsiyonel entegrasyon (NULL → eski syntax modu)
- **Sıradaki hedef:** Kullanıcı önceliği belirler. En doğal sıralama: (a) LLVM backend genişletme (çağrı/yapı/dizi), (b) constraint satisfaction, (c) `hiç`/`değer` ifade desteği + pattern binding, (d) inter-procedural escape v2, (e) Bölge Katman 2 (concurrency), (f) LSP, (g) stdlib, (h) self-host.

---

## Bu Oturumun Çalışma Kuralları

- Her dosya yazıldıktan sonra `make` çalıştır, sıfır uyarı hedefi (`-Wall -Wextra -Wpedantic`)
- Test binary'leri için ASan derleme bayrakları: `-fsanitize=address,undefined -g -fno-omit-frame-pointer`
- Bellek alan modüller için her test çalıştırmasında ASan aktif olmalı; `ERROR: AddressSanitizer` çıkarsa adım onaylanmaz
- Her mantıksal birim bitince Türkçe küçük git commit
- Türkçe UTF-8 hex escape kuralı: ayrıntı için yukarıdaki **"Türkçe UTF-8 Dikkat Noktası"** bölümüne bak (kural: her Türkçe karakter escape'inden sonra 0-9 / a-f / A-F geliyorsa concatenation şart)
