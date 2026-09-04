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

# Sadece arena testleri (VAR — 19/19; "YAPILACAK" notu D-492'de düzeltildi)
make calistir_arena_test

# Sadece AST testleri (VAR — 31/31)
make calistir_ast_test

# Sadece parser testleri (VAR — 90/90)
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
│   ├── test_llvm.c                    — LLVM backend entegrasyon (derle + çalıştır + exit kodu) (TAMAMLANDI ✓ — 263/263, multi-int + metin + yapı + float/dizi/struct-by-value + Katman 2 görev/kanal/dondur + lambda dönüş çıkarsaması + görev<T> genişletme + D-295 bloker onarımları + D-297 dar-T/örnek kapsamı)
│   ├── test_gorev_rt.c                — Katman 2 görev+kanal runtime — 13/13; GERÇEK thread + S1/S2 ρ ayrıklığı + kanal BLOKLAMA + sıralı-fallback-yok invaryantı (D-291/292/296)
│   ├── test_drf.c                     — Concurrency/DRF tip kontrolü — 50/50 (D40-D46 kanal_oluştur; D47-D50 görev/kanal kesirli reddi + metin/tam64 kabulü)
│   └── ornekler/
│       ├── hasta.kem                  — Mevcut örnek (TAMAMLANDI ✓)
│       ├── gorev_temel.kem            — Katman 2: görev_başlat/birleştir (D-291) — exit 42
│       ├── kanal_mesaj.kem            — Katman 2: kanal mesaj geçişi + akış denetimi (D-292) — exit 15
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

### 41 Anahtar Kelime (D-492: "35" idi, ÖLÇÜLDÜ 41)
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

### Self-host dizi temsili — KRİTİK güvenlik invaryantı
- Self-host derleyici (`selfhost/*.kem`) **heap-uniform**: diziler **her zaman** heap'te (`KdlDizi*`), stack `[N×T]` yolu YOK.
- Sonuç: heap dizi erişimi **runtime'da** sınır-kontrollü (`kdl_runtime.c` — hem `i < 0` hem `i >= boyut`) → self-host codegen'de **inline stack-OOB kontrolü hiç gerekmez**. `codegen.kem`'de `icmp uge = 0` olması "eksik kontrol" değil, "stack dizi yok" demektir.
- C derleyici (bootstrap) hem stack hem heap diziyi destekler → orada inline stack sınır-kontrolü gereklidir (D-069; `güvensiz` blokta opt-out).
- **KURAL:** Self-host yoluna ileride stack dizi (`[N×T]`) EKLENİRSE, inline stack-OOB kontrolü **aynı commit'te** zorunludur. Kontrolsüz stack dizisi = bellek-güvenliği regresyonu.

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

**Test sayısı (TARİHÎ — ADIM 23 anı):** 505/505 + 3 stdlib --check.

> ⚠ **[D-492] BU SAYI ÇOK ESKİDİR — güncel ölçü `make test_tumu`dur.**
> Bugün tek başına `llvm_test` **286**, `checker_diff` **162**, `codegen_diff`
> **156**; `test_tumu` **67 kapı** çağırıyor. Belgeye gömülü test sayıları
> yazıldıkları gün doğru, ertesi gün bayattır — bu dosyada üç ayrı yerde
> yaşandı (bu satır · yukarıdaki "YAPILACAK" yorumları · `SORRY_TRACKER.md`
> D-491). **Sayıyı belgeye gömmek yerine kapının kendisini koştur.**

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
- ~~**Concurrency runtime — `görev` tarafı**~~ ✓ **D-291** (Katman 2 CANLI): `görev_başlat`/
  `görev_birleştir` codegen + GERÇEK host thread (`kdl_gorev_basla_kapanis`); her görev
  kendi ρ_sahip'ini alır (S1/S2 yapısal — test [7] ρ ayrıklığını ölçer). `dondur` V1'de
  identity. **D-294:** runtime i64 taşımaya geçti → **`görev<metin>` çalışıyor**
  (T ∈ {≤64-bit tamsayı, işaretçi-benzeri, boş}). **Kesirli T REDDEDİLİR** (DRF001):
  runtime sonucu x0/rax'tan okur, float v0/xmm0'dadır → bitcast sessiz çöp olurdu;
  `--llvm` tip kontrolünü atlasa bile `trunc i64→double` geçersiz → LLVM gürültülü reddeder
  (katmanlı savunma, ikisi de ölçüldü). **Kalan (bu sırayla):**
  - ~~**`kanal`**~~ ✓ **D-292** + **D-295**: `kanal_oluştur<T>(kapasite)` kurucusu (T beklenen
    tipten; bağlamsız → DRF006) + gönder/al codegen + **BLOKLAYAN** host runtime (koşul değişkeni).
    Mehmet kararı (D-292): tek yönsüz `kanal<T>`. **D-303 (Karar 2) BUNU GENİŞLETTİ:**
    `gönderen<T>`/`alan<T>` yön uçları eklendi (projeksiyon: `gönderen(k)`/`alan(k)` identity;
    `kanal_gönder` gönderen|kanal, `kanal_al` alan|kanal — yanlış yön DRF007). `kanal<T>`
    full-duplex fabrika kalır (geriye-uyumlu). `TIP_KANAL.yon` alanı; keyword DEĞİL (tip
    pozisyonunda generic-user-tip, `alan` serbest tanımlayıcı kalır; projeksiyon düşüşe-güvenli
    `fn_var_mi`/`!ik`). V1: uçlar lineer değil. Bedeli D-292'de "yön garanti EDİLMEZ"di; artık
    uç-tutucu için derleme-zamanı garanti var. Kapatılan asıl kusur: boş kanalda `kanal_al` 0
    dönüyordu — gerçek bir 0'dan ayırt edilemez (sessiz yanlış cevap); dolu kanalda gönderim
    mesajı sessizce düşürüyordu. Kanal/görev ABI'si host+bare-metal'de artık AYNI.
    **D-295:** tampon `int64_t` → `kanal<tam64>`/`<metin>`/`<Dizi<T>>` GERÇEKTEN çalışır
    (D-292'nin 32-bit kısıtı KALKTI — o kısıt yalnız `--check`i kapatıyordu, `--llvm` yolundan
    sessiz kırpma sızıyordu). Kalan kısıt: **kesirli T** (DRF006, katmanlı savunma).
- ~~**Sıralı görev fallback'i**~~ ✗ **KALDIRILDI — D-296**: thread yaratılamazsa görevi sıralı
  çalıştırmak, gövde bloklayan bir kanal işlemi yaparsa **KALICI KİLİTLENME** üretiyordu
  (ölçüldü: aynı program, simüle spawn hatasıyla — eski exit 124/asıldı, yeni açık panik).
  Koddaki "görev semantiği korunur" iddiası yanlıştı: **safety korunuyor, liveness kayboluyor.**
  Sıralı çalıştırma bir eşzamanlılık ilkelinin geçerli yedeği DEĞİLDİR (runtime gövdenin
  bloklanıp bloklanmayacağını bilemez). Aynı adımda `görev_birleştir(NULL) → 0` sessiz-0'ı
  da kapatıldı.
- ~~**`görev_başlat` spawn başarısızlığı = panik**~~ ✓ **ÇÖZÜLDÜ — D-301 (Karar 1, Mehmet):**
  `görev_başlat` artık `görev<T>` değil **`sonuç<görev<T>, metin>`** döner (panik yerine
  DEĞER — çökmezlik). Runtime spawn başarısızsa NULL döner; codegen dallanmasız
  `{i8,ptr,ptr}` ile sarar (görev<T>+metin ikisi de ptr → aggregate T-bağımsız). C+self-host
  paritesi + FIXPOINT. ~~**Bedel (V1 known-limit, T18):** `sonuç` içindeki lineer `görev<T>`
  için L001 leak uyarısı tetiklenmez (`tip_lineer_mi` sonuç'a özyinelemez) — eşleş'siz
  düşen görev join edilmez.~~ ✓ **KAPANDI — D-467.** Sınır yalnız `görev<T>` için
  görülmüştü ama `Dosya` (D-452) ve `Baglanti` (D-466) aynı deseni sonradan
  kullanınca **sessizce yayıldı** — üç alt-sistemde birden sızıntı, tanı yok.
  Onarım üç parçalı (özyineleme + atılan-lineer-değer + `eşleş` skrutini
  tüketimi); T18 testi bilinen-yanlışı sabitlediği için tam tasarlandığı gibi
  kırmızıya dönüp bildirdi. **Ergonomi:** her görev_başlat bir `eşleş` ister (`?`-yayılımı V2).
  Hata tipi V1'de `metin`; payload'lı `çeşit` gelince `GörevHata`ya yükseltilebilir.
- **⚠ TEST ALTYAPISI (D-297):** `test_llvm.c` / `test_simd_llvm.c` geçici dosya yolları artık
  **PID ile benzersiz** (`build/test_llvm_<pid>.kem` …) ve koşum sonunda siliniyor. Sabit yol,
  aynı testin **iki eş zamanlı koşumunda** birbirini ezip **sahte kırmızı** üretiyordu
  (252/252 yerine 157/252 gözlendi — kaynak TEMİZDİ, regresyon YOKTU). Yeni derle-çalıştır
  testi eklerken bu deseni koru; sabit `build/*_temp.*` yolu KULLANMA.
- **⚠ SÜREÇ DERSİ (D-295):** ön-merge adversarial denetim, D-291→D-294'te **3 BLOKER** buldu —
  hepsi "tip kurtarılamadı → sessizce `i32` varsay" kökünden; diff hata modunu **loud→silent**
  çeviriyordu. **Taşıyıcı genişliğini (i32→i64) değiştirince TÜM fallback'leri denetle.**
  Ayrıca: LLVM `declare` imza uyuşmazlığını **sessizce kabul eder** (ölçüldü) — "yanlış tip geçir,
  LLVM reddetsin" bir savunma mekanizması DEĞİLDİR.
  - ~~**ρ_sahip serbest bırakma**~~ ✓ **D-309**: KOŞULLU serbest — codegen POZİTİF
    hapsedilme kanıtı üretir (P1 dönüş skaler / P2 kanal yükü skaler / P3 iç-içe görev
    yok / P4 dolaylı çağrı yok; **`default:` DENY** → bilinmeyen düğüm kanıtı düşürür),
    `rho_serbest` bayrağıyla runtime'a taşır, `görev_birleştir` yalnız kanıtlıysa
    `kdl_bolge_serbest` çağırır. Kanıtsız = eski davranış (sızdır). Kaçış yüzeyi ÖNCE
    ölçüldü: **kanal CANLI kaçış** (naif serbest = UAF, ampirik), dönüş LLVM-RED,
    küresel E011, yakalanan-değişken env-kopyası. Ölçüm kapısı `kdl_bolge_bakiye`
    (test_gorev_rt [14]-[16]): bayrak 0 → +1 sızıntı, 1 → +0. C↔self 10/10 birebir +
    FIXPOINT. Kendi kanıtımda 2 kusur ölçümle yakalandı (`kanal_gönder` bayt uzunluğu
    13≠14; **ifade-form lambda gövdesi P1'i atlıyordu** = soundness deliği).
  - ~~Semaforlar / bariyerler (Plan Karar F V2)~~ ✓ **D-456**
  - `kanal` KEMGU-OS'ta (bare-metal .kem): ABI hazır (aynı imza), test yok
- ~~**Lambda ifade-form dönüş-tipi çıkarsama**~~ ✓ **D-293**: lifted lambda dönüşü artık
  gövdenin doğal IR tipinden çıkarsanıyor (eskiden SABİT i32 → `|| "selam"` / `|| 3.5`
  tip kontrolünden geçip LLVM'de patlıyordu). Closure çağrı yeri dönüş tipini **bildirilen**
  `işlev(...) -> T`'den alıyor (`LlvmIsim.kapanis_donus_ir`) — fat value `{ptr,ptr}` T'yi
  sildiği için bu şart; olmadan `metin_uzunluk(f())` segfault veriyordu.
  ~~**Kalan:** blok-form gövde i32~~ ✓ **ÇÖZÜLDÜ — D-304:** blok-form (`|| { ...; ver x; }`)
  artık hem tip kontrol (gövde deyim olarak, dönüş ver'lerden çıkarsanır) hem codegen
  (bildirilen `işlev()->T` IR'ı, `lambda_beklenen_donus`) uçtan uca çalışır — ifade-form ile
  parite (metin/kesirli/tam32/çok-deyim). ~~C-only (self-host genel closure desteklemez)~~
  ✓ **D-322: SELF-HOST'ta da var** (fat value {fnptr,envptr}; blok-form dâhil). Ortak
  pre-existing sınır: `işlev()->tam64 = || 8589934592` (büyük literal default'u; ifade-form da).
  **NOT (D-291 düzeltmesi):** bu, `görev<T>`'yi TEK BAŞINA AÇMAZ — `kdl_gorev_birlestir`
  de i32 döner, `kanal<T>` sınırı ise runtime tamponundan (int32_t). Genişletme runtime işi.
- **⚠ İMZA-ÜSTÜ TİP DENETİMİ EKSİKTİ — D-369.** `kontrol_govde` yalnız BLOK'a
  iniyordu → işlev **dönüş ve parametre tiplerindeki** tanılar (DRF001/DRF006/
  CT006/T030/T031) HİÇ ÇIKMIYORDU (`işlev f() -> görev<kesirli64>` sessizce
  geçiyordu). Tek satırlık gezinti düzeltmesi D-366/367/369'un kapsamını birden
  genişletti. T030/T031 portlandı; TIP_KULLANICI adı **çocuk[0]'daki TANIMLAYICI**
  düğümünde (yan-kanal gerekmedi), yalnız bound tablosu için `gp_*` eklendi.
  **Kapsam 70/74; kalan 4'ün 2'si ölü → gerçekte 2** (T011/T014, tip evreni).
- **ÇEŞİT ALT-SİSTEMİ KAPANDI (4/4) — D-368.** M004 için `cv_*`e payload TİP tablosu
  eklendi (`cv_pb`/`cv_pt`). Beklenen tip BAĞLAM olarak geçmeli — `Dar(tam8)` +
  literal `5` geçerlidir (S68 sabotajı: bağlamsız hâlde sahte M004).
  **Kapsam 68/74; kalan 6'nın 2'si ölü → gerçekte 4** (T011/T014 tip evreni,
  T030/T031 generic bound).
- **SABİTSÜRE ALT-SİSTEMİ KAPANDI (8/8) — D-367.** Taint yayılımı pahalı sanılmıştı;
  ölçüm TEK ÖZYİNELİ YÜKLEM olduğunu gösterdi (aritmetik/bit operandı sabitsüre ise
  sonuç da sabitsüre; karşılaştırma ve `ifşa` dışarıda). Yol üstünde MEVCUT bir
  yanlış-pozitif bulundu: **`ifşa` (ş ile!)** built-in listesinde yoktu → geçerli
  declassify çağrıları sahte T002 alıyordu (D-361'de ASCII `ifsa` denenip yanlış
  gerekçeyle elenmişti). **Kapsam 67/74; kalan 7'nin 2'si ölü → gerçekte 5.**
- **DRF ALT-SİSTEMİ KAPANDI (7/7) — D-366.** Kesirli T reddi İKİ yolda (annotasyon
  + yapıcı) ve konumlar farklı düğümlerde (DRF001→argüman, DRF006→çağrı,
  DRF007→argüman). **Yön TİPTEN OKUNAMIYOR:** `alan<T>` → `TIP_KULLANICI` ve
  kullanıcı-tipi adı düğümde YOK → yön DEĞERDEN (`alan(k)`/`gönderen(k)`
  projeksiyonu) okunuyor (`yerel_yon`). **Kapsam 59/74**; kalan 15'in 8'i sabitsüre.
- **MMIO + YETKİ ALT-SİSTEMİ KAPANDI (5/5) — D-365.** CP005 **yeni kural değil**:
  L001/L002/L004'ün yetki karşılığı. `tip_node_tekkez_mi`'ye `TIP_YETKI` eklendi →
  mevcut lineer makinenin TAMAMI yetki için çalışıyor; `lin_yet` biti yalnız
  raporlanan KODU seçiyor (D-313'ün `yapı tekkez` deseni). `geri_al` TÜKETİR,
  `mmio_*`/`bölge_al` ÖDÜNÇ alır. **Kapsam 52/74.**
- **MMIO + yetki — D-364.** MM001/MM002/MM003 + CP004 portlandı (kapsam **51/74**).
  Yeni yan-dizi `yerel_yet` (`yetki<R>` kaynak adı). **CP005 bilinçli olarak YOK:**
  `deg_lineer_mi` TIP_YETKI'yi saymıyor ve kod L002 değil CP005 olmalı → lineer
  makineye yetki-ayrımlı yol eklemek ayrı adım.
- **MODÜL ALT-SİSTEMİ KAPANDI — D-363.** T041 (private-by-default) portlandı;
  engel `genel`in düğüme yansımamasıydı (`parse_genel` çıplak tanım döner) →
  `gen_node` yan-kanalı. **`checker_diff` 120/120, MUAFİYET LİSTESİ BOŞ.**
  Kapsam **47/74**; kalan 27'nin 2'si ölü, 23'ü dört alt-sistemin tip temsiline
  bağlı. **Tek tek portlanabilecek genel-amaçlı kod KALMADI** — bundan sonrası
  alt-sistem işi (sabitsüre / DRF / MMIO+yetki) ya da tip evreni.
- **RUNTIME UTF-8 YOL ONARIMI — D-362.** `runtime/kdl_runtime.c` düz `fopen`
  kullanıyordu → Windows ANSI codepage yüzünden `kütüphane/` **açılamıyordu**
  (self-host o modülleri SESSİZCE yüklemiyordu). `kdl_fopen_utf8`
  (`MultiByteToWideChar`+`_wfopen`, ana.c deseni) eklendi, 8 çağrı yeri çevrildi.
  Ölçüm: aynı IR, eski runtime exit 7 → yeni runtime exit 42. **Bayat-obje tuzağı
  yaşandı:** `stash pop` sonrası make "up to date" dedi; `rm -f` şart.
  Ardından T040 + T016 açıldı (kapsam **46/74**), muafiyet 2 → 1.
- **⚠ MODÜL YÜZEYİ — D-361.** `test/moduller/` ölçülünce
  self-host'un **geçerli programları reddettiği** 3 yanlış-pozitif çıktı: 6 eksik
  built-in (`bölge_al` vb.) ve seçili import adlarının parser'da ATILMASI. Onarıldı;
  T042 portlandı (kapsam 44/74). **T040 GERİ ALINDI:** sessiz "modül bulunamadı"
  atlaması gerçek bir kusuru maskeliyormuş — C `ana.c` `MultiByteToWideChar`
  kullanıyor çünkü Windows `fopen` ANSI codepage'e düşüp `kütüphane/` yolunda
  BAŞARISIZ olur; self-host runtime bunu yapmadığından **`kütüphane/` modülleri
  sessizce yüklenmiyor**. Önce runtime yol onarımı, sonra T040.
  `checker_diff` artık `test/moduller/`i de kapsıyor: **88 → 118 dosya** (2 muaf).
- **KALAN TANI KODLARININ HARİTASI — D-360.** `--token` 235 dosyada temiz. Kalan
  31 kodun **2'si ÖLÜ** (T015/T023 — C parser'ı o şekilleri reddediyor, portlanmayacak),
  kalan 29'un **23'ü yalnız 4 alt-sistemin tip temsiline bağlı**: sabitsüre (CT001-008),
  DRF (DRF001-007), MMIO+yetki (MM001-003/CP004-005), modül (T016/T040-042).
  Dağınık kalanlar: T011/T014 (tip evreni), T030/T031 (bound), M004 (payload tip).
  **Genel-amaçlı ucuz port sınıfı D-358'de bitti.** E013 portlandı (43/74):
  `cip_bag` `guv_bag`'den AYRI tutulmalı — `güvensiz` blok işlevi çıplak YAPMAZ.
- **⚠ PARSE KAPISI SESSİZ AYRIŞMIŞTI — D-359.** `--parse` 12 dosyada yeşilken 212
  gerçek dosyada **8 sapma** vardı (`küresel`: C'de DUGUM_DEGISKEN+bayrak, `--ast`
  "DEGISKEN" basar; self-host ayrı KURESEL düğümü). Dump eşlendi (`dump_ad`), iç ad
  korundu. **Kapı büyüklüğünü periyodik ölç:** mevcut check/cg korpuslarını
  `--parse`'tan geçirmek bedava genişletmedir. **T015 ve T023 ÖLÜ tanı** (C parser'ı
  o şekilleri zaten reddediyor) → portlanmadı; kalan kod listesini körü körüne tüketme,
  önce ULAŞILABİLİRLİĞİ ölç. T011 ertelendi (generic tip paramları parser'da atılıyor).
- **G002/G003/G004 — D-358.** Üçü de "kabul et, çalışırken çök"ü derleme zamanına
  çeker. G003 İKİ daraltmalı: yalnız **annotasyonsuz** bağlama yasak (annotasyonlu
  `Dizi<T>` ve doğrudan literal argüman serbest). **Kapsam 42/74** — kalan 32 kodun
  tamamı özel alt-sistem (CT*/DRF*/MM*/CP*) ya da modül/generic.
- **M002/M003 — D-357.** ÜÇ ayrı bölge (değer `Çeşit::V` / yapıcı `Çeşit::V(a)` /
  desen), C'de üç ayrı kod. Desende varyant **skrutininin tipinden** aranır, desen
  önekinden DEĞİL. Çıplak desen (`Secim::Bir =>`, alt-desen yok) M003 VERMEZ.
  **M004 portlanmadı** (payload tip tablosu + generic substitüsyon). **Kapsam 39/74.**
- **E011/E012 — D-356.** Küresel tip yalnız skaler/ham-işaretçi, init yalnız
  sabit-literal (`METIN` literal listesinde YOK → `küresel s: metin = "a"` hem
  E011 hem E012 alır). C'nin `pre_populate` 4. geçişine yerleştirildi (T024 ile
  serpiştirme sırası korunur). **DERS:** sabotaj S31 ilk turda SESSİZ kaldı —
  kural doğruydu ama korpusta o şekil yoktu; `tc13_03` eklenince kırmızıya döndü.
  Sabotajın sessizliği bir SONUÇTUR: korpusu genişlet, kuralı silme. **Kapsam 37/74.**
- **L007/L008 — D-355.** `kullan` YALNIZ `tekkez<T>` alır (`yapı tekkez` almaz);
  `imha` her lineeri alır; hata → operand tüketilmez (L001 kaskadı C ile birebir).
  **DERS:** yan-dizi bitlerini "pozitif bilgi" olarak kur — `0` çoğu zaman
  "hayır" değil "bilinmiyor"dur (annotasyonsuz `değişken t = tekkez_olustur(..)`
  bunu ölçtürdü; sabotaj S28 kalıcı kapı). **Kapsam 35/74.**
- **T013 + LİTERAL UYARLAMA — D-354.** T013'ün iki yolu var ve **suçlanan eleman
  farklı** (annotasyon varsa ilk eleman suçlanabilir) → `dizi_bek` bağlamı şart.
  Yol üstünde ayrı bir kusur: `ifade_tip` tamsayı literalini **kesirli** bağlama
  uyarlıyordu (C uyarlamaz) → `değişken x: kesirli64 = 1` sessizce geçiyordu.
  **T014 portlanmadı** (boş dizi bağlamı C'de sezgiye aykırı: `ver []` OK ama
  `g([])` T014 → gerçek beklenen-tip yayıcısı ister). **Kapsam 33/74.**
- **"YANLIŞ ŞEKİL" TANILARI — D-353'te self-host'a portlandı:** T005/T006/T007/
  T008/T027 (hepsi tek kalıp: tip bilinen skaler, bağlam yapı/dizi/işlev istiyor).
  INDEKS sırası C ile birebir (ptr→T005/G001, skaler→T008, dizi→T005). Yeni
  yan-dizi `yerel_dizi`. **Self-host checker kapsamı 32/74** (D-350'de 24).
  Kalan 42 kod çoğunlukla özel alt-sistem (sabitsüre/DRF/MMIO/yetki/modül).
- **`eşleş` KAPSAYICILIK (M001) — D-352'de self-host'a portlandı** (çeşit dalı).
  Yan-kanal: `cv_cesit`/`cv_ad` (varyant adları — `parse_cesit` bunları ATIYORDU)
  + `yerel_ham` (süzülmemiş annotasyon tip adı; `yerel_tip` çeşidi "?"e düşürüyor).
  ~~**Bilinçli sınır:** `seçimlik<T>` ve `sonuç<T,H>` dalları YOK — self-host'ta
  bileşik tip temsili yok; kapanması tip çıkarsaması işine bağlı.~~
  ⚠ **BU SINIR ARTIK YOK (D-522'de ÖLÇÜLDÜ).** Dört şekilde de C ↔ self-host
  **birebir** M001 (kod+satır+sütun): çeşit eksik varyant · `seçimlik` eksik
  `hiç` · `sonuç` eksik `hata` · joker (`_`) TEMİZ. Sınır bir ara kapanmış ama
  **not güncellenmemişti** — şekiller korpusta olmadığı için kapı da hiç
  ölçmüyordu. `tc44_01_esles_kapsayicilik.kem` ile kapıya bağlandı.
  **DERS:** "bilinçli sınır" notu da bir İDDİADIR; başlamadan önce ÖLÇ
  (D-406'nın *"muafiyet gerekçesi de bir iddiadır"* dersinin tekrarı; bu
  oturumda EBNF ve SORRY_TRACKER'da da aynı sınıf çıkmıştı).
- **GÜVENSİZ-TIER KAPILARI — D-351'de self-host'a portlandı.** `G001` (ham işaretçi
  deref/indeksleme) + `E010` (küresel erişim) artık self-host checker'da da var;
  `çıplak` gövde örtük güvensiz (yan-kanal `cip_node`/`g_ciplak` — düğüme alan
  eklemek `--ast` paritesini bozardı). Öncesinde self-host **güvensiz programı
  işaretsiz geçiriyordu**. *(Kapsam notu ESKİDİ — o gün 26 koddu; D-387 itibarıyla
  **70** ve portlanacak kod kalmadı. Güncel durum için "SELF-HOST CHECKER PARİTESİ
  TAMAMLANDI" bölümüne bak.)*
- **`olarak` TİP KURALLARI — D-350'de self-host'a portlandı.** E001/E002/E003/E004 artık
  self-host checker'da da var (kod+satır+sütun C ile birebir; `güvensiz` ayrımı dâhil —
  D-248 `tamsayı↔*T`, D-349 `&T→*U`). Kök neden tek satırdı: `kontrol_dugum` başındaki
  `metin_baslar(ad,"TIP_")` koruması `TIP_DONUSTUR`'u yutuyordu — oysa o bir İFADE.
  **DERS (PR #108 deseninin tekrarı):** `checker_diff` yeşilse kural DOĞRU demek değil,
  korpusta o şekil YOKSA kapı sessizce yeşil kalır. Yeni checker kuralı = korpusa
  uyandırıcı örnek (`test/check_korpus/tc7_*`) + kural-başına sabotaj kapısı.
- **PARİTE BORCU — D-300'de TAM KAPANDI.** D-299: `kanal_oluştur/gönder/al`, `dondur`,
  `görev_birleştir` (+ `ll_tip` görev/kanal→ptr, `cg_aic`, i64 genişlet/daralt). **D-300:
  `görev_başlat` + closure/lambda codegen** — capture analizi (flat-AST, C
  `lambda_serbest_tara` aynası) + HEAP env + lifted-lambda GÖVDE-SONRASI emisyon
  (doğrudan-stdout için kuyruk; C'nin tmpfile+hoist_renumber yerine). Lifted lambda
  DAİMA i64 döner (runtime KdlGorevBare ABI) → imza-önden-yazma sorunu çözülür.
  Korpus **84** (+3: cg_gorev_baslat/capture/kanal). D-299 yan bulgusu: self-host `TAM`
  literalini daima i32 sayıyordu → `kanal<tam64>`de 2^33 sessizce bozuluyordu
  (`i64_genislet` immediate'ı tam genişlikte materyalize ederek onardı).
  **SONUÇ: D-291→D-297'nin tamamı self-host codegen'de; C ile birebir eşdeğer.**
  Blok-form lambda dönüşü D-304'te ÇÖZÜLDÜ. **D-341: KAPANIŞ KONTEYNERDE de self-host'ta**
  — `k.fn()` (yapı alanı) + `xs[i]()` (dizi elemanı, by-value 16B `kdl_dizi_*_yapi` yolu),
  ortak `fat_cagri_uret` dispatch; 5/5 şekil C↔self birebir, korpus 105→107. D-334'ün
  parite borcu KAPANDI. ~~Kalan: self-host dizi elemanı olarak `%Yapi`~~ ✓ **D-342:**
  nominal yapı dizi elemanı da self-host'ta (by-value memcpy + `ptrtoint getelementptr`
  boyut hesabı; 5 çağrı yeri ortak `dizi_yapi_*_emit` yolundan). **Port, C'de SESSİZ
  YANLIŞ CEVAP buldu:** `için` dalı by-value elemanı `@kdl_dizi_al_tam` ile okuyordu
  (declare i32 vs call `%Nokta` — LLVM sessizce kabul eder) → exit 14, doğrusu 42.
  Onarıldı + test_llvm 286'da kilitlendi.
  **D-322: GENEL KAPANIŞ da self-host'ta** —
  fat value `{fnptr, envptr}`, lifted ABI `(ptr %rho[, ptr %env], params...)`, dönüş IR'ı
  BİLDİRİLEN tipten (`cg_aic`). 6/6 şekil C↔self birebir. ~~kapanış-parametresi C'de de
  parser-red~~ **YANLIŞ ÖLÇÜM (D-323): kapanış parametresi İKİSİNDE DE ÇALIŞIYOR** (eski
  testte işlev adı olarak `uygula` anahtar kelimesi kullanılmıştı). **D-323:** G005
  daraltıldı — yalnız İŞARETÇİ yakalayan kaçan kapanış reddedilir (env HEAP olduğu için
  skaler yakalama güvenli). ~~⚠ G005 self-host'ta YOK~~ ✓ **D-324: PORTLANDI** — C'nin
  ESC_CAGIRAN tetikleyicileri ölçülerek taklit edildi (`ver <ad>` ∨ çağrı ARGÜMANI; çağırmak
  kaçış değil), 5 şekilde kod+satır+sütun birebir; checker_diff 56/56, 3 sabotaj kapısı.
  **D-325:** annotasyonsuz kapanış dönüşü artık GÖVDEDEN çıkarsanıyor (ikisinde de) —
  C'de SESSİZ YANLIŞ CEVAP vardı (`define double` / `call i32`; LLVM dolaylı çağrıda imza
  denetlemez → exit 127/105, doğrusu 42), self'te LLVM-RED. Aynı tahmin hem define'a hem
  çağrı yerine verilir → yapısal olarak ayrışamazlar.
  *(Eski not:)* görev/kanal codegen (D-291/D-292) ve lambda dönüş çıkarsaması (D-293)
  `selfhost/codegen.kem`'de YOK → C derleyici ileride. Gateler geçiyor (korpusta bu
  şekiller yok) ama port ayrı iş olarak duruyor.
- ~~**Skaler referans okuma**~~ ✓ **ÇÖZÜLDÜ — D-305:** `*v` ile `&T`'den `T` oku (güvenli
  referans deref — güvensiz GEREKMEZ; ham pointer `*T` hâlâ G001 ister). OP_DEREFERANS
  artık TIP_REFERANS'ı kabul eder; codegen zaten load ediyordu (D-265 yolu). C+self-host
  parite, codegen_diff 86/86. **auto-deref DEĞİL** (`ver v` hâlâ T020) — IR opak-ptr
  sessiz-miscompile riski; explicit `*v` seçildi (AST tip bilinir, loud>silent).
- **Inter-procedural escape analizi** (callee escape özetleri — escape.c v2)
- ~~**`hiç`/`değer` ifade desteği + pattern binding**~~ ✓ C2.5 (sonuç/seçimlik value codegen: yapıcılar + eşleş destructuring + binding). ~~Kalan: custom ADT/enum + eşleş exhaustiveness~~ ✓ **ZATEN YAPILMIŞ** (2026-07-25 ölçümü): çeşit ADT var (D-302/D-306) ve exhaustiveness **M001** ile denetleniyor — eksik varyantı adıyla söyler (`M001: esles exhaustive degil — eksik varyant(lar): [Mavi]`). Madde eskimişti.
- **LSP v3** — ⚠ **KISMEN YAPILMIŞ (D-431'de ölçüldü, madde eskimişti).**
  `src/lsp.c` sürüm **0.2**; capabilities'ten okundu:
  `textDocumentSync: 2` (**incremental ✓**) · `referencesProvider ✓` ·
  `documentSymbolProvider ✓` (roadmap'te yoktu, bonus) · hover/definition/
  completion ✓. İşlenen metotlar: didOpen/didChange/didClose/hover/definition/
  completion/documentSymbol/references.
  ~~**KALAN yalnız 2:** `semanticTokens` · `workspace/*`~~
  ✓ **D-432: `semanticTokens/full` EKLENDİ.** Kaynak **LEXER**'dir (AST değil):
  token zaten satır/sütun/uzunluk taşır, anahtar kelime/literal/operatör ayrımı
  lexer'da YAPILMIŞ ve **hatalı kaynakta AST yokken bile token üretilir** →
  sözdizimi renklendirme bozuk dosyada da çalışır. Legend: keyword · string ·
  number · operator · variable.
  - **⚠ `data` DELTA'lı ve UTF-16'dır:** `[Δsatır, Δbaşlangıç, uzunluk, tip, 0]`;
    Δbaşlangıç aynı satırda önceki token'a göre, satır değişince MUTLAK. Sütun
    ve uzunluk **UTF-16 kod birimi** — `ölç` 5 BAYT ama 3 birimdir. Bayt
    kullanmak ASCII testte GÖRÜNMEZ; test bilerek Türkçe kaynak kullanır
    (sabotaj S15 = bayt'a çevir → test ✗).
  - **⚠ Anahtar kelime aralığı `lexer.h`'den ÖLÇÜLDÜ:** `TOK_EGER` .. `TOK_GENEL`.
    İlk yazımda `TOK_ISLEV`'den başlatmıştım — enumun ilk 6 anahtar kelimesi
    (`eğer`/`değilse`/`için`/`iken`/`eşleş`/`ver`) dışarıda kalırdı.
  - **⚠ BAYAT ARTEFAKT (bu oturumda 3. artefakt tuzağı):** sabotajı geri alınca
    `make` `build/test_lsp.exe`i YENİDEN KURMADI → test 21/22 kaldı ve kaynak
    TEMİZKEN sahte kırmızı verdi. `rm -f build/test_lsp.exe` → 22/22.
    Sabotaj döngüsünden sonra ilgili **test ikilisini de** sil, yalnız `.o`yu
    değil. (Diğer ikisi: `build/codegen.exe` dosya kilidi, `git stash` sonrası
    bayat obje.)
  ~~**KALAN yalnız 1:** `workspace/*`~~ ✓ **D-434: `workspace/symbol` EKLENDİ
  → LSP v3 MADDESİ TAMAMEN KAPANDI.**
  - **⚠ SymbolKind ≠ CompletionItemKind** — iki AYRI enum. `completion`
    bilerek CompletionItemKind kullanır (`islev`=3); `workspace/symbol` ve
    `documentSymbol` **SymbolKind** ister (`islev`=12 · `sabit`=14 ·
    `yapı`=23 · `çeşit`=10 · `özellik`=11). Değerler `documentSymbol`dan
    OKUNDU; karıştırmak istemciye yanlış ikon gösterir. Test `kind == 12`
    denetler.
  - **SINIR (bilinçli):** yalnız AÇIK belgeler taranır. Diski gezip projeyi
    indekslemek ayrı iş (FS tarama + izleme); "açık dosyalar" LSP'de geçerli
    bir workspace tanımıdır.
  - Test `test_workspace_symbol_capraz`: iki dosya, sorgu `"ortak"` →
    2 sonuç **FARKLI uri**'lerle (aynı uri iki kez = çapraz arama YOK).
    **Sabotaj S17** (döngüyü `i < 1`) → 24/24 → 23/24 ✅ — bu kez `perl`
    değil **Edit** kullanıldı (D-433'ün dersi).

### 🎯 D-470: `ag.kem` okuma zaman aşımı + lineer yapıcı/alan tüketimi
D-466'nın açık bıraktığı zaman aşımı eklendi (`zaman_asimi_ayarla`). Yol
üstünde lineer kuralların **yapıcı argümanı** ve **yapı alanı** konumlarında
tüketim saymadığı ölçüldü ve onarıldı; **D-470 devamı** ile aynı kurallar
`selfhost/codegen.kem`'e portlandı (D-407 gereği: aynı soruyu iki yerde ayrı
yanıtlayan kod ayrışır).

### 🎯 D-471: paylaşılan `codegen.exe` hedefi + asan kapılarına zaman aşımı
`build/codegen.exe` **38 ayrı yerden** yazılıyordu (D-297'nin çarpışma sınıfı)
ve 13 kez yeniden kuruluyordu (~36 sn tekrar). Tek Makefile hedefine indirildi.
Ayrıca **iki ASan kapısında zaman aşımı YOKTU**; bu oturumda tam takım **İKİ
KEZ** saatlerce asılı kaldı (2.5 sa ve 1.5 sa). **Asılan kapı, sessiz kapı
kadar kötüdür** — kimse onu koşturmaz. (`ag_kosum` D-466 ve `codegen_genis`
D-468 ile birlikte bu sınıfın ÜÇÜNCÜ örneği.)

### 🎯 D-493→D-495: sıfır-uyarı kapısı · F4.4 `bölge_al` ekseni KAPANDI
- **D-493 — "sıfır uyarı hedefi" İLAN EDİLMİŞ ama ÖLÇÜLMÜYORDU.** Bayraklar
  var, `-Werror` YOK, hiçbir kapı zorlamıyordu → uyarı sessizce sızabilirdi.
  **Önce kuralın tutup tutmadığını ölçtüm** (kapıyı ilan etmeden): 0 uyarı,
  yani sabitlenecek gerileme yok. **`-Werror` DEĞİL ayrı kapı:** bu depo ÜÇ
  derleyici kullanıyor; `-Werror` yeni bir sürümde yapımı TAMAMEN kırardı,
  kapı ise yalnız kırmızı olur. **`build/`e DOKUNMAZ** (geçici dizin —
  D-297 çarpışma sınıfı). Kapsam `src/`+`runtime/` 38 dosya; `test/*.c`
  bilerek dışarıda (sınır zorlayan kod, gürültü olurdu).
  **⚠ Kapım ilk sürümde yanlıştı, kod değil:** 4 `runtime/` dosyası
  `-DKEMGU_UART_MOCK` ister ve olmadan bilerek `#error` verir.
  **Kırmızı önce KAPIYI şüpheli kılar.** Doğrulama: GCC 16 · clang · GCC 11.

- **🎯 D-494 — F4.4'ün NE OLDUĞU ÖLÇÜMLE DEĞİŞTİ.** `kdl_bolge_serbest`
  **ZATEN** gerçek toplu serbesttir → F4.4 "toplu serbest YAZ" değil,
  **"ÇAĞRILMADIĞI YERİ BUL"**muş. Sızıntı izi OKUNDU (D-405'te tahminle
  yanlış kök seçmiştim): `bölge_al(y,n)` **koşulsuz ham `malloc`** yayıyordu
  ve sonuç hiç serbest edilmiyordu — adı "bölgeden al" olan yerleşik
  bölgeden tahsis ETMİYORDU. Artık hapsedilme kanıtı VARSA
  `kdl_bolge_ayir(ρ_yerel, n)`; kanıt YOKSA eski `malloc` **aynen** kalır
  (sızar ama UAF yok — *"sızıntı bir hata, UAF bir felaket"*).

- **🎯 D-495 — `bölge_al` EKSENİ TAMAMEN KAPANDI, muafiyet 9 → 5.**
  ```
  bolge_al_grow  2→0   tam64 1→0   tam8 1→0   struct 0→0
  hepsi: exit 42 · UAF 0 · çift-serbest 0
  ```
  Üç kural: **`bellek_kopyala`(memcpy) argümanını TUTMAZ** → kürate beyaz
  liste · `ky_var_gecer`e cast dalı · `ky_confined`e cast dalı.
  **⚠⚠ `bellek_serbest`(free) BİLEREK LİSTEDE YOK VE BU KRİTİK:** onu
  hapsedilmiş saymak hem açık `free` hem bölge serbestını çalıştırırdı =
  **ÇİFT SERBEST**. Liste *"işaretçi alan her yerleşik"* değil,
  *"argümanını TUTMAYAN yerleşik"* demektir (D-459 disiplini).
  - **⚠⚠ İKİ CAST KÖKÜ BİSECT'LE BULUNDU, akıl yürütmeyle DEĞİL.** memcpy
    kuralından sonra `bolge_al_grow` HÂLÂ malloc yayıyordu. Üç küçük vaka:
    tek işaretçi+memcpy GEÇİYOR, iki işaretçi+memcpy DÜŞÜYOR → kök
    `ky_var_gecer`in `default: return 1` dalıydı. Onu onarınca `tam8` hâlâ
    düşüyordu → aynı boşluk `ky_confined`de de vardı. **D-438'in "komşu
    şekilleri ölç" dersi ÜÇÜNCÜ kez karşılığını verdi.**
  - **KALAN 5 SIZINTI BAŞKA KÖKENDEN** (kapanış heap env ×3 · kanal tamponu ·
    görev) — `bölge_al` ekseninde DEĞİL. "Hepsi halloldu" DEĞİL.
    ⚠ Kapanış env'i **bilinçli** heap kopyasıdır (D-309); serbest bırakmak
    *"kapanış artık ölü mü"* sorusunu ister — hapsedilme kanıtından FARKLI
    ve daha zor bir soru, muhtemelen tasarım kararı.
  - Sağlamlık korundu: kaçan işaretçi HÂLÂ `malloc`ta ve kapı bunu ayrıca
    ölçer (`bölge_al: ρ_yerel=1 malloc=1`).

### 🎯 DEĞİŞMEZ AVI (D-497→D-505): altı eksen, dört açık, dördü de kapandı
Yöntem: dilin **ilan ettiği her garantiyi** düşmanca şekillerle sınamak ve
sonucu üç kategoriye ayırmak — derleme zamanı red (iyi) · çalışma zamanı
panik (iyi) · **sessiz yanlış / UB (AÇIK)**.

| Eksen | Sonuç |
|---|---|
| Buffer overflow | ✅ tutuyordu — `PANIK: dizi sınır ihlali (i=10, boyut=3)`; `metin_kes` kırpıyor, ASan temiz |
| Null pointer | ✅ tutuyordu — runtime metin yerleşiklerini tutarlı NULL-koruyor |
| Çökmezlik | 🔴 `a/0` → SIGFPE → **D-502** (panik) |
| Lineer tipler | ✅ **en sağlam** — 5 kaçış yolunun 5'i (yapı alanı LR002 · referans L004 · dizi/kapanış/`sonuç` L001) |
| Yetki | 🔴 `geri_al` sonrası ödünç → **D-503** (CP005) |
| DRF | 🔴 iki görev aynı diziye yazıyor → **D-505** (L002) |

**DÖRT AÇIĞIN ORTAK DESENİ — bu, avın asıl bulgusudur:**
> Kural ya **belgede vardı** (R-YAKALAMA-THREAD spec'te yazılı, DRF Lean'de
> ispatlı) ya da **mekanizma koddaydı** (CP005 tüketim yolunda çalışıyor,
> `kdl_panik` dizi sınırında kullanılıyor). Eksik olan kural değil,
> **kuralın SORULMADIĞI YERDİ.**
> Bu yüzden dördünün üçü **yeni tanı kodu GEREKTİRMEDİ** (L002/CP005 yeniden
> kullanıldı); yalnız D-499 (G006) yeni kod istedi ve o da Mehmet'in kararıydı.

**⚠ PROBE'UN KENDİSİ SIK SIK YANLIŞTI — sonuç okunmadan önce probe ölçülmeli:**
- `mantıksal olarak tam32` **E002** verir (örtük dönüşüm yok) → 3 probe geçersizdi
- `metin_bayt` `tam8` döner, `dtam8` değil → T001
- DRF'de ilk 3 probe **L001 ile maskelendi** (görev birleştirilmemişti)
- DRF'de 4. probe **sıralı koştu** (birinci görev ikinciden ÖNCE birleştirildi)
  → yarış hiç oluşmadı; ikisi de canlı olacak biçimde iç içe kurulunca çıktı
- `-fsanitize=undefined` **ham IR girdisine kontrol EKLEMEZ** (UBSan C→IR
  aşamasında enstrümante eder) → sıfıra bölmeyi maskeledi, `sdiv`i IR'da
  okuyunca görüldü

**⚠ ASan BU SINIFLARIN İKİSİNİ GÖRMÜYOR:** `ver &yerel` (D-497) ve sıfıra
bölme — `detect_stack_use_after_return=1` ile bile sessiz. Yani bir güvenlik
değişmezi için *"program çalıştı + ASan temiz"* **yetersiz kanıttır** (D-417
ve D-488'in üçüncü tekrarı).

### 🔴 D-509: interprocedural özet self-host'a + D-506'NIN SOKTUĞU SOUNDNESS DELİĞİ
**Port (planlanan iş):** D-488'in parametre-tutma özeti (`ea_param_tutmuyor`)
`selfhost/codegen.kem`'e portlandı. Yeni analiz İCAT EDİLMEDİ — *"f, p'yi
saklıyor mu?"* sorusu `ky_confined` ile **birebir aynıdır**, kanıt çağrı yerine
taşındı. Her eksende default-DENY (bilinmeyen callee · gövdesiz imza ·
**özyineleme** · yığın taşması). Gerçek programlarda self artık C ile birebir:
`parser 22/22 · checker 23/23 · codegen 24/24 · regex 2/2`.

**🔴 ASIL BULGU — SABOTAJ KAPIYI, KAPI DA BİR SOUNDNESS DELİĞİNİ AÇTI.**
S89 yönlendirmeyi düşürdü (`%5`→`%4`) ama kapı **YEŞİL** kaldı: `rho_sinifi`
her `%N`'i YEREL sayıyordu ve `main`'de ρ_global **de** bir `%N`'dir
(`kdl_global_bolge_al()`). İki bölgenin ömrü BAMBAŞKA (global serbest
EDİLMEZ, yerel `ret`te edilir). Sınıf artık bölge-açma atamalarından OKUNUYOR;
register NUMARASI hâlâ yok sayılıyor.

Sertleştirilmiş kapı **temizde 2 sapma** gösterdi — maskelenmiş, önceden var
olan bir açık: `Dizi<metin>` ve `Dizi<&H>` üzerinde **C=YEREL, self=GLOBAL**.
**Self DOĞRU, oracle YANLIŞTI** (D-442 sınıfı). Kök **D-506'da benim
kodumdu**:
```c
const char *dz_elem = (eb == 8) ? "i64" : "i32";   // ← ELEMAN IR'ini BAYT'a indirger
```
Bayt genişliğine indirgeme **işaretçi-liği SİLİYOR**: `tam64` (gerçek skaler,
güvenli) ile `metin`/`&Yapi`/`Dizi<T>` (ptr eleman, **GÜVENSİZ**) aynı kovaya
düşüyor, ikisi de "skaler" sayılıp ρ_yerel'e gidiyordu — oysa
`bolge_yerel_yonlendir`in **kendi koşulu** ptr elemanı REDDEDER (iç heap-ref
`dizi_al` ile kaçabilir → **UAF**). Onarım: gerçek `eir` taşınır, bilinmeyen
eleman → `"ptr"` = DENY.

**⚠ D-506'NIN KAZANCI KORUNDU** (dar onarım, geri alma DEĞİL):
```
Dizi<tam32> (bench2)      → YEREL    17x bellek kazanci AYNEN
Dizi<metin> · Dizi<&H>    → GLOBAL   ptr eleman DOGRU reddediliyor
```

**⚠⚠ DERSLER, üçü de bu turda ölçüldü:**
- **Bir performans optimizasyonu bir güvenlik değişmezini sessizce delebilir.**
  D-506 tüm kapıları geçmişti (ASan dâhil) — fikstürlerde ptr-elemanlı
  `dizi_olustur` YOKTU. *"Korpusta o şekil yok"* dersinin (D-356) beşinci
  tekrarı.
- **Sabotajın sessizliği önce KAPIYI şüpheli kılar** (D-443): S89 uygulanmıştı,
  davranış değişmişti, kapı yine yeşildi → zayıf olan kapıydı.
- **Windows'ta `python3` YOK.** `wsl.exe -e python3` ile bir Windows dosyasına
  yaptığım İKİ düzenleme **sessizce kayboldu** (`grep -c` ile yakalandı) ve bir
  an *"sertleştirme işe yaramıyor"* diye kaydedecektim. Türkçe dosyada
  düzenleme = **Edit aracı**.

### 🟠 D-520 (AÇIK — ÖLÇÜLDÜ, KAPATILMADI): çok-segmentli `kullan` T041'i ATLIYOR
Değişmez avı **modül gizliliği** eksenine taşındı. Dört erişim yolu **tuttu**:
```
gizlim::gizli_fn()          -> T041 ✓     gizlim::GIZLI_SBT   -> T041 ✓
kullan gizlim olarak g      -> T041 ✓     kullan gizlim::{..} -> T041 ✓
kullan gizlim + genel uye   -> OK   ✓ (pozitif)
```
**AMA legacy yol açık:**
```kemgu
kullan ic::derin;              // ÇOK SEGMENTLİ -> legacy düzleştirme
işlev main() -> tam32 { ver derin_gizli() + 35; }   // private! -> OK 🔴
```
D-427 bunu C'nin davranışı olarak **kaydetmişti** (*"tüm üst düzey adlar
görünür, `genel` gerekmez, T041 UYGULANMAZ"*) ama **gizlilik güvencesine
etkisi ölçülmemişti**: private-by-default'un, herkesin bir yol yazarak
alabileceği bir **kaçış kapısı** var.

**⚠ MARJİNAL DEĞİL — ÖLÇÜLDÜ: 67 içe aktarmanın 37'si (%55) bu yolda**
(`drivers/virtio` tamamı dâhil). Yani gerçek kodun çoğunda gizlilik KAPALI.

**⚠⚠ KAPATILMADI ÇÜNKÜ MALİYET ÖLÇÜLDÜ VE BEDAVA DEĞİL.** Legacy yolu geçici
olarak kapatıp saydım: **17 dosyanın 14'ü kırılıyor**, 40+ tanı — çoğu
**T040 (modül bulunamadı)**.

**T040'IN KÖKÜ BULUNDU (ve ölçüldü):** parser `yol`u HAM saklıyor
(`"drivers::virtio::constants"`) ve yeni yükleyici `<dizin>/drivers::virtio::
constants.kem` diye arıyor → dosya yok. `::` → `/` çevirisi eklendiğinde
**T040 TAMAMEN KAYBOLUYOR** (deneysel olarak doğrulandı).

**AMA GERÇEK MALİYET ÇEVİRİDEN SONRA GÖRÜLDÜ:** T040 gidince yerine
**T002 patlıyor** — `virtio_blk` 130, `virtqueue_bind` 105, `virtio_blk_oku`
164 … toplam **~700 ad referansı**. Sebep: sürücü kodu legacy düzleştirmeye
güvenip adları **NİTELİKSİZ** kullanıyor; yeni yol nitelik ister.
Yani göç *"`genel` ekle"* değil, **~700 referansı nitelendirmek** (ya da
modülleri yeniden dışa vermek). **Karar Mehmet'in.**

**⚠ `::`→`/` ÇEVİRİSİ DE UYGULANMADI (D-430):** bugün **ULAŞILAMAZ** kod
olurdu — çok-segmentli import legacy yola gider, tek-segmentte `::` yoktur,
seçili/alias ise P046 ile tek-segmente zorlanır. Doğru ve gerekli bir
ön koşuldur; göç kararı verilirse **ilk adım odur**.

**⚠ İLK ÖLÇÜMÜM "0 MALİYET" DEDİ VE YANLIŞTI** — yalnız `T041` sayıyordum;
dosyalar daha ÖNCE T040 ile düşüyordu, ben sıfır görüyordum. *Bir kuralı
kapatmanın maliyetini ölçerken O KURALIN kodunu değil, TÜM tanıları say.*
(D-500'ün bu turdaki üçüncü tekrarı.)

### 🔴✅ D-519: `olarak` GİZLİLİĞİ SESSİZCE DÜŞÜRÜYORDU — `ifşa`nın yanında ikinci kanal
Değişmez avı **`sabitsüre` (sabit-süre / gizli veri) eksenine** taşındı. Altı
kaçış şeklinin beşi tuttu:
```
gizli -> yapı alanı        -> T001 ✓      gizli -> dizi elemanı  -> T001 ✓
gizli -> işlev dönüşü      -> CT003 ✓     gizli -> döngü koşulu  -> T021 ✓
gizli -> küresel           -> CT003 ✓
gizli -> `olarak tam32`    -> **OK** 🔴   ← AÇIK
```
**SPEC İHLALİ, tasarım tercihi DEĞİL.** `KEMGU_Sabitsure_Spec_V1.md:49`:
*"`sabitsüre<T> -> T` **ASLA** otomatik değil — `ifşa(s)` çağrısı **zorunlu**"*.

**⚠ ASIL ZARAR DENETLENEBİLİRLİKTİR.** `ifşa` ayrı bir keyword yapılmayıp
**yerleşik işlev** olarak seçildi — gerekçesi spec:94'te yazılı. Amaç
declassification noktalarının **`grep`lenebilir** olmasıdır. İkinci ve
**görünmez** bir kanal, o güvencenin kendisini yok eder: gizli veri sızdıran
bir satır artık denetimde görünmezdi.

**KÖK D-517 İLE AYNI SINIF:** kural VARDI (CT003 zaten "implicit dönüşüm
yasak" demek), yalnız **bu sitede sorulmuyordu**. **Yeni tanı kodu YOK.**
C'de `tekkez` için birebir aynı kalıp (E003) zaten duruyordu — kardeşi eklendi.

**⚠ DAR TUTULDU:** `gizli → gizli` (`s olarak sabitsüre<dtam32>`) **SERBEST**;
yasak olan yalnız **gizliliğin düşürülmesi**. Fikstür bunu ve `ifşa`yı
**pozitif** olarak ölçer — yalnız negatif olsaydı *"her sabitsüre cast'ini
reddet"* sabotajı kapıdan GEÇERDİ (D-425).

**⚠ ETKİ ALANI ÖLÇÜLDÜ:** depodaki tüm ilgili `olarak` kullanımları
`sabitsüre_olustur((0 olarak dtam8))` biçiminde — yani **sarmalamadan ÖNCE**
(düz→düz). `stdlib/kripto.kem` ve `stdlib/kripto/anahtar.kem` **etkilenmedi**.

**Sabotaj 2/2:** S97 (C) · S98 (self) → ikisi de `checker_diff` 167→166, rc=2.
**Kapılar:** checker_diff **167/167 (0 muaf)** · check_kapisi 267/274 (0 RED) ·
sıfır uyarı 38/0.

### 🔴✅ D-518: ÜSTEL KESİRLİ LİTERAL GEÇERSİZ IR ÜRETİYORDU + kapı sertleştirildi
Değişmez avı **`olarak` yüzeyine** taşındı. Beş şeklin dördü doğru davrandı
(`tam64→tam8` daraltma **E004**, ilgisiz yapı dönüşümü **E002**, `3.9→3`
tanımlı, işaretli→işaretsiz tanımlı). Beşincisi bir **codegen kusuru** açtı.

**KUSUR:** `%.15g` |x| ≥ 1e15 ve küçük kesirlerde **üstel biçime** düşer ve
LLVM noktayı **MANTİSTE** ister:
```
C   : 1e+30     (nokta YOK)   -> "integer constant must have integer type"
SELF: 1e+30.0   (nokta SONDA) -> yine GEÇERSİZ
1.5e+300  -> GEÇERLİ (mantiste nokta var)
```
İkisi de **LINK-RED**: geçerli bir KEMGU programı **derlenmiyordu**. Kural
*"üstel varsa reddet"* değil, *"nokta mantiste olmalı"*dır.
**İki farklı kök, aynı sınıf:** C hiç nokta eklemiyordu; self-host `.0`'ı
**sona** ekliyordu. D-457'nin "nokta garanti" kuralı üstel biçimi kaçırmıştı.
Onarım ikisinde de: üstel varsa nokta `e`den **ÖNCE** girer. D-457'nin kayıpsız
gidiş-dönüşü korunur (`3.14159265358979` tam basılır).

**🎯 ASIL BULGU — SABOTAJ KAPININ KÖR NOKTASINI AÇTI.**
**S96** (C onarımını geri al) → kapı **KIRMIZI OLMADI**, yalnız sayı
**162 → 161** düştü: C'nin **geçersiz IR üretmesi** bir başarısızlık değil bir
**ATLAMA** olarak görünüyordu (`⚠ oracle yok, atla`). Yani `codegen_diff`
**oracle tarafındaki her gerilemeyi sessizce yutuyordu** — D-424'ün *"atlama
listesi bir KÖR NOKTA ENVANTERİDİR"* dersinin aynısı.
Atlama **kürate listeye** bağlandı (`cgmodul_mat` · `cgmodul_zincir` —
çapraz-modül yüklemesi ister, C oracle tek başına derleyemez; **liste ÖLÇÜLDÜ**,
tahmin edilmedi). Listede olmayan her oracle-link hatası artık **sert hata**.
Aynı S96 sertleştirmeden sonra: **161/162, rc=2** ✓

**⚠⚠ BU TURDA ÖLÇÜM ARACIM DÖRT KEZ YANILDI:**
1. İlk `-O0`/`-O2` karşılaştırması **aynı ikili yolunu** (`/tmp/c`) kullanıyordu;
   `c3` hiç linklenmedi ve **`c2`nin bayat ikilisi** koştu → "exit 3" gördüm ve
   *"UB yok"* diye kaydedecektim. Ayrı yollar + açık `link rc` ile ortaya çıktı.
2. `grep -oE "double [0-9.e+-]+"` C'de `fadd double 0.0, X` desenindeki **ilk**
   sayıyı yakaladı → sabit sütunu hep `0.0` göründü.
3. **`sed -i`'yi Türkçe içeren `src/llvm.c` üzerinde kullandım** (deponun kendi
   yasağı). Desen tutmadı, **hasar yok** — ama geri alma da olmadı ve bir tur
   boşa gitti. Satır sayısı iki tarafta eşit diye doğrulandı.
4. Harness'a **var olmayan bir değişken** (`hata=1`) yazdım; gerçek sayaç
   `fail`. Kaynağı okuyunca çıktı.

**Kapılar:** codegen_diff **162/162** · yapi_diff 137/137 · snapshot 50/50 ·
parser_diff 13/13 (dump paritesi bozulmadı).

### 🔴✅ D-517: `güvensiz`SİZ HEAP TAŞMASI — ham bellek yerleşikleri artık G001 istiyor
Değişmez avı **`güvensiz` sınırı** eksenine taşındı ("ham işaretçi yalnız
`güvensiz` blokta" — manşetteki iddia). **On iki şeklin on biri tuttu:**
```
*p param . p[i] . yapi alani *T . takma ad . lambda govdesi .
gorev kapanisi . method govdesi          -> G001 ✓
*p = ... (guvensiz DISINDA)              -> T022 ✓
diziyi yakalayan kacan kapanis           -> G005 ✓
```
**🔴 AMA BİRİ TUTMADI — ve tam olarak dilin manşet değişmezini deliyordu:**
```kemgu
işlev kopyala(hedef: metin, kaynak: metin, n: tam64) -> tam32 {
    bellek_kopyala(hedef, kaynak, n);      // `güvensiz` YOK
    ver 0;
}
işlev main() -> tam32 {
    değişken kucuk: metin = bellek_al(4);
    değişken buyuk: metin = bellek_al(256);
    kopyala(kucuk, buyuk, 256);            // 4 baytlik tahsise 256 bayt
    ver 42;
}
```
Program **hiç `güvensiz` bloğu içermiyor**, `--check`ten **temiz** geçiyordu ve
ASan altında **her iki derleyicide** `heap-buffer-overflow` veriyordu.

**KÖK D-443/D-452 İLE AYNI SINIF:** `bellek_al`/`bellek_kopyala`/
`bellek_serbest` **`metin` olarak tipleniyor** (D-459: *"HAM BELLEK"*, opak) →
G001'in `*T` koşulu onlara **hiç uygulanmıyordu**. *Opak tutamak `metin` olarak
tiplendiğinde ona bağlı KURALLAR da düşer.*

**ONARIM — YENİ TANI KODU YOK.** G001 zaten *"yalnız güvensiz blok içinde"*
demektir ve site başına farklı mesaj basar (deref / indeksleme); üçüncü mesaj
eklendi. C + `selfhost/checker.kem`, kod ve konum birebir.

**⚠ ETKİ ALANI ÖNCE ÖLÇÜLDÜ** (dil yüzeyi değişikliği — önceden geçerli
programları reddediyor): depoda bu yerleşikleri çağıran **4 dosya**, **3'ünde
`güvensiz` ZATEN vardı**; `selfhost/codegen.kem`'deki tek eşleşme **YORUMDU**
(gerçek çağrı yok). Tek düzeltme `cg_yerlesik_ad_eslemesi.kem`'e bir
`güvensiz` sarmalı.

**Fikstür `tc42_01`** — negatif (`kotu()` → 2×G001) **ve pozitif** (`iyi()` →
temiz). Pozitif şart: yoksa *"her `bellek_*` çağrısını reddet"* sabotajı
kapıdan GEÇERDİ (D-425).

**⚠⚠ PROBE VE SABOTAJ ÜÇ KEZ KENDİ HATAMLA BOZULDU:**
1. İlk taşma probe'u **`memcpy-param-overlap`** verdi — iki tampon aynı
   bölgeden geliyordu, ASan asıl taşmayı hiç ölçmedi. Ayrı tahsislere geçince
   `heap-buffer-overflow` çıktı.
2. `grep -c "G001"` **4** saydı, gerçek tanı **2**'ydi — her tanı iki satır
   (mesaj + `-->`). Kodda değil ÖLÇÜMDE hata.
3. **Sabotaj S94 eksik uygulandı:** `if (0 && A || B || C)` — C'de `&&` daha
   sıkı bağlar → yalnız `bellek_al` kapandı, diğer ikisi ateşlemeye devam etti.
   Kapı yine kırmızı verdi ama **yanlış sebeple**; `if (0) if (...)` ile
   tekrarlandı ve açık gerçekten geri geldi (`h7` → `OK`).

**⚠⚠ ÜÇ-UYGULAMA TUZAĞINA YİNE DÜŞTÜM (D-503'te KAYITLI).** Kuralı C'ye ve
`selfhost/checker.kem`'e portladım; `checker_diff` **166/166 YEŞİL** verdi. Ama
`self_driver` **`selfhost/codegen.kem`**'in check yolunu kullanıyor ve oraya
portlanmamıştı → **tam takım orada kırmızı** (C 2×G001, sürücü 0).
**Hedefli kapının yeşil olması portun tam olduğunu KANITLAMAZ** — bir kuralı
portlarken HANGİ kapının HANGİ uygulamayı okuduğunu **önce ölç**.
Üçüncü uygulamaya da portlandı → sürücü 134/134, FIXPOINT ✓.

**⚠ İKİNCİ KAÇIŞ — ETKİ ALANI ÖLÇÜMÜ GÖMÜLÜ KAYNAKLARI GÖRMEDİ.**
`--include="*.kem"` ile taramıştım; `test/test_llvm.c` içindeki **C dizgisine
gömülü** KEMGU kaynağı (`[70] bellek_al + serbest`) taramanın dışındaydı ve
tam takım orada da kırmızı verdi. Kusur değil — D-517'nin doğru sonucu; sarmal
eklendi (286/286). **Gömülü kaynaklar AYRI BİR YÜZEYDİR** (ölçüm aracı kontrol
listesine eklendi).

**Sabotaj 2/2:** S94 (C) · S95 (self) → ikisi de `checker_diff` 166→165, rc=2.
**Kapılar:** checker_diff **166/166 (0 muaf)** · check_kapisi 266/273 (0 RED) ·
check_genis 133/133 · codegen_diff 161/161 · sıfır uyarı 38/0.

### ✅ D-516: UAF AVI — ρ_yerel genişlemesine karşı, DELİK YOK (kapıya çevrildi)
Bu oturumda D-494/D-506/D-507/D-509 ρ_yerel'e giden şeyleri **genişletti** ve
D-509'da tam bu eksende bir soundness deliği çıktı. F4.2b'nin 18-UAF avı o
genişlemelerden ÖNCEYDİ → yeni yüzeye karşı **taze düşmanca av** yapıldı.

**Altı kaçış şekli, altısı da DOĞRU davrandı:**
```
(a) diziyi DOGRUDAN dondur                      -> CALLER   ✓
(b) callee diziyi SAKLIYOR (D-509 ozeti)        -> GLOBAL   ✓
(c) dizi YAPI alaninda, yapi donduruluyor       -> CALLER   ✓
(d) ic dizi DIS diziye, dis donduruluyor        -> CALLER   ✓
(e) bölge_al isaretcisi yapida donduruluyor     -> @malloc  ✓ (kanit DUSTU)
(f) diziyi yakalayan kapanis donduruluyor       -> G005 RED ✓ (derleme zamani)
```
(b) en kritiktir: D-509'un **yeni** interprocedural özetinin RED yönünü ölçer.

**⚠ "ASan TEMİZ" TEK BAŞINA KANIT DEĞİLDİ** (D-488/D-417'nin üçüncü tekrarı):
altı probe de ASan altında temiz ve exit 42 verdi. Yanlış yönlendirilmiş bir
tahsis de öyle yapabilirdi. **Asıl ölçüm YÖNLENDİRME KARARIDIR** — ρ sınıfı
ayrıca okundu ve C↔self **birebir** çıktı.

**AV KAPIYA ÇEVRİLDİ:** `cg_uaf_kacis_matrisi.kem` (a–d tek dosyada, çıkış 42).
*"Elle taranan ölçüm eskir, kapı eskimez"* (D-462). Ölçüm
`calistir_bolge_operand`da: bu dosyadaki **hiçbir tahsis YEREL olmamalı**.
**Sabotaj S93** (`ea_param_tutmuyor` daima 1 = özeti toptan izin ver) →
fikstür KIRMIZI, 7/163 sapma, rc=1.

**Kapsanmayan (dürüstçe):** (e) ve (f) fikstüre alınmadı — (e) `yetki` +
`güvensiz` gerektirir ve `bolge_operand` dizi tahsisi arar; (f) zaten
**derleme zamanında** reddedildiği için codegen'e hiç ulaşmaz (G005 kendi
korpus fikstürlerinde kapılı).

### ⛔ D-515 (NEGATİF SONUÇ): dar kanal-serbestı YAZILMADI — ölçülebilir kazancı YOK
D-511 kanal ömrünü *"birleştirme-duyarlı → dil kararı"* diye bırakmıştı. Dar bir
altküme (görevlere **yakalanmayan** kanalı, yaratan işlevin sonunda serbest
bırak) mekanik ve sağlam görünüyordu. **İki ölçüm bunu çürüttü:**

**(1) Altküme BOŞ DEĞİL — ilk varsayımım yanlıştı.** *"Kanal zaten paylaşım
içindir, tek thread'de anlamsızdır"* diye düşünmüştüm. Ölçüm: `kanal_oluştur`
kullanan **13 dosyanın 5'inde `görev_başlat` HİÇ YOK** (`cg_kanal_temel` ·
`_metin` · `_param` · `_tam64` · `_yon`) — tamponlu kanal tek thread'de
geçerli bir desen. **Varsaymak yerine ölçtüğüm için yakalandı.**

**(2) AMA O BEŞ DOSYA SIZDIRMIYOR.** ASan+LSan ile koşuldu: beşinde de
**sıfır sızıntı raporu** (exit kodları kendi sözleşmeleri: 42/5/7/42/42).
Sebep: kanal işaretçisi `main`'in **canlı yuvasında** duruyor →
LeakSanitizer *ulaşılabilir* tahsisi sızıntı SAYMAZ. Ölçülen tek kanal
sızıntısı `kanal_mesaj`'dır ve o **görevlere yakalanan** kanaldır — yani
tam olarak dar kuralın **kapsamadığı** dosya.

**SONUÇ: kod YAZILMADI.** Serbest bırakma yolu eklemek gerçek bir çift-serbest/
UAF riski taşır; kazancı ise **hiçbir ölçümle görünmez**. D-430'un disiplini
(*"ayırt edilemeyen kod doğrulanmamış yüzeydir"*) ve D-490'ın disiplini
(*"hiçbir şey kanıtlamayan kapı eklenmez"*) burada aynı yönü gösteriyor.
⚠ D-510'dan farkı: o bir **varsayılanın yönüydü** (güvensiz yönde yanlış
olamaz); bu ise **bellek serbest bırakan yeni kod** olurdu.

**Kayıt:** `kdl_kanal_serbest` (`kdl_runtime.c:1758`) **bilerek ölü kalıyor.**
Onu canlandıracak şey birleştirme-duyarlı ömürdür; o gelene kadar silinmemeli
(D-459'un tersi: burada ölü kodu bırakmak doğru, çünkü sessiz-başarısız bir
yol açmıyor — hiç çağrılmıyor).

### ✅ D-513→D-514 KAPANDI: kaydırma miktarı artık **temiz duruyor**
**Mehmet seçenek (b)'yi seçti** — çalışma zamanı panik. Gerekçe tutarlılık:
dil zaten **dizi sınırı** (D-069) ve **sıfıra bölme** (D-502) için panik
seçmişti. (c) maskeleme (`n & 31`) *sessizce yanlış cevap* üretirdi;
(a) derleme-zamanı red yalnız SABİT miktarı yakalar, değişkeni kaçırırdı.
```
ÖNCE:  sol_sabit · sag_degisken · isaretsiz · negatif
       -> -O0 exit=0, -O2 exit=1   (AYNI PROGRAM, AYNI IR = UB)
SONRA: dördü de -> PANIK: kaydirma miktari gecersiz, exit 134
       -O0 ve -O2'de AYNI (UB gitti)
       21 << 1 -> 42 (etkilenmedi)
```
Mekanizma sıfıra bölmeyle **birebir**: inline `icmp` + `br` +
`kdl_panik(noreturn)` + `unreachable`. **Yeni tanı kodu YOK, tip değişikliği
YOK.** C + self-host, kod ve mesaj birebir.

**⚠ TEK KARŞILAŞTIRMA YETER — ikinci kontrol GEREKMEZ.** `icmp uge`
**işaretsizdir**, dolayısıyla negatif miktar (`0-1` → `0xFFFFFFFF`) aynı dala
düşer. Ayrı bir `icmp slt 0` yazmak gereksiz dal olurdu; `negatif.kem`
fikstürü bunu **pozitif olarak ölçer**.

**İki bilinçli daraltma:** yalnız **tamsayı** (kesirlide kaydırma yok) ·
**sabit ve aralıkta** olan miktarda kontrol yayılmaz (yaygın durum, gereksiz
dal); aralık dışı sabitte yayılır ki `a << 40` da temiz dursun.

**KAPI AYRI AÇILMADI:** `calistir_sifir_bolme` genişletildi (10 → **20 ölçüm**).
Mekanizma ve değişmez aynı; ayrı kapı envanteri gereksiz bölerdi.
`normal.kem` (21<<1 → 42) **zorunlu**: yalnız negatif şekiller olsaydı
*"her kaydırmayı reddet"* sabotajı kapıdan GEÇERDİ (D-425).
**Sabotaj 2/2:** S91 (C) · S92 (self) → her ikisinde de dört şeklin dördü
`exit=0` ile yakalandı, `rc=2`.

**⚠ AYRIM ÖNEMLİ — aritmetik taşma BU SINIFTA DEĞİL.** `add i32`'de
`nsw`/`nuw` bayrağı YOK → iki'nin tümleyeni sarması **TANIMLI** ve
`-O0`/`-O2`'de **KARARLI** (ölçüldü: `2147483647 + 1` → iki seviyede de aynı).
Sarma için panik/`sonuç` istenip istenmediği ayrı bir **dil yüzeyi sorusudur**;
kaydırma ise bir **kusurdu** — kimse *"cevap optimizatöre bağlı olsun"* diye
tasarlamaz.

**⚠ PROBE'UN KENDİSİ ÖLÇÜLDÜ:** `dtam32` şekli `olarak tam32` istedi (örtük
dönüşüm yok) ve `-1` için `0 - 1` yazmak gerekti. Ayrıca WSL `/tmp` **üç kez**
silindiği için ölçüm bir turda tamamen boş döndü (D-508'in kendi dersi).

### (TARİHÎ) 🔴 D-513 ölçümü: kaydırma ≥ bit genişliği = UB, `-O2`de FARKLI CEVAP
Değişmez avı (D-497→D-505) altı ekseni taramıştı; **tamsayı taşması
taranmamıştı**. Tarandı ve eksen **ikiye ayrıldı** — ikisi AYNI ŞEY DEĞİL:

**(1) Aritmetik taşma — SESSİZ ama TANIMLI, kusur DEĞİL.**
```
tam32 2147483647 + 1   ->  -O0 exit=0   -O2 exit=0    (sarma, KARARLI)
tam8  127 + 1          ->  sarma
IR: `add i32` — `nsw`/`nuw` bayrağı YOK  ->  iki'nin tümleyeni sarması TANIMLI
```
Bayrakların yokluğu bilinçli bir seçimin sonucudur: sarma UB değildir.
Panik/`sonuç` istenip istenmediği bir **dil yüzeyi sorusudur** (D-502'nin
sıfıra bölme sorusuyla aynı sınıf), ama **acil bir kusur değildir.**

**(2) 🔴 KAYDIRMA ≥ BİT GENİŞLİĞİ — GERÇEK UB, GÖZLENEBİLİR.**
```
                         -O0   -O2
a << 40  (SABİT)          0     1     ← AYNI PROGRAM, AYNI IR
a >> n   (değişken 40)    0     1
dtam32 a << n             0     1
a << (0-1) (NEGATİF)      0     1
a << 1   (GEÇERLİ)       42    42     ← onarım BUNU BOZMAMALI
```
`shl i32 %6, %7` (miktar ≥ 32) LLVM'de **poison**tur. Bu sarma değil, **cevabın
optimizasyon seviyesine bağlı olması**dır — deponun en ağır saydığı sınıf.
**Kimse "cevap optimizatöre bağlı olsun" diye TASARLAMAZ** → bu bir tasarım
tercihi değil, bir kusurdur.

**MEKANİZMA HAZIR, YENİ ŞEY GEREKMİYOR:** sıfıra bölme (D-502) tam bu kalıbı
kullanıyor — inline `icmp` + `br` + `kdl_panik(noreturn)` + `unreachable`
(`src/llvm.c:3688`, dizgi `:8046`). Dizi sınırı (D-069) da aynı.

**⚠ KARAR MEHMET'İN** (D-502'de olduğu gibi): (a) derleme-zamanı red —
yalnız SABİT miktarda mümkün, değişken miktarı yakalamaz · (b) **çalışma
zamanı panik** — dizi sınırı ve sıfıra bölme ile TUTARLI, **önerilen** ·
(c) miktarı maskele (`n & 31`) — C/Java davranışı, ama *sessizce yanlış cevap*
üretir ve dilin DNA'sına aykırı.

**⚠ PROBE'UN KENDİSİ ÖLÇÜLDÜ:** `dtam32` şekli `olarak tam32` istedi (örtük
dönüşüm yok) ve `-1` için `0 - 1` yazmak gerekti. Ayrıca ilk turda
`/tmp`deki IR dosyaları **silinmişti** (D-508'in kendi dersi) → `grep` boş
döndü ve bir an "IR'da shl yok" sanıldı.

### 📐 D-511: kalan 2 sızıntının kökü OKUNDU — biri KUSUR DEĞİL
F4.4'ün "kalan 2 sızıntı" maddesi bugüne dek yalnız DOSYA ADIYLA kayıtlıydı.
Yığın izleri tek tek okundu (tahmin yok) ve madde **ikiye ayrıldı**:

**`gorev_temel` — 65.580 bayt — KUSUR DEĞİL, DOĞRU BİR KARARIN MALİYETİ.**
ρ_sahip'tir ve D-309'un kanıtı onu **bilerek** reddediyor. IR'dan ölçüldü:
üç `görev_başlat`ın `rho_serbest` bayrakları **`i32 0, 1, 1`** — sıfır olan,
T'si **`metin` (İŞARETÇİ)** olan görev. P1 (*dönüş skaler*) tam tasarlandığı
gibi düşüyor: dönüş ρ_sahip'in İÇİNE işaret ediyor olabilir, serbest bırakmak
**UAF** olurdu. Diğer iki görev (skaler T) bayrağı **1** alıyor ve bölgeleri
**gerçekten serbest ediliyor** — yani mekanizma çalışıyor.
Kapatmak *"dönen işaretçi ρ_sahip'in içine mi bakıyor?"* sorusunu ister —
hapsedilme kanıtından **farklı ve daha zor** bir analiz.

**`kanal_mesaj` — 8+168+16 bayt — GERÇEK boşluk ama naif onarım İŞE YARAMAZ.**
`kdl_kanal_serbest` runtime'da **ZATEN VAR** (`kdl_runtime.c:1758`) ama
**hiçbir derleyici onu çağırmıyor** (`grep -c` C'de 0, self-host'ta 0) —
D-462'nin *"kod var, hiçbir ölçüm ateşlemiyor"* sınıfı.
⚠⚠ Kanalı yaratan işlevin sonunda serbest bırakmak yalnız **hapsedilmiş**
kanallar için güvenlidir; oysa kanalın **varlık sebebi görevlere yakalanmaktır**
(D-505 onu taşımadan bilerek muaf tuttu) ve yakalanan bağlama `ky_confined`'in
LAMBDA dalından **DENY** alır → kural bu dosyayı **zaten kapsamaz**. Gerçek
kapanış *"kanalı tutan tüm görevler birleştirildi mi?"* bilgisini ister =
**birleştirme-duyarlı ömür → dil yüzeyi kararı (Mehmet).**

**⚠ DERS: "kalan N sızıntı" bir SAYIDIR, bir İŞ LİSTESİ DEĞİL.** İkisini de
"F4.4 borcu" diye kaydetmiştim; ölçünce biri **doğru davranış** çıktı. Bir
borcu kapatmaya girişmeden önce **borç olduğunu ölç** (D-406'nın
*"muafiyet gerekçesi de bir iddiadır"* dersinin tekrarı).

### 🔴✅ D-539 KAPANDI: mono tip tanımı KULLANIMDAN SONRA yazılıyordu
`al(olustur(b))` — iç içe generic-yapı çağrısı — **her iki derleyicide de
derlenmiyordu** (`Cannot allocate unsized type`). Geçerli bir program
reddediliyordu; D-464/D-518 sınıfı, sessiz DEĞİL. **İki ayrı kök çıktı.**

**C KÖKÜ 1 — çıkarsama:** D-536'nın dalı yalnız **tanımlayıcı** argümanı
kapsıyordu; iç içe çağrıda yan-kanal yok. Argümanın **değerlendirilmiş IR
tipi** (`%Kutu$i64`) mangle edilmiş adı taşır → sonekten okunuyor.

**C KÖKÜ 2 — SIRALAMA, ve kaynaktaki yorum YANLIŞTI.** `mono_tip_tanimlari_emit`
yorumu *"LLVM adlı-tipleri modül-genelinde çözer, forward-ref güvenli"* diyordu.
**Ölçüldü — `alloca` için YANLIŞ:**
```
define i64 @al$i64(...) { %0 = alloca %Kutu$i64 ... }   <- satır 186
%Kutu$i64 = type { i64 }                                <- satır 211
error: Cannot allocate unsized type
```
`alloca` tipin **boyutunu ayrıştırma anında** ister; **imza** konumu istemez —
bu yüzden `define` satırı geçiyor, kusur yalnız gövdede görünüyordu. Fonksiyon
gövdeleri artık `tmpfile()`a yazılıyor; sonda **önce tipler**, sonra tampon.
Saf bayt kopyası (register numaralandırma fonksiyon-yerel).

**SELF-HOST YARISI — FARKLI KÖK, AYRI ÇÖZÜM (aynı oturumda kapandı).** Self'te `%Kutu$i64` **hiç
tanımlanmıyor**: `yapi_tip_emit` gövdelerden ÖNCE koşar, geç keşfedilen örnek
hiçbir zaman yayılmaz. `yaz_str` doğrudan stdout'a yazar (tampon YOK) → C'nin
hoist çözümü uygulanamaz; ya ön-geçişte kayıt ya sessiz-birinci-geçiş gerekir.
Self'te `%Kutu$i64` **hiç tanımlanmıyordu**: `yapi_tip_emit` gövdelerden ÖNCE
koşar, `yaz_str` doğrudan stdout'a yazar (**tampon YOK**) → C'nin hoist çözümü
uygulanamaz. Çözüm **ön-kayıt**: tek tip paramlı her generic yapı için skaler
genişlik kümesi (`i1..ptr`) `mono_kesif`ten hemen sonra kaydedilir.
**⚠ ÜST-YAKLAŞIM VE BİLİNÇLİ:** kullanılmayan adlandırılmış tip LLVM'de
**yasaldır**, maliyeti yapı başına 8 satırdır. Kesin küme, çağrı yerindeki T
çıkarsamasını ön-geçişte **tekrarlamayı** ister — aynı soruyu iki yerde ayrı
yanıtlamak D-407'nin ayrışma sınıfıdır. **Çok paramlı yapı (`Cift<A,B>`)
kapsanmadı**: çapraz çarpım gerekir ve o şekil ölçülmüş bir kusur üretmiyor.

**Fikstür `cg_ic_ice_generic_yapi.kem`** — `tam64` + 2^33 (ayırt edici;
`tam32` bu sınıfı gösteremez).

**Kapılar:** codegen_diff **167/167** · yapi_diff **148/148 (21 muaf)** ·
snapshot 50/50 · modul_codegen 22/22 · llvm_test 286/286 · stdlib rc=0.
**Sabotaj S112** (self ön-kaydını kapat) → fikstür exit 1.

### ⛔ D-538 (NEGATİF SONUÇ): dönüş-tipi-güdümlü çıkarsama YAZILMADI
K4 muafiyetinin "asıl kökü" diye kayıtlı olan bu özellik, **ölçüldükten sonra
gereksiz çıktı.** Kod değişikliği YOK.

**FALLBACK'İN HER SINIFI TEK TEK ÖLÇÜLDÜ** (probe'lar `tam64` + 2^33 ile
ayırt edici; `tam32` bu sınıfı GÖSTEREMEZ — D-535):

| şekil | sonuç |
|---|---|
| `bos_yap<T>() -> Dizi<T>` | `@bos_yap$i32` ama **exit 42** — `Dizi<T>` her genişlikte `ptr`, eleman tipi annotasyonlu bağlamadan gelir |
| `dizi.oluştur<T>(böl) -> Liste<T>` | 0 baytlık tahsis + T-bağımsız yerleşim → **görünmez** |
| `hata_yap`/`k_hata`/`k_tamam` | D-411'in kendi sınıfı: fallback annotasyonla **uyuşur** |
| `bos_kutu<T>() -> Kutu<T>` | **LINK-RED**: `'%4' defined with type '%"Kutu$i32"' but expected '%"Kutu$i64"'` |
| iç içe çağrı argümanı `al(olustur(b))` | **LINK-RED, her iki derleyicide** |

**SONUÇ: kalan her fallback ya GÖRÜNMEZ ya LOUD.** Sessiz-yanlış-cevap veren
iki sınıf **parametre konumundaydı** (D-535 `Dizi<T>`, D-536 değer `Kutu<T>`)
ve ikisi de kapandı. Dönüş-tipi-güdümlü çıkarsamanın **ölçülebilir tek kazancı
yapısaldır** (yapi_diff'in K4 muafiyeti, 12 dosya) — D-430 tam bu gerekçeyle
bir değişikliği geri almıştı: *ayırt edilemeyen kod doğrulanmamış yüzeydir.*

**⚠ BU, D-411'İN İDDİASININ GEÇERLİ OLDUĞU YERİ DE KESİNLEŞTİRİR.** O iddia
(*"fallback yanlışsa LLVM REDDEDER"*) **dönüş konumunda DOĞRU**, parametre
konumunda YANLIŞTI. Sınır artık ölçülmüş: yanlış tip **taşıyıcıya** (dizi
`ptr`, skaler register) sığıyorsa sessiz; **adlandırılmış tipe** (`%Kutu$i64`)
yansıyorsa LLVM yakalar.

**AÇIK KALAN (yeni, ayrı iş):** `al(olustur(b))` — iç içe generic-yapı çağrısı
**her iki derleyicide de derlenmiyor** (`Cannot allocate unsized type`). Geçerli
bir program reddediliyor; D-464/D-518 sınıfı, sessiz değil.

### 🎉 D-560→D-566: CI TAM YEŞİL — Linux **ve** Windows
D-554 CI'yı ilk kez çalıştırdı; bu seri Windows'u da yeşile çekti. Her adım
aynı döngüyle ilerledi: **logu oku → hipotez → ölç → onar**.

| # | logdaki kanıt | kök | onarım |
|---|---|---|---|
| D-560 | 67 program *"STDOUT farklı"* ama `diff` **tek satır bile** basmıyor | `diff -q` eksik dosyada da sıfır-dışı döner; ayrıntılı `diff`in hatası `2>/dev/null` ile yutulmuş | eksik dosya / boş diff / gerçek fark **üç ayrı mesaj** |
| D-561 | `[ -f ]=VAR` ama `/usr/bin/diff`: *No such file* | recipe kabuğu (Git-for-Windows `sh`) ile MSYS2 araçları **ayrı `/tmp` bağlamaları** çözüyor | geçici dizin **depo-göreli** |
| D-562 | bir sonraki kapı **aynı imzayla** düştü | kök tek harness'ta değil **paylaşılan kalıpta** | **27 harness**'ın hepsi çevrildi |
| D-563 | `/usr/bin/diff: /dev/fd/63: No such file` | **süreç ikamesi** `<(...)` MSYS2 araçlarıyla çalışmaz | gerçek dosyaya yaz |
| D-565 | `exit=127` ama ham koşum *"PANIK: sifira bolme"* basıyor | **134 = 128+SIGABRT POSIX'e özgü**; Windows'ta `abort()` → 127 (trivial programla ölçüldü) | `ABORT_RC` platforma göre |
| D-566 | `zirve RSS 5544 KB > eşik 4096` | eşik **Linux** ölçümünden türetilmişti | eşik platforma göre (Win 12288) |

**⚠ ORTAK SINIF: POSIX VARSAYIMLARI.** Altısının hiçbiri derleyici kusuru
değildi — hepsi **kapıların** taşınabilirlik varsayımıydı (`/tmp` bağlaması,
`/dev/fd`, sinyal-tabanlı çıkış kodu, RSS muhasebesi). D-477/D-481'in
(POSIX `system()`/çıkış kodu) aynı ailesi.

**⚠⚠ D-564 GEÇERSİZ BİR HİPOTEZDİ — ve bunu sabotaj değil ÖLÇÜM çürüttü.**
Israrlı `127`yi D-413'ün Defender yarışına yorup 12 kez yeniden deneme
ekledim; **çözmedi**. Tanı çıktısı gerçek kökü gösterdi (program çalışıyor,
doğru paniklıyor, yalnız kod POSIX değil). *Bir hipotezi "makul" olduğu için
uygulamak, ölçmenin yerine geçmiyor.*

**⚠ İKİ KEZ BAYAT ARTEFAKTA DÜŞMEDİM:** Windows yerelde `codegen_diff`
166/169 ve `bolge_operand` 14 sapma gördüm; ikisinde de `build/codegen.exe`
**7 gün eskiydi**. Taze kurunca ikisi de temiz — "Windows ayrışması" diye
kaydetmedim.

**⚠ YEREL YEŞİL, CI KIRMIZI OLABİLİR:** yerel Windows takımı
*"Tum testler gecti!"* dedi ama `perf_bellek` orada **atlanmıştı**
(`/usr/bin/time` yok). Yani yerel yeşil, o kapı hakkında **hiçbir şey**
söylemiyordu.

**SONUÇ:** `main` üzerinde **Linux success + Windows success**. Dört aydır
hiç koşmamış olan CI artık iki platformda da tam takımı ölçüyor.

### 🎯 D-555→D-559: CI KAMPANYASI — "hiç koşmuyor"dan "Linux tam yeşil"e
D-554 CI'yı ayağa kaldırdı; sonrası **log okuyup ölçerek** ilerledi. Her adımda
bir hipotez kuruldu, **logla doğrulandı**, sonra onarıldı.

| # | ölçülen | onarım |
|---|---|---|
| D-555 | 18 test `opt -passes=verify`de düştü; `opt` CI'da YOK. Test `2>/dev/null` ile *"command not found"*u yutup **"IR reddedildi"** diyordu | `llvm` kuruldu **+** eksik araç artık ayrı bir **ORTAM HATASI** banner'ıyla bildiriliyor |
| D-556 | `ld.lld: No such file or directory` → `calistir_uart_merhaba_bare_metal` Error 127 | `lld` kuruldu |
| D-557 | QEMU yokken beş temsilcinin **beşi de atlandı** ama özet yine *"5/5 geçti"* diyordu | özet artık atlamayı söylüyor |
| D-558 | Windows: `clang: command not found` (iş UCRT64 kabuğunda, clang Clang64'te) | `MSYS2_PATH_TYPE: inherit` + PATH ön-eki |
| D-559 | Windows: `Error -1073741515` = **0xC0000135 STATUS_DLL_NOT_FOUND** (ASan runtime DLL'i) | PATH ön-eki **her** Windows adımına |

**SONUÇ:** Linux işi **`Tum testler gecti!`** — tam takım CI'da yeşil.
`llvm_test` 286/286 · `checker_diff` 173/173 · bare-metal hedefleri koşuyor.

**⚠ D-551'İN ÇIKARIMI KISMEN YANLIŞTI.** Orada *"`ld.lld` ve `qemu` yok →
bare-metal/QEMU kapıları atlanır"* demiştim. `ld.lld` **atlanmadı, SERT HATA
verdi** (D-486 birçok atlamayı sert hataya çevirmişti) ve asıl ilk engel hiç
tahmin etmediğim **`opt`**tu. *Çıkarım, ölçümün yerine geçmez.*

### 🔴 AÇIK: Windows `codegen_genis` 3/70
Windows artık takımın neredeyse tamamını koşuyor, `codegen_genis`te duruyor:
67 programda **çıkış kodu AYNI, "STDOUT farklı"**.

**⚠ İLK HİPOTEZİM (CRLF) YANLIŞ OLURDU — logu okuyunca çürüdü.** Harness
farkı basıyor:
```sh
diff "$TMP/$b.c.out" "$TMP/$b.s.out" 2>/dev/null | head -6
```
ve log'da **tek bir fark satırı bile YOK**. `diff -q` eksik dosyada da
sıfır-dışı döner; ayrıntılı `diff`in "No such file" hatası ise `2>/dev/null`
ile yutulur. Yani kanıt *"içerik farklı"* değil, **"çıktı dosyalarından biri
hiç oluşmamış"** diyor. Satır sonlarını "onarsaydım" hiçbir şey değişmezdi
(D-421: yanlış kökü onarmak).

**SIRADAKİ:** harness'ın eksik dosyayı içerik farkından **ayırması** (bugün
ikisi aynı mesajı veriyor), sonra Windows'ta `.out`un neden yazılmadığının
ölçülmesi.

### 🔴✅ D-554: CI 4 AYDIR HİÇ ÇALIŞMAMIŞ — tek bir tırnaksız `:` yüzünden
`main` push edildikten sonra CI sonuçlarına bakıldı. Bulgu beklenenden ağırdı.

**ÖLÇÜM (public API, kimlik doğrulamasız):**
```
13 Mayis 2026'dan bu yana ornekleme: 399 kosum, 399 basarisiz, 0 basarili
100/100 kosum created_at == updated_at, 0 is  ->  STARTUP FAILURE
```
Yani işler hiç başlamıyordu; "CI kırmızı" değil, **CI hiç koşmamış**.

**KÖK TEK SATIR:**
```yaml
- name: apt: clang + gcc + make      # ← tirnaksiz
```
Tırnaksız YAML skalerinde `": "` iç içe eşleme başlatır → dosya
ayrıştırılamaz → GitHub workflow'u **hiç çalıştırmaz**. Yerel doğrulama:
`yaml.safe_load` → *"mapping values are not allowed here, line 74, column 18"*.
Ad tırnaklandı; YAML artık parse ediliyor (2 iş, 10 + 9 adım).

**⚠ BU, D-548'İN ÖLÇÜMÜNÜ DÜZELTİR.** Orada *"CI VAR ve `claude/**`
push'unda `test_tumu` koşuyor"* demiştim — **koşmuyordu**. `ci.yml`in var
olması, çalıştığı anlamına gelmiyordu. *Bir kapının dosyada durması,
koştuğunun kanıtı değildir* (D-446'nın CI karşılığı).

**İLK GERÇEK KOŞUM (a0e431e) — adım düzeyinde sonuç:**
```
Linux : checkout ok . apt ok . `make` (DERLEME) ok . `make test_tumu` BASARISIZ
Windows: MSYS2 ok . "Derleyici kontrolu" BASARISIZ (derleme hic denenmedi)
```
**Linux'ta derleyici CI'da KURULUYOR** — bu yeni ve olumlu bir bilgi.

**HİPOTEZLER (loglar 403, kimlik doğrulaması olmadan okunamıyor):**
- **Windows:** adım `clang --version` çalıştırıyor ama iş `msystem: UCRT64`
  kabuğunda koşuyor; `clang` **Clang64** paketinden geliyor ve o dizin
  PATH'te değil. CLAUDE.md tam bunu şart koşuyor
  (`export PATH=/c/msys64/clang64/bin:...`) — workflow o adımı atlamış.
- **Linux `test_tumu`:** `ld.lld` ve `qemu` `ubuntu-latest` taban imajında
  YOK (D-551'de ölçüldü) → bare-metal hedefleri düşer. Ayrıca D-486 birçok
  "atla"yı **sert hataya** çevirdi; CI'da eksik ikili artık sessizce
  geçilmez.
**⚠ İkisi de HİPOTEZ — log okunmadan doğrulanmadı.**

### ✅ D-553: RT007 self-host'a portlandı — RT alt-sistemi TAMAMLANDI (7/7)
D-550'de *"parser `çevrim:` alanını atıyor → portlanamaz"* diye kaydedilen son
kod. Engel gerçek ama **aşılabilirdi**: alan tüketiliyordu, kaydedilmiyordu.

**ÇÖZÜM D-351'in DESENİ:** `asm_node` ile **paralel** bir bayrak (`asm_cev`).
**Düğüme alan EKLENMEDİ** — bu şart, çünkü `--ast`/`--parse` dump'ı düğüm
alanlarını basar ve bir alan eklemek `parser_diff` + `snapshot` paritesini
sessizce bozardı. (Ölçüldü: parser_diff **13/13**, snapshot **50/50** bozulmadı.)

**⚠ BİLİNMEYEN DÜĞÜM → "ÇEVRİM YOK" SAYILIR** (default-DENY): yan-kanalda kaydı
olmayan bir asm düğümü RT007 alır. C'nin kendi gerekçesi budur — *opak asm
maliyeti sessizce 0 sayılamaz*; gürültülü taraf doğru taraftır.

**Fikstür `tc46_03_realtime_asm.kem` ÜÇ şekil taşır:**
```
kotu()   gerceklzamanli + asm, `cevrim:` YOK   -> RT007
iyi()    gerceklzamanli + asm, `cevrim: 24`    -> TEMIZ   (POZITIF)
normal() gerceklzamanli DEGIL + anotasyonsuz   -> TEMIZ   (kural yalnizca
                                                  gerceklzamanli govdelerde)
```
Pozitif şekiller olmasa *"her asm'i reddet"* sabotajı kapıdan GEÇERDİ (D-425).

**🎯 RT ALT-SİSTEMİ ARTIK TAM:** RT001 · RT002 · RT003 (doğrudan **ve** zincir) ·
RT004 · RT005 (bilinmeyen callee **ve** dolaylı çağrı) · RT007 — **yedisi de**
C ↔ `checker.kem` ↔ `codegen.kem` üçlüsünde birebir.
**Repo taraması: 729 dosya, 0 RT sapması.**

**Kapılar:** checker_diff **173/173 (0 muaf)** · self_driver **140/140 +
FIXPOINT ✓** · parser_diff 13/13 · snapshot 50/50 · check_genis 133/133.
**Sabotaj 3/3:** S136 (checker RT007 dalı) → 172/173 rc=2 · **S137 (`çevrim`
bayrağını hep 0 yap)** → 172/173 rc=2 — *pozitif yolu ölçen sabotaj bu;
`iyi()` yanlışlıkla RT007 alınca kapı kırmızı oluyor* · S138 (codegen dalı) →
139/140 rc=2.

### 🔴✅ D-552: RT004 / RT005 / RT003-ZİNCİR self-host'a portlandı — ve sabotaj YANLIŞ BİR KURAL buldu
D-550'nin bıraktığı iş. **Madde "önce ULAŞILABİLİRLİĞİ ölç" diyordu** (D-360),
ölçüldü — beşinin beşi de ulaşılabilir:
```
RT004 (non-rt cagri) . RT005 (bilinmeyen callee) . RT005 (dolayli cagri)
RT003 (zincir)       . RT007 (asm `cevrim:` yok)  + POZITIF `cevrim: 24` -> OK
```

**PORTLANAN (dördü):** RT004 · RT005×2 · RT003-zincir. C D-545'in **`sessiz`
bayrağı birebir aynalandı**: iç keşif yürüyüşü tanı basmaz, yalnız zincir
bulgusu bayrakla dış çağrı yerine taşınır. Derinlik aşımı → **RT005**
(sessiz atlama DEĞİL).
**⚠ Dil `dizi_cikar` sunmuyor** → yığın için mantıksal uzunluk sayacı
(`rt_yigin_n`); dizi küçülmez, sayaç küçülür.

**🎯 ASIL BULGU — SABOTAJIN SESSİZLİĞİ YANLIŞ BİR KURALI AÇIĞA ÇIKARDI.**
İlk sürümüm yerleşikleri ayırıyordu (*"C'nin sembol tablosu yerleşikleri
bulmaz → RT005"*). **727 dosyalık tarama SIFIR sapma dedi** ve kural doğru
görünüyordu. Ama **S133 (ayrımı kapat) YEŞİL kaldı** → mekanizma ayırt
edilemiyordu. Ayırt edici şekli arayınca kural **çürüdü**:
```
metin_uzunluk (KAYITLI yerlesik)  C: RT004   ilk surumum: RT005  ← YANLIS
dizi_boyut    (hic sembol degil)  C: RT005   ilk surumum: RT005  ← dogru
```
Yani ayrım *"yerleşik mi"* değil, **"SEMBOL MÜ"** ayrımıdır — ve `fn_ad`
üyeliği tam olarak onu verir. `fn_kul` alanı hem **gereksiz** hem **yanlıştı**;
kaldırıldı (D-459: ölü kod bırakma).
> **DERS: 727 dosyalık yeşil bir tarama, kuralın DOĞRU olduğunu kanıtlamaz —
> yalnız korpusun o şekli içermediğini gösterir** (D-356'nın altıncı tekrarı).
> Kuralı kanıtlayan şey, sabotajın KIRMIZI olabildiği bir şeklin var olmasıdır.

**PORTLANMADI — RT007** (`satıriçi_asm` + `çevrim:`): parser o alanı
**tüketip atıyor**, iki uygulamada da yan-kanal gerekiyor. Ulaşılabilir ve
gate'lenebilir olduğu ÖLÇÜLDÜ (geçerli probe'lar yazıldı); ayrı iş olarak
kaydedildi.

**Fikstür `tc46_02_realtime_cagri.kem`** — RT004 · RT005 (yerleşik-olmayan) ·
RT005 (dolaylı) · RT003×2 (zincir) · **RT004 (kayıtlı yerleşik)** + **pozitif**
`rt_kok → rt_yaprak` (gerçekzamanlı, gerçekzamanlıyı çağırabilir; zincir YOK).
Son iki şekil BİRLİKTE "sembol mü" ayrımını gate'ler.

**Kapılar:** checker_diff **172/172 (0 muaf)** · self_driver **139/139 +
FIXPOINT ✓** · check_kapisi 274/281 (0 RED) · check_genis 133/133 ·
codegen_diff 169/169 · wcet_test 37/37 · **repo taraması 728 dosya, 0 RT
sapması**.
**Sabotaj 3/3 (geçerli):** S132 (checker zincir dalı) → 171/172 rc=2 ·
S134 (codegen RT004/RT005) → 138/139 rc=2 · S135 (yanlış yerleşik ayrımını
geri getir) → 171/172 rc=2.
**⚠ S133 GEÇERSİZ DEĞİL, TEŞHİSTİ:** yeşil kalması kapının zayıflığını değil
**kuralın yanlışlığını** gösterdi.

### 🔴✅ D-551: CI ORTAM DENETİMİ — ve `kanal_omru`da SESSİZ GEÇEN bir ölçüm bulundu
D-548'in bıraktığı maddenin **karar** kısmı Mehmet'in (push dışa dönük bir
işlemdir); **ölçüm** kısmı yapıldı: *"`test_tumu` WSL'e özgü şeyler kullanıyor →
CI'da geçip geçmeyeceği ölçülmedi"*.

**ARAÇ ENVANTERİ (72 kapı tarandı, kelime-sınırlı):**
```
setarch            -> asan_e2e_denetim . asan_matris_calistir . kanal_omru
/usr/bin/time      -> perf_bellek                 (KORUMALI)
lake               -> lean_sorry/tam/aksiyom      (KORUMALI)
qemu-system-*      -> Makefile                    (D-453: bildirerek atlar)
ld.lld · objcopy   -> Makefile (bare-metal)
```
CI `ubuntu-latest`te yalnız **clang gcc make file** kuruyor: `setarch`
(util-linux) ve `objcopy` (binutils) taban imajda gelir; **`ld.lld` ve `qemu`
GELMEZ** → bare-metal/QEMU kapıları CI'da atlanır.
**⚠ Bu son cümle ÇIKARIMDIR, ölçüm DEĞİL** — CI koşulmadığı için doğrulanamaz;
gerçek liste ilk yeşil CI koşumunda ölçülmeli.

**🔴 ASIL BULGU — KENDİ YAZDIĞIM KAPIDA SESSİZ GEÇEN ÖLÇÜM.**
`kanal_omru`ın ASan sağlık satırı `setarch`i **korumasız** çağırıyor ve
`|| true` ile bitiyordu:
```sh
timeout 60 setarch -R "$W/${et}a" >/dev/null 2>"$W/$et.err" || true
ah=$(grep -cE "ERROR: AddressSanitizer" "$W/$et.err")
```
`setarch` başarısızsa program **hiç koşmuyor**, hata dosyası **boş** kalıyor,
`grep -c` **0** dönüyor → **SAĞLIK ölçümü SESSİZCE GEÇİYOR.** Oysa o ölçüm bu
kapının **var oluş gerekçesiydi**: D-543'te gerçek bir UAF'ı (SEGV in
`__asan free`) yalnızca o yakalamıştı.

**AYIRT EDİCİ DENEY (D-534 deseni)** — `setarch -R true` BAŞARAN ama gerçek
koşumda 127 dönen bir sahte ikili ile:
```
fix VAR  -> 🔴 ASan ikilisi KOSMADI (rc=127) — olcum YAPILMADI ; rc=2
fix YOK  -> 10/10 olcum gecti                                  ; rc=0
```
Yani düzeltme olmadan kapı **yeşil kalıyor ve hiçbir şey ölçmüyordu**.

**ONARIM İKİ PARÇALI:**
- `ASAN_RUN` **yeteneği ölçülerek** kuruluyor — `command -v setarch` YETMEZ:
  bazı çekirdeklerde `setarch` var ama `-R` başarısız olur, o yüzden
  `setarch -R true` koşturulur (`asan_matris_calistir.sh`ın zaten kullandığı
  desen; kardeş harness'lar korumalıydı, **yalnız benimki değildi**).
- ASan koşumunun **çıkış kodu** okunuyor: `127` (komut yok) ve `124` (zaman
  aşımı) artık **sert hata**. *Boş hata dosyası hem "temiz" hem "hiç
  çalışmadı" demektir; ikisi ayrılmadan "0 hata" bir KANIT DEĞİLDİR.*

**Kapılar:** kanal_omru **10/10** · asan_matris 12/12.
**Sabotaj:** S131 yukarıdaki ayırt edici deneydir (fix var → rc=2, fix yok →
rc=0). ⚠ İlk denemem **S130 geçersizdi**: `setarch`i tümüyle 127 yapınca
yetenek probe'u doğru şekilde `ASAN_RUN=""`e düşüyor ve ikili setarch'siz
GERÇEKTEN koşuyor — yani o, kusuru değil **zarif düşüşü** ölçmüştü.

### 🔴✅ D-550: SELF-HOST'a RT DENETİMİ EKLENDİ — kör nokta ölçülüp kapatıldı
D-545'te ölçülmüştü: `grep -c RT00` → checker **0**, codegen **0**. Yani
`gerçekzamanlı` bir gövdedeki döngü self-host'ta **sessizce geçiyordu** ve
hiçbir parite kapısı bunu görmüyordu — çünkü **korpusta RT tanısı veren tek bir
dosya bile yoktu** (D-356'nın *"korpusta o şekil yok"* sınıfı).

**ÖNCE HANGİ KAPININ GÖRECEĞİ ÖLÇÜLDÜ** (maddenin kendi talimatı). Geçici bir
fikstür korpusa kondu:
```
checker_diff   -> 170/171, rc=2   GORUYOR  (selfhost/checker.kem)
self_driver    -> 137/138, rc=2   GORUYOR  (selfhost/codegen.kem)
codegen_genis / ct_bariyer -> GORMUYOR
```
Sonuncusunun sebebi ölçüldü ve şaşırtıcıydı: **C `--llvm` RT ihlaline rağmen IR
üretiyor** (rc=0, 193 satır) — WCET yalnız `--check` yolunu durduruyor. Yani bu
port bir **check-zamanı** işidir; `--llvm` kapıları burada kör olmak zorunda.

**⚠ ÜÇ-UYGULAMA TUZAĞI TAM OLARAK BEKLENDİĞİ GİBİ ISIRDI.** Önce yalnız
`checker.kem`e portladım: `checker_diff` **171/171 YEŞİL**, ama `self_driver`
**137/138 KIRMIZI** — o kapı `codegen.kem`i okuyor. D-517'de kayıtlı; bu kez
madde de uyarıyordu ve yine de tek uygulamayla başlamak yetmedi.

**ONARIM:** parser `GERCEKZAMANLI`yı **yutuyordu** (D-363'ün `genel` deseni) →
`çıplak` yan-kanalının birebir aynası (`rt_node`). Ardından tek bir yürüyüş:
RT001 (dizi literali / lambda) · RT002 (`iken`/`için`) · RT003 (**doğrudan**
self-call). **Yeni tanı kodu YOK.**

**İki ince parite kuralı, ikisi de C'den OKUNDU:**
- **C işlev başına EN FAZLA BİR tanı verir** (`walk` ilk hatada `-1` döner) →
  self-host yürüyüşü de ilk bulguda durur; yoksa dump ayrışırdı.
- **Sıra:** C'de WCET tip kontrolünden **SONRA** ayrı bir geçiştir (`ana.c`) →
  pas `kontrol_ust`tan sonra koşar; per-fonksiyon serpiştirilseydi hem tip hem
  RT tanısı olan dosyada sıra bozulurdu.

**KAPSAM (bilinçli, ölçülmüş):** RT004/RT005 (çağrılanın gerçekzamanlı olup
olmadığını + yerleşik kümesini bilmek ister) · RT003'ün **zincir** dalı (D-545,
çağrı grafiği) · RT007 (asm `çevrim:` — parser o alanı atıyor) **PORTLANMADI**.
*Kısmi port ayrışmayı ARTIRMAZ:* self bugün hiç RT tanısı vermiyordu, portlanan
her kural farkı yalnızca **azaltır**.

**YANLIŞ-POZİTİF TARAMASI — 727 `.kem`, TEK sapma:** `p3_bildirimler.kem`de
C'nin **RT005**'i (portlanmayan kod). Self'in C'de olmayan bir RT bastığı
**tek bir dosya yok**. O dosya `parse_korpus`ta ve `--token`/`parser_diff` ile
gate'leniyor, `checker_diff`i etkilemiyor.

**Fikstür `tc46_01_realtime.kem`** — üç negatif şekil AYRI işlevlerde (tek-tanı
kuralı yüzünden şart) + **iki pozitif**: `temiz` (geçerli gerçekzamanlı işlev)
ve `normal` (gerçekzamanlı DEĞİL; döngüsü serbest kalmalı). Pozitifler olmasa
*"her gerçekzamanlı işlevi reddet"* sabotajı kapıdan GEÇERDİ (D-425).

**Kapılar:** checker_diff **171/171 (0 muaf)** · self_driver **138/138 +
FIXPOINT ✓** · check_kapisi 274/281 (0 RED) · check_genis 133/133 ·
codegen_diff 169/169 · ct_bariyer 14/14 · wcet_test 37/37.
**Sabotaj 2/2:** S128 (checker pası) → 170/171 rc=2 · S129 (codegen pası) →
137/138 rc=2.

### 🔴✅ D-549: `lean_tam` ARTIK GERÇEKTEN DERLİYOR — D-548'in (c) gerekçesi YANLIŞTI
**D-548'de kendi yazdığım ölçüm yanlıştı ve düzeltiyorum.** Orada
*"Windows'taki `lake.exe` bayat bir elan kilidiyle bloke ve ağ güncellemesi
istiyor"* demiştim. İkisi de yanlış:
- Kilit **geçiciydi** (eşzamanlı bir çağrının kilidi).
- Ağ denemesi, `lake.exe`i **proje dizini DIŞINDA** çalıştırdığım içindi: orada
  elan projenin pinini göremez, **varsayılan** toolchain'i çözüp indirmeye
  kalkar. Proje `leanprover/lean4:v4.29.0` pinliyor ve o sürüm **KURULU**.

**ÖLÇÜM (doğrusu):** WSL→Windows interop **çalışıyor** ve proje dizininde
`lake build` **çevrimdışı** tamamlanıyor — **33 iş, 58 sn, rc=0**.

**🎯 ASIL KÖK: KEŞİFTİ, ORTAM DEĞİL.** D-529 bu kapıyı *"lean/lake Windows'ta,
takım WSL'de"* diye opt-in bırakmıştı. Gerçek sebep: harness `lake`i ararken ne
`/mnt/c/.../.elan/bin`e ne de **`.exe`** adına bakıyordu → WSL'de **her zaman
atlıyordu**. Arama düzeltildi; kapı artık Windows worktree'sinden koşulduğunda
**ispatları gerçekten derliyor**.

**⚠ AMA BİR ORTAM SINIRI GERÇEK — ve incedir:** Windows `lake`, projesi **WSL
dosya sisteminde** olan bir ağacı **derleyemez**. Interop'ta cwd Windows'a
**UNC** görünür (`\wsl.localhost\...`); `lake --version` çalışır (dosya
sistemi işi yok) ama `lake build` yalnız *"error: 1"* deyip düşer.
```
/mnt/c worktree   -> 33 is, 58 sn, rc=0      (DERLIYOR)
~/kemgu (WSL fs)  -> rc=1, "error: 1"        (UNC cwd)
```
Kapı bu durumu artık **ADIYLA bildirip** atlıyor (sebep + çözüm yazılı), eski
yanıltıcı *"lake YOK"* mesajıyla değil.

**`test_tumu`ya BAĞLANMADI — D-548'in kararı GEÇERLİ, ama artık DOĞRU gerekçeyle:**
takım `~/kemgu`da (WSL fs) koşuyor ve orada bu kapı **yapısal olarak**
koşamaz. Her koşumda "atlandı" basan bir kapı eklemek D-486/D-490'da kapatılan
kapsam yanılsamasıdır.

**🔴 YOL ÜSTÜNDE D-529'DAN KALAN BİR KUSUR:** hata yolundaki
`echo "… bir \`require\` dış klon istiyor."` satırında **backtick komut
ikamesidir** → ağ hatası dalında kapı `require`i çalıştırmayı deneyecekti.
(Aynı sınıfı D-548'de **kendi** kapımda da yapmıştım; ikisi de düzeltildi.)

**Hata raporlaması da düzeltildi:** S127 ilk turda kırmızı verdi ama ekranda
yalnız bir **linter ipucu** vardı — kırmızının SEBEBİ görünmüyordu. Artık önce
gerçek `error:` satırları basılıyor:
```
error: Kemgu/Drf/Drf.lean:123:0: Not a definitional equality: the left-hand side
error: Kemgu/Drf/Drf.lean:123:30: Type mismatch
```

**Kapılar:** lean_tam **Windows worktree'de rc=0 (31 .olean)**; WSL fs'te
**bildirilerek atlıyor** · lean_sorry 32 dosya, 0 sorry + `require` artığı yok.
**Sabotaj S127** (`theorem s127_bozuk : 1 = 2 := rfl`) → gate **rc=1**, hata
satırı ve konumu adıyla basılıyor; geri alınca rc=0.

### ⛔✅ D-548: `lean_tam`ı otomatikleştirmenin ÜÇ YOLU DA ÖLÇÜLDÜ — hiçbiri bugün ateşlenmiyor
Madde *"kod yazmadan önce hangisinin GERÇEKTEN koşulacağını değerlendir"*
diyordu. Üçü de ölçüldü ve **üçü de bugün ateşlenemez:**

| seçenek | ölçüm |
|---|---|
| (a) belgede "elle koşulacak kapılar" listesi | belge eskir; D-530'un ARM64 listesi meşruydu çünkü o iş **otomatikleştirilemez** — bu iş edilebilir |
| (b) CI'ya bağla | **CI VAR** (`.github/workflows/ci.yml`, `claude/**` push'unda `test_tumu`) ama `origin/main` **D-349**'da, HEAD **D-547** → **~200 artım push EDİLMEMİŞ**; CI bu işlerin hiçbirini görmedi |
| (c) WSL'e lake kur, `test_tumu`ya bağla | **⚠ BU SATIR D-549'DA DÜZELTİLDİ.** WSL'de lean/lake YOK doğru; ama *"Windows lake ağ istiyor"* YANLIŞTI — ölçümü proje dizini DIŞINDA yapmıştım, orada elan projenin pinini görmez. Gerçek engel **UNC cwd**: Windows lake, WSL dosya sistemindeki bir projeyi derleyemez. Karar (opt-in) değişmedi, GEREKÇE değişti. |

**SONUÇ: `lean_tam` OPT-IN KALDI.** Sessizce atlayan (ya da her koşumda
"atlandı" basan) bir kapı eklemek, D-486/D-490'da kapatılan kapsam
yanılsamasının ta kendisi olurdu.

**🎯 BUNUN YERİNE ATEŞLENEBİLEN ŞEY KAPILANDI — D-529'un onarımı KORUMASIZDI.**
`lake build`i aylarca koşulamaz kılan şey bir ispat sorunu değil,
**kullanılmayan bir `require mathlib`** idi. O satır geri gelirse ispatlar
yine sessizce derlenmez olur ve **hiçbir şey fark etmez**: `lean_tam` opt-in,
`lean_sorry` ise lakefile'a bakmıyordu (*"0 sorry" bir tip denetimi DEĞİLDİR*).
Kural **`lean_sorry`ye** eklendi — o kapı `test_tumu`da **her koşumda** çalışır
(ayrı kapı envanteri gereksiz bölerdi; D-514/D-546 deseni).

**⚠ KURAL "`require` OLMASIN" DEĞİL** — o, meşru bir bağımlılığı da yasaklardı.
Kural D-529'un **kendi cümlesidir**: bir `require` ancak **gerçek bir kullanıma**
dayanıyorsa meşrudur → `require X` varsa en az bir dosya `import X…` etmeli.
**Pozitif ölçüm (S126) bunu kanıtlar:** gerçekten import edilen bir `require`
kapıdan **geçer** (rc=0). Yalnız negatif şekil olsaydı *"her `require`i reddet"*
sabotajı kapıdan geçerdi (D-425).

**🔴 KAPI İLK KOŞUMUNDA GERÇEK BİR ŞEY BULDU:** WSL ağacındaki `lakefile.lean`
**hâlâ eski sürümdü** — D-529'un onarımı Windows'ta yapılmış, gate'lerin
koştuğu ağaca **hiç kopyalanmamıştı**. Yani o ortamda `lake build` bugün de
eski gerekçeyle düşerdi. (İki-ağaç senkron kaybının bu oturumdaki **üçüncü**
örneği.)

**Kapı:** lean_sorry **32 dosya, 0 sorry/admit + lakefile `require` artığı YOK**.
**Sabotaj 2/2:** S125 (kullanılmayan `require` geri koy) → rc=2 ·
**S126 POZİTİF** (gerçekten `import` edilen `require`) → **rc=0**.

**⚠ KENDİ KAPIMDA BİR KUSUR — `bash -n` GÖRMEDİ.** Mesaj dizgilerinde
backtick kullanmıştım; çift tırnak içinde backtick **komut ikamesidir**, yani
kapı `require`i çalıştırmayı deneyecekti. Sözdizimi denetimi bunu **geçerli**
sayar; yalnız satırı okumak yakaladı. (D-456'da `check_genis`in muafiyet
dizgisinde ölçülen sınıfın aynısı.)

### 🧹 D-547: `codegen_genis`in 9 ATLAMASI ÖLÇÜLDÜ — kayıtlı gerekçe YANLIŞTI
`codegen_genis`'in oracle-link dalı **tamamen sessizdi**
(`atla=$((atla+1)); continue` — mesaj YOK) → **oracle tarafındaki her gerileme
sessizce yutuluyordu.** D-518'de `codegen_diff` için ölçülen kör noktanın
birebir aynısı; aynı sertleştirme uygulandı.

**⚠ KAYITLI İDDİA YANLIŞTI.** Roadmap *"9 atlama, hepsi meşru bare-metal link
hatası"* diyordu. Dokuz link hatası **tek tek okundu**:
```
kem_heap · kem_mmio_kernel · mmio_smoke · virtio_blk_config_selfhost
virtio_net_mac_selfhost · virtio_net_selfhost · virtio_selfhost
virtio_selfhost_rw          -> kdl_mmio_oku32 / kdl_mmio_yaz32   (8, GERCEKTEN bare-metal)
05_yapi                     -> "base element of getelementptr must be sized"
                               (1, bare-metal DEGIL — C ORACLE KUSURU, D-419)
```
Dokuzuncusu bare-metal değil, **oracle'ın kendi geçersiz IR'ı**. *"Muafiyet
gerekçesi de bir iddiadır"* (D-406) dersinin tekrarı.

**⚠ LİSTE AD-BAZLI DEĞİL, SEBEP-BAZLI.** `BM_MUAF`'taki bir dosya **başka** bir
sebeple linklenemezse kapı KIRMIZI olur (`kdl_mmio_` aranıyor). Düz bir ad
listesi o gerilemeyi de yutardı — muafiyetin kabul edilebilirliği
GENİŞLETMEMESİ kuralı (D-421).

**Kapı:** codegen_genis **70/70 (9 atlandı — her biri artık ADIYLA ve
GEREKÇESİYLE basılıyor)** · codegen_diff 169/169 · sifir_bolme 28/28.
**Sabotaj 2/2:** S122 (`mmio_smoke`u listeden çıkar) → *"oracle link BAŞARISIZ
ve kurate listede YOK"*, 70/71, rc=2 · S123 (`05_yapi`yi `BM_MUAF`a taşı) →
*"listede ama link hatası MMIO DEĞİL"*, 70/71, rc=2. İkisi de yeni mantığın
**iki dalını da** ölçüyor.

**⚠⚠ ÜÇÜNCÜ SABOTAJ GEÇERSİZDİ VE BİR ŞEY ÖĞRETTİ.** S124 gerçek bir *oracle
gerilemesi* denemesiydi (D-546'nın panik baytını 29→27 yap → geçersiz IR).
Kapı rc=2 verdi ama **kendi mesajlarından hiçbirini basmadı**: o gerileme önce
**`build/codegen`in kurulmasını** kırıyor, kapı hiç koşmuyor (S113 sınıfı —
sabotaj kapıyı değil YAPIMI ölçtü).
> **Bunun kendisi bir bulgu:** bu dala ulaşacak kadar ağır bir oracle
> gerilemesi, çoğunlukla **bootstrap'ı daha önce ve daha gürültülü** kırar.
> Kurasyonun asıl değeri o sınıf değil, **tek bir dosyayı** etkileyen dar
> gerilemelerdir — S122/S123 tam olarak o dalları ölçüyor.

**⚠ ÖLÇÜM ARACIM YİNE YANILDI:** ilk taramada `undefined symbol:` (lld sözdizimi)
aradım; bu ortamda bağlayıcı **GNU ld** ve *"undefined reference to"* yazıyor →
sekiz dosyanın da sembol listesi **BOŞ** göründü ve bir an *"sebep bilinmiyor"*
diye kaydedecektim.

### 🔴✅ D-546: `vektor_eleman` ARALIK DIŞI İNDEKSTE UB — cevap `-O2`de DEĞİŞİYORDU
Değişmez avı **SIMD eksenine** taşındı. Altı şeklin dördü tuttu:
```
<4> * <8>            -> V003 ✓      <tam32,4> * <kesirli32,4> -> V003 ✓
vektör<T,0>          -> V002 ✓      annot <8> = deger <4>     -> T001 ✓
vektor_eleman(v, 99) -> **OK** 🔴   vektor_eleman(v, -1)      -> **OK** 🔴
```

**🔴 GERÇEK UB, GÖZLENEBİLİR — deponun en ağır saydığı sınıf:**
```
                        -O0    -O2
vektor_eleman(a, 99)    224     1      ← AYNI PROGRAM, AYNI IR
vektor_eleman(a, 0-1)   224     1
vektor_eleman(a, 3)      42     42     ← onarim BUNU BOZMAMALI
```
`extractelement` LLVM'de aralık dışı indekste **poison**tur. Bu sarma değil,
**cevabın optimizasyon seviyesine bağlı olması**dır.

**MEKANİZMA HAZIRDI, KARAR DA VERİLMİŞTİ — yeni politika icat edilmedi.** Dil
bu sınıf için **panik** seçmişti: dizi sınırı (D-069) · sıfıra bölme (D-502) ·
kaydırma miktarı (D-514). Aynı kalıp: inline `icmp uge` + `br` +
`kdl_panik(noreturn)` + `unreachable`. **Yeni tanı kodu YOK, tip değişikliği
YOK.** C + `selfhost/codegen.kem`, davranış birebir (`-O0` ve `-O2`'de 134).

**İki bilinçli daraltma, ikisi de fikstürde POZİTİF ölçülüyor:**
- **TEK KARŞILAŞTIRMA YETER:** `icmp uge` **işaretsizdir** → negatif indeks
  (`0-1` → `0xFFFFFFFF`) aynı dala düşer; ayrı bir `icmp slt 0` gereksiz daldır.
  `negatif.kem` bunu ölçer.
- **Sabit ve aralıkta** olan indekste kontrol yayılmaz (yaygın durum); aralık
  dışı **sabitte** yayılır ki `vektor_eleman(v, 99)` da temiz dursun.
  `degisken_disi.kem` ise derleme-zamanı reddin **kaçıracağı** şekli ölçer —
  D-514'ün kendi gerekçesi.

**KAPI AYRI AÇILMADI:** `calistir_sifir_bolme` genişletildi (20 → **28 ölçüm**).
Mekanizma ve değişmez aynı; üçüncü bir kapı envanteri gereksiz bölerdi.
`normal.kem` (21+21 → 42) **zorunlu**: yalnız negatif şekiller olsaydı
*"her `vektor_eleman`'ı panikletir"* sabotajı kapıdan GEÇERDİ (D-425).

**Kapılar:** sifir_bolme **28/28 (C + SELF)** · simd_test 30/30 ·
simd_llvm_test 5/5 · llvm_test 286/286 · codegen_diff 169/169 ·
yapi_diff 149/149 (22 muaf) · sıfır uyarı 38/0.
**Sabotaj 2/2:** S120 (C) → `sabit_disi`/`negatif` **exit=0**, `degisken_disi`
exit=7 · S121 (self) → **exit=100/100/7**; ikisi de rc=2.

**⚠⚠ ARAÇ HATALARI — üçü de KAYITLI derslerin tekrarıydı:**
1. **Heredoc `\n`'i `\n`'e indirgiyor** → ürettiğim C kaynağına **gerçek satır
   sonu** girdi ve dizgi literali koptu (*"missing terminating"*). D-518'de
   aynısı yaşanmıştı; çare `chr(92)` ile kurmak.
2. **`src/llvm.c` SATIR SONLARI KARIŞIK** — dosyanın çoğu CRLF ama
   `vektor_eleman` bölgesi **LF**. `\r\n` ile kurulan çok satırlı çapa sessizce
   eşleşmedi. Çözüm: satır sonunu **kaynaktan oku**, çapayı **indeksle** bul.
3. **BAYT SAYISINI SAYDIM, ÖLÇMEDİM:** `"vektor lane indeksi gecersiz"` 28+NUL
   = **29**, ben `[27 x i8]` yazdım → tüm IR **LINK-RED**. D-543'te üç tur
   kaybettiren hatanın aynısı; bu kez `len(msg)+1` ile hesaplatıldı.

### 🔴✅ D-545: `gerçekzamanlı` KARŞILIKLI ÖZYİNELEMEYE AÇIKTI — sınırlı WCET vaadi geçersizdi
Değişmez avı **REALTIME/WCET eksenine** taşındı. Altı şeklin dördü tuttu:
```
`eşleş` kolunda dongu   -> RT002 ✓      `güvensiz` blokta dongu -> RT002 ✓
`dizi_olustur(N)`       -> RT005 ✓      duz govde (taban)       -> TEMIZ ✓
karsilikli a->b->a      -> **OK** 🔴    dolayli  a->ara->a      -> **OK** 🔴
```

**🔴 KESİN ÖLÇÜM — GEÇERLİ SAYILAN BİR PROGRAM HİÇ DURMUYOR:**
```kemgu
gerçekzamanlı işlev a(n: tam32) -> tam32 { ver b(n); }
gerçekzamanlı işlev b(n: tam32) -> tam32 { ver a(n); }
```
`--check` **rc=0 (TEMİZ)** · çalıştırınca **exit 124 (asıldı)**. `gerçekzamanlı`
işaretinin tek vaadi **sınırlı WCET**tir; bu şekilde o vaat tamamen geçersizdi.

**KÖK — RT003 YALNIZ DOĞRUDAN self-call'i soruyordu.** `wcet.h`'nin kendi notu
*"Direct self-recursion yasak (RT003)"* diyerek sınırı **adlandırmış** ama
sınırın **garantiyi deldiği** hiç ölçülmemişti. D-503/D-517 ile aynı sınıf:
kural vardı, **sorulmadığı yer** vardı.

**ONARIM — YENİ TANI KODU YOK.** RT003'ün mesajı zaten *"özyineleme (V1 yasak)"*.
Çağrı zinciri bir yığında tutulur (`cagri_yigin`); çağrılan işlev **zaten açık
zincirdeyse** RT003. Keşif `walk`'un KENDİSİYLE yapılır — ikinci bir gezgin
YAZILMADI (D-407): iç yürüyüş `sessiz`dir, tanı yalnız dış çağrı yerinde bir kez
basılır ve `hata_sayisi` iç yürüyüşte artmaz.
**Sembol → `ast_dugumu` zaten vardı** → yeni tesisat, `ana.c` değişikliği ve
imza değişikliği GEREKMEDİ.

**⚠ DERİNLİK AŞIMI SESSİZ ATLAMA DEĞİL:** zincir 32'yi aşarsa **RT005**
(*"WCET hesaplanamaz"*) — default-DENY. Sessizce geçmek, tam da kapatılan
kusurun sınıfı olurdu.

**⚠ WCET SAYISI DEĞİŞMEDİ:** iç yürüyüşün döndürdüğü maliyet **atılır**; V1
sözleşmesi gereği çağrı başına sabit 50 cycle korunur → ekleme **sayı-nötr**.

**YANLIŞ-POZİTİF TARAMASI TÜM DEPODA:** 600+ `.kem`, **tek** RT tanısı ve o da
**önceden var olan** `RT005 callee bilinmiyor` (`p3_bildirimler.kem`) →
**sıfır yeni yanlış-pozitif**.

**POZİTİF ÖLÇÜM ZATEN VARDI VE KALDI:** `W18` (a→b, döngü YOK → 0 hata). Yalnız
negatif şekiller olsaydı *"her çağrıyı RT003 yap"* sabotajı kapıdan GEÇERDİ
(D-425).

**⚠ SELF-HOST'TA RT DENETİMİ HİÇ YOK** (`grep -c RT00` → checker 0, codegen 0).
Bu **önceden var olan** bir boşluktur ve bu artımın kapsamı DIŞINDADIR; parite
kapıları kırmızı olmuyor çünkü korpusta RT tanısı veren dosya yok. İş olarak
kaydedildi.

**Kapılar:** wcet_test **35 → 37/37** · sıfır uyarı 38/0 · check_kapisi 274/281
(0 RED) · checker_diff **170/170 (0 muaf)** · check_genis 133/133 ·
codegen_diff 169/169.
**Sabotaj S119** (zincir keşfini kapat) → `[19]` ve `[20]` ✗, 35/37, rc=2.

**⚠⚠ PROBE'LARIN İKİSİ KENDİ HATAMDI ve ölçümü geçersiz kılıyordu** (D-500):
ileri bildirim (`işlev b(...) -> tam32;`) KEMGU'da YOK → **P017**, yani ilk
karşılıklı-özyineleme probe'um dili değil sözdizimini ölçüyordu (iki işlevi
sırayla tanımlamak yeterli — `pre_populate` ileri referansı zaten çözüyor); ve
`dizi_oluştur` yerleşiğinin adı `dizi_olustur`. Ayrıca `rc=$?`'yi **borudan
sonra** okuyup `head`'in kodunu ölçtüm (D-444'ün tekrarı) — ilk turda altı
probe de "rc=0" göründü.

### ✅ D-544-b: KANAL ÖMRÜ SELF-HOST'A PORTLANDI — kapı artık İKİ derleyiciyi ölçüyor
D-543/D-544 yalnız C'deydi; `selfhost/codegen.kem` **sıfır** serbest yayıyordu.
`codegen_diff` çıkış koduna baktığı için bu ayrışma **sessiz** kalıyordu
(D-486'nın *"çağrılan ama ölçmeyen kapı"* sınıfı).

**PORT — yeni analiz İCAT EDİLMEDİ.** `ky_confined` aynası self-host'ta zaten
vardı (18-UAF avından geçmiş makine); eklenen yalnız **kanal kipi**:
`Ayr`e `k_modu`/`k_spawn`/`k_join`/`k_imha`/`k_ver` + `kanal_ad`;
`ky_kanal_seffaf_mi` · CAGRI dalında sayaçlar ve iki gevşetme ·
**yeni `VER` dalı** (`k_ver` bayrağı) · `kanal_kanit_kur` · `kanal_serbest_emit`.
Sayımlar **aynı yürüyüşten** toplanır — `ky_confined` 1 dönerse ağacın tamamı
gezilmiştir, ayrı bir üst-düzey tarama gerekmez (C ile aynı gerekçe).

**Ölçülen parite — 8 kanal dosyasının sekizinde de BİREBİR:**
```
kanal_mesaj 3/3 · cg_kanal_yon 1/1 · cg_kanal_omru 2/2 · kacan_uc 0/0
cg_kanal_temel 1/1 · _metin 1/1 · _param 1/1 · _tam64 2/2      (C / SELF)
```

**⚠ LAMBDA SIFIRLAMASI PORTA DA GEREKLİ:** lifted lambda gövdesindeki `ver` de
aynı çıkış noktasından geçer; liste sıfırlanmazsa `cg_var_bul` **başka bir
yuvayı** bulup çöp işaretçi serbest bırakır (C tarafında ölçülmüş SEGV).

**KAPI İKİ DERLEYİCİYİ DE ÖLÇÜYOR** (`calistir_kanal_omru` artık
`$(BUILD)/codegen$(EXE)`e de bağımlı): **3/3 → 10/10**.

**Kapılar:** kanal_omru **10/10 (C + SELF)** · codegen_diff **169/169** ·
yapi_diff **149/149 (22 muaf)** · check_genis 133/133 ·
self_driver **TÜM MODLAR + FIXPOINT ✓**.
**Sabotaj 2/2:** S117 (self `ver` koruması) · S118 (self takma ad kanıtı) →
ikisi de negatif fikstürde **1 serbest + SEGV**, rc=2.

**⚠⚠ BU TURDA BİR SABOTAJ WSL AĞACINDA UNUTULDU VE BİR TUR ÖLÇÜMÜ GEÇERSİZ
KILDI.** Önceki turda öldürülen bir **orphan** koşum `src/escape.c`'yi **S116
uygulanmış** halde bıraktı. Sonuç iki aşamalı ve sinsiydi:
- Önce **ikili eskiydi** (S116'dan önce kurulmuş) → parite tablosu ve
  `kanal_omru` **10/10 yeşil** verdi; iddia doğruydu ama **kanıt geçersizdi**.
- Sonra bir `make` kaynağı yakaladı ve **sabote edilmiş bir oracle** kuruldu →
  `codegen_diff` **168/169** ile kırmızıya döndü ve bir an *"portum bozdu"*
  sandım. Kaynak `git`te **TEMİZDİ**.
> **DERS: iki ağaçlı kurulumda ölçümden ÖNCE ağaçları `diff`le** — `grep`
> sabotaj izi aramak YETMEZ, çünkü izin kaynakta olması ile ikilide olması
> AYRI şeylerdir. Teşhis sırası: (a) git temiz mi · (b) İKİ ağaç `diff`-eşit mi
> · (c) ikili o kaynaktan mı kurulmuş. (D-457/D-528'in üçüncü tekrarı.)

**🎯 OLUMLU YANI: KAPI TAM DA TASARLANDIĞI GİBİ ÇALIŞTI.** S116'lı oracle
negatif fikstürde `ver gönderen(k)` için serbest yaydı ve program
**segfault/asılma** verdi — yani D-544'ün negatif fikstürü, ben onu *başka* bir
şey için yazmışken, gerçek bir kontaminasyonu yakaladı.

### ✅ D-544: PROJEKSİYONLU KANALLAR DA SERBEST ALIYOR — ve bir UAF'ı önledi
D-543 `gönderen(k)`/`alan(k)` taşıyan kanalları **bilerek DENY** etmişti
(projeksiyon AYNI handle'ı verir; takma adı ayrıca kanıtlamadan serbest
bırakmak `ver g` ile UAF üretirdi). O sınır kaldırıldı.

**İKİ PARÇALI KANIT — biri düşerse kanal serbest ALMAZ:**
- `escape.c` kanal modunda projeksiyon **ŞEFFAF** (argümanı çerçeve dışına
  kaçırmaz), **ama yalnız `ver` ALT-AĞACININ DIŞINDA** (`g_ver_altinda`).
- `llvm.c`de **TAKMA AD KANITI**: her `değişken g = gönderen(k)` bağlaması için
  `escape_kanal_hapsedilmis(g)` AYRICA kanıtlanır.

**Ölçülen kazanç:** `kanal_mesaj` sızıntı **192 → 8 bayt** (D-541 prototipiyle
birebir) · `cg_kanal_yon` 1 serbest, sızıntı 0.

**🔴 İLK ÇÖZÜMÜM SAĞLAMSIZDI, NEGATİF FİKSTÜR YAKALADI.** Projeksiyonu
**koşulsuz** şeffaf yapmak `ver gönderen(k)` şeklini de *hapsedilmiş* gösteriyordu
→ kanal serbest bırakılıyor, çağıran ÖLÜ bir uç tutuyordu (ölçüldü: **exit 139**,
ASan SEGV). Şeffaflık `ver` içinde kapatıldı.

**⚠⚠ NEGATİF FİKSTÜR İKİ ŞEKİL TAŞIR VE İKİSİ DE GEREKLİ:**
```kemgu
ver gönderen(k);                        // (1) doğrudan kaçış  -> escape.c korumasını (S116) gate'ler
değişken g = gönderen(k); ver g;        // (2) TAKMA AD kaçışı -> llvm.c kanıtını (S115) gate'ler
```
Şekil (2)'de **`ver` ifadesinde `k` HİÇ GEÇMEZ** — escape yürüyüşü tek başına
`k`yı hapsedilmiş sanır. Takma ad kanıtının TEK ayırt edici ölçümü budur.

**🎯 S115 İLK TURDA YEŞİL KALDI — ve az kalsın YANLIŞ SONUCA GÖTÜRÜYORDU.**
Şekil (2) korpusta YOKTU, yani takma ad kanıtı **ayırt edilemiyordu**. D-430
gereği *"ayırt edilemeyen kod doğrulanmamış yüzeydir"* diye **silmeye** hazırdım:
8 kanal dosyasının sekizinde de S115'li ve temiz çıktı **birebir aynı**ydı.
Silmeden önce *"peki bu kod HANGİ ŞEKLİ koruyor?"* diye sordum → şekil (2)
çıktı. **Silseydim bir UAF yolu açılacaktı.**
> **DERS: "hiçbir ölçüm ayırt etmiyor" ÖNCE KORPUSU ŞÜPHELİ KILAR** (D-356),
> kodu değil. D-430'un silme kuralı ancak *"koruduğu şekil de yok"* kanıtlandıktan
> sonra uygulanabilir — aksi hâlde bir güvenlik kanıtını "ölü kod" diye silersin.

**⚠ KAPIYA ZAMAN AŞIMI EKLENDİ ve aynı turda karşılığını verdi:** fikstür
koşumlarında `timeout` YOKTU; sağlamsız hâlde program **asılıyordu**. *Asılan
kapı, sessiz kapı kadar kötüdür* (D-466/D-468/D-471). Eklendikten sonra S116
asılma yerine temiz **`exit=124`** verdi.

**Kapılar:** kanal_omru **3/3 → 5/5** · codegen_diff **169/169** · yapi_diff
**149/149 (22 muaf)** · sıfır uyarı 38/0.
**Sabotaj 2/2:** S115 (llvm.c takma ad kanıtı) → negatif fikstür 1 serbest +
**SEGV** · S116 (escape.c `ver` koruması) → 1 serbest + `exit=124`.
**⚠ V1 SINIRI KORUNDU:** **self-host portu YOK** (C 2+ serbest yayıyor, self 0;
`codegen_diff` çıkış koduna baktığı için yeşil kalıyor).

### ✅ D-543: KANAL ÖMRÜ AÇILDI — `kdl_kanal_serbest` artık ÇAĞRILIYOR
D-511 ölçmüştü: runtime işlevi **vardı** ama iki derleyicide de çağrı sayısı
**0** — kanal tamponu + kilidi sızıyordu. D-540 tavanı ölçtü (23/23 işlev şekli
sağlıyor), D-541 elle prototiple kazancı doğruladı, **D-543 uyguladı.**

**KANIT (P1–P4, hepsi gerekli; biri düşerse ESKİ davranış = sızar ama UAF yok):**
P1 yerel `değişken` + doğrudan `kanal_oluştur` · P2 işlevde `imha` YOK ·
P3 `join >= spawn` · P4 `escape_kanal_hapsedilmis`.

**🎯 ASIL TASARIM KARARI — İKİNCİ GEZGİN YAZILMADI (D-407).** P4 bir kaçış
yürüyüşüdür ve `ky_confined` ile aynı sorudur; `ast.h`'de genel çocuk
yineleyici YOK (escape.c'de ~30 elle yazılmış özyineleme). Yeni gezgin yerine
`ky_confined`e **kanal modu** eklendi. **Sayaçlar da aynı yürüyüşte toplanır:**
`ky_confined` 1 dönerse ağacın TAMAMI gezilmiştir → `spawn`/`join`/`imha`
sayımları **işlev genelidir** ve ayrı bir üst-düzey taraması gerekmez.

**🔴 ASan AVI GERÇEK BİR UAF YAKALADI — kapının var oluş gerekçesi.**
`drf_gorunurluk` → `SEGV in __asan free`, adres `0x000100000004`. Kök: **lifted
lambda gövdesindeki `ver` de aynı çıkış noktasından geçiyor** ve kanal listesi
`main`'den **sıfırlanmamış** kalıyordu → `isim_bul` lambda kapsamında başka bir
yuvayı bulup **çöp işaretçi** serbest bırakıyordu. Fikstür yine `exit 15`
veriyordu; **yalnız davranışa bakan bir kapı bunu GÖREMEZDİ.**

**⚠ ÜÇ BAYT SAYIMI YANLIŞTI ve kanıt bu yüzden hiç ateşlenmedi** (üç tur):
`kanal_al` **8** bayt (9 yazmıştım), `kanal_gönder` **13** (14). `görev_başlat`
14 doğruydu — o yüzden `spawn` sayacı çalışıyor, gevşetme çalışmıyordu. Tanıyı
kapatan şey argümanın **gerçek düğüm tipini** basmak oldu.

**YENİ KAPI `calistir_kanal_omru` — üç ölçüm:** yapı (≥1 `kdl_kanal_serbest`) ·
davranış (exit 15) · **sağlık (ASan 0 hata)**. Üçüncüsü zorunlu: yukarıdaki UAF
yalnız onunla görünür.

**⚠ V1 SINIRI (bilinçli):** `gönderen(k)`/`alan(k)` **projeksiyonu** olan
kanallar KAPSAM DIŞI — projeksiyon AYNI handle'ı verir, takma adı ayrıca
kanıtlamadan serbest bırakmak `ver g` ile UAF üretirdi. `cg_kanal_yon` ve
`kanal_mesaj` bilerek serbest ALMAZ. **Self-host portu da açık** (C 2 serbest,
self 0; `codegen_diff` çıkış koduna baktığı için yeşil).

**Ölçülen kazanç:** `cg_kanal_metin`/`_param`/`_tam64`/`_temel` → sızıntı
**0 bayt** (öncesinde kanal sızıyordu).
**Kapılar:** kanal_omru 3/3 · codegen_diff **167/167** · yapi_diff **148/148**.
**Sabotaj 2/2:** S113b (kanal modunu kapat) · S114 (emisyonu kapat) → ikisi rc=2.
⚠ İlk sabotaj (S113) GEÇERSİZDİ: `#define` derlemeyi kırdı, yani kapıyı değil
yapımı ölçtü.

### 🔴 D-537: cast yolunda LİTERAL GENİŞLİĞİ — self-host sessizce yanlış değer
D-536 ölçülürken yol üstünde çıktı ve **ayrı bir kusurdur**:
```
`8589934592 olarak tam64`
  C   : add i64 0, 8589934592      ← literal HEDEF genişlikte
  SELF: sext i32 8589934592 to i64 ← immediate'ın KENDİSİ aralık dışı
```
Program koşuyor, link temiz, değer yanlış (exit 1 / doğrusu 42). **D-299 düz
literal yolunu onarmıştı; CAST yolu açık kalmıştı** — cast operandı bağlamsız
değerlendiriliyor, literal i32'de doğuyor.

**ONARIM DAR — yalnız GENİŞLETME.** Daraltma (`300 olarak tam8`) `trunc` ile
kalır: orada kırpma **kasıtlıdır** ve C de kırpar. Fikstür bunu **pozitif**
ölçer (`b != 44 → ver 2`); olmasaydı *"her cast'i hedef genişlikte doğur"*
sabotajı kapıdan GEÇERDİ (D-425). Literal immediate'dır (komut yaymaz) →
doğrudan döndürmek ölü komut bırakmaz.

**Kapılar:** codegen_diff **166/166** · yapi_diff **148/148 (20 muaf)** ·
snapshot 50/50 · stdlib_check rc=0. **Sabotaj S111** → exit 1.
Fikstür `test/cg_korpus/cg_cast_literal_genislik.kem`.

### 🔴 D-536: DEĞER konumundaki generic yapı parametresi de T'yi kaybediyordu
D-535'in tavanı ölçülürken çıktı (655 dosya, 14 fallback olayı, 5 bölge). Üçü
ölçülerek zararsız çıktı; dördüncüsü **aynı sessiz-yanlış-cevap sınıfıydı**:
```kemgu
yapı Kutu<T> { icerik: T; }
işlev al<T>(k: Kutu<T>) -> T { ver k.icerik; }
Kutu<tam64> + 2^33   ÖNCE: @al$i32 → exit 1   (oysa @olustur$i64 DOĞRUydu)
                     SONRA: @al$i64 → exit 42
```
**HER İKİ derleyicide.** Mevcut dal yalnız **`&Kullanıcı<T>`** (referans) konumunu
tanıyordu; **değer** konumu açıktı. C'de kaynak `generic_arg_ir` yan-kanalı;
self-host'ta `argt` o konumda **`%Kutu$i64`** taşır — yapı IR'i type-erased ama
**mangle edilmiş AD T'yi korur** (`mono_ir_sonek`). Kanal yoksa dokunulmaz
(default-DENY). İndeks önceliği D-535 ile aynı gerekçeyle kurulur.

**⚠⚠ ÖNCEKİ TURDA "SELF SPECIALIZE ETMİYOR" DİYE KAYDETMİŞTİM — YANLIŞTI.**
`grep ... | head -12` çıktıyı **kırpmıştı**; self `@olustur$i64`'ü zaten yayıyor,
ben yalnız taban gövdeyi (`define %Kutu$i32 @olustur`, K4) görmüştüm. O yanlış
okuma yüzünden onarımı bir tur "dönüş-tipi-güdümlü çıkarsama gerekiyor" diye
**erteledim**. *Kırpılmış çıktı, yanlış bir mimari sonuca götürür.*

**🔴 YOL ÜSTÜNDE AYRI BİR KUSUR ÖLÇÜLDÜ — D-537 (AÇIK):** self-host'ta
`8589934592 olarak tam64` → **`sext i32 8589934592 to i64`** (literal i32'de
materyalize ediliyor, aralık dışı) → sessiz yanlış değer. C temiz (`add i64 0,
8589934592`). D-299 düz literal yolunu onarmıştı; **cast yolu açık kalmış.**
Fikstür bu şekli bilerek KULLANMAZ — yoksa D-536 kapısı **yanlış sebeple**
kırmızı olurdu (D-421).

**Kapılar:** codegen_diff **165/165** · yapi_diff **147/147 (20 muaf)** ·
snapshot 50/50 · stdlib_check rc=0.
**Sabotaj 2/2:** S109 (C dalı) → exit 1 · S110 (self dalı) → exit 1.
Fikstür `test/cg_korpus/cg_generic_yapi_deger.kem`.

### 🔴 D-535: `Dizi<T>` PARAMETRESİNDEN ÇIKARSAMA YOKTU — SESSİZ YANLIŞ CEVAP
`yapi_diff` K4 kökünü (*"generic BASE gövdesi"*) ölçerken D-401'in sınırının hâlâ
geçerli olduğu doğrulandı — **ama yanında belgelenmemiş, daha ağır bir açık çıktı.**

```kemgu
işlev al_ilk<T>(xs: Dizi<T>, v: T) -> T { ver dizi_al(xs, 0); }
Dizi<tam64> + 2^33   ÖNCE: call i32 @al_ilk$i32  → exit 1   (generic'siz aynı iş: 42)
                     SONRA: @al_ilk$i64          → exit 42
```
Derleme temiz, link temiz, program **koşuyor** — deponun en ağır saydığı sınıf.

**KÖK:** monomorfizasyon T'yi (a) çıplak `x: T`, (b) `v: *T`, (c) `l: &Kul<T>`
konumlarından çıkarsıyordu; **`xs: Dizi<T>` dalı YOKTU** → `"i32"` fallback'i.
`Dizi<T>` **`stdlib/dizi.kem`'in TAMAMININ dayandığı şekildir** — en yaygın olan
konum eksikti. `argt` yalnız `"ptr"` taşır; eleman tipi paralel bir kanaldan
(C `eleman_llvm_tip`, self `cg_var_elem_bul`) gelmeli.

**🔴 D-411'İN SAĞLAMLIK İDDİASI ÖLÇÜLDÜ VE ÇÜRÜDÜ.** *"Fallback yanlışsa define
ve çağrı yine anlaşır ama annotasyonla uyuşmaz → LLVM REDDEDER; hata GÜRÜLTÜLÜ
kalır, sessiz yanlış cevaba dönüşmez."* `Dizi<T>` konumunda **annotasyon da
uyuşur** (dizi her genişlikte `ptr` taşır) → LLVM **susar**, değer kırpılır.
İddia D-415'te bir kez daha yanlış çıkmıştı (`büyü<T>`); bu ikinci kez ve bu kez
**sessiz**. Yorum ikisinde de gerçeğe göre güncellendi.

**⚠⚠ AYIRT EDİCİ TİP ŞART — `tam32` İLE ÖLÇÜLEMEZ.** Doğru çıkarsama ve `i32`
fallback'i `tam32`de **birebir aynı IR** üretir; fikstür `tam64` + 2^33 kullanır.
İlk üç probe'um `tam32` ile yazılmıştı ve **açığı göremiyordu**.

**⚠⚠ ÖNCELİK PARAMETRE İNDEKSİNDEDİR — self-host portu İLK DENEMEDE HİÇBİR ŞEY
DEĞİŞTİRMEDİ.** C parametreleri **sırayla** gezip ilk çıkarsamayı alır; yukarıdaki
şekilde çıplak `v: T` **index-1**'de ve argümanı literal `0` (i32). Self-host'un
`fn_param_idx`i sıra gözetmeden tüm parametreleri tarayıp o çıplak T'yi buluyordu
→ yeni dal `bulundu == yanlış` koruması yüzünden **hiç ateşlenmedi**
(`@al_ilk$i32` aynen kaldı). Onarım dalı eklemek değil, **indeks önceliğini
kurmaktı**. *Bir dalı eklemek onun kazanacağı anlamına gelmez.*

**⚠⚠ FİKSTÜR İLK HÂLİYLE YANLIŞ SEBEPLE YEŞİLDİ.** `al_ilk<T>(xs: Dizi<T>, v: T)`
şeklinde C'nin (c) dalını kapatınca sonuç **yine 42** çıktı: `değişken g: tam64 =
al_ilk(a, 0)` beklenen tipi literal `0`'a yayıyor ve çıplak `v: T` dalı i64
veriyor. Ayırt eden şekil, T'yi taşıyan **tek** parametrenin dizi olmasıdır
(`ilk_tek<T>(xs: Dizi<T>) -> T`). Fikstürde **ikisi de** var — biri C'yi, diğeri
self-host'un indeks önceliğini kilitler. *Parite kapısı tek başına yetmez: iki
taraf birden yanlışsa yeşil kalır → her iki taraf AYRI sabote edilmeli.*

**⚠⚠ DÖRT GEÇERSİZ SABOTAJ ÜST ÜSTE** (D-500 listesinin üç maddesi birden):
`make build/kemgu` hiçbir şeyi yeniden kurmadı (bayat ikili) · sabotaj desenim
**tek satırlıydı, kod çok satırlı** → sessizce eşleşmedi ve beni *"C'de başka bir
yol da çıkarsıyor"* diye **yanlış bir hipoteze** soktu · `/tmp` `wsl.exe`
çağrıları arasında **siliniyor** → ölçüm dosyası yokken çıkan `exit=1`'i bir an
gerçek sonuç sandım. Reçete: tek çağrı · `build/` altında çalışma dizini · desen
sayısını **bastır** · `grep` ile doğrula.

**Kapılar:** codegen_diff **164/164** · yapi_diff **147/147 (19 muaf)** ·
stdlib_check rc=0 · check_kapisi 269/276 (0 RED).
**Sabotaj 2/2:** S107 (self indeks önceliği) → 163/164 · S108e/f (C dalı) → 42→1.
Fikstür `test/cg_korpus/cg_generic_dizi_cikarsama.kem`.
**Kalan (değişmedi):** dönüş-tipi-güdümlü çıkarsama hâlâ YOK (`@bos_yap$i32`) —
K4 muafiyetinin asıl kökü odur ve ayrı bir iştir.

### 🧹 D-534: `check_kapisi`de BAYAT MUAFİYET — kör nokta ölçümle kanıtlandı
8 muafiyet girdisi **tek tek** ölçüldü (kapı 7 sayıyordu — fark ipucuydu).
Yedisi hâlâ gerçek; **biri ölü**: `cg_skaler_deref.kem` gerekçesi *"kasıtlı
skaler deref cast (E002)"* diyordu ama dosya `--check`ten **temiz geçiyor**.
Listenin kendi kuralı: *"Muafiyet KALICI DEĞİL: gerekçe ortadan kalkarsa satır
silinmeli."*

**⚠ BAYAT MUAFİYET ZARARSIZ DEĞİLDİR — AYIRT EDİCİ DENEYLE ÖLÇÜLDÜ.** Aynı
dosyaya kasıtlı bir tip hatası enjekte edildi (S106) ve kapı **iki durumda**
koşuldu:
```
(a) muafiyet SILINMIS   -> 1 RED,  rc=2   kapi YAKALIYOR
(b) muafiyet GERI KONMUS -> 8 muaf, 0 RED, rc=0   kapi SESSIZCE YUTUYOR
```
Yani girdi dururken o dosyanın **gerçek bir kırılması görünmez** olurdu.
*Silme işlemi "temizlik" değil, bir kör noktanın kapatılmasıdır.*

**⚠ TEK BAŞINA "268/275, 7 muaf" YETERSİZ KANITTI:** girdi ölü olduğu için
silmek **hiçbir sayıyı değiştirmedi**. Değişikliğin değeri ancak *"yanlışın
gözlenebilir olduğu şekli"* (D-425) kurunca ölçülebildi.

**Doğrulanan diğer 7:** `kem_os` T002 (parça dosya) · `kem_asm_kernel`
AS001+T001 · `kem_kullanici` AS001 (**ve iddia edilen `--mimari arm64`
yeniden denetimi harness'ta GERÇEKTEN var**, satır 83) · `lineer_hata`
L001/L002/L004/LR002 (belgede yazılı dördü birebir) · `sifrele_dosya` T002 ·
`cg6_trunc` E004 · `cg_deref_pointer` T001.

### ✅ D-533: D-531'İN BULGU 2'Sİ YANLIŞTI — eksik PORT, dil kararı DEĞİL
D-531'de *"self-host legacy düzleştirmeyi HİÇ uygulamıyor"* diye kaydetmiştim
ve **bunu bir dil yüzeyi kararı sandım**. Ölçüm düzeltti:
```
C            -> OK    (duzlestirme; private ad gorunur)
codegen.kem  -> OK    (D-427'de ALMIS; IR de dogru)
checker.kem  -> T002  (legacy dali ve kullan_yeni_bicim_mi YOK)
```
**Okuduğum kod bir uygulamadan (`codegen.kem`), test ettiğim ikili
başkasındandı (`checker.kem`).** Üç-uygulama tuzağının bu oturumdaki
**dördüncü** tekrarı — ve bu kez beni *"karar gerekiyor"* diye **yanlış bir
sınıfa** soktu: aslında kural zaten yazılmış ve çalışıyordu, yalnız bir
uygulamada yoktu. **Port edildi, karar gerekmedi.**

**⚠ PORTTA BİR SADELEŞTİRME — ÖLÇÜLDÜ, VARSAYILMADI.** `codegen.kem`'in
yükleminde bir de ALIAS (`al_yol`) döngüsü var; `checker.kem`'de o dizi yok.
Parser `P046` ile *"seçili/alias import v1'de TEK modül adı gerektirir"* diyor
→ **`seg > 1` iken seçili de alias da imkânsızdır**, yani o döngüler bu dalda
**ulaşılamaz**. `si_yol` yine de korundu (checker'da var; P046 ileride gevşerse
doğru kalsın).

**Fikstür geri kondu** (`ana_legacy_gizlilik.kem` + `ic/derin.kem`) — artık
kapıyı kırmıyor çünkü altındaki gerçek ayrışma onarıldı.
**Sabotaj S105** (`legacy` bayrağını `yanlış` sabitle) → `checker_diff`
170 → **169/170**, rc=2 ✓
**Kapılar:** checker_diff **170/170 (0 muaf)** · modul_codegen 22/22 ·
check_genis 133/133.

**⚠ D-520 HÂLÂ AÇIK VE DEĞİŞMEDİ:** çok-segmentli `kullan` T041'i atlıyor;
kapatmak ~700 referanslık göç ister ve **o** bir dil yüzeyi kararıdır. Bu
artım yalnız **iki uygulamayı hizaladı** — açığı kapatmadı, ölçülebilir kıldı.

### 🔍 D-531: D-520'yi kilitleme denemesi İKİ YENİ ŞEY ÖLÇTÜ — fikstür GERİ ÇEKİLDİ
D-520'nin *"çok-segmentli `kullan` T041'i atlıyor"* açığını **fikstürle
kilitlemeye** çalıştım. Fikstür yazıldı, iki yeni gerçek ölçüldü, sonra
**bilerek geri çekildi**.

**BULGU 1 — LEGACY MODÜL ÇÖZÜMÜ CWD-GÖRELİDİR**, içe aktarana göre DEĞİL:
```
test/moduller dizininden:  kullan ic::derin;   -> OK
depo kokunden:             ayni dosya          -> T040 (modul bulunamadi)
```
Yeni-biçim yükleyici **içe aktaranın dizinini** arar (D-427'de belgeli);
legacy yol **CWD**'ye bakar. `drivers/virtio` da bu sözleşmeyi kullanıyor
(`kullan drivers::virtio::constants;` — kök-göreli). **Bu ayrım hiçbir yerde
yazılı değildi** ve kapılar kökten koştuğu için modül-dizini-göreli yazılmış
her legacy import sessizce T040 alır.

**🔴 BULGU 2 — SELF-HOST LEGACY DÜZLEŞTİRMEYİ HİÇ UYGULAMIYOR:**
```
C   : OK                       (legacy duzlestirme; private ad gorunur)
SELF: T002 26:9 · T002 26:25   (adlar TANIMSIZ)
```
Yani **D-520'nin kaçış kapısı C'de VAR, self-host'ta YOK.** `test/moduller`de
çıplak çok-segmentli tek bir import bile yoktu (`ana_secili` seçili-biçim
kullanıyor) — bu yüzden `checker_diff` bu ayrışmayı hiç görmemişti.

**⚠ FİKSTÜR NEDEN GERİ ÇEKİLDİ:** `checker_diff`in muafiyet listesi **bilerek
BOŞ** ve yorumu *"modül yüzeyi D-361/362/363'te tamamen kapandı"* diyor.
Fikstür oraya konsa ya kapı kalıcı kırmızı olurdu ya da o listeye **ilk
muafiyet** girerdi — ikisi de belgelenmiş bir kazanımı bozar. Üstelik madde
açıkça *"göçü BAŞLATMA"* diyordu; yeni bir parite cephesi açmak o sınırı aşar.
**Bulgu kaybolmadı: iş olarak `LOOP.md` Sırada'ya girdi.**

**⚠ SONUÇ D-520'Yİ DEĞİŞTİRİYOR.** Orada *"gerçek kodun çoğunda gizlilik
kapalı"* denmişti; doğrusu **oracle'da kapalı, self-host'ta zaten kapalı
değil**. Göç kararı verilirse iki uygulamayı da hizalamak gerekir — ama
hangi yönde hizalanacağı (C'yi sıkılaştır / self'i gevşet) **dil yüzeyi
kararıdır**.

### 📋 D-530: ARM64 fiziksel donanım kontrol listesi — D-490'ın borcu prosedüre çevrildi
D-490 *"gerçek doğrulama yalnız fiziksel ARM64 donanımında yapılabilir"*
demişti; kaynak dosyanın başlığında bir uyarı vardı ama **çalıştırılabilir adım
yoktu** — yani borç ertelenmiş değil, **unutulmaya açık** haldeydi.
`belgeler/ARM64_Fiziksel_Donanim_Kontrol_Listesi.md` yazıldı: ön koşullar ·
taban koşumu · sabotaj · sonucun nasıl OKUNACAĞI · kaydın nasıl güncelleneceği.

**⚠ EN ÖNEMLİ MADDE — YEŞİL SONUÇ "BARİYER GEREKSİZ" DEMEK DEĞİLDİR.** Tam
olarak QEMU'da yaşanan budur ve orada yanıltıcı olduğu ölçülmüştür. Yeşilse
prosedür testi **güçlendirmeyi** söyler (öğe sayısını büyüt, iş başına hesabı
küçült, 100 kez tekrarla); *"aralıklı bir kırmızı bile kesin kanıttır"* —
zayıf bellek hataları belirlenimci değildir.

**⚠ BELGEDEKİ HER İDDİA DEPOYA KARŞI DOĞRULANDI** ve **ikisi yanlış çıktı:**
- `grep -c "dc civac"` → **9** döner (yorumlar da sayılıyor); kodda **2** var.
  Doğru desen `grep -cE '"dc (civac|ivac)'`.
- `dsb sy` **4 kez** geçer: ikisi önbellek bakımına eşlik eder, **ikisi
  spinlock yolundadır**. Sabotaj yalnız ilk ikisini hedeflemeli — spinlock
  bariyerini kaldırmak **başka bir şeyi** ölçer ve sonucu yorumlanamaz kılar.
  Bu ayrım belgede de kaynakta da yazılı değildi.

**🎯 YOL ÜSTÜNDE ÖZ-GÖNDERGESEL BİR TUZAK:** kaynak dosyaya sayım desenini
yorum olarak eklediğimde **desen kendini saydı** ve sayılar `2→3`, `4→5` oldu.
Yani *talimatın kendisi öğrettiği ölçümü bozuyordu.* Desenler kaynaktan
çıkarılıp yalnız kontrol listesinde bırakıldı; kaynak buna **işaret ediyor**.

### 🎯 D-529: LEAN İSPATLARI İLK KEZ GERÇEKTEN DERLENİYOR — `require mathlib` ÖLÜYDÜ
`calistir_lean_sorry` yalnız `sorry`/`admit` **sayıyordu** ve kendi çıktısında
dürüstçe *"⚠ lake build KOŞULMADI"* diyordu. **`sorry`suz ama DERLENMEYEN bir
dosya hiçbir şey kanıtlamaz** — sayım, tip denetiminin yerine geçmez.

**KÖK ÖLÇÜLDÜ:** `lake build` mathlib4'ü klonlamaya çalışıp
`git exited with code 128` ile **11 DAKİKA** sonra düşüyordu. Ama:
```
32 .lean dosyasi · `import Mathlib` = 0 · tum importlar ic (`Kemgu.*`)
```
`require mathlib` **bildirim artığıydı**. Kaldırılınca proje **çevrimdışı
45 saniyede, 33/33 iş, sıfır hata** ile derlendi. Yani ispatlar bugüne kadar
hiç tip-denetlenmemişti ve engel bir ispat sorunu değil, kullanılmayan bir
bağımlılıktı.

**YENİ KAPI `calistir_lean_tam`** — `test_tumu`ya **BAĞLANMADI** (bilinçli):
lean/lake bu depoda **Windows**'ta (`~/.elan/bin`), takım **WSL**'de koşuyor.
Opt-in; `lake` yoksa **bildirerek** atlar (D-453'ün QEMU deseni).
`lean_sorry`nin bayat uyarısı da güncellendi: ikisi birbirini tamamlar.

**⚠ POZİTİF KANIT EKLENDİ:** artımlı derlemede lake hiçbir şey basmaz → "0 iş"
tek başına *"önbellekten mi geçti, gerçekten derledi mi"* sorusunu yanıtlamaz.
Kapı ayrıca **`.olean` sayar** (31); sıfırsa *"boşa koştu"* diye kırmızı olur.

**Sabotaj S104** (`theorem s104_bozuk : 1 = 2 := rfl`) → `error: 1 = 2`,
`Kemgu.Drf.Drf` başarısız, **rc=2** ✓

**⚠ İKİ PROBE HATASI:** (1) `set -u` altında `$USER` Git Bash'te **tanımsız**
→ harness kendi hatasıyla düştü (`${USER:-${USERNAME:-}}`). (2) Atlama dalını
`PATH`i boşaltarak sınadım ama **harness kendisi `~/.elan/bin`'i PATH'e
ekliyor** → atlama hiç tetiklenmedi; `HOME` de gizlenince doğru ölçüldü.

**⚠ DÜRÜSTÇE:** `lake-manifest.json` hâlâ mathlib girdisini taşıyor;
`lake update` ağa çıkacağı için **dokunulmadı**. İddia *"derleniyor"*,
*"manifest tutarlı"* DEĞİL.

### ✅ D-528: D-506'nın 17× bellek kazancı artık KAPI ALTINDA
Yeni kapı `calistir_perf_bellek`: `test/perf/bench{1,2}.kem` çalıştırılır ve
**zirve RSS** okunur, her iki derleyicide (4 ölçüm). Eşik **4096 KB**.

**NEDEN GEREKLİ — davranışsal kapılar bu sınıfa KÖRDÜR** (D-417/D-488'in
üçüncü tekrarı): yönlendirme bozulsa program yine `exit 42` verir,
`codegen_diff` yeşil kalır, ASan susar; yalnız **bellek büyür**.
`bolge_operand` IR'daki ρ **SINIFINI** ölçer (yapısal), bu kapı **gerçek
tüketimi** ölçer (davranışsal) — biri diğerinin yerine geçmez.

**Eşik ölçümle seçildi:** bugün her iki derleyicide de **1152 KB**
(D-506'nın kaydettiği değerle birebir); ρ_caller'a dönüş **19968 KB**.
4096 KB ≈ 3.5× başlık bırakır ama regresyonu kesin yakalar. Dar eşik ortam
gürültüsünden aralıklı kırmızı verirdi.
**Çıkış kodu da denetlenir (42):** yalnız RSS ölçmek yetmez — hiç çalışmayan
program da düşük RSS verir (D-506'da `rc=127` ile `0.00 sn` ölçülmüştü).

**Sabotaj S103** (D-506'nın `bolge_yerel_yonlendir` çağrısını `rho_ref`e
döndür) → `🔴 C bench2: zirve RSS 19968 KB > eşik 4096 KB`, rc=2.

**⚠⚠ SABOTAJ ZİNCİRLEME İKİ ARTEFAKT ÜRETTİ — ikisi de kayıtlı derslerin
tekrarıydı:**
1. **`perf_bellek` hedefi `$(BUILD)/codegen`e BAĞIMLI** → make onu **S103
   etkinken yeniden kurdu**. Sabotajı geri alırken yalnız `kemgu`yu
   kurmuştum. *Bir sabotaj döngüsünde, sabote edilen derleyiciyle ÜRETİLEN
   her artefakt da kirlenir.*
2. **WSL ağacındaki `selfhost/codegen.kem` hâlâ S100 taşıyordu** — D-526'nın
   sabotajını Windows'ta geri alıp WSL'e kopyalamamıştım. Belirti `sext i32 1
   to i1` (K1'in 9 dosyası LINK-RED) idi ve bir an D-526'yı bozdum sandım;
   **git'teki kaynak TEMİZDİ**. D-517'de tam bu dersi yazmıştım.
   → Teşhis sırası: (a) git'teki kaynak temiz mi, (b) İKİ ağaç senkron mu,
   (c) ikili o kaynaktan mı kurulmuş.

**Kapılar:** perf_bellek 4/4 · codegen_diff **163/163** · yapi_diff
**147/147 (18 muaf)**. `test_tumu` + `.PHONY`ye bağlandı.
⚠ `/usr/bin/time` yoksa kapı **bildirerek** atlar (D-453'ün QEMU deseni);
D-486'nın yasakladığı şey SESSİZ atlamadır.

### ✅ D-527: `kanal` ABI'si artık BARE-METAL'de de ölçülüyor
Roadmap *"`kanal` bare-metal: ABI hazır ama test yok"* diyordu. **Ölçüldü:
`runtime/*.kem` ve `kem_os.kem` içinde `kanal_oluştur/gönder/al` kullanan TEK
BİR SATIR YOKTU** — yani "ABI hazır" iddiasını hiçbir kapı zorlamıyordu
(D-446'nın *"var olan ama çağrılmayan kapı"* sınıfının fikstür karşılığı).

`runtime/kem_kanal_abi.kem` eklendi (`çıplak`, ARM64). `baremetal_diff`
`runtime/*.kem`i gezdiği için otomatik kapsandı: **4/4 → 5/5 birim**.
IR iki derleyicide de birebir:
```
declare ptr @kdl_kanal_olustur(i32) . void @kdl_kanal_gonder(ptr, i64)
declare i64 @kdl_kanal_al(ptr)      . define i64 @kem_kanal_abi_yoklama(i32)
```

**⚠ İDDİA DAR VE DÜRÜST: "ABI iki derleyicide AYNI yayılıyor", "kanal
bare-metal'de ÇALIŞIYOR" DEĞİL.** D-490 ölçtü ki `kem_os` QEMU'da **`-smp`
olmadan** koşar (tek çekirdek) → orada koşturmak DRF hakkında hiçbir şey
kanıtlamaz. Gerçek koşum fiziksel ARM64 donanımının işidir.

**⚠⚠ İLK SABOTAJIM (S101) GEÇERSİZDİ VE SESSİZ KALDI.** Fikstürün KAYNAĞINI
değiştirmiştim; `baremetal_diff` C↔self karşılaştırır, kaynak değişikliği
**iki derleyiciyi birden** etkiler → parite bozulmaz. *Parite kapısında sabotaj
TEK TARAFI bozmalıdır.* S102 (self-host `kdl_kanal_al` declare'ını `i32` yap)
→ **rc=2** ✓

### ✅ D-526: K1 KAPANDI — `-> mantıksal` dönüşü self-host'ta da `i1`
`yapi_diff`in en büyük muafiyet kökü (9 dosya) kapandı: **27 → 18 muafiyet**.
C `-> mantıksal` için `define i1` yayıyordu, self-host `define i32`.

**DAR TUTULDU — `ll_tip` DEĞİŞTİRİLMEDİ.** Eşleme yalnız `islev_donus_tip`te:
`mantıksal` aritmetik bağlamda hâlâ `i32` taşınır. D-430 bu işi *"çekirdek
skaler yola dokunur"* diye **iki kez bilerek ertelemişti**; sonuç konumunu
ayırmak o riski ortadan kaldırdı.

**🔴 İLK DENEME 9 DOSYAYI LINK-RED YAPTI — kök `tip_genislik`ti.**
`ret` uyarlaması `int_uydur`dan geçiyor ama `tip_genislik("i1")` tabloda YOKTU
ve **varsayılan 32**'ye düşüyordu → daraltma görülmedi, `sext i32 .. to i1`
yayıldı = GEÇERSİZ IR (`sext` GENİŞLETİR). `i1 → 1` eklenince `trunc i32 → i1`
doğru yayıldı.
**DERS: bir tip tablosuna yeni bir tip sokmadan önce, o tipi OKUYAN tüm
yardımcıların onu TANIDIĞINI ölç** — `int_ll_mi` `i1`i tanıyordu,
`tip_genislik` tanımıyordu; ikisi yan yana duruyor.

**Sabotaj S100** (`i1` girdisini kapat) → `codegen_diff` 163 → **154/163**,
rc=2 (tam da ilk denemedeki 9 dosya).
**Kapılar:** codegen_diff **163/163** · yapi_diff **147/147 (18 muaf)** ·
baremetal_diff 4/4 · ct_bariyer 14/14 · llvm_test 286/286.

### ✅ D-525: D-510'un dalı ULAŞILABİLİR ÇIKTI — borç kapandı, fikstür eklendi
D-510 bilinmeyen-eleman varsayılanını `"i32"` (İZİN) → `"ptr"` (DENY) yapmış
ama **dalı ateşleyen bir şekil bulamamıştım** → *"ölçülemeyen ama tek yönlü"*
diye borç kaydedilmişti. **O kayıt YANLIŞTI.**

**ÖLÇÜM (kaynağa geçici iz koyarak, tahmin YOK):** dal **ULAŞILABİLİR**.
```
dizi_al([40,2],0)            -> 2 iz     çeşit payload K::Tek([42])  -> 1 iz
al([1]) (literal argüman)    -> 1 iz     iç içe [[42],[1]]           -> 2 iz
generic kimlik([42])         -> 0 iz
```
Ortak nokta: **bağlamın eleman tipini vermediği** yerler. D-510'da bulamamamın
sebebi probe seçimiydi — annotasyonsuz bağlama ve yapı alanı denemiştim,
ikisi de bağlamı sağlıyor.

**⚠ KORPUS TARAMASI: `cg_korpus` + `test/perf` = 0 İZ.** Dal gerçek şekillerde
ateşleniyor ama **hiçbir fikstür onu içermiyordu** — *"korpusta o şekil yok"*
(D-356) sınıfının bir kez daha tekrarı. `cg_bilinmeyen_eleman.kem` eklendi;
altı tahsisin altısı da **GLOBAL** (DENY doğru tarafta), C↔self birebir.

**⚠ ÖLÇÜM ARACI YİNE YANILDI:** ilk enstrümantasyon (iç içe heredoc'ta python)
**hiç uygulanmadı** ve beş şekil de `0 iz` verdi — *"ulaşılamaz, borç kapalı"*
diye kaydedecektim. `grep -c` ile yamanın indiğini doğrulayınca ortaya çıktı.
**Sessiz sıfır, önce ARACI şüpheli kılar** (D-500).

### 🟡 (TARİHÎ) D-510: bilinmeyen eleman → DENY
D-509'un ardından **komşu şekiller tarandı** (D-438 disiplini). Aynı indirgeme
kalıbı C'de başka yerde YOK, ama self-host'ta **iki** yönlendirme çağrısı
bilinmeyen elemanı `"i32"`ye — yani **SKALER = İZİN** tarafına — düşürüyordu.
D-509'un ölçtüğü UAF sınıfının tam varsayılanı. İkisi de `"ptr"` = DENY yapıldı;
`5859`'da yönlendirme elemanı emisyon elemanından **ayrıldı** (emisyon bilinmeyen
elemanda `"i32"`ye düşmek ZORUNDA — bayt genişliği/eleman işlemleri).

**⚠⚠ DÜRÜSTÇE: BU DEĞİŞİKLİĞİ AYIRT EDEN BİR ŞEKİL BULAMADIM.** Dört probe
denendi: annotasyonsuz literal (C heap dizisi bile YAYMIYOR — stack yolu) ·
annotasyonlu `Dizi<metin>` (zaten GLOBAL) · annotasyonsuz `dizi_olustur`
(**`--check` REDDEDİYOR**) · yapı alanı literali (zaten GLOBAL).

**D-430 ("ayırt edilemeyen kod doğrulanmamış yüzeydir") GEREĞİ GERİ ALINMADI —
fark şu:** D-430'da geri alınan şey bir **anlambilim eklemesiydi** ve YANLIŞ
olabilirdi. Bu ise bir **varsayılanın yönü** ve yalnız **DENY** tarafına:
güvensiz yönde yanlış OLAMAZ, en kötü ihtimalle ulaşılamaz bir yolda bir
optimizasyon kaçırır — ve kaçırmadığı ölçüldü (`bench2` HÂLÂ `YEREL`, D-506'nın
17× kazancı yerinde; üç kapı yeşil). Değeri, o yolu ileride ulaşılabilir kılan
kişinin **ALLOW varsayılanını miras almaması**.
**Bu satır bir BORÇ kaydıdır, gizleme değil:** o yol ulaşılabilir hâle gelirse
ayırt edici bir fikstür EKLENMELİ.

### 🎯 D-507→D-508: kapanış env + `bölge_al` ρ_yerel'e — sızıntı 9 → 2
**D-507 (C):** kapanış env'i `@malloc` ile alınıp **HİÇ serbest edilmiyordu**.
Hapsedilme kanıtı varsa artık ρ_yerel'den. Üç eksik dal ölçümle bulundu:
`imha(e)`/`kullan(e)` (`ky_var_gecer` ve `ky_confined`'de dal YOKTU → lineer
yakalayan HER kapanış düşüyordu; **bisect: skaler yakalayan aynı şekil
GEÇİYORDU**) ve **çağırmak tutmak değildir** (`g()` — değişkenin ÇAĞRI HEDEFİ
olması kaçış sayılıyordu). Sağlamlık G006'ya dayanır: lifted gövde env'e
işaretçi döndüremez. **ARGÜMAN konumu HÂLÂ DENY yolundan geçer.**

**🎯 D-508 — D-494/D-495/D-507'nin SELF-HOST PORTU.** Üçü de YALNIZ C'ye
uygulanmıştı; `bolge_operand` kapısı bunu **İLK KEZ görünür kıldı** (4 dosyada
sapma). Portlanan üç kural:
- `bölge_al` işaretlemesi (`TIP_POINTER` annotasyon + CAGRI + `ky_confined`)
- **`bellek_kopyala` (memcpy) argümanını TUTMAZ** → kürate beyaz liste.
  **⚠⚠ `bellek_serbest` (free) BİLEREK LİSTEDE YOK VE BU KRİTİK:** onu
  hapsedilmiş saymak hem açık `free` hem bölge serbestını çalıştırırdı =
  **ÇİFT SERBEST.** Liste *"işaretçi alan her yerleşik"* değil,
  *"argümanını TUTMAYAN yerleşik"* demektir (D-459 disiplini).
- `ky_cast_soy` — `olarak` escape semantiğini DEĞİŞTİRMEZ; ham işaretçi bu
  yerleşiklere DAİMA `olarak metin` ile geçer.

**Muafiyet 4 → 0:** `bolge_operand` **160/160, SIFIR muaf**.
`cg_bolge_al_hapsedilme`de 1 `@malloc` KALIYOR — kanıtsız dal, kasıtlı
(*"sızıntı bir hata, UAF bir felaket"*).

**⚠⚠ KAPI BAYAT İKİLİYİ ÖLÇTÜ — port DOĞRUYKEN 6/160 SAPMA gösterdi.**
Ben `build/cg` kurmuştum, harness `build/codegen` okuyor. Doğrudan
karşılaştırma eşitken kapı kırmızıydı ve bir an portu yanlış sandım.
**"Kapı kırmızı ama doğrudan ölçüm yeşil" ilk olarak ARTEFAKT şüphesi
doğurur** (D-457'nin üçüncü tekrarı): önce HANGİ ikiliyi okuduğunu ölç.

**⚠⚠ YAPISAL KAPI YEŞİLKEN DAVRANIŞSAL KAPI KIRMIZI — `declare` EKSİKTİ.**
`bolge_operand` **160/160** derken `codegen_diff` **152/158** verdi: altı dosya
*"KEMGU IR link edilemedi"*. Self-host `@kdl_bolge_ayir`ı **çağırıyor ama
BİLDİRMİYORDU** (dizi yolu `kdl_dizi_olustur` üzerinden gittiği için o declare
hiç gerekmemişti). Yapısal kapı IR **metnini** karşılaştırır — iki tarafta da
aynı çağrı vardı, eksik declare **görünmez**. **D-502'nin dersinin simetriği:**
o turda `yapi_diff` kördü, `codegen_diff` yakaladı; burada tersi olmadı ama
gerekçe aynı — **davranışsal ve yapısal kapılar birbirini TAMAMLAR, biri
diğerinin yerine geçmez.**

**Sabotaj S88** (self-host yönlendirmesini kapat) → `4/160 SAPMA`, rc=1;
geri alınca 160/160 rc=0.

**KALAN 2 SIZINTI BAŞKA KÖKENDEN** (`kanal_mesaj` kanal tamponu ·
`gorev_temel` ρ_sahip) — bu eksende DEĞİL. "Hepsi halloldu" DEĞİL.

### 🎯 D-506: `dizi_olustur(N)` da ρ_yerel'e yönlendiriliyor — bellek 17×
D-494'ün `bölge_al` dalının **birebir simetriği**. Önceden YALNIZ dizi
**literali** (`[1,2,3]`) hapsedilme için işaretleniyordu; `dizi_olustur(N)`
bir **CAGRI** düğümüdür ve hiç işaretlenmiyordu → codegen onu **koşulsuz**
ρ_caller'a yayıyordu. `dizi_olustur` boyutlu dizi kurmanın **deyimsel**
yoludur (`stdlib/dizi.kem` onu kullanır) → literal-only kapsam **gerçek kodu
dışarıda bırakıyordu.**

**ÖLÇÜLDÜ** (taban `HEAD`den ayrı ağaca çıkarılıp kuruldu — tahmin YOK):
```
bench2 (200K dizi_olustur çağrısı)
  taban  ρ_caller  19968 KB zirve  0.09 s
  D-506  ρ_yerel    1152 KB zirve  0.06 s     → BELLEK 17×
```

**⚠⚠ AYNI ÖLÇÜM BİR YAVAŞLAMA DA GÖSTERDİ — KÖKÜ AYRI ÇIKTI.**
`bench1` (okuma-ağırlıklı, 400M dizi erişimi): taban 1.65 s → D-506 2.06 s
(%25 yavaş). **AYIRT EDİCİ TEST:** aynı iş dizi **literali** ile — literal
D-506 ÖNCESİNDE DE ρ_yerel'e gidiyordu:
```
literal, taban 2.00 s   ·   literal, D-506 1.99 s   → AYNI
```
Yani yavaşlık D-506'nın **soktuğu bir gerileme DEĞİL**: **ρ_yerel dizileri
ρ_caller'dan ~%25 yavaş** ve bu **önceden var olan** bir özellik. D-506
yalnızca `dizi_olustur`u zaten yavaş olan yola taşıdı.
**KÖK ÖLÇÜLMEDİ → AÇIK SORU** (muhtemel: taze bölgenin ayrı `malloc` bloğu →
sayfa yerleşimi). Bu, gerçek bir optimizasyon hedefidir.

**🎯 YENİ YAPISAL KAPI `calistir_bolge_operand` — VE ZORUNLUYDU.**
Port yapılmadan önce C `%5` (ρ_yerel), self-host `%rho` (ρ_caller) yayıyordu
ve **ÜÇ KAPI DA YEŞİLDİ**:
```
codegen_diff  → ÇIKIŞ KODU;      yanlış bölge de 42 döndürür
yapi_diff     → `define` KÜMESİ; bölge operandı define'da YOK
checker_diff  → TANI dump'ı;     codegen'e hiç bakmaz
```
D-417'nin (spekülasyon bariyeri) **birebir tekrarı**: davranışsal kapılar
bellek-yönetimi özelliğine **KÖRDÜR**.
Kapı **register NUMARASINI kasten yok sayar** (iki derleyicide farklı
olabilir); yalnız ρ **SINIFINI** karşılaştırır — `%rho`=CALLER, `%N`=YEREL.
Dizi tahsisi içeren dosya bulunamazsa **koşmayı reddeder**.
**Sabotaj S87** (self-host portunu geri al) → `1/159 dosyada SAPMA`, rc=2.

**⚠⚠ İLK ÖLÇÜMLERİM ÜÇ KEZ GEÇERSİZDİ** (D-500 listesinin üç maddesi birden):
1. **`git stash` HİÇ OLUŞMADI** — worktree'nin `.git` dosyası **Windows yolu**
   taşıyor, WSL'in git'i çözemiyor (`not a git repository: .../C:/Users/...`)
   → aynı yapımı iki kez ölçüp *"fark yok"* diyecektim.
2. **Taban ikilisi HİÇ KURULMADI** — `make ... >/dev/null` link hatasını yuttu;
   `rc=127` ile olmayan ikiliyi ölçtüm. **Negatif süre (−4.07 s) ele verdi.**
3. **Windows `/tmp` ile WSL `/tmp` AYRI dizinler** → baseline kopyası sessizce
   tutmadı. Çözüm: iki tarafın da gördüğü bir yola yaz.


**🔴 KENDİ BULGUMU DÜZELTTİM — "%25 yavaşlık" BİR ÖLÇÜM ARTEFAKTIYDI.**
Yukarıdaki *"ρ_yerel dizileri ρ_caller'dan ~%25 yavaş"* saptamasını **açık
soru** diye kaydetmiştim. Ardından ölçtüm ve **çürüdü**:
```
-O0'da FARK YOK        taban 2.78–2.86   D-506 2.74–2.86
-O2'de fark VAR        taban 1.71–1.83   D-506 2.12–2.26   (10 dönüşümlü tur)
```
`-O0`'da farkın olmaması okuma yolunun **birebir aynı** olduğunu gösterdi →
ceza çalışma zamanı maliyeti değil, **yerleşim** etkisi olmalı. Ayırt edici
deney: gerçek diziden ÖNCE küçücük bir **kukla tahsis** (adresi kaydırır):
```
pad=0  2.02 2.05 2.02 2.01     ← ceza
pad=1  1.95 1.78 1.74 1.75     ← ceza GİTTİ
pad=7  1.80 1.80 1.80 1.79     ← taban (1.75) ile AYNI
```
Dizinin içeriği ve okuma döngüsü **birebir aynı**; değişen tek şey ADRES.
→ Ceza **ADRESE BAĞLI** (önbellek-kümesi/sayfa hizalaması), ρ_yerel'in
doğasında olan bir maliyet **DEĞİL**. D-506'nın hız etkisi **~nötr**;
bellek kazancı (17×) aynen geçerli.

**⚠ DERS: ÜÇ TUTARLI KOŞUM "GERÇEK" DEMEK DEĞİLDİR.** İlk ölçümüm
(2.38/2.42/2.40) çok sıkıydı ve bana **sahte güven** verdi; aynı ikili başka
bir turda 5.26 verdi. Mikro-benchmark'ta sıkı varyans, etkinin gerçek
olduğunu değil, yalnızca o koşum penceresinde **kararlı** olduğunu gösterir.
Ayırt edici deney (girdiyi değil YERLEŞİMİ değiştir) olmadan bu bir
"özellik" diye kaydedilecekti.

Fikstürler `test/perf/` altında (kalıcı ölçüm tabanı).

### ✅ D-504→D-505 KAPANDI: `görev` yakalaması artık SAHİPLİK TAŞIYOR
**Mehmet seçenek (a)'yı seçti.** Spec'in kendi kuralı uygulandı — icat YOK:
```
∀ vᵢ ∈ YD(c) : sahiplik_transfer(vᵢ, ρ_yeni)   [R-YAKALAMA-THREAD]
```
`KEMGU_Bellek_Modeli.md` sat.144/323. **Yeni tanı kodu YOK** — mevcut **L002**.

**KAPANAN AÇIK (D-504):** iki görev aynı `Dizi<tam32>`'e yazıyordu, hiç
`güvensiz` yoktu, `--check` sıfır tanı veriyordu; çalışma zamanında 100000
yerine **62868 / 58426 / 71619** (8/8 koşumda kayıp). Artık **L002**.

**İKİ BİLİNÇLİ DARALTMA — ikisi de fikstürde POZİTİF olarak ölçülüyor:**
- **Skaler yakalama muaf:** env HEAP kopyasıdır, skaler kopya yarışamaz
  (G005'in kendi D-323 daraltmasının aynısı).
- **`kanal<T>` muaf:** threadler arası paylaşım için TASARLANMIŞTIR; R-KANAL
  transferi kanal **yükü** içindir, kanalın **kendisi** için değil.
  **⚠ Muafiyet AYRI yüklemle** (`gorev_tasima_muaf`) — `yakalama_isaretci_benzeri`
  DEĞİŞTİRİLMEDİ, çünkü onu G005 de okuyor ve orada kanal hâlâ işaretçidir
  (kaçan closure'da tehlikeli). *Aynı tabloyu iki tüketici farklı doğrulukta
  okuyorsa birini diğerine mahkûm etme* (D-439).

**⚠ MUAFİYET ÖLÇÜMLE BULUNDU, TASARLANMADI:** ilk sürüm **üç gerçek programı**
reddetti (`cg_gorev_kanal` · `cg_rho_sahip_kacis` · **kendi D-489 kapı
fikstürüm** `drf_gorunurluk`). Üçü de aynı şekil: kanalı göreve yakala, ana
thread'den `kanal_al`. Taban değer **stash ile** ölçüldü (0→1 = yeni).

**⚠⚠ ÜÇ UYGULAMA HATASI, üçü de ölçümle yakalandı:**
1. **SEGFAULT (exit 139).** `TipKontrol` alanları **tek tek** başlatılıyor
   (`memset`/`arena_ayir_sifir` DEĞİL) → eklediğim `tasinan_sayi` çöp okundu.
   Deponun kendi kuralı (*"`arena_ayir_sifir` tercih et"*) tam bunu uyarıyordu.
2. **Self-host lambdanın KENDİ gövdesini suçladı** (fazladan L002 23:42/33:42):
   işaretlemeyi gövde gezilmeden ÖNCE yapıyordum → çağrı çocukları
   gezildikten SONRAYA taşındı.
3. **`fad` kapsam dışı** (T002): iki çıkış noktasından biri onu görmüyor →
   yardımcı çağrılan adını kendisi türetiyor.

**SIFIR YANLIŞ-POZİTİF:** 631 dosya tarandı; L002 veren 8 dosyanın **hepsi
önceden var olan** kasıtlı fikstür (11 → 3 muafiyetle → 8, taban ile tutarlı).

**Sabotaj 2/2:** S85 (C) → fikstür `OK`, yarış geri geldi · S86 (self) →
`checker_diff` 165→164.
**Kapılar:** checker_diff **165/165 (0 muaf)** · check_kapisi 262/269 (0 RED) ·
check_genis 133/133 · codegen_diff 157/157 · drf_gorunurluk 100/100 ·
sıfır uyarı 38/0.


**⚠⚠ TAM TAKIM İKİ KEZ DARALTTIRDI — hedefli kapılar yeşilken:**
1. `drf_test` [38] → **gerileme DEĞİL**, sabitlenmiş V1 sınırı tam
   tasarlandığı gibi kırmızıya döndü (beşinci kez; beklenti `h>=1` yapıldı).
2. `stdlib_check` → **GERÇEK yanlış-pozitif**: `stdlib/bariyer.kem`.
   `Bariyer`/`Semafor`/`Kilit` **kullanıcı YAPISIDIR** (opak `metin` handle
   sarmalar) ve threadler arası **senkronizasyon için TASARLANMIŞTIR** —
   tip kategorisiyle `Dizi<T>`den ayırt edilemezler.

**SONUÇ — KURAL `Dizi<T>` İLE SINIRLANDI** (ölçüme dayalı üç gerekçe):
- `metin` **DEĞİŞMEZ** (yerinde yazma API'si YOK; `metin_*` hepsi yeni metin
  döndürür) → paylaşımı güvenli, taşıma gereksiz
- sync primitifleri kullanıcı yapısıdır ve paylaşım **kasıtlıdır**
- `Dizi<T>` yerinde mutasyona uğrar (`dizi_yaz`) + heap-paylaşılır →
  D-504'ün ölçtüğü yarış **tam olarak budur**

**⚠ KALAN AÇIK (dürüstçe):** `Dizi<T>` **İÇEREN** bir kullanıcı yapısı iki
göreve yakalanırsa hâlâ yakalanmaz. Kapatmak için dilde *"bu tip
paylaşılabilir"* işareti gerekir (Rust'ın `Send`/`Sync`'i gibi) — **dil
yüzeyi kararı, icat edilmedi.** Bu artım ÖLÇÜLEN açığı kapatır, hepsini
değil.

**⚠ SINIR (dürüstçe):** kural YALNIZ `görev_başlat`ın DOĞRUDAN lambda
argümanını tarar. Ada bağlı kapanış (`değişken f = || …; görev_başlat(f)`)
bu yoldan geçmez — G005 orayı ayrıca kapsıyor ama taşıma işaretlenmez.

### 🎯 D-503: iptal edilmiş yetki ÖDÜNÇ ALINAMAZ (değişmez avının 5. ekseni)
**ÖLÇÜLEN AÇIK:** `geri_al(y)` **sonrası** `mmio_yaz32(y, …)` `--check`ten
**TEMİZ** geçiyordu — iptal edilmiş yetkiyle **donanıma yazma**. Yetki
sisteminin varlık sebebi tam olarak bunu engellemekti.
```
geri_al(y); mmio_yaz32(y,..)   ÖNCE: OK   SONRA: CP005
geri_al(y); mmio_oku32/64(y,..) ÖNCE: OK   SONRA: CP005
geri_al(y); bölge_al(y, 4)      ÖNCE: OK   SONRA: CP005
mmio_yaz32(y,..); geri_al(y)    OK (geçerli sıra — yanlış-pozitif YOK)
```
**KÖK:** tüketim yolu (`geri_al`, çağrı argümanı) canlılığı **ZATEN**
denetliyordu (`lineer_tuketildi >= 1` → CP005); **ödünç** yolu
(`mmio_*` · `bölge_al` · `soket_*`) denetlemiyordu. Üç kardeş doğrulayıcının
üçü de *"ÖDÜNÇ alınır — TÜKETİLMEZ"* diyor ama bağlamanın **hâlâ canlı**
olduğunu sormuyordu.

**YENİ TANI KODU YOK:** mevcut **CP005** (*"move sonrası erişim"*) zaten bu
anlamdır. Eksik olan kural değil, kuralın **sorulmadığı yerdi** — D-502'nin
aynı gerekçesi.

**⚠ ÜÇ AYRI UYGULAMA TUZAĞI (CLAUDE.md'de yazılı, yine ısırdı):**
portu önce `codegen.kem`'e yaptım; **`checker_diff` `checker.kem`'i ölçüyor**.
Kapı kırmızı kaldı ve bir an onarımı yanlış sandım. *Bir kuralı portlarken
HANGİ kapının HANGİ uygulamayı okuduğunu önce ölç.*

**⚠ YANLIŞ-POZİTİF TARAMASI STASH'LE YAPILDI:** 630 dosyada 4 dosya CP005
veriyordu; **dördü de değişiklikten ÖNCE aynı sayıyı** veriyordu (kasıtlı
CP005 fikstürleri, `test_ag.kem` dâhil) → **sıfır yeni yanlış-pozitif**.
"Ad CP005 içeriyor, demek fikstürdür" demedim; taban değeri ölçtüm.

**Fikstür `tc40_01`** — üç ihlal + **bir GEÇERLİ sıra**. Yalnız negatifler
olsaydı *"her ödüncü reddet"* sabotajı kapıdan GEÇERDİ (D-425).
⚠ Numara üç kez çakıştı (tc26/tc27/tc28 dolu) — `ls | grep -oE "^tc[0-9]+"`
ile boş numara **ölçülmeli**, tahmin edilmemeli.

**Sabotaj 2/2:** S83 (C) → fikstür `OK`'e döndü, açık geri geldi ·
S84 (self) → `checker_diff` 164→163, rc=2.
**Kapılar:** checker_diff **164/164 (0 muaf)** · sıfır uyarı 38/0.

### ✅ D-501→D-502 KAPANDI: sıfıra bölme artık **temiz duruyor**
**Mehmet seçenek (b)'yi seçti** — çalışma zamanı panik. Gerekçe tutarlılık:
**dil zaten dizi sınırı için panik seçmişti** (`xs[10]` `sonuç` döndürmüyor,
paniklıyor). (c) `sonuç<T,H>` bölmeyi diziden *daha katı* yapardı.

```
ÖNCE:  a/0 · a%0 · tam64 · dtam32 · literal 10/0  →  SIGFPE exit 136, mesaj YOK
SONRA: hepsi  →  PANIK: sifira bolme, exit 134   ·   84/2 → 42 (etkilenmedi)
```
Mekanizma dizi sınır ihlaliyle **birebir** (D-069): inline `icmp` + `br` +
`kdl_panik(noreturn)` + `unreachable`. Yeni tanı kodu YOK, tip değişikliği YOK.

**İki bilinçli daraltma:** yalnız **tamsayı** (kesirli 0-bölme IEEE-754'te
tanımlı, tuzak değil) · **sabit sıfır-olmayan** bölende kontrol yayılmaz
(gereksiz dal); sıfır literalinde yayılır ki `10 / 0` da temiz dursun.

**⚠⚠ YOL ÜSTÜNDE İKİ TUZAĞA DÜŞTÜM, İKİSİ DE KAYITLIYDI:**
1. **Bayat ikili** — Windows'ta derleyip WSL'de ölçtüm; IR doğruydu ama
   runtime hâlâ SIGFPE veriyordu. *Kendi D-500 listemdeki madde.*
2. **🔴 KEMGU DİZGİ LİTERALİNDE KAÇIŞ YOK** (D-400/409/416'da ÜÇ KEZ
   kayıtlı) — self-host'a `c\"…\00\"` yazdım, düz ters bölü bastı,
   **`codegen_diff` 0/157**, tüm IR geçersiz oldu. Doğrusu `yb(34)`/`yb(92)`
   ve o desen zaten `@.gorev_hata_str` satırında duruyordu.
   **⚠ `yapi_diff` bu felaketi GÖRMEDİ** (132/132 yeşil) — yalnız `define`
   kümesine bakıyor. Yakalayan `codegen_diff` oldu: davranışsal ve yapısal
   kapıların birbirini tamamlamasının somut kanıtı.

**YENİ KAPI `calistir_sifir_bolme`** — 10 ölçüm, **her iki derleyicide**.
`normal.kem` (84/2 → 42) bilerek var: yalnız negatif şekiller olsaydı
"her bölmeyi panikletir" sabotajı kapıdan GEÇERDİ (D-425).
**Sabotaj 2/2:** S81 (C) · S82 (self) → dört şeklin dördü de `exit=136`
yakalandı, `HARNESS rc=1`; geri alınca rc=0.
⚠ İlk sabotaj ölçümümde `| tail -3` hem çıkış kodunu MASKELEDİ (rc=0 gösterdi)
hem kanıtı kırptı (yalnız 1 dosya göründü) — D-444/D-500'ün tekrarı.

**Kapılar:** codegen_diff 157/157 · yapi_diff 132/132 · checker_diff 163/163 ·
self_driver **FIXPOINT ✓** · llvm_test 286/286 · snapshot 50/50.
`test_tumu` **68 kapı**.

### ✅ D-497→D-499 KAPANDI: `ver &yerel` artık **G006** ile reddediliyor
**Mehmet seçenek (b)'yi seçti** — ayrı kod. Gerekçe deponun kendi
konvansiyonu: `T005/T006/T007/T008/T027` *aynı kalıp* olduğu hâlde **beş ayrı
kod** (D-353). Ayrı kod = mesaj her site için kesin kalır, `grep G005` tek
kuralı bulur.

**Uygulandı:** C (`ver` + ifade-formu lambda gövdesi) · `selfhost/checker.kem`
· `selfhost/codegen.kem`. Beş şeklin beşinde **kod+satır+sütun birebir**.

**Sağlamlık daraltmaları** (G005'in kendi *"over-reject yok"* ilkesi):
`&küresel` GEÇERLİ · `&*p` bildirilmez (kök çözülemez) · **çağrı argümanlarına
inilmez** (`ver f(&a)` `&a`'yı döndürmez). **625 dosya tarandı, tek eşleşme
kasıtlı fikstür → SIFIR yanlış-pozitif.**

**⚠⚠ BU TURDA ÜÇ ÖLÇÜM ARACI HATASI, biri BEŞ TUR kaybettirdi:**
1. **Self-host ikilisini `--checkdump` bayrağıyla çağırdım; harness onu
   BAYRAKSIZ çağırıyor** (`kemcheck.exe "$f"`). İkili bayrağı dosya adı sanıp
   boş çıktı verdi → "kanca ateşlenmiyor" diye **olmayan bir kusuru** kovaladım.
   **Mevcut bir tanıyla (T020) sınayınca ortaya çıktı** — o da sessizdi, yani
   sorun kancada değil ÇAĞRIMDAYDI. *Yeni bir kuralı sınamadan önce ESKİ bir
   kuralın aynı yoldan çıktığını doğrula.*
2. `&&` zincirli sessiz kurulum → **hiç var olmayan** ikiliyle ölçtüm.
3. İç içe kabukta `\&` kaçışı test dosyasını bozdu (C bile P001 verdi).

**Gerçek iki kusur ancak araç temizlenince göründü:**
- `eğer/değilse` bağlanması `ERISIM` inişini **sessizce** düşürüyordu
  (`&n` çalışıyor, `&n.x` çalışmıyordu) → açık erken-dönüşle yeniden yazıldı.
- **Self-host parser'ı ASCII `"&degisken"` yazar, Türkçe `"&değişken"` DEĞİL.**

**Sabotaj 2/2:** S79 (C'de kapat) → fikstür 5×G006 → OK · S80 (self'te kapat)
→ `checker_diff` 163→162, rc=2.
**Kapılar:** checker_diff **163/163** · check_kapisi 262/269 (0 RED) ·
check_genis 133/133 · codegen_diff 157/157 · self_driver **FIXPOINT ✓**

**TAM TAKIM DOĞRULAMASI (G006 sonrası, dil yüzeyi değişikliği):**
```
67 kapı · rc=0 · 0 kırmızı · 0 atlama · 83 dk 4 sn
SELF-HOST BOOTSTRAP: FIXPOINT ✓ (stage1 IR == stage2 IR, 73.860 satır birebir)
```
⚠ Süre önceki koşumun (35 dk) iki katı — sebep **tahmin edilmedi**: bu koşum
`src/tip_kontrol.c` + iki self-host dosyası değiştiği için çok daha fazla
yeniden derleme yaptı. Kapı sayısı ve sonuçlar aynı.
**FIXPOINT en anlamlı sinyal:** yeni dil kuralı bootstrap zincirinin hiçbir
yerinde sapma yaratmadı.

### ⛔ D-490 (NEGATİF SONUÇ): QEMU zayıf belleği MODELLEMİYOR — kapı EKLENMEDİ
D-489'un 1. sınırını (*"x86-TSO'da geçmek ARM64'te kanıt değildir"*) kapatmak
için ARM64'e bakıldı. **İki ölçüm, iki negatif sonuç:**
- **`kanal` bare-metal, o sınırı KAPATMAZ.** `kem_os` QEMU'da **`-smp` olmadan**
  koşuyor (Makefile:1616) → **tek çekirdek**. Tek çekirdekli preemptive
  zamanlayıcıda bağlam anahtarları tam bariyerdir; zayıf bellek hiç zorlanmaz.
  Madde OS bütünlüğü için hâlâ değerli, ama **DRF doğrulaması sağlamaz.**
- **🎯 QEMU TCG ÖNBELLEĞİ VE ZAYIF BELLEĞİ MODELLEMİYOR — ÖLÇÜLDÜ.**
  `test/bare_metal/smp_queue_arm.c` (2 çekirdek, gerçek `ldaxr`/`stxr` kilit,
  iş-çalma kuyruğu) üzerinde `dc civac` + `dc ivac` + eşlik eden `dsb sy`
  komutlarının **dördü de `nop`a çevrildi** → test **birebir aynı sonuçla
  GEÇTİ** (toplam=20540).
  - **Sonuç 1:** o dosyadaki bariyerler **doğru ve gerekli** (gerçek donanımda
    onlarsız bozulur) ama **hiçbir kapı zorlamıyor** — sessizce silinseler
    QEMU kapısı yeşil kalır. Not dosyanın başlığına yazıldı.
  - **Sonuç 2:** QEMU üzerine kurulacak bir "zayıf bellek" kapısı **hiçbir şey
    kanıtlamaz** → böyle bir kapı **EKLENMEDİ** (D-425: yanlışın gözlenebilir
    olduğu şekli ölçemeyen kapı, kapı değildir).
  - **Sonuç 3 (DGX Spark için eyleme dönük):** gerçek doğrulama **yalnız
    fiziksel ARM64 donanımında** yapılabilir. Oraya taşınınca `smp_queue_arm`
    ilk koşulacaklardan olmalı ve **aynı sabotaj orada TEKRARLANMALI** —
    gerçek donanımda **KIRMIZI** olması beklenir; olmazsa test yeterince
    zorlamıyordur.
- **⚠ ARAÇ HATASI ÜÇ KEZ:** `perl -0pi` deseni tutmadı (sessiz) · `sed`
  `\n`'i **gerçek satır sonuna** çevirip C dizgisini bozdu (derleme
  başarısız, kapı hiçbir şey basmadı) · ancak dosyadan okunan hazır satırla
  doğru uygulandı. **Sabotajın sessizliği önce SABOTAJI şüpheli kılar**
  (D-402) — bu turda ilk "geçti" sonucu GEÇERSİZDİ.

### 🎯 D-487→D-489: sessiz atlama temizligi · escape v2 · DRF gorunurlugu
- **D-487 — tum harness yuzeyi sessiz atlama icin tarandi.** D-486'yi tek
  noktada birakmak yanlis olurdu. Uc kalinti, ucu de ÖLÇÜLDÜ: `ag_kosum`
  dinleyicisi **meşru** (gerçek TCP karşı tarafı ortama bağlı, sertleştirmek
  kapıyı ortam-kırılgan yapardı) · `lean_aksiyom` `test_tumu`da **değil** →
  sahte yeşil üretmiyor · `check_kapisi` takımda **ve** hedefi ikiliye bağımlı
  → **sertleştirildi**. O kapı D-486'dan **kıl payı** kurtulmuş: bir yedek
  satırı doğru ikiliyi bulmuş — yani doğruluk **tasarım değil tesadüftü**.
  **⚠ İlk sabotajım tetiklenmedi** ve "kapı zayıf" diye kaydedecektim; değildi,
  yedek satırı kötü değeri geçersiz kılıyordu → **sabotaj yanlıştı** (D-402).
  **`test_tumu` yüzeyinde sessiz atlama KALMADI.**

- **🎯 D-488 — INTERPROCEDURAL PARAMETRE-TUTMA OZETI (escape v2).**
  `f(xs)` çağrısında xs'in **hapsedilme kanıtı koşulsuz düşüyordu** → gerçek
  programlarda ρ_yerel yönlendirmesi ~%1'e iniyordu:
  ```
  dosya          toplam  ÖNCE  SONRA  (tavan)
  parser.kem        35      0     22     22
  checker.kem      181      1     23     23
  codegen.kem      267      3     25     25
  regex.kem         27      1      2      5
  ```
  **YENİ ANALİZ İCAT EDİLMEDİ:** *"f, p'yi saklıyor mu?"* sorusu
  `ky_confined(f.govde, param)` ile **birebir aynıdır** ve o yüklem 18-UAF
  avından geçmiş makinedir. Kanıt çağrı yerine taşındı. Her eksende
  **default-DENY**: bilinmeyen çağrılan · gövdesiz imza · **özyineleme** ·
  yığın taşması · kurulmamış tablo → hepsi eski konservatif yola düşer.
  - **⚠⚠ İKİ ÖLÇÜM HATASI, ikisi de "değmez" sonucuna götürüyordu.**
    (1) **Yanlış düğmeyi ölçtüm:** `escape_kategori`nin arg-yükseltmesini
    kapattım, hiçbir şey değişmedi — çünkü `bolge_yerel_yonlendir` onu
    **hiç okumuyor**, `escape_kesin_yerel` okuyor.
    (2) **Tavanı temsili olmayan korpusta ölçtüm:** 154 tahsisin 2'si (%1,3)
    → "değmez" diye kaydedecektim. D-420: *kapsam sayısı yalnız ölçülen yüzey
    kadar geniştir.*
  - **⚠ Kendi uyarıma yakalandım:** tabloyu `escape_analiz_program`da kurdum ve
    yorumuna "`escape_analiz_islev` doğrudan çağrılırsa tablo BOŞ kalır"
    yazdım. Ölçtüm: derleyici `escape_analiz_program`ı **hiç çağırmıyor**.
    Tablo arena-destekli program-genelinde statiğe çevrildi.
  - **`regex` 2/5 KUSUR DEĞİL:** dizileri **yapı alanlarında** saklıyor
    (`Regex.op: Dizi<tam32>`) → ρ_caller'da kalmaları DOĞRU. **Tavan bir hedef
    değildir** — kontrolü tümüyle kapatan sağlamsız bir üst sınırdır.
  - **🎯 YENİ YAPISAL KAPI `calistir_bolge_yonlendirme`** (1 yerel / 2 caller).
    **⚠⚠ EN ÖNEMLİ ÖLÇÜM:** sağlamsız hâlde (S73b) program **exit 42** verdi ve
    **ASan SIFIR bulgu** bastı — ρ_yerel serbestı `main` sonunda, okumalar
    ondan önce. Yani bu kapı kaldırılırsa D-488'in sağlığını ölçen **hiçbir şey
    kalmaz**. D-417'nin ampirik tekrarı: *"program doğru çalıştı" + "ASan
    temiz" bir bellek-yönetimi özelliğinde YETERSİZ KANITTIR.*
    Sabotaj **iki yönde** ayırt ediyor: devre dışı vs sağlamsız.

- **🎯 D-489 — DRF: FENCE EMIT GEREKMİYOR (ölçüldü).** Roadmap "C++11 weak
  memory fence emit" diyordu; ölçüm maddeyi DEĞİŞTİRDİ:
  (a) `kdl_gorev_*`/`kdl_kanal_*` **declare'lerinde hiçbir nitelik yok** →
  LLVM etraflarında yeniden sıralama yapamaz; (b) runtime **gerçek OS
  primitifleri** kullanıyor (pthread mutex/condvar · CriticalSection) — bunlar
  spec gereği tam bellek bariyeri. HB kenarları **zaten kuruluyor**.
  **Asıl boşluk ÖLÇÜMDÜ:** Lean'de ispatlı (gerçek `sorry` **0**; ⚠
  `SORRY_TRACKER.md` **BAYAT**, 47 diyor) ama çalışan derleyicide görünürlüğü
  **hiçbir şey ölçmüyordu**. Yeni kapı `calistir_drf_gorunurluk`: sekiz
  küresele yaz → kanala jeton → alıcıda sekizini oku (`-O2` bilerek).
  Ayırt edicilik: senkronizasyonlu **0/300**, senkronizasyonsuz **300/300**.
  - **⚠⚠ İKİ DÜRÜST SINIR (harness başlığında da yazılı):** x86-TSO **güçlü**
    bir modeldir → ARM64'te doğruluk KANITLAMAZ (orada koşulamıyor: `görev`/
    `kanal`ın bare-metal kullanımı **sıfır**) · senkronizasyonsuz karşı-örnek
    **deterministik** düşüyor → bu ZAMANLAMA etkisidir, saf bellek-sıralama
    değil.
  - **⚠ Ölçüm aracım yine yanıldı:** `cmd || fail++` — bu testin BAŞARI KODU
    **42**; 300/300 "başarısız" bastım ve bir an gerçek ihlal sandım.

### 🐧 D-485/D-486: BARE-METAL LINUX'TA + "yeşil takım" bir İDDİADIR
`lld` + `qemu-system-x86` kurulunca bare-metal cephesi açıldı.
- **D-485 `16#` bash'e özgü.** `kem_os_arm` LINCHPIN fazında düştü
  (`arithmetic expression: expecting EOF`). Ubuntu'da `/bin/sh` **dash**,
  Windows/MSYS'te bash → kusur orada GÖRÜNMEZ. **Ölçüldü:** `$((0x...))`
  HER İKİSİNDE çalışır, `16#` yalnız bash'te.
  `SHELL := /bin/bash` **seçilmedi** (tüm tariflerin kabuğunu değiştirir);
  dar düzeltme. Tek vaka olduğu VARSAYILMADI — tarif satırları bashism için
  tarandı (`[[`, `<<<`, `source`, diziler, `N#`), başka gerçek vaka YOK.
- **🎯 KEMGU-OS LINUX'TA TAM BOOT EDİYOR — 24 fazın TAMAMI, 15 sn:** MMU
  çeviri/fault · gerçek trap · timer IRQ · preemption · EL0 syscall · süreç
  izolasyonu · disk/FS · ağ ARP + **PING CANLI** · ELF yükleme · W^X · DTB.
  `qemu_cekirdek` 5/5 (53 sn). **Sürüm farkı ÖLÇÜLDÜ, sorun çıkarmadı:**
  clang 15 / ld.lld 14.
- **🔴🔴 D-486 — SEKİZ PARİTE KAPISI SESSİZCE ATLANIYORDU.** Tam takım
  `rc=0` + "Tum testler gecti!" verdi; koşum SONRASI dürüstlük kontrolü
  yeşilin bir kısmının **İÇİ BOŞ** olduğunu gösterdi:
  `ct_bariyer · yapi_diff · modul_codegen · surucu_diff · check_genis ·
  codegen_genis · baremetal_diff · ag_kosum` — sekizi de atlandı, `make`
  yine **0** döndü.
  **Kök:** Makefile `CODEGEN=build/codegen.exe` diye SABİT geçiyordu;
  Linux'ta ad `build/codegen`. **D-469'un ikinci yarısı görülmemiş** —
  o artım 32 varsayılanı çevirdi ama harness'lara GEÇİLEN değişkenler
  taramanın dışında kaldı.
  **İKİ KATMANLI ONARIM, yalnız yolu düzeltmek YETMEZDİ:** (1) `$(EXE)`;
  (2) **ATLAMA → SERT HATA** (9 harness). Dokuz hedefin hepsi
  `$(BUILD)/codegen$(EXE)`e BAĞIMLI → make yolunda ikili GARANTİ, yani o dal
  **ölü koddu** ve tek işlevi tam da böyle bir yol hatasını yutmaktı.
  Yalnız (1) yapılsaydı kapı bugün yeşil olurdu ama **sessiz dal dururdu.**
  `codegen_diff`in atlama mesajı ayrıca BAYATTI (D-072/CG1 dönemine atıf,
  oysa kapı 155/155).
  **⚠ KENDİ YAMAM HATA YAPTI:** `ag_kosum`da DİNLEYİCİ dalını da sertleştirdim
  (awk deseni ikinci dala taştı). O AYRI ve MEŞRU bir atlama — gerçek TCP
  karşı tarafı ORTAMA bağlı (D-466). Geri alındı.
  **Sabotaj S70** (sabit `.exe`e dön) → `rc=2` + açık mesaj; sertleştirme
  ÖNCESİNDE aynı sabotaj **rc=0** verirdi.
- **⚠⚠ GENEL DERS: "TAM TAKIM YEŞİL" BİR SONUÇ DEĞİL, BİR İDDİADIR.**
  `rc=0` gördükten sonra **kapı sayısını ve ATLAMA izlerini AYRICA ölç.**
  D-446 aynı dersi "çağrılmayan kapı" için vermişti; bu, "çağrılan ama
  koşmayan kapı" biçimi.

### 📊 LINUX vs WINDOWS — atlamasız tam takım
```
Windows  ~45 dk        (62 kapı)
WSL       15 dk 47 sn  (62 kapı, 0 atlama, 0 kırmızı)   ≈ 3×
```
⚠ İkisi de sıcak yapım; soğuk koşum WSL'de 28 dk ölçüldü. Kazanç
paralelleştirmeden DEĞİL ortamdan geliyor (D-468: süreç doğurma 96–170 ms
+ Defender ≈ %65). `checker_diff` **35×**, `self_driver` **6.6×**.

### 🐧 D-474→D-484: LINUX/WSL TAŞIMASI — on bir kusur, hepsi TAŞINABİLİRLİK
Takım Windows'ta ~45 dk sürüyordu ve darboğazın **%65'i ortamsaldı** (süreç
doğurma 96–170 ms + Defender). WSL'e geçildi. **Derleyicinin ÖZÜ taşınabilir
çıktı** — `codegen_diff` 155/155, `checker_diff` 162/162, bootstrap FIXPOINT
Linux'ta da birebir. Çıkan on bir kusurun **hiçbiri derleyici mantığında
değil**, hepsi test altyapısının Windows varsayımlarındaydı.
- **D-474 POSIX özellik-test makrosu.** `-std=c11` KATI ISO'dur → glibc POSIX
  bildirimlerini GİZLER. `_POSIX_C_SOURCE` **her `#include`dan ÖNCE** gelmeli.
  Linux derlemesi tamamen KIRIKTI.
- **D-475 eksik LLVM intrinsic bildirimleri.** Üretilen IR **kendi kendine
  yetmiyordu**; Windows'ta clang toleranslıydı, Linux'ta değil.
- **D-476/D-478/D-484 ASan × ASLR entropisi.** Modern çekirdekte ASan gölge
  belleği çakışır → ikili **BAŞLANGIÇTA SESSİZCE ÇÖKER** (exit 139, rapor YOK).
  `setarch -R` çözer. **ÜÇ ARTIMDA KAPANDI ve bu kendi başına bir ders:**
  D-476 kusuru buldu, D-478 "hangi ikililer ASan'lı" listesini ÜÇ KEZ eksik
  yazıp sonunda listeyi KALDIRDI, **D-484 ise D-478'in Makefile dışında kalan
  yarısını** (kendi ASan ikilisini kuran iki harness) kapattı.
  **DERS: "listeyi kaldır, hepsine uygula" dediğinde HANGİ hepsi olduğunu ölç.**
- **D-477/D-481 `system()` ve çıkış kodu.** POSIX'te `system()` **BEKLEME
  DURUMU** döner, çıkış kodu değil (`WEXITSTATUS` şart) ve çıkış kodu **8 BİTE
  MASKELENİR** — `1000 & 255 = 232`. Windows 32-bit taşıdığı için ikisi de
  görünmezdi. **Mantık DOĞRUYDU; taşınabilir olmayan şey testin değeri çıkış
  koduyla taşımasıydı.**
- **D-479 satır sonları.** `.ast` anlık görüntüleri CRLF; Linux çıktısı LF →
  50/50 düştü. **Normalleştirme doğru çözüm, dosyaları LF yapmak DEĞİL** —
  kusuru bir platformdan diğerine taşımak olurdu.
- **D-480 `usleep` → `nanosleep`** · **D-482 `.gitattributes`** (kabuk
  betikleri LF).
- **🎯 LEAKSANITIZER: LINUX'UN KAZANDIRDIĞI YENİ ÖLÇÜM (D-483).** Windows ASan
  runtime'ında LeakSanitizer **BULUNMAZ** → 7 sızıntı HEP ORADAYDI, proje HİÇ
  GÖRMEMİŞTİ. Yığın izi OKUNDU: `bölge_al` + kapanış env kopyası + kanal
  tamponu. **Kusur DEĞİL, belgelenmiş tasarım durumu** — `kdl_sizan_al`
  runtime'da açıkça "sızan tahsis kısayolu" ve yorumu toplu serbestin **F4.4**
  olduğunu söylüyor. `ALLOWLIST`E EKLENMEDİ (o dosyayı TAMAMEN atlar, UAF
  denetimini de kapatırdı); ayrı ve DAR `SIZINTI_MUAF` yalnız sızıntı
  satırlarını süzer. `detect_leaks=0` REDDEDİLDİ — Linux'un tek yeni ölçüm
  yeteneğini çöpe atardı.
  **⚠ Başarısızlık kümesi TURLAR ARASINDA DEĞİŞİYOR** (`görev`/`kanal` thread
  kullanıyor) → bu alan F4.4'e kadar TAM BELİRLENİMCİ DEĞİL.
- **⚠⚠ İKİ ELEME HATASI, ikisi de aynı sınıf:** D-481'de `WEXITSTATUS`
  *geçiyor* diye iki dosyayı "zaten doğru" saydım — `#include` eksikti.
  D-482'de "düz kullanımların çoğu yorumdur" dedim — ikisi gerçek atamaydı.
  **GÖRÜNMEK ≠ DOĞRU OLMAK.**
- **⚠ ÖLÇÜM ARACIM İKİ KEZ YANLIŞTI:** `apt-get -s update` (geçersiz birleşim)
  olmayan bir ağ hatası bildirdi; `grep -c` `declare` satırını saydı.

### 📐 PARALELLEŞTİRME ÖLÇÜLDÜ — KAZANÇ SIFIR (Amdahl)
`make -j` uygulandı (`test_tumu_paralel`) ama **ölçüm fayda göstermedi**:
`self_driver` tek başına takımın **%60'ı** ve seri. Paralelleştirme
darboğazı değiştirmiyor. **Gerçek kazanç WSL'den geldi:** `checker_diff`
**35×**, `self_driver` **6.6×**.

### ⏳ BEKLEYEN: `lld-15`
Bare-metal/QEMU kapıları WSL'de onu bekliyor: `wsl sudo apt-get install -y lld-15`.

### 🚚 D-469: test altyapısı `EXE`-farkında + varsayılan triple platformdan
DGX Spark (yerli ARM64/Linux) taşımasının kalan iki ön koşulu. İkisi de
mekanik; **dil ya da kapı semantiği DEĞİŞMEDİ.**
- **🔴 ASIL RİSK: derleyici taşınır, KAPILAR TAŞINMAZDI.** Ölçüldü: 27
  harness'ın **25'i** `build/kemgu.exe`yi SABİT yazıyor · Makefile 24 harness
  çağrısının yalnız **9'una** değişken geçiriyor · `export` **0**. Linux'ta
  ikili `build/kemgu` (uzantısız) olacağı için harness'lar onu **bulamazdı**.
  **Kapısız bir taşıma bu depoda en tehlikeli şeydir.**
  Onarım: `export EXE` + 32 varsayılanın `build/X${EXE}`ye çevrilmesi + her
  harness'a **kendine yeten tespit** (make'siz doğrudan çağrımda `set -u`
  altında ÇÖKMESİN diye).
- **⚠ ÖLÇÜM BİR HATAMI YAKALADI — `${EXE:=}` DEĞİL `${EXE=}`.** İki noktalı
  biçim **boşu da "tanımsız" sayar** → Makefile'ın Linux'ta vereceği `EXE=`
  (boş) otomatik-tespit tarafından EZİLİRDİ ve bayat bir `build/kemgu.exe`
  duruyorsa yanlış uzantı seçilirdi. Ölçüm: `EXE=''` → `build/kemgu` ·
  `EXE` tanımsız → `build/kemgu.exe`.
- **Varsayılan triple SABİT `x86_64-pc-windows-gnu` idi** → Linux'ta üretilen
  her IR yanlış triple taşırdı. clang çoğu durumda yine derler, yani **SESSİZ**
  bir yanlışlık; ARM64'te veri modeli/çağrı sözleşmesi açısından ciddi sapma.
  Makefile `PLATFORM`/`ARCH`'ı ZATEN tespit ediyordu; eksik olan makronun
  ondan BESLENMESİYDİ.
- **⚠ DOĞRULAMANIN SINIRI (dürüstçe):** Windows davranışı BİREBİR korundu ama
  **Linux/ARM64'te gerçekten çalıştığı BURADA KANITLANAMAZ** — yalnız "doğru
  yolu arıyor" ve "doğru triple seçilecek" ölçüldü. Gerçek kanıt DGX Spark'taki
  ilk tam koşumdur; orada farklı çıkan hiçbir şey "platform farkı" diye
  geçiştirilmemeli (bu depoda parite sapmalarının sessiz kalma eğilimi
  defalarca ölçüldü).

### 🎯 D-468: ARM64 spekülasyon bariyeri (`csdb`) + kapı mimari-farkında
`sabitsüre` disiplini **koşulsuz** x86 `lfence` yayıyordu — `--mimari arm64`te
bile. O komut ARM64 hedefinde **GEÇERSİZDİR**, yani sabit-süre kullanan 15
dosya ARM'da derlenmezdi. Kusur LATENT kaldı çünkü ARM kapıları `sabitsüre`
KULLANMIYOR (D-453'te ölçülmüştü); yerli ARM64'e taşınırken yüzeye çıkardı.
- **BARİYER SEÇİMİ ÖLÇÜLDÜ** (aarch64 hedefiyle gerçekten derlenerek):
  `csdb` → **seçildi** (ARM'ın Spectre-v1 için resmî önerisi, `lfence`in bu
  koddaki amacıyla birebir) · `llvm.aarch64.isb(15)` → `isb` (komut senk.,
  farklı amaç) · `llvm.aarch64.dsb(15)` → `dsb sy` (yanlış araç) ·
  `sb` → **ARMv8.5 GEREKTİRİR, temel ARMv8'de DERLENMİYOR → reddedildi**.
  `csdb` hint uzayındadır → desteklemeyen çekirdekte NOP'a düşer.
- **⚠ ETİKET `"arm64"`, `"aarch64"` DEĞİL** (`ana.c`:
  `llvm_hedef_ayarla("arm64", "aarch64-unknown-none-elf")` — ilki ETİKET,
  ikincisi TRIPLE). İlk yazımda `"aarch64"` ile karşılaştırdım: koşul HİÇ
  TUTMAZDI ve ARM64'te yine `lfence` yayılırdı.
- **KAPI YARISI KÖR KALACAKTI:** `ct_bariyer` yalnız x86 `lfence` sayıyordu;
  ARM64 dalı hiçbir ölçümle korunmazdı ve orada bariyeri düşüren bir değişiklik
  **sessizce** geçerdi — tam olarak bu kapının var oluş gerekçesi (D-417).
  Kapı artık ARM64'te de sayıyor, sayıların x86 ile EŞİT olduğunu ve ARM
  hedefine `lfence` SIZMADIĞINI denetliyor. **Sabotaj 2/2** (S67 C · S68 self)
  → ikisi de 13/13 → 6/13.
- **AYRICA `codegen_genis`e ZAMAN AŞIMI eklendi.** Yoktu ve bir tam takım
  koşumu tam orada **2.5 SAAT** çıktısız asılı kaldı (ölçüldü: `make` + 10
  `bash` canlı, HİÇBİR derleyici süreci yok = bloke). `ag_kosum`da S66 ile
  yakalanan sınıfın aynısı. **Asılan kapı, sessiz kapı kadar kötüdür.**
- **⚠ SÜREÇTE KENDİ KURALIMI ÇİĞNEDİM:** tam takım koşarken performans ölçümü
  için üç kapıyı ELLE koşturdum. İkisi `build/codegen.exe` yazıyor (38 ayrı
  yerden!) — D-297/D-414'te kayıtlı çarpışma sınıfı, üstelik **aynı oturumda
  paralelleştirmenin ÖN KOŞULU olarak kendim işaretlemiştim.** Kuralı yazmış
  olmak uygulamaya yetmiyor.

### 📐 TAKIM SÜRESİ ÖLÇÜLDÜ (D-468/469 sırasında) — darboğaz ORTAMDA
Tam koşum ~45 dk. Sebep tahmin edilmedi, **ölçüldü**:
- **Süreç başlatma bu makinede 96–170 ms.** `/usr/bin/true` (hiçbir iş
  yapmayan, güvenilen ikili) **96 ms**; `cmd /c exit` 146 ms. Linux'ta 1–5 ms.
  Windows + MSYS2 katmanının bedeli (gerçek `fork` yok).
- **Yeni üretilen `.exe`nin İLK çalıştırması ~194 ms fazladan** (Defender;
  gerçek-zamanlı koruma AÇIK). Aynı exe tekrar: 183 ms.
- **Link süresi KARARLI** (570–639 ms, %10 varyans) → CPU-bağımlı; disk/bellek
  darboğazı YOK. `-O0` yalnız **%14** kazandırıyor → **UYGULANMADI**: depoda
  `-O2` inline'ının gerçek bir kusuru MASKELEDİĞİ ölçülmüş (D-289); 90 ms için
  kusur-maskeleme riski kötü takas.
- Kapı süreleri: `codegen_diff` **309 sn** · `yapi_diff` 124 · `checker_diff`
  **70** (clang ÇAĞIRMIYOR — dosya başına 0.44 sn vs codegen_diff'te 2.0 sn;
  clang payının doğrudan kanıtı).
- **Kabaca %65'i süreç doğurma + Defender**, ~%25'i gerçek derleme.
- **Yapısal kalanlar (uygulanmadı, öneri):** `codegen.exe` **13 kez** yeniden
  kuruluyor (~36 sn tekrar) · 62 kapı **seri** koşuyor, makine 12 çekirdekli.
  ⚠ `make -j` ÖN KOŞULU paylaşılan tek yapımdır: `build/codegen.exe` **38**,
  `build/codegen.ll` **18** ayrı yerden yazılıyor → paralelde çarpışır (D-297).

### 🎯 D-466: `stdlib/ag.kem` — TCP istemci (`yetki<Soket>` + `tekkez Baglanti`)
Mehmet'in kararı: network, **yalnız TCP istemci**, yetki + lineer bağlantı.
- **⚠ ADI `Ağ` DEĞİL `Soket`:** ölçüm gösterdi ki `Soket` yetki sisteminde
  **ZATEN AYRILMIŞ** (`yetki_olustur` kaynak_tipi **2**, dilin kendi sabit
  listesinde: `1=Dosya 2=Soket 3=Bellek 4=Donanim 5=OTP_Anahtar 6=MMIO`).
  Aynı kavrama ikinci ad vermek **D-407 sınıfıdır** — iki ad er ya da geç
  ayrışır. Kavram aynı, var olan ad kullanıldı.
- **İKİ GÜVENLİK KATMANI, üçü de DERLEME ZAMANINDA ölçüldü:** yetkisiz
  bağlanma **MM002** · kapatmayı unut **L001** · iki kez kapat **L002** ·
  doğru kullanım TEMİZ. `checker_diff` **156/156, SIFIR muafiyet**.
  **Pozitif fikstür ŞART** (`tc24_03`): yalnız negatifler olsaydı "her şeyi
  reddet" sabotajı kapıdan GEÇERDİ (D-425).
- **🔴 KENDİ NOTUM YANLIŞTI, DERLEYİCİ DÜZELTTİ:** "yetki ÖDÜNÇ alınır"
  yazmıştım; `yetki<R>` **parametresi** işlev sonunda **CP005** verir. Dilin
  yerleşik deseni `drivers/virtio`da duruyor (sahiplen + sonda `geri_al`).
  Yani **BİR YETKİ = BİR BAĞLANTI**. Not gerçeğe uyduruldu.
- **🔴 WINSOCK ÇALIŞMA-ZAMANINDA YÜKLENİR** (`LoadLibrary`/`GetProcAddress`),
  bağlama zamanında DEĞİL. Önce doğrudan bağlanma denendi ve ÖLÇÜLDÜ:
  `-lws2_32` bağımlılığı **EN BASİT programı bile** kırıyor ("undefined
  symbol: WSAStartup") — depoda **41 Makefile + 18 harness = ~59 bağlama
  noktası** var ve bare-metal yolları ws2_32 **ALMAMALI**.
  **İLKE: yeni bir yetenek, onu KULLANMAYAN programların bağlama sözleşmesini
  DEĞİŞTİRMEMELİ.** Link yüzeyi hiç değişmedi.
  Cast zinciri de ölçüldü: `(Fn)(void*)` → **16 uyarı**, `(Fn)` → **7**,
  `(Fn)(void(*)(void))` → **0**. Sıfır uyarı korundu.
- **🔴 ÖNEK EŞLEMESİ AD-YAKALAYICIDIR:** `soket_` öneği derleyicinin **KENDİ**
  yardımcısı `soket_kontrol`u da yakaladı → `@kdl_soket_kontrol` tanımsız,
  **self-host DERLENEMEDİ**. Kapalı bir küme için **TAM EŞLEŞME** kullanıldı
  (iki derleyicide de). Mevcut `metin_`/`dosya_`/`kilit_` önekleri çakışma
  olmadığı için çalışıyor — bu bir tasarım güvencesi değil, ŞANS.
- **🎯 SABOTAJ KAPIYI DÜZELTTİ (S66):** yetki argümanını atlamayı kaldırınca
  kapı kırmızı olmak yerine **ASILDI** — dinleyici gelmeyen bağlantıyı
  sonsuza dek bekliyordu. **Asılan kapı, sessiz kapı kadar kötüdür**: kimse
  10 dakika bekleyen bir testi koşturmaz. `timeout 15` + dinleyiciyi öldürme
  eklendi; S66 artık temiz kırmızı veriyor. **S65** (`soket_gecerli` daima 1)
  → `ag: BASARISIZ`, make exit 2.
- **YENİ KAPI `calistir_ag_kosum`:** GERÇEK TCP gidiş-dönüşü, **her iki
  derleyicide**. `stdlib_check` yalnızca dış bağımlılığı olmayan yolları
  ölçer (doğrulama + bağlanamama); pozitif yol bir karşı taraf ister ve
  sürekli koşan kapıya konsaydı ortamdan ötürü **ARALIKLI kırmızı** verirdi.
  `test_tumu` artık **62 kapı** çağırıyor.
- **V1 SINIRLARI (kodda yazılı):** dinleyici YOK · **TLS YOK** (yarım TLS,
  TLS olmamasından TEHLİKELİDİR — kullanıcı korunduğunu sanır) · zaman aşımı
  yok · `al` NUL-sonlandırmalı `metin` döner → **gömülü NUL içeren İKİLİ VERİ
  KIRPILIR** (V1 bilinçli olarak metin protokolleri içindir).

### 🔴 D-465: yapıcılar ARGÜMAN ve ATAMA pozisyonunda çözülmüyordu
`hiç` / `değer(N)` / `tamam(N)` / `hata(m)` şu şekillerde DÜŞÜYORDU:
`f(hiç)` (C "tanimsiz tanimlayici", self `@hiç` RED) · `f(değer(42))` ve
`o = değer(42)` (her iki derleyicide `@değer` tanımsız = LINK-RED).
- Şekiller KEMGU'da **ÇOK DOĞAL** (`sonuç`/`seçimlik` üzerine kurulu her kod)
  ama **hiçbir korpus dosyası içermiyordu**. D-464'ün fikstürünü yazarken
  çıktılar ve o artımda **BİLEREK** kapsam dışı bırakılmışlardı (kapı yanlış
  sebeple kırmızı olmasın diye — D-421); kayda geçirilmişti, sonra kapandı.
- **KÖK: mekanizma ZATEN VARDI** (`yapici_uret` + `beklenen_tip`/`beklenen_ll`);
  **BAĞLAMI KURAN KAPILAR DARDI.** C'de argüman bağlamı yalnız `Dizi<T>` için
  (D-070), atama bağlamı yalnız dinamik dizi için (D-092) kuruluyordu.
  Onarım ikisinde de mevcut koşulun genişletilmesi — **icat edilen şey YOK**.

### 🔴 D-464: kapsayıcı `eşleş` + işaretçi/kesirli dönüş → GEÇERSİZ IR
Gövdesi **kapsayıcı bir `eşleş`** olan ve `metin`/`kesirli64` dönen **HER**
işlev derlenmiyordu: ulaşılamaz düşüş yolunda ham `ret ptr 0` / `ret double 0`
yayılıyordu → *"integer constant must have integer type"*. LINK-RED, yani
sessiz DEĞİL — ama kullanıcıyı GEÇERLİ bir programı yeniden yazmaya zorluyordu.
- **D-407 DESENİ:** doğru mantık C'de **D-304'ten beri VARDI** ve yorumu bile
  `ret ptr 0`ın geçersizliğini yazıyordu — ama YALNIZ lifted-lambda yolunda.
  Sıradan işlev yoluna hiç uygulanmamıştı. Self-host'ta da aynı boşluk
  (`ret_bos_yaz` yalnız `void`ü ayırıyordu, D-418).
- **Şekil KEMGU'da ÇOK DOĞAL** (`sonuç`/`seçimlik` üzerine kurulu her kod) ama
  **hiçbir korpus dosyası içermiyordu** → kaçmıştı. D-463'ün yakalama API'sini
  kullanan İLK test bu duvara anında çarptı.
- **⚠ FİKSTÜR YAZARKEN ÜÇ AYRI ÖNCEDEN-VAR-OLAN KUSUR ÇIKTI** (üçü de
  `git stash` ile doğrulandı; hepsi kayıtlı tek sınırın görünümleri —
  **yapıcılar ancak BEKLENEN TİP bilindiğinde çözülür**):
  çıplak `hiç` ARGÜMAN olarak (C: "tanimsiz tanimlayici", **self KABUL EDER**) ·
  `değer(1)` ARGÜMAN olarak · `o = değer(k)` ATAMA olarak (ikisi de `@değer`
  tanımsız). **Fikstür bilerek DAR** tutuldu (yapıcılar yalnız annotasyonlu
  başlatıcıda): bunları içine almak DÖNÜŞ YOLU yerine yapıcı çözümünü ölçerdi,
  kapı **yanlış sebeple** kırmızı olurdu (D-421'in dersi). **Bu üçü AÇIK.**
- **Doğrulama:** `cg_kapsayici_esles_donus.kem` → C=42, SELF=42; ham
  `ret ptr 0` SIFIR, `ret ptr null` 2 + `ret double 0.0` 1 (birebir).
  **Sabotaj 2/2** (S63 C · S64 self) → ikisi de LINK-RED.
  ⚠ S63 ilk denemede UYGULANMADI (`perl` satır aralığı tutmadı, `grep` 0) ve
  kapı yeşil kaldı — sessizlik önce SABOTAJI şüpheli kılar (D-402).
  Kapılar: `codegen_diff` **154/154** · `yapi_diff` **129/129**.

### 🎯 D-463: regex YAKALAMA GRUPLARI — doğrusallık korundu
Mehmet'in kararı **(a)**: genel eşleşme **EN UZUN** kalır (D-461'in sözleşmesi
bozulmaz), **yakalama ayrımı ÖNCELİK tabanlıdır**. Seçenek (b) tam POSIX
alt-ifade kuralı REDDEDİLDİ (yarım uygulamak ince ve **sessiz** yanlış
yakalamalar üretirdi); (c) genel anlambilimi değiştirmek 29 ölçümü kırardı.
- **Mekanizma:** Pike VM'de iş parçacığı başına yuva dizisi. Epsilon-kapanışı
  bir DFS'tir ve yuva dizisi bu DFS boyunca **PAYLAŞILIR**; `KAYDET` (op 8)
  eski değeri saklar, özyinelemeden sonra **GERİ ALIR**. Kopya YALNIZ yaprak
  listeye eklenirken alınır → çatallanma başına kopya maliyeti YOK.
- **Öncelik kuralı EK MEKANİZMA İSTEMEDİ:** `damga` bir komuta İLK ulaşanı
  tutuyor ve `CATAL` sol kolu (`a`) sağdan (`b`) önce ekliyor.
- Numara **AÇILIŞ PARANTEZİ** sırasında ayrılır (`((a)(b))` → dış 1).
  Yakalanmamış grup **-1**'dir, boş dizgi DEĞİL.
- **⚠⚠ İKİ AYRI KURAL — kendi testimde karıştırdım, ÖLÇÜM DÜZELTTİ:**
  genel eşleşme uzunluğu **EN UZUN** kazanır; yakalama ayrımı yalnız **EŞİT
  UZUNLUKTAKİ** eşleşmeler arasında öncelikle belirlenir. `(a)|(ab)` üzerinde
  `"ab"` → sağ kol daha UZUN eşler, öncelik **devreye girmez**. Önceliğin
  gerçekten ölçüldüğü vaka ayrıca eklendi (`(a)|(a)`).
- **⚠ `nyuva == 0` KORUMASI:** yakalamasız koşumda `yuva` BOŞTUR; korumasız
  `KAYDET` runtime **sınır ihlali** verirdi.
- **🎯 DOĞRUSALLIK YENİDEN ÖLÇÜLDÜ** (asıl risk buydu): `(a+)+b` **yakalamalı**
  sürümde girdi **20 → 320 (16 KAT)** büyürken süre 254–529 ms bandında kaldı;
  boş program tabanı ~309 ms. D-461'in tezi yakalamayla birlikte AYAKTA.
- **Sabotaj 2/2:** S61 (grup başlangıç yuvası) · S62 (yaprak yuva kopyası).

### 🎯 D-462: YENİ KAPI `calistir_belge_kapisi` — kendi ilk koşumunda kusur buldu
"Kod var ama hiçbir ölçüm ateşlemiyor" sınıfı bu depoda **DÖRT kez elle**
yakalandı (D-458 `\b`/`\f` · D-461 `\d\w\s` · D-462 `\" \\ \/`) ve her
seferinde gerçek bir şey sakladı. *Elle taranan ölçüm eskir, kapı eskimez.*
- **KAPI KENDİ İLK KOŞUMUNDA BİR BOŞLUK BULDU:** `\u`nun **BAŞARI** yolu hiç
  ölçülmüyordu. Ölçüyor sanılan satır `json_kacis_bayt("aAb", 65, ...)` idi —
  **içinde ters bölü YOK**, kaçış hiç ateşlenmiyordu; "vekil çifti" testi de
  kaynakta DÜZ EMOJİ kullanıyordu. D-458 bunları "destekleniyor" saymıştı.
  **ÖNCE kusur mu diye ölçtüm:** hepsi doğru çalışıyor → eksik olan yalnız ÖLÇÜM.
- **Kapsam bilerek DAR** (yalnız `json.kem` kaçış kolları): genel bir "belge
  iddiası" tarayıcısı yanlış-pozitif üretip kapıyı gürültüye çevirirdi. Kapı
  kolları **KODDAN**, ölçümleri **TESTTEN** okur; prozadan tahmin YOK.
  Bilinmeyen kol eklenirse kapı *"eşleme tablosuna EKLE"* diyerek KIRMIZI olur.
- **⚠ ARGÜMAN ÇIKARIMI `grep -oE` İLE YAPILAMAZ:** POSIX ERE'de tembel
  niceleyici yoktur ve çağrılar ÇOK SATIRLI olabilir → ilk sürüm İKİ
  yanlış-pozitif verdi. **Ölçüm aracının kendisi yanlıştı, kod değil.** Awk
  artık parantez derinliği izler.
- **⚠ KAYNAKTA KAÇIŞ DİZİSİ BİRLEŞTİRMEYLE KURULUR:** düzenleme araçları
  `A`i "A"ya **ÇÖZÜYOR** (bu artımda iki kez) ve ölçüm sessizce
  anlamsızlaşıyor. `ters_bolu()` + `"u0041b"` deseni kullan.
- **⚠ SÜREÇTE İKİ HATAM:** Türkçe `.kem`de `sed -i` kullandım (yasak) ·
  `git checkout` ile sabotajı geri alırken **commit'lenmemiş** testlerimi de
  sildim. Sabotaj 2/2 (S1 `\/` · S2b her iki `\u`).

### 🎯 D-461: `stdlib/regex.kem` — Thompson NFA (ReDoS'a kapalı), saf KEMGU
Roadmap'in son büyük stdlib maddesi. **Yeni yerleşik / runtime primitifi
GEREKMEDİ** — mevcut `metin_*` yeterli (D-435'te ölçülmüştü, doğru çıktı).
- **⚠⚠ MOTOR SEÇİMİ TEZDEN ÇIKAR: Thompson simülasyonu (Pike VM), GERİ İZLEME
  DEĞİL.** PCRE/Java/Python geri izlemelidir ve `(a+)+b` masum görünürken uzun
  girdide ÜSTELE çıkar (ReDoS). **Çökmezlik vaat eden bir dilin stdlib'i,
  kullanıcının fark edemeyeceği bir desende ASILAMAZ** — bu tam olarak D-296'da
  reddedilen sınıf ("safety korunuyor, liveness kayboluyor").
  **BEDELİ AÇIK: geri-referans (`\1`) YOK** — çünkü geri-referans zaten geri
  izlemeyi ZORUNLU KILAN şeydir. Eksiklik değil, bilinçli takas.
- **🎯 DOĞRUSALLIK ÖLÇÜLDÜ, İDDİA EDİLMEDİ:** `(a+)+b` deseni, girdi
  **20 → 160** (8 kat) büyürken süre **SABİT** kaldı (466/441/439/498 ms).
  Boş program 367 ms → regex'in payı ~70-130 ms ve **girdiyle büyümüyor**.
  Geri izlemeli bir motor N=40'ta zaten pratikte asılırdı.
- **⚠ ÇEKİRDEK İNVARYANT — geri izlemeden kaçınmak TEK BAŞINA YETMEZ:** her
  girdi konumunda bir komut en fazla BİR KEZ etkin listeye girmeli (`nesil`
  damgası). **Sabotaj S59** (tekilleştirmeyi kaldır) → aynı test **45 sn'de
  ASILDI (exit 124)**, normalde 0.5 sn. Yani damga taşıyıcı, süs değil.
- **UTF-8 KOD NOKTASI düzeyinde:** `.` bir KARAKTER eşler. Test bilerek Türkçe:
  `...` deseni `çığ`ı (6 BAYT, 3 KARAKTER) tam eşler — bayt düzeyinde çalışan
  bir motor burada DÜŞERDİ.
- **Kapsam (V1, dürüstçe):** literal · `.` · `*` `+` `?` · `|` · `()` ·
  `[a-c]`/`[^a-c]` · `^` `$` · `\d \w \s` · kaçışlar.
  **YOK (V1'de):** geri-referans · tembel niceleyici · `{n,m}` · **yakalama
  grupları** (gruplar yalnız gruplama) · `\D \W \S`.
  ⚠ **BU SATIR ESKİDİ:** yakalama grupları **D-463**'te, `\D \W \S` ve
  `{n,m}` **D-472**'de eklendi. Kalan gerçek YOK'lar: geri-referans ve tembel
  niceleyici — ikisi de geri izleme gerektirdiği için BİLİNÇLİ olarak
  eklenmeyecek.
- **"En uzun" (leftmost-longest) seçildi:** Thompson'da tüm kollar EŞ ZAMANLI
  ilerler, yani geri izlemenin "ilk bulunan"ı gibi ALTERNATİF SIRASINA bağlı bir
  cevap yoktur — en uzunu bildirmek tek tutarlı seçim.
- **Yol üstünde ölçülenler:** `Dizi<T>` DEĞERLE geçse de HEAP-DESTEKLİ, mutasyon
  çağırana yansır (`&değişken` gereksiz — referans işaretlemek yanlış güvence
  verirdi) · yapı alanları `;` ile ayrılır, `,` ile değil.
- **⚠ KENDİ KURALIMI ÇİĞNEDİM:** Türkçe UTF-8 `.kem` dosyasında `awk`/`perl`
  kullandım; `awk`ta `&` "tüm eşleşme" demek olduğu için **17 satırı bozdum**
  (`rx_bitti(rx_bitti(a)değişken a)`). Edit ile onarıldı. Kural zaten CLAUDE.md'de
  yazılıydı — **yazılı olması uygulamaya yetmiyor.**
- **🎯 KENDİ DENETİMİM İKİ KÖR NOKTA BULDU (merge öncesi adversarial denetim):**
  1. **`\d \w \s` kodda vardı, `--check`ten geçiyordu, ama HİÇBİR ÖLÇÜM
     ateşlemiyordu** — D-458'de `\b`/`\f`nin "destekleniyor" yazılıp kolunun hiç
     olmamasıyla AYNI SINIF. Test eklendi; **sabotaj S60** (`\d`yi `[A-F]` yap)
     → `YANLIS SONUC: \d+ / ab123cd`.
  2. **Boş eşleşme davranışı sessiz bir tasarım kararıydı.** Ölçüldü: `a*` on
     `"bbb"` → 4 eşleşme, `degistir` → `-b-b-b-` — **Python `re.sub` ile
     BİREBİR**. Varsaydığım doğruydu ama ölçmeden bırakmak yanlış olurdu;
     sabitlendi (boş eşleşmede ilerlememek SONSUZ DÖNGÜ olurdu).
- **`check_genis` muafiyeti (E3):** tek başına `derle` tanımsız → C `hata(m) =>`
  kolundaki `m` için 2 T002 basar, self bağlayıp susar (C 14 / self 12).
  `test_kilit`/`test_semafor`/`test_bariyer` ile aynı sınıf. Not **yorum
  bloğuna** yazıldı — D-456'da dizginin içine yazmanın kapıyı sessizce
  zayıflattığı ölçülmüştü.
- **Doğrulama:** `test/stdlib/test_regex.kem` — **29 ölçüm** (literal/niceleyici/
  alternatif/sınıf/çapa/UTF-8/bozuk desen/ReDoS/`tum_bul`/`degistir`/hazır
  sınıflar/boş eşleşme). C ve SELF'te **exit 0**. `stdlib_check` **12 modül**;
  `check_genis` **131/131 (13 muaf)**. **Sabotaj 2/2:** S59 (nesil damgası) →
  45 sn ASILDI · S60 (`\d` sınıfı) → kırmızı.

### ✓ D-460: JSON `Ondalik` varyantı + `metin_kesirli` (askıdaki 2. iş)
- **`JsonDeger`e `Ondalik(kesirli64)` eklendi**; yazıcı `kesirli_metin` (D-457,
  kayıpsız) ile geri yazar, ayrıştırıcı `.`/`e`/`E` görünce ondalık dala girer.
  **M001 kapsayıcılık denetimi eksik 7 kolu tek tek bildirdi** — ADT değişimini
  güvenli kılan tam olarak buydu (test dosyasındaki bir yardımcıyı da yakaladı).
- **⚠ TAMSAYI ve ONDALIK AYRI VARYANTLAR, sessizce dönüşmezler:** `json_sayi_al`
  ondalığa `hiç`, `json_ondalik_al` tamsayıya `hiç` döner. Gerekçe: 2^53 üstü
  tamsayılar `kesirli64`de KAYIPSIZ DEĞİLDİR — otomatik dönüşüm `Sayi`nın tam64
  kesinliğini sessizce bozardı.
- **Yeni yerleşik `metin_kesirli` + `metin_kesirli_gecerli`** (`metin_tam`ın
  simetriği). **Saf KEMGU'da basamak toplamak REDDEDİLDİ:** `tam + kesir/10^k`
  uzun ondalıklarda doğru-yuvarlanmış sonucu kaçırır ve D-457'nin gidiş-dönüş
  garantisini bozardı. Geçerlilik JSON dilbilgisidir; `strtod` tek başına
  `"0x10"`, `"inf"`, `"nan"`, `"+1"` ve baştaki boşluğu KABUL EDERDİ.
- **🎯 SABOTAJ SESSİZ KALDI VE BİR KÖR NOKTA AÇTI.** S58 (yüklemi daima 1 yap)
  `test_json`i yeşil bıraktı: `"1."` zaten **ayrıştırıcının kendi denetiminde**
  reddediliyor, yüklem o yoldan **hiç ateşlenmiyor** (savunma katmanı). Yani
  yüklemi ölçen kapı YOKTU. Ayrı fikstür (`cg_metin_kesirli.kem`) yüklemi
  DOĞRUDAN çağırıyor → S58 artık exit 7. **Bir kuralın hangi yolla ateşlendiğini
  ölçmeyen kapı o kuralı ölçmüyordur** (D-443'ün tekrarı; bu kez sessizlik
  sabotajın değil KAPININ eksikliğiydi).
- **Doğrulama:** `cg_metin_kesirli.kem` → C=42, SELF=42; `test_json.kem`e 10
  ondalık ölçümü (gidiş-dönüş + `[1,1.5]` karışık dizi + tamsayı yolunun
  bozulmadığı + `1.`/`1e`/`1e+` reddi). **Sabotaj 2/2:** S57 (ondalık dalını
  kapat) → 6 ayrıştırma hatası · S58 (yüklem) → fikstür 7.
  Kapılar: `codegen_diff` **153/153** · `checker_diff` **153/153 (0 muaf)** ·
  `yapi_diff` **128/128** · `check_genis` **130/130** · `stdlib_check` 11 modül.

### 🔴 D-459: `tam_metin`/`metin_tam` + YERLEŞİK ÇAĞRI SONUCU METİN-LİĞİNİ KAYBEDİYORDU
Öneri sırasının 1. maddesi (askıya alınmıştı — ön koşulu bir oracle kusuruydu).
- **🔴 ÖN KOŞUL KUSURU (D-449'un kaçırdığı):** çağrı sonucunun `metin_mi`
  bilgisi **yalnız kullanıcı işlevlerinden** alınıyordu (`ik ? ik->donus_metin :
  0`); yerleşikte `ik == NULL` → bilgi kayboluyordu. Ölçüldü:
  `metin_birlestir("","b") == "b"` → **C exit 7** (`icmp eq ptr`),
  **self-host 42** — **parite TERS yönde** (D-442'nin sınıfı).
  Annotasyonlu bağlama bilgiyi taşıdığı için kusur yalnız **doğrudan çağrı** ve
  **annotasyonsuz bağlama** şeklinde görünüyordu; D-449'un korpusundan bu yüzden
  kaçmıştı.
- **KÜRATE LİSTE, "dönüş tipi metin mi" DEĞİL:** tip tablosunda `metin` dönen
  **21** yerleşik var ama hepsi dizgi değil — `bellek_al`/`bellek_kopyala` HAM
  BELLEK, `dosya_ac`/`kilit_olustur`/`semafor_olustur`/`bariyer_olustur` OPAK
  HANDLE. Onları içerik karşılaştırmasına sokmak YANLIŞ olurdu (ölçüldü:
  `bellek_al(8) == bellek_al(8)` iki derleyicide de doğru davranıyordu; liste o
  doğruluğu KORUR). **Aynı liste iki yerde** (`builtin_metin_donus_mu` +
  self-host `ifade_metin`) — D-407 gereği ikisi birlikte değişmeli.
- **Yerleşikler:** `tam_metin(tam64) -> metin`, `metin_tam(metin) -> tam64`,
  `metin_tam_gecerli(metin) -> mantıksal`. **Genişlik tam64'tür, tam32 DEĞİL:**
  `JsonDeger::Sayi` tam64 taşır ve KEMGU'da örtük dönüşüm YOKTUR → tam32 alan
  bir yerleşik JSON yolundan çağrılamazdı.
- **Eski `kdl_metin_to_tam` KALDIRILDI** (`atoi` sarmalı, çağrısı yoktu):
  başarısızlığı SESSİZDİ (`"abc"` → 0, gerçek `"0"`dan ayırt edilemez). Ölü kodu
  bırakmak, sonraki okuyucunun onu "hazır primitif" sanıp sessiz-başarısız yolu
  yeniden açmasına davetiye olurdu. Yerine ayrık yüklem (D-449/D-458 deseni);
  kural: isteğe bağlı işaret + EN AZ BİR rakam + TAMAMEN tüketilmiş (`"12ab"` ve
  `" 12"` GEÇERSİZ — `strtoll` tek başına ikisini de kabul ederdi).
- **⚠ İKİ MERDİVEN VAR, HANGİSİNE YAZDIĞIN ÖNEMLİ.** `tam_metin`i önce GEÇ
  merdivene koydum; `param_beklenen` argümanlar **değerlendirilmeden önce**
  okunuyor (~5082) → değer `i32` geçti, LLVM **sessizce kabul etti** (D-295) ve
  `tam_metin(0 - 4207)` yanlış dizgi üretti. Eşleme ERKEN merdivene taşındı.
- **🔴 YAN BULGU (önceden var):** self-host'ta D-393'ün i64 genişletmesi
  **yalnız kullanıcı işlevleri** içindi; yerleşikler dışarıdaydı → `yaz_tam64`
  de `i32` geçiyordu (ölçüldü, dokunmadığım bir yerleşik). `builtin_param_ir`
  ile kapandı; `yaz_tam64` yan kazanç olarak düzeldi.
- **Doğrulama:** fikstür `cg_tam_metin.kem` → C=42, SELF=42 (gidiş-dönüş +
  2^33 genişliği + yüklem + üç metin-lik şekli). **Sabotaj 2/2:** S55 (kürate
  yüklemi kaldır) → C exit 7 · S56 (self-host yerleşik genişletmesi) → SELF 7.
  Kapılar: `codegen_diff` **152/152** · `checker_diff` **153/153 (0 muaf)** ·
  `yapi_diff` **127/127**.

### 🔴 D-458: `kod_metin` + JSON `\n` SESSİZCE `n` HARFİNE ÇÖZÜLÜYORDU
Öneri sırasının 4. maddesi (`\uXXXX`). Yerleşik küçük; **açığa çıkardığı kusur
sessiz veri bozulmasıydı.**
- **Yerleşikler:** `kod_metin(kod: tam32) -> metin` + `kod_gecerli(kod) ->
  mantıksal`. UTF-8 kodlayıcı runtime'da **zaten vardı** (`kdl_yazdir_karakter`)
  ama stdout'a yazıyordu — D-450'nin deseni: yeni algoritma yok, çıkışı tampona
  al. `json.kem`'in "saf KEMGU ile AŞILAMAZ, denemeden önce oku" notu bu yüzden
  ESKİMİŞTİ.
- **HAM BAYT→METİN BİLEREK YOK:** `metin`in geçerli-UTF-8 olma değişmezi
  korunmalı; ham bayt primitifi geçersiz dizgi kurmaya izin verirdi. UTF-16
  **vekil çiftleri** çağıranda (`json.kem`) saf KEMGU aritmetiğiyle birleşir.
- **⚠ KOD 0 GEÇERSİZDİR — ölçülmüş karar, keyfi değil:** KEMGU `metin`i
  NUL-sonlandırmalı C dizgisidir (`metin_uzunluk`→`strlen`, `birlestir`→`strlen`,
  ölçüldü) → **gömülü NUL TEMSİL EDİLEMEZ**. Geçerli saysaydık `` sessizce
  düşerdi. Loud > silent: `json.kem` açık hata veriyor.
- **🔴 KUSUR — `satir_sonu()`/`sekme()`/`satir_basi()` HARF DÖNDÜRÜYORDU.**
  `metin_kes("\n", 1, 1)` ile kuruluyorlardı; KEMGU'da kaçış YOKTUR (D-400/
  D-409/D-416'da üç kez kaydedilmiş) → `"\n"` iki karakterdir ve indeks 1
  **`n` HARFİDİR (bayt 110, ölçüldü)**. Yani JSON `"a\nb"` sessizce **`anb`**
  çözülüyordu — SESSİZ VERİ BOZULMASI, hiçbir test ölçmüyordu.
  `tirnak()`/`ters_bolu()` aynı idiomu kullanır ama **DOĞRUdurlar** (bayt 34/92
  ölçüldü): orada istenen karakter zaten `\`den sonraki bayttır. **Aynı idiom,
  iki farklı sonuç — ölçmeden "hepsi bozuk" ya da "hepsi doğru" demek yanlış.**
- **🔴 `\b` ve `\f` KOLLARI HİÇ YOKTU** — başlık yorumu ikisini de "desteklenir"
  diye sayıyordu (kollar yalnız 34/92/47/110/114/116). Belgeyi koda uydurmak
  yerine **eksik kollar eklendi** (JSON standardı gerektiriyor).
- **⚠ ARAÇ TUZAKLARI (üçü de ölçümle yakalandı):** (1) yorumuma `` yazmak
  dosyaya **GERÇEK NUL baytı** soktu ve lexer'ı kırdı — iki kez; kaçış
  birleştirmeyle kuruldu. (2) `awk` ile yazılan sabotaj `\\n`i gerçek satır
  sonuna çevirip **geçersiz KEMGU üretti** → ölçüm geçersizdi, Edit ile
  tekrarlandı (deponun kendi kuralı). (3) Kabuk heredoc'unda `$1` ikamesi kaçtı
  → bayt 49 ölçtüm; **ölçüm aracı da yanlış olabilir.**
- **Doğrulama:** fikstür `cg_kod_metin.kem` → C=42, SELF=42 (1/2/3/4 baytlık
  kodlama + sınır 0x10FFFF DAHİL + geçersizler). `test_json.kem`e 12 yeni ölçüm:
  kaçışlar **BAYT DEĞERİYLE** denetlenir ("hata döndü mü" YETMEZ — D-443).
  **Sabotaj 3/3:** S52 (`satir_sonu`u eski hâline döndür) → `kacis YANLIS bayt
  cozdu` · S53 (`kod_gecerli` daima 1) → fikstür 7 + `` KABUL EDİLDİ ·
  S54 (self-host önek eşlemesi) → LINK-RED.
  Kapılar: `codegen_diff` **151/151** · `checker_diff` **153/153 (0 muaf)** ·
  `yapi_diff` **126/126** · `check_genis` **130/130** · `stdlib_check` 11 modül.

### 🔴 D-457: `kesirli_metin` + KESİRLİ LİTERALLER DERLEME ANINDA KIRPILIYORDU
Öneri sırasının 3. maddesi (`kesirli64` → `metin`). Yerleşik küçük, **yol
üstünde çıkan iki kusur büyük.**
- **Yerleşik:** `kesirli_metin(x: kesirli64) -> metin` — **locale bağımsız** ve
  **kayıpsız**. `setlocale` bu depoda HİÇBİR YERDE çağrılmıyor (ölçüldü) → C
  programları varsayılan `"C"` locale'dedir, `%g` zaten nokta üretir; yine de
  `localeconv()` ile gerçek ayıraç okunup `.` yapılır (runtime başka bir
  programa gömülüp o program `setlocale(LC_ALL,"tr_TR")` çağırırsa JSON çıktısı
  SESSİZCE virgülle bozulurdu). **Varsayıma değil ölçüme dayan.**
- **KISA GİDİŞ-DÖNÜŞ:** `%.15g/%.16g/%.17g` sırayla denenir, `strtod` ile geri
  okunup bit-eşitliği doğrulanır. `0.1` → `"0.1"` (ham `%.17g`
  `"0.10000000000000001"` verirdi), `1/3` → 16 basamak. Fikstürün (a) maddesi
  tam bunu kilitler.
- **🔴 KUSUR 1 — KESİRLİ LİTERAL DERLEME ANINDA KIRPILIYORDU.** `%g` ALTI
  anlamlı basamak verir → `3.14159265358979` daha IR'a yazılırken `3.14159`
  oluyordu, **her iki derleyicide** ve hem `--ast` dökümünde hem IR'da.
  **Kayıpsız bir dönüştürücü TEK BAŞINA yetmezdi — veri zaten kaybolmuştu.**
  Onarım **TEK KAYNAK** (`kesirli_kisa_bicimle`, `ast.c`): döküm ve IR aynı
  yardımcıyı çağırır. Ayrı ayrı biçimlemek `parser_diff`i SESSİZCE ayrıştırırdı
  (D-407: aynı soruyu iki yerde ayrı yanıtlayan kod ayrışır). Self-host tarafı
  `kdl_ondalik_bicimle` (aynı kural) — o dizgi parse'ta AST'ye girip hem
  `--parse` dökümüne hem IR'a aktığı için tek noktadan onarılır.
- **🔴 KUSUR 2 — self-host'ta kesirli TEKLİ EKSİ geçersiz IR:**
  `sub double 0, 0.5` → *"integer constant must have integer type"*.
  `git stash` ile ölçüldü: **D-456 tabanında da vardı**, yan etkim DEĞİL —
  hiçbir korpus dosyasında negatif kesirli literal yokmuş. C `fsub double 0.0,
  %n` yayar; self artık `kesirli_ll_mi` ile ayırıp `fsub` yayıyor.
- **⚠ KENDİ HATAM (1):** `test_tumu` KOŞARKEN kaynağı değiştirdim (D-402'nin tam
  uyardığı şey) → o koşum `SELF-HOST BOOTSTRAP: BAŞARISIZ` verdi. **Gerçek
  gerileme DEĞİL**, stage1/stage2 farklı kaynaktan kuruldu; sonuç GEÇERSİZ
  sayılıp temiz yeniden koşuldu. Uzun koşum sürerken kaynağa dokunma.
- **⚠⚠ KENDİ HATAM (2) — `cp` İLE GERİ ALMA `make`İ KANDIRIR.** Sabotaj (S49)
  sonrası `src/ast.c`yi `cp` ile geri aldım; `cp` mtime'ı **kopyalama anına**
  set eder ve `build/ast.o` **AYNI SANİYEDE** üretilmişti → `make` objeyi
  GÜNCEL saydı, **sabotajlı obje yerinde kaldı.** Sonuç: kaynak TEMİZKEN
  `codegen_diff` 149/150 (`C exit=7 ≠ KEMGU exit=42`) — sahte kırmızı, üstelik
  onarımın kendisini geri almış gibi görünüyordu. Teşhis: kaynakta yama VAR
  (`grep`), ikili ESKİ davranıyor, `ls` mtime'ları eşit.
  **KURAL: sabotaj döngüsünden sonra ilgili `.o`yu VE ikiliyi `rm -f` et; salt
  `make` yetmez.** (D-432 aynı dersi TEST ikilisi için kaydetmişti; bu kez
  ARADAKİ obje idi. Aynı sınıfın üçüncü tekrarı.)
  **Ayrıca: "kapı tek başına yeşildi ama takımda kırmızı" ilk olarak ARTEFAKT
  şüphesi doğurur** — sıra: (a) kaynakta yama duruyor mu, (b) ikili gerçekten
  o davranışı mı gösteriyor, (c) `rm -f` + yeniden kur, (d) hâlâ kırmızıysa
  gerçek kusur.
- **Doğrulama:** fikstür `cg_kesirli_metin.kem` → C=42, SELF=42; `--ast`/
  `--parse` ve IR C↔self **birebir**. **Sabotaj 3/3:** S49 (tek kaynağı `%g`ye
  döndür) → C exit 7 · S50 (self lexeme biçimlemesi) → SELF exit 7 ·
  S51 (`fsub` onarımını geri al) → LINK-RED.
  Kapılar: `parser_diff` **13/13** · `codegen_diff` **150/150** ·
  `checker_diff` **153/153 (0 muaf)** · `yapi_diff` **125/125** ·
  `snapshot_test` **50/50**.
- **KALAN (kütüphane işi, dil yüzeyi kararı DEĞİL):** `json.kem`e
  `Ondalik(kesirli64)` varyantı + ayrıştırıcıda ondalık/üstel sözdizimi +
  8 `eşleş` kolu. Ondalık sınırının KÖKÜ kalktı.

### 🎯 D-456: `Semafor` + `Bariyer` (kapsamlı API) + `check_genis` muafiyet warty
İkinci öneri turunun 2. maddesi; Mehmet **kapsamlı API** dedi.
- **Runtime YENİ kod istedi** (D-455'in aynısı, önce ölçüldü): mevcut
  `kdl_kosul_*` **`static`** ve **`KdlKanal*`** alıyor — kanala gömülü, dışa
  verilebilir API değil. Bağımsız `KdlSemafor`/`KdlBariyer` + 7 dışa-verilen
  işlev yazıldı (opak `void*`, thread'siz derlemede NO-OP).
- **⚠ BARİYERDE KAPSAM BİLEREK YOK — ve bu bir eksiklik değil.** Kapsam
  `kilit`/`semafor`da GERÇEK bir hata sınıfını (unutulmuş/çift `bırak`) yapısal
  olarak imkânsız kılar. Bariyerin eşlenecek çifti YOKTUR: `bekle` tek çağrıdır.
  `bariyerde(b, ||{..})` yazmak yalnız `bekle`yi süsler; **önlediği bir hata
  olmadan kapsam görüntüsü vermek okuyucuyu var olmayan bir güvence konusunda
  yanıltır.** Semaforda kapsam ayrıca kilitten DAHA değerlidir: fazladan `bırak`
  sayacı şişirir ve karşılıklı dışlama **sessizce** kaybolur.
- **Bariyerde `nesil` sayacı ŞART** (yeniden kullanım): yalnız `vardi == hedef`
  beklemek, hızlı bir thread ikinci tura girip sayacı tekrar artırdığında
  kaçırılmış-uyandırma üretir. Bekleme her yerde `while (koşul)` döngüsünde.
- **🔴 YOL ÜSTÜNDE: `check_genis` MUAFİYET LİSTESİ SESSİZCE ŞİŞMİŞ.** Açıklama
  metinleri `MUAF="..."` **çift tırnaklı dizgisinin İÇİNE** yazılmış (D-436'dan
  beri, D-455'te ben de ekledim) → dizgideki her **çıplak kelime bir muafiyet
  adı** olur ve her **backtick komut ikamesi** tetikler. Ölçüldü: **72 token,
  yalnız ~10'u gerçek dosya adı** (`T001`, `exit`, `de`, `tam`, `self` …).
  Kusuru kendi notumdaki "bariyer" kelimesi `stdlib/bariyer.kem`i muaf edince
  yakaladım (`⚠ MUAF ama artık EŞLEŞİYOR` uyarısı + `stdlib_check: command not
  found`). **Ölçüldü: hiçbir gerçek dosya kaçmamış** (çöp token'lar hiçbir
  taranan dosya adıyla çakışmıyor) — yani gizli bir gerileme YOK, ama gizli bir
  TUZAK vardı. Dizgi temizlendi: **72 → 12 token, hepsi dosya adı.**
  **KURAL: muafiyet dizgisinin içine açıklama yazma; açıklama yorum bloğunda.**
- **Doğrulama:** `semafor`/`bariyer` testleri C ve SELF'te exit 0; ikisi de
  **GERÇEK çok-thread'li** ölçüm yapar (semafor: iki görev + tek izin + küresel
  sayaç = 2; bariyer: iki görev + `main` = 3 katılımcı, buluşmadan SONRA okunur
  — `görev_birleştir` bilerek sonraya bırakıldı, önce çağrılsaydı bariyeri
  anlamsız kılıp testi sahte-yeşile çevirirdi). `stdlib_check` **11 modül**.
  Fikstür `cg_semafor_bariyer.kem` → C=42, SELF=42 (muaf DEĞİL).
  **Sabotaj 4/4:** S45 (`bariyer_bekle` no-op) → buluşma kayboldu, exit 1 ·
  S46 (`semafor_birak` no-op) → **exit 124 (asıldı)** · S47 (self-host önek
  eşlemesi) → LINK-RED · S48 (muafiyeti çıkar) → kapı kırmızı.
  Kapılar: `codegen_diff` **149/149** · `checker_diff` **153/153 (0 muaf)** ·
  `yapi_diff` **124/124** · `check_genis` **130/130**.
- **⚠ YAN ÖLÇÜM:** self-host `--check` tanı BASSA DA daima **exit 0** döner
  (`test_json` 63 tanıyla da 0), C ise 1. Genel ve önceden var olan SÜRÜCÜ
  davranışı; `--check` kapıları dump karşılaştırdığı için görünmüyor. Ayrı iş.

### 🎯 D-455 (KARAR 7): `Kilit` kapsamlı mutex + İKİ self-host parite boşluğu
Sıralamanın son maddesi. **⚠ Kendi gerekçem yanlıştı:** öneride "runtime
primitifleri hazır" demiştim; ölçüm gösterdi ki mevcut `kdl_kilit_*`
**`static`** ve **`KdlKanal*`** alıyor (kanala gömülü) — genel amaçlı API
DEĞİL. Bağımsız `KdlKilit` + `kdl_kilit_olustur/_al/_birak/_yok` YAZILDI
(opak `void*`, thread'siz derlemede NO-OP). **Gerekçe metni de bir iddiadır —
ölç** (D-453'ün dersi, üst üste 2. kez kendi gerekçemde).
- **Tasarım: kapsamlı API.** `kilitle(k, || {...})`; ham `al`/`bırak` çifti
  DIŞA VERİLMEZ (unutulmuş-bırak = deadlock, çift-bırak = UB). Kapanışlar iki
  derleyicide de çalıştığı için (D-322) sahiplik dile devredilebiliyor.
- **`Kilit` LİNEER DEĞİL** ve bu bilinçli: `Dosya` (D-452) tam bir kez
  tüketilmeli, kilit ise DEFALARCA kullanılır — lineerlik ikinci `kilitle`yi
  imkânsız kılardı. Test bunu açıkça ölçer.
- **🔴 İki ÖNCEDEN VAR OLAN self-host parite boşluğu açığa çıktı** (C doğru,
  self GEÇERSİZ IR):
  1. **Adlandırılmış işlevi değer olarak geçirmek** (`cagir(kirk)`): C fat
     value `{ptr @kirk, ptr null}` yayıyor; self adı bilinmeyen değişken sanıp
     `load i32, ptr ` (BOŞ operand) üretiyordu. **D-391'in `sabit` dalıyla AYNI
     SINIF** — onun yanına işlev-değeri dalı eklendi.
  2. **İç içe kapanış:** `lam_kuyruk_emit` kuyruk boyutunu BAŞTA BİR KEZ
     alıyordu → emit sırasında kuyruğa giren lambda hiç yayılmıyordu
     (`use of undefined value '@lambda_N'`). **Kuyruk SABİT NOKTAYA kadar
     işlenmeli** (C'nin `bekleyenler` worklist'i zaten böyle — D-401).
- **⚠ Test beklentim yanlıştı:** `|| { n = n + 40; ver n; }` ile yakalanan
  YEREL değişkeni mutasyona uğratıp dışarıdan okumaya çalıştım → görünmedi.
  Kilit kusuru DEĞİL: kapanış çevreyi KOPYALAYARAK yakalar (D-309). Paylaşılan
  durum için doğru yol küresel + `güvensiz` (test (d) tam bunu yapar).
- **⚠ `uygula` ANAHTAR KELİMEDİR** — fikstürde işlev adı yapınca P014 aldım;
  D-323 tam bu tuzağı kaydetmişti. `cagir` olarak değiştirildi.
- **Doğrulama:** `kilit` testi C ve SELF'te exit 0 (test (d) **gerçek
  iki-thread'li karşılıklı dışlama** ölçer: iki `görev`, küresel sayaç = 2).
  `stdlib_check` döngüsü **9 modül**. Fikstür `cg_islev_deger_ic_ice.kem` →
  C=42, SELF=42, `define` kümesi BİREBİR (muaf DEĞİL).
  **Sabotaj 2/2** (S43 işlev-değeri · S44 kuyruk) → ikisi de LINK-RED.
  Kapılar: `codegen_diff` **148/148** · `yapi_diff` **123/123**.

### 🎯 D-454 (KARAR 6): `bir()` / `sıfır()` intrinsic'leri — `kuvvet(x,0)` onarıldı
D-441'de `kuvvet<T>(x,0)`in **x** döndürdüğü (doğrusu 1) ölçülmüş ve testle
SABİTLENMİŞTİ; kök bir kusur değil **dil sınırıydı**: generic `T` içinde
`ver 1;` → `T020`.
- **İddia ÖNCE ölçüldü, hâlâ geçerliydi** (D-401 mono'yu eklediği için
  bayatlamış olabilirdi; olmamış).
- **Çözüm tam constraint sistemi DEĞİL, iki DAR intrinsic:** dönüş tipi
  BEKLENEN'den gelir (`hiç` gibi bağlamsal); monomorfizasyonda somutlaşır,
  codegen doğru genişlik/kind'da **SABİT yayar** — `@bir` diye sembol yok.
  `sayisal.kem`in kendi yorumu bu çözümü zaten işaret ediyordu ("tipik bir
  yaklaşım `sifir<T>()` + `bir<T>()` helper'ı"); yorum biliyordu, kimse
  yazmamıştı.
- **`kuvvet_tam` EKLENMEDİ:** geçici boşluk için KALICI API borcu olurdu.
  **`x / x` de DEĞİL:** `x=0`da sıfıra bölme = ÇÖKME; yanlış cevabı çökmeyle
  takas etmek iyileştirme değildir (D-441).
- **⚠ Üç yanlış yerleştirme, üçünü de derleyici/ölçüm yakaladı:**
  (a) özel-durumu önce `tip_belirle`ye koydum — orada `beklenen` YOK; doğru yer
  `tip_belirle_beklenen`. (b) `"s\xc4\xb1f\xc4\xb1r"` → **hex escape out of
  range**: `\xb1`den sonraki `f` HEX RAKAM (CLAUDE.md'nin tam uyardığı tuzak).
  (c) self-host'ta beklenen tipi yalnız `beklenen_ll`den okudum — o kanal YALNIZ
  tagged-union taşır → `double`da `add i32 0, 1` (LLVM-RED); zincir
  `beklenen_ll` → `beklenen_elem` → `cur_ret` yapıldı.
- Fikstür `cg_birim_deger.kem` → C=42, SELF=42 (tam32 · tam64 · kesirli64 ·
  `sıfır()` + boş dizide çarpımsal birim). **Sabotaj S43** → exit 33.
- D-441'in sabitlediği test gerçeğe göre güncellendi (`!= 1`). **Sabitlenmiş
  bilinen-yanlış test tam da tasarlandığı gibi kırmızıya dönüp bildirdi** —
  D-449'daki `ac("yok.txt")` ile aynı desen, ikinci kez işe yaradı.
- Kapılar: `codegen_diff` 147/147 · `checker_diff` 153/153 (0 muaf) ·
  `yapi_diff` 122/122 · `stdlib_check` 8 modül. Fikstür `yapi_diff`te bilinen
  **K4** (generic BASE gövdesi) sınıfında; bölmek çare değil çünkü `bir()`
  zaten yalnız generic gövdede anlamlıdır.

### 🎯 D-453 (KARAR 5): `calistir_qemu_cekirdek` — oracle'ın bare-metal kanıtı
D-448 saptamıştı: `src/llvm.c`ye dokunan bir değişiklik KEMGU-OS'u kırsa
**hiçbir host kapısı görmez**. O gün iki hedefi ELLE koşturmuştum — ama
**elle koşturulan ölçüm kapı DEĞİLDİR** (D-395).
- **Tam süpürme DEĞİL:** ~128 QEMU hedefi koşumu saatlere çıkarır, o zaman
  kimse çalıştırmaz. Beş temsilci: `qemu_smoke` (boot) · `kem_os_arm` (24 faz:
  asm/MMU/syscall) · `sha256_selfhost_arm` (imzasız kaydırma) ·
  `virtio_selfhost_arm` (yapı+işaretçi) · `bignum_selfhost_arm` (tamsayı
  genişlikleri). **Ölçüldü: 79 saniye** → o kadar ucuz olduğu için `test_tumu`ya
  DOĞRUDAN bağlandı (elle hatırlanacak kural bırakmak yerine). QEMU yoksa alt
  hedefler zarifçe atlar → QEMU'suz makinede kırmızı olmaz.
- **⚠ SABOTAJ BİR GEREKÇEMİ ÇÜRÜTTÜ:** `sha256_selfhost_arm`ı "D-446/sabitsüre
  alanı" diye işaretlemiştim; **S41** (sabitsüre soymasını boz) → kapı YEŞİL
  kaldı. Ölçtüm: o dosya **`sabitsüre` KULLANMIYOR** (86 `dtam32`, 0 sabitsüre).
  O risk `kripto_kosum` ile zaten takımda. **Gerekçe metni de bir İDDİADIR —
  ölç** (D-406'nın dersi, bu kez kendi yazdığım gerekçede).
- **⚠ İkinci sabotaj UYGULANMADI ve yeşil verdi:** S42'nin ilk denemesi
  "DESEN YOK" bastı (CRLF) ama kapı yeşil çıktığı için bir an "kapı zayıf"
  diye kaydedecektim. **Sabotajın sessizliği önce SABOTAJI şüpheli kılar**
  (D-402). `grep`le doğrulanıp Edit ile uygulandı → **EXIT 2**.
- `test_tumu` artık **60 kapı** çağırıyor.

### 🎯 D-452 (KARAR 4): dosya handle'ı `yapı tekkez Dosya` — lineer opak tip
D-443'ün kök nedeni (*"opak handle `metin` olarak tipleniyor"*) ve D-449'un
açık borcu (`kapat("")` yakalanamıyor) KAPANDI.
- **Altyapı HAZIRDI — önce ölçüldü.** Dört probe: `yapı tekkez`+`imha` ✓,
  sızıntı L001 ✓, çift tüketim L002 ✓, ve `sonuç<Dosya,metin>` desen bağlaması
  da C'de L001 veriyor. D-301'in `görev<T>` için kaydettiği "`sonuç` içinde
  lineerlik kaybolur" sınırı `yapı tekkez`e UYGULANMIYORMUŞ.
- **Kazanım (üçü de derleme zamanında):** `kapat("")` → **T001** ·
  `kapat(d); kapat(d)` → **L002** · açıp kapatmamak → **L001**.
- **Yüzey DAR:** 17 işlevden yalnız `ac`/`kapat` handle'ı dışarı veriyor;
  diğer beş kullanım tek işlev içinde aç-kapat (handle KAÇMIYOR).
- **🔴 Yol üstünde ÜÇ gizli kusur çıktı:**
  1. **D-450'nin kendi boşluğu** — `dosya_gecersiz`in self-host CODEGEN dönüş
     tipi bağlanmamış (`ret ptr 0` = LLVM-RED). Hiçbir kapı görmedi çünkü
     korpusta onu çağıran dosya YOKTU.
  2. **Self-host lineer izleme `yapı tekkez`i görmüyordu** — `desen_bagla_tip`
     yalnız ÖNEK arıyordu (`görev<`/`tekkez<`/`yetki<`); lineer yapının adı düz
     tanımlayıcı (`Dosya`). **Ölçüldü: D-452 ÖNCESİNDE de vardı** (C=L001,
     SELF=OK) — yan etkim değil.
  3. **`checker.kem` AYRI uygulama** — `checker_diff` onu kullanıyor ve
     D-449/D-450 yerleşikleri oraya hiç eklenmemişti (T002). CLAUDE.md'nin
     "üç ayrı uygulama var" uyarısı somutlaştı.
- **⚠ ÜÇ YANLIŞ TEŞHİS, üçünü de ENSTRÜMANTASYON düzeltti:** (a) `bt` "?"
  geliyordu — checker dizgisi kullanıcı tipini SİLİYOR (**D-439**: ham gerçeği
  ayrı kanalda taşı → `fn_rlin`); (b) `fn_rlin`i `imza_topla`ya (CODEGEN
  ön-geçişi) koymuşum, checker `imza_kaydet` kullanıyor (**D-420**: "bu yolda
  kim dolduruyor?"); (c) `fn_rlin` `fn_ad` ile PARALEL ama `yerlesik_ekle`
  yalnız `fn_ad`a ekliyordu → **indeksler kaydı**, kayıt doğruyken arama boş
  dönüyordu.
- **Doğrulama:** üç korpus fikstürü **muafiyetsiz** (negatif 2 + pozitif 1 —
  D-425: pozitif şekil olmadan toptan sıkılaştırma kapıyı GEÇER).
  `checker_diff` **153/153 (0 muaf)**. **Sabotaj S40** → iki fikstür KIRMIZI.
  `stdlib_check`: `dosya: TUM TESTLER GECTI` (C + self, artefakt yok).
- **Kalan (bilinçli):** `kapat("")` C'de T001, self-host'ta tanı YOK (kullanıcı
  yapı parametresi için argüman tip denetimi self'te yok). Soundness sorunu
  değil (self daha müsamahakâr); korpusa konamaz, kayda geçti.

### 🎯 D-451 (KARAR 3): `Dizi<kesirli64>` / `Dizi<kesirli32>` ONARILDI
D-417'nin "ölçüldü ama oracle değişikliği ister" maddesi; onay geldi.
- **⚠ İDDİA ESKİMİŞTİ:** CLAUDE.md `C exit 7, self exit 1` diyordu; **bugün
  ikisi de LINK-RED** (sessizden gürültülüye kaymış). Ayrıca belgelenenden
  FAZLA kusur vardı — **İNDEKS de `double`a dönüşüyordu**.
- **Kök 1 (belgelenen): erişimci soneği.** Eleman BAYT GENİŞLİĞİ zaten
  doğruydu (`eleman_byte`=8) → bellek güvenliği sorunu YOKTU, değer bozuktu.
  **Runtime değişikliği GEREKMEDİ:** `_tam64` 8 baytı int64 taşır, BİTLER
  korunur; derleyici `bitcast double↔i64` (kesirli32'de `_tam` + i32) yapar.
- **Kök 2 (belgelenmemişti): İNDEKS tip sızıntısı.** `dizi_deger_arg = 1`
  hem `dizi_ekle(d,e)` (arg1=ELEMAN) hem `dizi_al(d,i)` (arg1=**İNDEKS**)
  için kullanılıyordu. Tamsayıda MASKELİ (`int_donustur` çevirir), kesirlide
  indeks `fadd double 0.0, 0.0` doğuyor → `i32 %<double>`. `dizi_al` için
  `dizi_deger_arg = -1`.
- **DÖRT AYRI KOD NOKTASI**, biri diğerini onarmıyor (ölçüldü: yalnız
  yerleşiği onarınca `xs[0]+xs[1]`=**44**, `için`=**0**): `dizi_al` yerleşiği ·
  `xs[i]` indeksleme · `için x: xs` · annotasyonlu literal dizi.
- **🔴 KENDİ ONARIMIM SELF-HOST'TA KUSURU DAHA KÖTÜ SINIFA TAŞIYORDU.**
  Self-host eleman tipini DİZİDEN değil DEĞERDEN alıyor (`et = p.son_tip`);
  `Dizi<kesirli32>` + `dizi_ekle(xs, 20.0)` → literal `double` → taşıyıcı
  `i64` → **4 baytlık gözeye 8 BAYT = HEAP TAŞMASI**. Öncesinde yalnız DEĞER
  bozuktu. Dizinin eleman tipi OTORİTER yapıldı (+ gerekirse `fptrunc`).
  **Yalnızca `kesirli32` şeklini AYRICA ölçtüğüm için yakalandı.**
- **Yan kazanç:** aynı kural tamsayıya uzatılınca **önceden var olan** bir
  self-host kusuru kapandı — `Dizi<tam64>`de literal i32 doğup `_tam`
  (4 bayt) yazılıyor, `_tam64` (8 bayt) okunuyordu. **Ölçüldü: bu
  değişikliklerden ÖNCE de self 20 / C 42** → benim regresyonum DEĞİL.
- Fikstür `cg_kesirli_dizi.kem` → C=42, SELF=42 (altı ekseni birden ölçer).
  **Sabotaj S38** (C taşıyıcı) → LINK-RED · **S39** (self dizi-otoriter) → 41.
- Kapılar: `codegen_diff` 146/146 · `yapi_diff` 122/122 · `checker_diff`
  150/150 · `llvm_test` 286/286 · `snapshot_test` 50/50.
- **DERS: bir onarım kusuru DAHA KÖTÜ bir sınıfa taşıyabilir.** Kesirli64 tek
  başına sınansaydı yeşil görünüp gönderilecekti.

### 🎯 D-450 (KARAR 3): `yazdir_hata` stderr yerleşiği — D-424'ün borcu kapandı
`kdl_hata_yazdir` runtime'da ZATEN vardı ama **hiçbir derleyicide yerleşik
olarak açılmamıştı** → yazılacak yeni runtime mantığı yoktu, yalnız bağlantı.
- **Ad `yazdir_hata` (ters değil):** self-host eşlemesi ÖNEK tabanlı
  (`yazdir_` → `kdl_yazdir_`) → yeni kural gerekmez; ayrıca `hata` bir
  ANAHTAR KELİME, ona bitişik ad seçmek gereksiz risk. Runtime'a
  `kdl_yazdir_hata` eklendi (`kdl_yazdir_metin` aynası, akış stderr).
- **stdout'a yazmak OLMAZ:** IR stdout'a, tanı stderr'e gitmeli; aynı akış
  `--llvm | clang` boru hattını bozar. Bu invaryant **her codegen kapısı
  tarafından örtük korunuyor** (tanı stdout'a gitse IR bozulur, clang patlar).
- **D-424'ün borcu KAPANDI.** O kodun yorumu "stderr yazıcısı yok (… Mehmet'in
  kararı)" diyordu. Artık:
  `C: exit=1, stdout 0 bayt, stderr "hata[T002]…"` ·
  `SELF: exit=1, stdout 0 bayt, stderr "T002 1 30"`.
  Öncesinde self-host **hiçbir şey basmıyordu**. Biçim farkı kasıtlı: self-host
  `--check` ile aynı kompakt biçimi kullanır.
- Fikstür `cg_yazdir_hata.kem` → C=42, SELF=42; akışlar ikisinde de ayrı.
  **Sabotaj S37** (C eşlemesini `kdl_yazdir_metin`e çevir) → stderr BOŞALDI,
  stdout kirlendi ✅
- Kapılar: `codegen_diff` 146/146 · `yapi_diff` 122/122 · `checker_diff`
  150/150 · `ct_bariyer` 13/13 · `modul_codegen` 21/21.
- **⚠ CRLF ÇAPASI SESSİZCE ISKALIYOR:** `tip_kontrol.c` yaması uygulanmadı
  (dosya CRLF, çapa `\n`); `str.replace` eşleşme bulamayınca SESSİZCE hiçbir
  şey yapar. `grep` ile doğrulanmasa eksik yerleşikle devam edecektim.
  **Yamadan sonra sayıyı doğrula** — bu oturumda 4. tekrar.

### 🎯 D-449 (KARAR 1+2): `metin` üzerinde `==` artık İÇERİK karşılaştırıyor
Mehmet'in kararı: iki seçenekten **(a) içerik karşılaştırması**. Belirleyici
kanıt ölçümdü — `stdlib/dizi.kem`in generic aramaları (`icerir<T>`, `bul<T>`,
`indeks_bul<T>`, `say<T>`, `benzersiz<T>`) `x == hedef` üzerine kurulu, yani
**`Dizi<metin>` araması sessizce BOZUKTU**. Seçenek (b) (`==`i reddet) o
şekilleri kullanıcının kaçış yolu olmadan YAZILAMAZ hâle getirirdi
(constraint sistemi yok → `T` için özel dal yazılamıyor).
- **Mekanizma `isaretsiz`in birebir aynası** (IR'ın sildiği bir KEMGU tip
  özelliği için yan kanal): C'de `IfadeSonuc.metin_mi` + `LlvmIsim.metin_mi` +
  `IslevKayit.donus_metin` + `TipSubst.metin_mi`; self-host'ta `ifade_metin`
  yüklemi (`ifade_isz` aynası) + `cg_amet`/`cg_aemet`/`fn_rmet`/`mono_smet`.
- **DÖRT yol da gerekliydi** (fikstür dördünü birden ölçer): yerel · parametre ·
  **generic çıplak-T ikamesi** · **`için` döngü değişkeni** (`Dizi<T>` eleman
  metin-liği). İlk üçü çalışırken `dizi.kem` hâlâ bozuktu — döngü değişkeni
  eksikti.
- **⚠ YAMAYI YANLIŞ YERE KOYDUM** (bu oturumda 2. kez): iki parametre-kayıt
  yolu var; lifted-lambda olanını yamamıştım. Tahminle değil **enstrümantasyonla**
  bulundu — "DBG param" hiç basılmayınca yer yanlış demekti.
- **⚠ SIFIR UYARI:** yeni alan 81 `IfadeSonuc` başlatıcısını eksik bıraktı
  (`-Wmissing-field-initializers`). Derleyicinin verdiği satır numaralarıyla
  yamandı; 4'ü dizgi içinde virgül taşıdığı için (`"{ ptr, ptr }"`) koruma
  tarafından atlandı ve elle düzeltildi — **koruma bozulmayı önledi.**

### 🎯 D-449 (KARAR 2 devamı): dosya handle'ı — `dosya_gecerli` / `dosya_gecersiz`
Madde 1, madde 2'yi **zorunlu** kıldı: `==` içerik karşılaştırınca
`handle_gecerli_mi` tamamen çöktü (`metin_esit(FILE*, "")` işaretçiyi dizgi
gibi okuyor). Yani opak handle `metin` kaldıkça geçerlilik sınaması
YAZILAMAZ — D-443'te "tip tasarımı sorunu" diye kaydedilen şeyin kanıtı.
- İki yeni yerleşik: `dosya_gecersiz() -> metin` (**NULL** sentinel) ve
  `dosya_gecerli(h) -> mantıksal` (runtime null-sorgusu).
- **Geçersizliğin İKİ temsili vardı** (`""` sentinel + `dosya_ac`'ın NULL'ı) ve
  her kod yolu yalnız birini yakalıyordu. Tek temsile (NULL) indirgendi.
- **D-443'ün bilinen-yanlış davranışı KAPANDI:** `ac("yok.txt")` artık `tamam`
  değil **`hata`** dönüyor. D-441/D-443 deseniyle sabitlenen test tam da
  tasarlandığı gibi kırmızıya dönüp bunu bildirdi — sonra gerçeğe göre
  güncellendi.
- **Kalan (bilinçli):** `kapat("")` gibi rastgele bir `metin`in handle olmadığı
  ANLAŞILAMAZ (boş dizgi geçerli bir işaretçidir). Bu, `tekkez<Dosya>`nın
  (madde 4) çözeceği şeydir; testte açıkça belgelendi.

### Doğrulama
Fikstür `test/cg_korpus/cg_metin_esitlik.kem` → **C=42, SELF=42**.
**Sabotaj S35** (C dalı) ve **S36** (self dalı) → ikisi de **exit 106**.
Kapılar: `codegen_diff` **143/143** · `checker_diff` **150/150** ·
`yapi_diff` **120/120** · `stdlib_check` 8 modül · `kripto_kosum` 3/3 ·
`llvm_test` 286/286 · `snapshot_test` 50/50.

### ✅ D-448: oracle değişiklikleri QEMU'da doğrulandı (D-442/D-446 sonrası)
D-442 ve D-446 `src/llvm.c`ye dokundu ve **hiçbir host kapısı KEMGU-OS'un
gerçekten BOOT ettiğini ölçmüyor** (OS hedefleri QEMU ister, `test_tumu`da
yoklar — D-447'de bunun MEŞRU olduğu saptandı). Oracle'a dokunduktan sonra
bunu elle doğrulamak şart:
- `calistir_qemu_smoke` → EXIT 0, seri çıktı doğru (`Merhaba KEMGU`, `42`).
- `calistir_kem_os_arm` → **EXIT 0, 24 faz** (MMU FAULT/ÇEVİRİ · TRAP · TIMER ·
  PREEMPT · EL0 SYSCALL · İZOLASYON · UART RX · FS SYSCALL · SHELL · SPAWN ·
  ADRES ALANI · ELF YÜKLE · W^X · DTB · DİSK/FS RW · NET/ARP · PING CANLI).
- **Sonuç: oracle onarımları OS'u kırmadı.** ~140 QEMU hedefinin tamamını
  `test_tumu`ya bağlamak koşum süresini saatlere çıkarır — bu bir **derleme
  politikası** kararıdır (Mehmet). Ama **oracle'a (`src/llvm.c`) dokunan her
  değişiklikten sonra en az bu ikisi elle koşulmalı.**

### 🎯 D-447: kapı envanteri denetimi — 5 kapı daha takıma bağlandı
D-446'nın dersini ("kapı envanterini periyodik olarak `test_tumu`nun
çağırdıklarıyla karşılaştır") AYNI TURDA uyguladım.
- **Ölçüm:** 204 `calistir_*` hedefi tanımlı, `test_tumu` 54'ünü çağırıyor.
  Kalan 150'nin çoğu `_arm`/`_x86`/`_bare_metal`/QEMU — host'ta koşamaz,
  dışarıda olmaları MEŞRU. Geriye **9 host-koşabilir aday**; 2'si belgeli
  biçimde kapı değil (`*_bootstrap` oran raporlar), 1'i geçişli koşuyor.
- **Bağlanan 5:** `parser_diff` 13/13 · `lexer_diff` 22/22 ·
  `asan_matris` 12/12 · `arm64_test` · `asan_denetim` 137 PASS.
  **`parser_diff` CLAUDE.md'nin güncel 11-kapı listesinde YAZILIYDI ama hiçbir
  toplu koşumda ölçülmüyordu** — belgede "kapı" yazması koştuğu anlamına
  gelmiyor.
- **`asan_denetim` KIRMIZIYDI ama kusur değil:** `kem_mmio_ham`/`kem_pointer`
  host'ta eşlenmemiş MMIO (`0x0a000000`) okuyor. D-395'te ölçülmüş meşru sınıf
  ("C DE segfault ediyor, parite doğru"). ALLOWLIST'e gerekçesiyle eklendi →
  PASS=137 FAIL=0. Muafiyet kabul edilebilirliği GENİŞLETMİYOR (D-421): "bu
  dosya host'ta koşamaz" diyor, "bu program geçerli" demiyor.
- **Sabotaj S34** (`selfhost/parser.kem`: `"TANIMLAYICI"` → `"TANIMLAYICI_S34"`)
  → `parser_diff` **13/13 → 0/13**, Error 1 ✅
- `test_tumu` artık **59 kapı** çağırıyor.

### 🔴 D-446: `sabitsüre<T>` İMZASIZLIĞI KAYBEDİYORDU — SHA-256/ChaCha20 SESSİZCE YANLIŞ
`test_tumu`nun çağırdığı kapıları saydım: **`calistir_kripto_kosum` YOK** —
tanımlı, `.PHONY`de kayıtlı, ama hiçbir hedeften çağrılmıyor. Elle koşturdum:
**KIRMIZI** (ChaCha20 QR ve SHA-256("abc") ikisi de).
- **Önce "ben mi kırdım?" diye ölçtüm:** D-438 tabanını ayrı worktree'ye
  çıkarıp koşturdum → birebir aynı iki hata, kusur ÖNCEDEN VARDI.
- **Kök — harness'ın kendi hipotezi doğru çıktı.** Bundle IR'ında
  **15 `ashr` / 2 `lshr`**. Ayrımcı probe:
  `dtam32` param/yerel → **lshr ✓**, `sabitsüre<dtam32>` param/yerel → **ashr ✗**.
  Kusur TAM OLARAK sarmalayıcıda: `ast_tip_to_ir` `sabitsüre<T>`yi ZATEN
  açıyordu (runtime'da T, zero-overhead), **`ast_tip_isaretsiz_mi` AÇMIYORDU**
  → `TIP_BASIT` olmadığı için 0 = imzalı. `stdlib/kripto` tamamen
  `sabitsüre<dtamN>` üzerine kurulu → tüm rotasyonlar bozuktu.
- **Onarım iki derleyicide de tek satırlık soyma:** C `ast_tip_isaretsiz_mi`,
  self-host `ll_isz` (`TIP_SABITSURE` → çocuk). Self'te de AYNI boşluk vardı;
  4/4 şekil artık iki tarafta `lshr`.
- `calistir_kripto_kosum`: **3/3 vektör GEÇTİ**; kapı **`test_tumu`ya BAĞLANDI**.
  **Sabotaj S33** → yeniden KIRMIZI, Error 1.
- **DERS: VAR OLAN AMA ÇAĞRILMAYAN KAPI, OLMAYAN KAPIDAN DAHA TEHLİKELİDİR** —
  varlığı "bu alan ölçülüyor" yanılsaması yaratır. D-445'in
  "yazmak/bağlamak/koşturmak üç ayrı iştir" dersinin **güvenlik** sonucu.
  **Kapı envanterini periyodik olarak `test_tumu`nun çağırdıklarıyla karşılaştır.**

### 🟡 D-445: bağlanan testler KOŞMUYORDU — kapı sabit exit 0 varsayıyordu
D-441'de `dizi` main'ini 62→99, `matematik`inkini 34→47 teste bağladım ama
**hiçbir kapı onları çalıştırmıyordu**: davranış döngüsü çıkışın **0** olmasını
bekliyor, bu ikisi BAŞARIDA **42** döner (kendi sözleşmeleri) → **146 test
bağlanmış ama koşulmuyordu.**
- Kusur "eksik satır" değil **SABİT VARSAYIM**. Döngü girdisi artık
  `modul:beklenen_cikis`; beklenen çıkış AÇIK yazılır. Kapı 6 → **8 modül**.
- `dizi`/`matematik` başarıda hiçbir şey BASMAZ → çıktıya bakarak koştuklarını
  anlayamazsın. **Sabotaj S32** (`dizi:42`→`dizi:41`) →
  `FAIL(kosum): dizi - beklenen cikis 41, gelen 42`, MAKE EXIT 2.
- **DERS: bir testi YAZMAK, BAĞLAMAK ve KOŞTURMAK üç ayrı iştir.** İkisini
  yapıp üçüncüsünü atlamıştım ve dört artım boyunca fark etmedim — kapı
  yeşildi ve o modüller çıktıya hiçbir şey yazmıyordu.

### 🔴 D-444: `make | tail` ÇIKIŞ KODUNU MASKELİYORDU + D-443'ün E3 muafiyeti
- **ÖLÇÜM ARACI KUSURU:** tam takımı `mingw32-make test_tumu 2>&1 | tail -N`
  ile koşturuyordum; boru hattının çıkış kodu **`tail`in**dir → `make` KIRMIZI
  iken bildirim **"exit code 0"** dedi. O koşumda takım gerçekten başarısızdı
  (`check_genis` 127/128) ve yalnız son satırları okuduğum için gördüm.
  Önceki koşumlar gerçekten yeşildi ("Tum testler gecti!" yazıyordu) ama bu
  şansa dayanıyordu. **KURAL: `make`i boruya bağlarken `${PIPESTATUS[0]}`
  yazdır.** "Kapı sessiz düşmesin" ilkesini ÖLÇÜM ARACINA uygulamamıştım.
- **Sapma yeni kusur DEĞİL:** D-443'ün gerçek I/O gidiş-dönüşü
  `eşleş oku_metin(...)` kolları getirdi; `check_genis` dosyaları TEK BAŞINA
  denetler ve skrutini tanımsızken **C desen bağlamalarını hiç kurmaz**
  (`v`/`e` → T002), self bağlayıp susar (C 46 / self 36; fark = o 10 kaskad
  satırı). Harness'ta **zaten tanımlı E3 sınıfı** — listedeki diğerleri de
  (`test_metin`/`test_sonuc`/`test_json`) modülle birleştirilmek üzere yazılmış
  dosyalar. `test_dosya` eklendi → **127/127 (9 muaf)**.
  Soundness sorunu yok (ikisi de reddediyor); birleştirilmiş hâli
  `stdlib_check`te temiz. **Kapatılabilir borç:** self-host `eşleş`te skrutini
  çözülemezse desen bağlamalarını kaydetmemeli — ayrı ve riskli hata-yolu işi.

### 🔴 D-443: `dosya` kör noktası + D-440'IN SOKTUĞU REGRESYON + zayıf kapı
Kör-nokta envanterinin son modülü: `main` 27 testten **birini** çağırıyordu,
başlık yorumu da eskimişti ("stub, runtime primitif yok" — oysa Madde G ile
eklendi) → **gerçek dosya I/O'sunu hiçbir şey doğrulamıyordu.**
- **🔴 D-440'ta BEN REGRESYON SOKMUŞUM.** `handle_gecerli_mi`ı
  `h != ""` → `metin_uzunluk(h) > 0` yaptım. `h` DİZGİ DEĞİL:
  `kdl_dosya_ac` **`void*` (`FILE*`)** döner, KEMGU'da `metin` tiplenir.
  Ölçüm: `yaz_metin`→"dosya acilamadi" (dosya YARATILDIĞI hâlde), okuma boş,
  `sil`→"silinemedi" → **tüm yazma yolu koptu.** Oradaki `h != ""` içerik
  değil, `gecersiz_handle()` sentinel'iyle ADRES karşılaştırmasıydı =
  TAŞIYICI. Geri alındı. **Hiçbir kapı görmedi** (`dosya` döngüde yoktu;
  çağrılan tek test gerçek bir dizgi geçiyordu).
  **DERS: bir idiomu "yanlış görünüyor" diye düzeltmeden önce o tipin
  GERÇEKTEN ne taşıdığını ölç.** Aynı sözdizimi, iki farklı anlam.
- ~~**⚠ ALTTAKİ ASIL KUSUR (dil yüzeyi — Mehmet'e):** `dosya_ac` başarısızlıkta
  **NULL** döner, `""` değil → yüklem **açılamayan dosyayı GEÇERLİ sayar**
  (`ac("yok.txt")` → `tamam`). Kök: opak handle `metin` olarak tipleniyor.~~
  ✓ **KAPANDI:** null-sorgu yerleşiği **D-449** (`dosya_gecerli`/`dosya_gecersiz`
  → `ac("yok.txt")` artık `hata` döner), ayrı handle tipi **D-452**
  (`yapı tekkez Dosya`). Sabitlenen test tasarlandığı gibi kırmızıya döndü.
- **Yapılan:** `main` **1 → 20**; handle/mod · boş-yol korumaları (D-440'ın
  davranışı artık kapıda) · var-olmayan-dosya hataları · **GERÇEK GİDİŞ-DÖNÜŞ**
  (yaz→oku→UTF-8 doğrula→sil), yarattığı dosyayı **kendisi temizler**.
  Kapı döngüsü **6 modül**.
- **🔴 ZAYIF KAPI SABOTAJLA BULUNDU:** ilk sürüm yalnız "hata mı?" bakıyordu;
  **S30 SESSİZ KALDI** (çağrı yine hata döndü ama BAŞKA hata). `grep`
  uygulandığını doğruladı → kapı gerçekten zayıftı. Yardımcılar **beklenen
  mesajı** da denetler oldu; **S31** → `beklenen bos yol, gelen silinemedi`,
  make Error 1. Mesajlar **kaynaktan** okundu (gözlemden değil — `kopyala`
  tahminim yanlıştı). **Bir kuralın HANGİ YOLLA ateşlendiğini ölçmeyen kapı,
  o kuralı ölçmüyordur.**

### 🔴 D-442: annotasyonsuz heap-dizi bağlaması — sessiz yanlış cevap + SINIR KONTROLÜ ATLANMASI
D-441'in taraması `dizi`ye geldi: 37 test çağrılmıyordu; çağrılınca **altısı
çöp (işaretçi) değer** döndürdü. Ortak desen `değişken r = f();` + `r[0]`.
```
(A) değişken a = yap(); a[0]        → ÇÖP     (B) annotasyonlu → 42 ✓
(C) yap()[0] → 42 ✓                 (D) dizi_al(a,0) → 42 ✓  ← TESADÜF
IR(A): getelementptr i32, ptr  → KdlDizi BAŞLIĞI veri gibi okunuyor
```
- **🔴 BELLEK GÜVENLİĞİ:** heap yolu runtime sınır-kontrollüdür, ham GEP
  değil. `a[1000]` (1 elemanlı dizide): **C exit 0, sessizce okudu**;
  SELF `PANIK: dizi sınır ihlali`. "Buffer overflow imkânsız" invaryantının
  doğrudan ihlaliydi. **Self-host DOĞRU, C oracle YANLIŞTI** — paritenin
  ters yönü.
- **⚠ ÖLÇÜM İKİ KEZ YANLIŞ TEŞHİSTEN KURTARDI:** (1) "`dizi_al` çözüyor"
  sandım — hayır, `dizi_eleman_beklenen > beklenen > i32` sırası yüzünden
  **çağrı bağlamı** (`yazdir_metin` → "ptr") doğru cevabı veriyordu;
  (2) `heap_dizi_eleman_ast`in TANIMLAYICI dalı da aynı kurulmayan bayrağa
  bağlıymış. **Tek kök:** annotasyonsuz bağlama ne eleman tipini ne heap-liği
  kaydeder.
- **Onarım KÖKTE** (tüketicide değil) → INDEKS · `dizi_al`/`dizi_yaz` · `için`
  hepsi aynı bilgiyi görür (D-407). C: annotasyonsuz dalda
  `heap_dizi_eleman_ast` ile `eleman_llvm_tip`/`eleman_tip_ast`/
  `dinamik_dizi_mi`. Self: `fn_ret` "ptr" tutup elemanı sildiği için yeni
  paralel tablo **`fn_relem`** + çağrıda `son_elem` yayılımı (self'in belirtisi
  farklıydı: `add ptr` = LLVM-RED, sessiz değil).
- Fikstür `cg_annotasyonsuz_heap_dizi.kem` → C=42, SELF=42.
  **Sabotaj 2/2** (S28 C → fikstür 1 + OOB yeniden sessiz; S29 self → LLVM-RED).
  Kapılar: `codegen_diff` **143/143** · `yapi_diff` **120/120** ·
  `checker_diff` 150/150. `dizi` main **62 → 99**.
- **DERS: koşmayan testler bir BELLEK-GÜVENLİĞİ açığını saklıyordu.** Kusur
  `--check`ten, üç parite kapısından ve tam takımdan görünmüyordu — çünkü onu
  tetikleyen şekil hiçbir korpusta YOKTU.

### 🟡 D-441: koşmayan testlerin 2. partisi + `kuvvet(x,0)` sınırı görünür kılındı
D-440'ın kör-nokta taraması sürdürüldü. Kurtarılan: **opsiyonel 1/29 → 29**,
**karsilastir 1/20 → 20**, **sayisal 1/20 → 20**, **matematik 34/81 → 47**.
Kapının davranış döngüsü: `json metin` → `+ opsiyonel karsilastir sayisal`.
Beklentiler uydurulmadı (kaynak yorumları / yardımcı gövdeleri / uygulamadan
ölçüm). Bu üç modülde **kusur çıkmadı** ama artık 69 test gerçekten koşuyor.
- **🔴 KAPIDA SESSİZ ATLAMA VARDI:** döngü `[ -f "$f" ] || continue` idi ve
  yalnız `stdlib/` altına bakıyordu; `karsilastir`/`sayisal` **`stdlib/temel/`**
  altında → olduğu gibi eklesem kapı onları **sessizce atlar, yeşil kalır,
  hiçbir şey ölçmezdi**. Yol çözümü + eksik dosya artık **sert hata**.
  D-395'in dersi bu kez kendi eklediğim satırda karşıma çıktı.
- **🔴 `kuvvet(5,0)` → 5 (doğrusu 1).** `matematik`in çağrılmayan 47 testi
  matematiksel gerçeğe karşı denetlendi: 46'sı doğru, biri değil. Kök bir
  kusur DEĞİL, **ölçülmüş dil sınırı**: generic `T` içinde `ver 1;` → **T020**
  (constraint sistemi yok). **İddiayı ölçtüm — eskimemiş** (D-401 mono'yu
  eklediği için bayat olma ihtimali gerçekti).
  **`x / x` cazip ama DAHA KÖTÜ:** tip kontrolünden geçer, `x=0`da sıfıra
  bölme = ÇÖKME; oysa 0⁰ geleneksel olarak 1. **Yanlış cevabı çökmeyle takas
  etmek iyileştirme değildir.** Davranış DEĞİŞTİRİLMEDİ; test mevcut değeri
  sabitler + nedenini yazar → sınır artık SESSİZ değil.
  **✓ KAPANDI (D-454):** `bir()` intrinsic'i eklendi; `kuvvet(x,0)` artık 1 döner.
  Seçenek (b) `kuvvet_tam` bilinçli olarak REDDEDİLDİ (kalıcı API borcu).
- **Sabotaj S27** (`obe`nin Öklid adımını boz) → kapı KIRMIZI ✅
- ~~**Kalan:** `dizi` 37 test; `dosya` 26 test ayrı muamele ister; exit-42
  sözleşmesi kapının exit-0 döngüsüne eklenemez.~~ ✓ **ÜÇÜ DE KAPANDI:**
  `dizi` **D-442** (99 test; bir BELLEK-GÜVENLİĞİ açığı buldu), `dosya`
  **D-443** (19 test, yarattığı dosyayı kendi temizler), exit-42 **D-445**.

### 🔴 D-440: `metin` üzerinde `==` İŞARETÇİ karşılaştırıyor + KOŞMAYAN TESTLER
D-439 bitince "hangi yüzey kapısız?" diye ölçtüm: `calistir_stdlib_check`
13 test dosyasını `--check`ten geçiriyor ama **davranışsal olarak yalnız
`json`u** koşuyordu. Kör nokta ölçüldü — `main`'den çağrılan test sayısı:
**metin 1/89** · opsiyonel 1/29 · dosya 1/27 · karsilastir 1/20 ·
sayisal 1/20 · dizi 62/99 · matematik 34/81. ~185 testin çoğu HİÇ KOŞMUYOR.
- **Sakladığı kusur (IR'dan ölçüldü):**
  `a == b` → `icmp eq ptr` · `metin_esit(a,b)` → `call @kdl_metin_esit`.
  `esit_mi` yalnızca `ver a == b;` idi. **Literaller tekilleştirildiği için
  literal↔literal TESADÜFEN doğru** → koşan tek test geçiyordu. Hesaplanmış
  metinde sessiz yanlış: `esit_mi(birlestir("","b"), "b")` → **yanlış**.
  `dosya.kem`'in **15 boş-yol koruması hiç ateşlenmiyordu** (ampirik).
- ~~**⚠ DİL SORUSU MEHMET'E — DEĞİŞTİRİLMEDİ.** `==`in `metin` anlamı dil
  yüzeyidir: (a) içerik karşılaştırsın, ya da (b) **REDDEDİLSİN**. Şimdiki hâl
  üçüncü ve en kötüsü: kabul et, sessizce yanlış cevap ver.~~
  ✓ **KARARLAŞTI — D-449: seçenek (a).** `==` metinde artık İÇERİK
  karşılaştırır (dört yol: yerel · parametre · generic çıplak-T · `için`
  döngü değişkeni). Seçenek (b) ölçümle elendi: `Dizi<metin>` aramalarını
  (`icerir`/`bul`/`say`) kullanıcıya kaçış yolu bırakmadan yazılamaz kılardı.
- **Yapılan (dil değişikliği YOK):** adı/belgesi içerik eşitliği vaat eden
  kütüphane yerleri `metin_esit`/`metin_uzunluk`a çevrildi (`esit_mi`,
  `farkli_mi`, `bos_mu`, `dolu_mu`, `handle_gecerli_mi` + dosya.kem 15 koruma).
  `test_metin.kem` main'i **1 → 57** test çağırıyor; kapının davranış
  döngüsüne `metin` eklendi. **Sabotaj S26** → kapı KIRMIZI ✅
- **⚠ BEKLENTİLER MEKANİK TÜRETİLEMEZ:** "`_hayir`→yanlış, diğeri→doğru"
  kuralım 9 başarısızlığın **4'ünde YANLIŞ ALARM** verdi — ad beklentiyi
  değil GİRDİYİ tarif ediyor (`test_harf_karisik` = `sadece_harf_mi("abc123")`
  → yanlış dönmeli). `/tmp`de ölçtüğüm için depoya yanlış beklenti girmedi.
  Ayrıca `test_kes_aralik_ters` yanlış modele dayanıyordu (`kes`in 3. argümanı
  UZUNLUK, aralık SONU değil) → iki doğru testle değiştirildi.
  `sadece_harf_mi("")` → yanlış DOĞRUDUR (uygulamada açık `L == 0` dalı).
- **DERS: KOŞMAYAN TEST, OLMAYAN TESTTEN DAHA KÖTÜDÜR** — yeşil `--check` +
  dolu test dosyası kapsam YANILSAMASI yaratır. Dosyanın kendi yorumu kusuru
  zaten biliyordu (yerel `metin_es` yardımcısıyla dolanmış); bilgi ORADAYDI
  ama hiçbir kapı zorlamıyordu.
- ~~**KALAN (aynı sınıf):** opsiyonel/dosya/karsilastir/sayisal main'leri hâlâ
  tek test çağırıyor; `dizi`/`matematik` exit-42 sözleşmesi kullandığı için
  kapının exit-0 döngüsüne eklenemez.~~ ✓ **KAPANDI.** opsiyonel 29 ·
  karsilastir 20 · sayisal 20 (**D-441**) · dosya 19 (**D-443**) ·
  dizi 99 (**D-442**) · matematik 47 (**D-441**) · metin 57 (D-440).
  exit-42 sözleşmesi **D-445**'te çözüldü: kapı döngüsü artık
  `modül:beklenen_çıkış` alıyor (9 modül).

### 🔴 D-439: ÇEŞİT PAYLOAD'INDAN AGREGAT-ELEMANLI DİZİ — self-host LLVM-RED
D-437/D-438 erişimcileri açılınca `stdlib/json.kem` **C'de geçiyor, self-host'ta
derlenmiyordu**. **ÜÇ AYRI KÖK**, her biri bir öncekini onarınca çıktı:
1. **Satır içi agregat by-value sayılmıyordu.** C çeşidi ADLANDIRIR
   (`%V = type {i8,i32}`) ve `'%'` kuralıyla yakalar; self satır içi `{i8, i32}`
   yayar → aynı eleman C'de by-value, self'te SKALER →
   `kdl_dizi_ekle_tam(i32 <agregat>)`. `dizi_eleman_yapi_mi` + `dizi_eleman_byte`
   `'{'` önekine genişletildi (boyut de bozuktu: sabit `"4"`).
2. **Payload tipi checker dizgisinde SİLİNİYORDU.** `cv_pt` = `tip_str` çıktısı;
   `bilinen_tip_mi` kullanıcı tipini saymaz → `Dizi<W>` → `"?"`. Bu
   muhafazakârlık **checker için DOĞRU** (D-378) → gevşetilmedi; codegen için
   yeni yan-kanal **`cv_ptn`** (payload tip DÜĞÜMÜ) + `ll_eleman_tip`.
   **DERS: aynı tabloyu iki tüketici farklı doğrulukta okuyorsa, birini
   diğerinin muhafazakârlığına mahkûm etme — ham gerçeği ayrı kanalda taşı.**
3. **Payload YUVASI ile REGISTER genişliği ayrışıyordu** (`Sayi(0-4207)`: yuva
   i64, register i32). D-426'nın SİMETRİĞİ → aynı `int_uydur`. **Ön koşul
   ölçüldü: D-439 ÖNCESİNDE de vardı.**
- **🔴 KENDİ ONARIMIM REGRESYON ÜRETTİ, kapı yakaladı** (`codegen_diff` 141→140).
  `'{'` genişletmesi yüklemi paylaşan **dizi-DIŞI** çağrı yerine sızdı: 5083 dalı
  kapanış içindir ve mono-çeşit bağlamını GERİ ALIR → `Kap<metin>.o` bağlamı
  düşüyor, yapıcı BASE `{i8,i32}` ile kuruluyor. Ayrı DAR yüklem
  (`dizi_eleman_yapi_dar_mi`) eklendi.
  **⚠ İLK SUÇLADIĞIM YER YANLIŞTI** — akıl yürütmeyle `int_uydur`u ve 4067'yi
  suçladım, **ampirik bisect üçünü de eledi.** Yüklem GÖVDESİNDEKİ soru ile
  ÇAĞRI YERİNDEKİ soru ayrı şeydir; paylaşılan yüklemi genişletmek tüm çağrı
  yerlerini birden değiştirir.
- Fikstür `test/cg_korpus/cg_cesit_dizi_agregat.kem` → C=42, SELF=42
  (agregat elemanlı dizi + **kendine referanslı** çeşit + i64 yuva + skaler dal).
  **Sabotaj 3/3** (S22/S23/S24) → hepsi LINK-RED ✅
- Kapılar: `codegen_diff` 141/141 · `yapi_diff` 118/118 · `checker_diff` 150/150.
- **DERS: dogfooding tip kontrolünün göremediğini görüyor.** `--check` bu
  programı baştan beri temiz geçiriyordu; üç kusur da yalnız IR üretimindeydi.

### 🔴 D-438: İÇ İÇE `dizi_al` eleman tipini KAYBEDİYORDU (SEGFAULT)
D-437'yi onardıktan sonra **komşu şekilleri ölçerek** bulundu (4 şekil denendi,
3'ü temiz). `Dizi<Dizi<metin>>` üzerinde `dizi_al(dizi_al(d,0),0)`:
dış çağrı `_ptr` (doğru), **iç çağrı `_tam`** → `metin_esit(i32, ptr)` → 139.
- **⚠ ÖNCE "bunu ben mi soktum?" diye ölçtüm:** aynı şekil **çeşitsiz de
  çöküyor** → D-437'nin yan etkisi DEĞİL, önceden var olan AYRI kusur.
  Ara değişkene alınınca çalışıyor → kusur yalnız **İSİMSİZ ARA DEĞERDE**.
- **Kök:** `dizi_al` yerleşiğinin arg0 dispatch'i yalnız `TANIMLAYICI`/`ERISIM`
  tanıyordu; `CAGRI`/`INDEKS` dalı YOKTU. `heap_dizi_eleman_ast`in `CAGRI`
  dalı da yalnız KULLANICI işlevlerini tanıyordu (`dizi_al` YERLEŞİK → NULL).
- **Onarım:** C'de özyineli `dizi_al` dalı + `else if (arg0)` ortak-makine kolu.
  Self-host'ta `son_elem` yalnız IR DİZGİSİ tuttuğu için (`Dizi<Dizi<metin>>`
  → "ptr", iç tip kayıp) yeni kanal **`cg_aelem2`** + `son_elem2` eklendi.
- Fikstür `test/cg_korpus/cg_ic_ice_dizi_metin.kem` → C=42, SELF=42.
  **Sabotaj S21** → 139 ✅
- **DERS: bir kusuru onardıktan sonra KOMŞU ŞEKİLLERİ ölç.** D-437 tek başına
  "kapandı" görünüyordu; dört ek şekil bir SEGFAULT daha açığa çıkardı.

### 🔴 D-437: DERLEYİCİ HATASI — çeşit payload'ından `Dizi<metin>` okumak SEGFAULT
Dogfooding (`stdlib/json.kem` erişimcileri) buldu. Erişimciler geri alındı.
```kemgu
çeşit V { Bos, Tek(Dizi<metin>) }
... eşleş V::Tek(d) { V::Tek(a) => { metin_esit(dizi_al(a,0),"a") ... } }   → exit 139
```
**Kök (IR'dan okundu):** dizi bir çeşit payload BAĞLAMASINDAN geldiğinde
ELEMAN TİPİ kayboluyor →
`call i32 @kdl_dizi_al_tam` (doğrusu `ptr @kdl_dizi_al_ptr`) →
`call i1 @kdl_metin_esit(i32 %18, ptr %19)` — **i32 işaretçi yerine**.
**LLVM `declare` uyuşmazlığını SESSİZCE kabul etti** (D-295'in tekrarı).
- **Kapsam ÖLÇÜLDÜ (7 şekil):** `Cift(tam32,tam32)` ✅ · `(Dizi<tam32>,tam32)` ✅ ·
  `(tam32,Dizi<tam32>)` ✅ · `(Dizi<tam32>,Dizi<tam32>)` ✅ ·
  **`Tek(Dizi<metin>)` + eleman OKU 🔴** · `(Dizi<metin>,Dizi<tam32>)` metin OKU 🔴 ·
  aynısı metin OKUMADAN ✅. Yani kusur **İŞARETÇİ-elemanlı diziyi ÇEŞİT
  PAYLOAD'ından okumakta**; `Dizi<metin>` PARAMETRE olarak sorunsuz.
- **Sınıfı `Dizi<kesirli64>` ile AYNI** (erişimci soneki yanlış) ama o yalnız
  YANLIŞ DEĞER veriyordu, bu **ÇÖKÜYOR**.
- **Onarım `src/llvm.c`te (oracle)** — tasarım kararı DEĞİL, düpedüz kusur;
  `Dizi<kesirli64>`den farkı oracle DAVRANIŞINI değiştirmek gerekmemesi,
  yalnız doğru erişimciyi seçmek. Ayrı ve sınırlı iş.
- **Engellediği:** `json_alan` ve kardeş erişimciler. `json_uzunluk`/
  `json_indeks` tek başlarına ÇALIŞIYOR (ölçüldü).

### 🔴 D-433: LSP TEK BELGE TUTUYORDU — ikinci dosya birinciyi EZİYORDU
`workspace/*`'a başlarken ölçtüm: sunucu `Belge belge;` (TEK yapı) tutuyordu.
`didOpen` her çağrıda `belge_set` ile ÜZERİNE yazıyordu → **gerçek editörde
ikinci dosya açılınca birincinin hover/definition/diagnostic'i bozulur.**
VS Code birden çok dosyayı açık tutar ve her isteği KENDİ `uri`siyle gönderir.
- **`workspace/*`'ı bunun ÜSTÜNE kurmak sahte olurdu:** tek-belge bir sunucuda
  "workspace symbol" adı yanıltıcıdır (yalnız açık dosyayı arar). Önce MODEL
  düzeltildi — özellik eklemeden önce altındaki varsayımı ölç.
- Onarım: `BelgeTablo` (URI→Belge, dinamik dizi, boşalan yuva geri kullanılır).
  Dispatch `textDocument/` önekini yakalayıp `istek_uri` ile çözer; `didOpen`
  yuva AÇAR, diğerleri yalnız ARAR (bilinmeyen uri → handler çağrılmaz).
  Handler imzaları DEĞİŞMEDİ (`Belge *`) → değişim yüzeyi dar kaldı.
- Test `test_cok_belge_izolasyon`: iki dosya aç, BİRİNCİYE `documentSymbol` sor
  → `alfa` gelmeli. Tek-belge modelde `beta` gelirdi. **Sabotaj S16**
  (`tablo_ac` daima ilk yuvayı döndürsün) → **23/23 → 22/23** ✅
- **⚠ SABOTAJ İLK DENEMEDE UYGULANMADI** (`perl` deseni tutmadı, `grep` 0) ve
  kapı YEŞİL kaldı — bu oturumda DÖRDÜNCÜ kez. Sayıyı doğrulamadan sonuca
  bakma; `perl -0pi` çok satırlı desenlerde sessizce eşleşmiyor, **Edit aracı
  kullan**.
- ~~**LLVM v4** (dizi param/return, dizi length, generic islev codegen)~~ ✓ **ZATEN YAPILMIŞ**
  (2026-07-17 ölçümü — bu madde ESKİMİŞTİ, sonraki işlerde D-085/D-088 vb. ile kapanmış ama
  roadmap güncellenmemiş). Ampirik doğrulama (derle+çalıştır+exit): dizi param `topla(xs:
  Dizi<tam32>)`→42 ✓, dizi dönüş `yap() -> Dizi<tam32>`→42 ✓, `dizi_boyut`→3 ✓, generic
  `kimlik<T>(x:T)->T` → 42 ✓, generic+metin → 5 ✓. **DERS:** roadmap maddelerini başlamadan
  ölç — eskimiş olabilir.
- **Stdlib network/JSON/regex** — D-435'te ölçüldü:
  - **JSON** ✓ **YAPILDI** — `stdlib/json.kem` (saf KEMGU): `JsonDeger` çeşit
    ADT + yazıcı (D-435) + özyineli-inişli ayrıştırıcı (D-436) + erişimciler
    (D-437). Testi `test/stdlib/test_json.kem`, `calistir_stdlib_check`
    kapısında derlenip ÇALIŞTIRILIR. **İki derleyicide de** `exit 0`.
    **Dogfooding beklendiği gibi ödedi: ÜÇ derleyici kusuru buldu** — D-437
    (çeşit payload'ından `Dizi<metin>` = SEGFAULT), D-438 (iç içe `dizi_al`
    eleman tipini kaybediyor = SEGFAULT), D-439 (agregat elemanlı dizi,
    self-host LLVM-RED, 3 kök). Üçü de `--check`ten TEMİZ geçiyordu.
    **KALAN sınırların İKİSİ DE KAPANDI:** `\uXXXX` ✓ **D-458** (`kod_metin` +
    `kod_gecerli`; vekil çiftleri saf KEMGU'da, U+0000 açıkça reddedilir) ·
    ondalıklı sayı ✓ **D-457** (`kesirli_metin`, kayıpsız + locale bağımsız).
    Ondalıkta geriye yalnız **kütüphane işi** kaldı (`Ondalik` varyantı +
    ayrıştırıcı + 8 `eşleş` kolu) — dil yüzeyi kararı DEĞİL.
  - **network** ✓ **YAPILDI — D-466.** `stdlib/ag.kem`, **yalnız TCP
    istemci**. İki güvenlik katmanı, ikisi de derleme zamanında: `yetki<Soket>`
    (yetkisiz kod ağa çıkamaz) + `yapı tekkez Baglanti` (kapatmayı unutmak
    **L001**, iki kez kapatmak **L002** → soket sızıntısı yapısal olarak
    imkânsız). Winsock **çalışma zamanında** yüklenir; bağlama yüzeyi hiç
    değişmedi. Gerçek gidiş-dönüş `calistir_ag_kosum` kapısında, **her iki
    derleyicide**. **KALAN (bilinçli, ayrı kampanya):** dinleyici · **TLS**
    (yarım TLS, TLS olmamasından tehlikelidir) · zaman aşımı · ikili veri
    (V1 `metin` döndüğü için gömülü NUL kırpılır).
  - **regex** ✓ **YAPILDI — D-461.** `stdlib/regex.kem`, saf KEMGU (yeni
    yerleşik GEREKMEDİ). **Thompson NFA (Pike VM), geri izleme DEĞİL** →
    ReDoS'a kapalı; doğrusallık ÖLÇÜLDÜ (girdi 8× büyürken süre sabit).
    Bedeli: geri-referans yok (bilinçli takas). Testi `stdlib_check`te.
    **YAKALAMA GRUPLARI D-463'te EKLENDİ** (`ara_gruplar`/`grup_metin`/
    `grup_sayisi`); doğrusallık yakalamayla birlikte yeniden ölçüldü (16×).
    ~~**KALAN (bilinçli):** `\D \W \S` · `{n,m}`~~ ✓ **D-472'de EKLENDİ.**
    `\D \W \S` YENİ MEKANİZMA İSTEMEDİ: sınıf makinesi `cls_ters` bayrağını
    ZATEN taşıyordu (`[^a-c]` onu kullanıyor) → maliyet tek parametre.
    `{n,m}` **AÇILIM** ile (n zorunlu + (m-n) opsiyonel kopya); Pike VM komut
    kümesi HİÇ DEĞİŞMEDİ → **ReDoS bağışıklığı korundu, yeniden ölçüldü**
    (`(a+){2,8}b`, girdi 20→320 = 16 kat, süre sabit). Açılım programı
    büyüttüğü için ÜST SINIR var (`RX_TEKRAR_AZAMI = 1000`) ve aşılınca
    **açık hata** verir — sessiz kırpma yok.
    **KALAN (bilinçli):** tembel niceleyici · geri-referans. İkisi de GERİ
    İZLEME gerektirir → ReDoS bağışıklığını çöpe atardı; eklenmeyecekler.
- ~~**Semaforlar / bariyerler** (Plan Karar F V2) — D-435: runtime primitifi
  **YOK** → yeni yerleşik = dil yüzeyi, Mehmet'in kararı.~~
  ✓ **YAPILDI — D-456.** `stdlib/semafor.kem` (**kapsamlı**: `semaforda(s,
  ||{..})`; ham `al`/`bırak` dışa verilmez) + `stdlib/bariyer.kem`
  (`bekle(b)` — **kapsam bilerek YOK**, eşlenecek çift olmadığı için süs
  olurdu). D-435'in "primitif yok" saptaması doğruydu; mevcut `kdl_kosul_*`
  `static` + `KdlKanal*` (kanala gömülü) → bağımsız runtime yazıldı.
- **`kanal` bare-metal (.kem)** — D-435: `runtime/*.kem` ve `kem_os.kem`'de
  `kanal_*` kullanımı YOK. ABI hazır ama test/kullanım yok.
- **Linear V2:** ~~L005 (koşullu tüketim tutarlılığı)~~ ✓ **D-311** — tüketim takibi
  akış-duyarsız SAYAÇTI; hem YANLIŞ REDDEDİYOR (`eğer p { kullan(t); } değilse { imha(t); }`
  = spec'in kanonik örneği → L002, yani koşullu imha İMKÂNSIZDI) hem YANLIŞ KABUL EDİYORDU
  (tek dallı tüketim sessizce geçiyordu = lineer sızıntı). Artık dal-duyarlı: anlık-görüntü
  → dal izolasyonu → birleştir (iki dal=1 tüketim / tek dal=L005). test_linear 57→61,
  kod-duyarlı kapı + sabotaj doğrulaması. **D-312:** aynı disiplin `eşleş` kollarına
  (N-kollu genelleme: hepsi tüketir=1 tüketim / karışık=L005) ve **döngülere** genişletildi
  (yeni L-LOOP kuralı: `iken`/`için` gövdesi DIŞ bir lineer bağlamayı tüketemez — 0 iterasyon
  = sızıntı, ≥2 = çift tüketim; gövde-içi tanımlar serbest). test_linear 57→67.
  ~~**Kalan:** lineer alanlı yapı (`yapı tekkez K { ... }`)~~ ✓ **D-313**: `yapı tekkez K`
  eklendi — yapının kendisi lineer (mevcut L001/L002/L-COND/L-LOOP makinesi otomatik
  işler; bayrak TİPTE tutulur), LR002 muafiyeti YALNIZ lineer yapıya, `imha` kabul eder
  `kullan` etmez, **kısmi taşıma YASAK** (lineer alanı dışarı okumak aynı kaynağı iki kez
  imha ederdi; lineer-olmayan alan serbest). test_linear 67→74. **D-314: SELF-HOST'a
  portlandı** — driver + referans checker.kem ikisi de (7/7 senaryo C↔self birebir,
  checker_diff 49/49, bootstrap FIXPOINT). Self-host'ta tip nesnesi yok → parser lineer
  yapı ADLARINI kaydeder (`ly_ad`), mevcut L001/L002 makinesi otomatik işler.
  ~~**Kalan (V2.1):** alan-bazlı taşıma~~ ✓ **D-315** (bağlama başına bit-maske; ikinci taşıma L002; kısmi taşınmış yapı TAŞINAMAZ, yalnız imha; geçici değer red). ✓ **D-316 self-host portu TAMAM** (11/11 birebir; iki sessiz parite kaybı ölçümle bulundu: `deg_lineer_mi` ERISIM dalı + `fn_plin` lineer-yapı parametresi). ✓ **D-317: L-COND/L-LOOP de self-host'ta** (8/8 birebir; anlık-görüntü YIĞINI — iç-içe eğer/eşleş için şart). **Lineer alt-sistemde parite borcu KALMADI.** ✓ **D-318: `eşleş` YAPI DESENİ eklendi** (yeni sözdizimi, Mehmet onaylı): `Yapi { alan1, alan2 } =>` — lineer yapıda desen yapıyı TÜKETİR, alanlar bağlanır; TÜM alanlar zorunlu (T012), bilinmeyen alan T009. C+self-host parite, --ast dump paritesi yan-kanalla korundu. İcat EDİLMEYEN: yeniden-adlandırma, rest-deseni `..`, iç-içe desen.
- **Linear stdlib** (Spec B.6) — ⚠ **KARIŞIK DURUM (D-431'de ölçüldü):**
  - `OTP_Anahtar` ✓ **YAPILMIŞ** — `stdlib/kripto/anahtar.kem`, 11 `tekkez` kullanımı.
  - `Dosya` ✓ **YAPILDI (D-452)** — `yapı tekkez Dosya`. `ac` artık
    `sonuç<Dosya, metin>` döner, `kapat(d: Dosya)` TÜKETİR. `kapat("")` → T001,
    çift kapatma → L002, kapatmayı unutmak → L001 (üçü de derleme zamanında).
  - `Kilit` ✓ **YAPILDI (D-455)** — `stdlib/kilit.kem`, KAPSAMLI API:
    `kilitle(k, || { ... })`. Ham `al`/`bırak` çifti BİLEREK dışa verilmez
    (unutulmuş-bırak = deadlock, çift-bırak = UB). **`Kilit` LİNEER DEĞİL** —
    kilit defalarca kullanılır; lineerlik ikinci `kilitle`yi engellerdi.
    ⚠ Öneri yazılırken "runtime primitifleri hazır" denmişti; ÖLÇÜM düzeltti:
    mevcut `kdl_kilit_*` `static` ve `KdlKanal*` alıyor (kanala gömülü) →
    bağımsız `KdlKilit` + 4 dışa-verilen işlev YAZILDI.
  - ~~`matris_carpim`~~ ✓ **D-397: SIMD `vektör<T,N>` eklendi.** **DÖRT ayrı
    kök**, her biri bir öncekini onarınca ortaya çıktı (tek ölçümle
    görülemezlerdi): (1) `ll_tip` → `"<N x T>"`; (2) `vektor_doldur`/`_eleman`/
    `_topla` yerleşikleri; (3) **`kesirli_ll_mi` vektörü tanımalı** — yoksa
    lane-wise `mul <4 x float>` = LLVM-red; (4) annotasyon `"<N x T>"` ise
    `beklenen_ll` bağlamı — yoksa `vektor_doldur` lane/eleman bilgisini
    kurtaramaz. Parser ve checker ZATEN hazırdı; eksik olan yalnız codegen'di.
    **`reduce.fadd` (0.0 başlangıçlı) ile `reduce.add` (operandsız) ARİTELERİ
    farklıdır** — karıştırmak LLVM-red verir; ikisi de oracle'dan ölçüldü,
    `<4 x i32>` varsayılanı dâhil uydurulmadı.
  - ~~`gorev_temel`~~ ✓ **D-396'da ONARILDI** (muafiyet 2→1, kapı 66/66).
    `görev_başlat` artık T'yi `son_ic` ile yayınlar (`lam_ret_tahmin`), `eşleş`
    skrutininin `son_ic`ini HEMEN yerelde yakalayıp desen payload'ına taşır.
    **Kritik ayrım:** kuyruğa yazılan `lam_ret="i64"` runtime TAŞIYICISIDIR,
    `görev<T>`nin T'si DEĞİL — ikisini karıştırmak kusurun kendisiydi. Ayrıca
    `i64_daralt`ın ptr dalı en baştan DOĞRUYDU; eksik olan yalnız TİP BİLGİSİYDİ
    (yanlış bileşeni suçlamamak için ölçüm şart).
- Kapatılan kökler: yerleşik IR ad eşlemesi (`yazdir`/`bellek_al`/`otp_*`) ·
  `dizi_olustur(N)` çağrı-formu · `eşleş &Çeşit` auto-deref · üst-düzey `sabit`
  referansı · çağrı argümanının param IR tipine genişletilmesi · **KARAKTER
  literali** (hepsi `'A'`→0 idi).
- **Bu köklerin ÜÇÜ sessiz yanlış cevap üretiyordu** (geçerli IR, çalışan
  program, yanlış değer) — derleme hatasından ağır sınıf.

### ⚠ AÇIK CEPHE: ÇAPRAZ-DOSYA MODÜL CODEGEN'İ (D-398, 2026-08-07)
Geniş kapı doyunca ölçüm `test/moduller/`e çevrildi. Orada self-host `--check`
**131/131 paritede** ama `--llvm` **18/18 DÜŞÜYOR**. İki AYRI iş çıktı:
- ~~**Ad-mangling**~~ ✓ **D-398**: `@a.f`/`@d.i.f`; define + nitelikli çağrı +
  **modül-içi çıplak çağrı (MODÜL-ÖNCE bağlama)**. Satır içi `modül` bloklarıyla
  dosya yüklemeden BAĞIMSIZ ölçülür; (A)'nın da ön koşuludur.
- **(A) KALAN — çapraz-dosya yükleme.** C `modulleri_yukle` modülü AYNI arena'da
  parse edip sentetik `DUGUM_MODUL` olarak AST'nin BAŞINA **splice** eder (tek
  ağaç). Self-host `modul_yukle` ayrı bir `Ayr`e parse edip **yalnız ADları
  hasat ediyor, AST'yi ATIYOR**; üstelik `--llvm` dispatch'i yükleyiciyi HİÇ
  çağırmıyor. Mekanizma `selfhost/checker.kem`'de de KOPYALI.
  - **✅ DE-RİSK EDİLDİ (ölçüldü, D-398 sonrası): AST kopyalama GEREKMİYOR.**
    İlk teşhis "ayrı `Ayr`den indeks yeniden-eşlemesi şart, AST kopyalama
    yardımcısı yok" idi — bu PAHALI yol. **Kaynak düzeyinde birleştirme
    ÇALIŞIYOR:** modül kaynağını `modül <ad> { ... }` ile sarıp giriş dosyasının
    önüne koymak + `kullan` satırını düşürmek → tek lex+parse, tek AST, yan-kanal
    kaybı YOK. Ampirik: `ana_mat` için self-host **exit 42 = C**. D-398'in
    mangling'i bu yolu doğrudan besliyor (`@mat.topla` kendiliğinden çıkıyor).
  - ✓ **D-399 UYGULADI (KISMİ): 0/18 → 7/18**, sıfır regresyon. Kaynak-splice
    yolu `--llvm` dispatch'ine bağlandı (iki geçiş: parse → `kullan` listesi →
    birleştir → yeniden parse). Modül yoksa çıktı BİREBİR korunur.
  - ✓ **D-400: 7/18 → 11/18. AD ÇÖZÜMÜ SINIFLARININ TAMAMI KAPANDI.**
    Alias + seçili import çağrı yerinde çözülüyor (`alias_coz`, `si_ad`/`si_yol`).
  - **⚠⚠ `\n` KEMGU DİZGİ LİTERALİNDE KAÇIŞ DEĞİLDİR** — `metin_uzunluk("a\nb")
    == 4`, hem C hem self-host (dil davranışı, parite kusuru değil). `codegen.kem`
    her yerde `yb(10)` kullanır. Kaynak metni ÜRETEN kod yazarken bunu unutma:
    D-399'un sarmalı `"{\n"` ile kuruluyordu → üretilen kaynağa düz `\`+`n` çöpü
    girdi ve **beni tamamen yanlış bir köke sürükledi** ("iç içe lex+parse belleği
    eziyor" — D-399'a öyle yazmıştım, YANLIŞTI).
    **DERS: "A çalışıyor, B çalışmıyor" bir MEKANİZMA teşhisi değildir.** İki
    hipotezi ölçüp çürüttükten sonra bile üçüncü yanlış hipoteze gittim; doğru
    hamle en baştan ARA DEĞERİ BASTIRMAKTI. `mat`in çalışması TESADÜFTÜ — çöpü
    izleyen `//` yorumu satırı yutuyordu.
### ✓ GENERIC İŞLEV MONOMORFİZASYONU EKLENDİ — D-401 (aşağıdaki not ARTIK TARİHÎ)
Kök TEK SATIRDI: `parse_islev_genel` `tip_param_kaydet`i çağırmıyordu → işlev
generic param adları yakalanıp atılıyordu. İkame makinesinin GERİ KALANI zaten
vardı. Artık `@kimlik$i64`/`@kimlik$double` yayılıyor; `kesirli64` generic
LLVM-RED'den çıkıp çalışıyor.
- **V1 SINIRI:** çıkarsama yalnız **ÇIPLAK `T` parametresinden**. `Dizi<T>` gibi
  iç içe konumlar ve **dönüş-tipi-güdümlü** çıkarsama YOK. Bu yüzden base gövde
  de yayılmaya DEVAM eder (C atlar; C'nin çıkarsaması tam). Base'i atlamayı
  denedim → **regresyon 11/18 → 8/18**; ölçümle yakalandı, geri alındı.
- `test/moduller` 11/18'de SABİT — kalan 7 dönüş-tipi-güdümlü çıkarsama ister.
### ⚠⚠ KEMGU DİZGİ LİTERALİNDE KAÇIŞ YOK — `\n` DE `\"` DE (D-400, D-409)
`metin_uzunluk("a\nb") == 4` **ve** `metin_uzunluk("a\"b") == 4` (ikisi de
ölçüldü, hem C hem self-host). Ham dizgi `r#"..."#` ifade konumunda P010 verir.
`codegen.kem` çıktı için `yb(10)`/`yb(34)` kullanır — ama tırnak/satırsonu
**DEĞER** olarak gerektiğinde bu yetmez. Çözüm (D-409): `metin_kes("\"", 1, 1)`
→ tek `"` (iki karakterlik dizginin ikincisi; bayt 34 olarak doğrulandı).
**Kaynak metni ÜRETEN kod yazarken bunu unutma** — D-400'de düz `\n` çöpü
üretip tamamen yanlış bir kök aramama yol açmıştı.

### ⚠ AÇIK KALANLAR (D-408'de ölçüldü, kayda geçti)
- **`test/snapshots` yüzeyi (D-409'da açıldı): 61/62.** ✓ `cesit_sonuc` (D-410),
  ✓ `bolge_al_grow` + `d1_generic_sonuc_ptr` (D-411), ✓ `ad_cozum_sapma`
  (D-412: GÖRELİ modül yolu — `modül m` içinden `ic::g` = `m.ic.g`; `fn_coz`un
  MODÜL-ÖNCE kuralı nitelikli yollara genişletildi, kayıt kontrolüyle).
  **Kalan 1:** `asm_round_trip` (satıriçi_asm sessizce düşüyor — planı yukarıda).
- **D-411 dersi — D-401'in "çıkarsanamazsa mono'yu İPTAL ET" kararı YANLIŞTI.**
  `hata_yap<T,E>(e: E) -> sonuç<T,E>`de `E` çıkarsanır, `T` çıkarsanmaz; iptal
  edilince çağrı base gövdeye (hepsi fallback) gider ve ANNOTASYONLA uyuşmaz.
  Doğrusu: çıkarsanamayan için fallback IR kullan → `hata_yap$i32$ptr`, C ile
  birebir. **Fallback yanlışsa hata GÜRÜLTÜLÜ kalır** (annotasyon uyuşmazlığı),
  sessiz yanlış cevaba dönüşmez.
- **⚠ "define ≠ call" diye aramak yanlış yere bakmak olabilir.** D-411'de define
  ve çağrı BİRBİRİYLE anlaşıyordu; uyuşmazlık ANNOTASYONLAYDI. Hata satırını oku.
- ✓ **D-416: `satıriçi_asm` EKLENDİ — `test/snapshots` 62/62, TAM PARİTE.**
  Parser 5 yeni yan-kanalla şablon/kısıt/çıktı-adı/bozulan yakalıyor
  (`--ast` paritesi korundu, `parser_diff` kanıtlıyor); codegen üç çıktı-aritesini
  de C ile birebir yayıyor. **İki tuzak ölçümle yakalandı:** `sim_lex` HAM SİMGE
  metnini verir (sınırlayıcılar soyulmalı) ve **KEMGU'da `\\` de kaçış değildir**
  — `"\\0D"` iki ters-bölü basar, tek ters-bölü için bayt 92 yazılmalı.
  **Bu, "KEMGU'da kaçış yok" dersinin ÜÇÜNCÜ tekrarı** (`\n` D-400, `\"` D-409).
- **(TARİHÎ) 🔴 `satıriçi_asm` SELF-HOST'TA SESSİZCE DÜŞÜYOR** (`asm_round_trip`:
  C=42, KEMGU=1). Parser `SATIRICI_ASM` düğümünü üretiyor, checker AS001 ile
  doğruluyor, **codegen'de DAL YOK** → tüm blok yok sayılıyor, çıktı değişkenleri
  başlangıç değerinde kalıyor. Link hatası yok, IR geçerli — **sessiz yanlış
  cevap**, üstelik `güvensiz` blokta çalışan kod için.
  **✅ HEDEF BİÇİM TAM ÖLÇÜLDÜ (D-415 sonrası; üç çıktı-arite'si de C'den
  okundu, hiçbiri varsayılmadı):**
  ```
  0 çıktı : call void asm sideeffect "<şablon>", "<bozulanlar>"()
  1 çıktı : call <T>  asm sideeffect "<şablon>", "=r,<bozulanlar>"()
            store <T> %r, ptr <slot>            ← STRUCT DEĞİL, düz dönüş
  2+ çıktı: call { T0, T1 } asm sideeffect "<şablon>", "=r,=r,r,~{cc}"(i32 %6)
            %e0 = extractvalue { T0, T1 } %r, 0  +  store T0 %e0, ptr <slot0>
            %e1 = extractvalue { T0, T1 } %r, 1  +  store T1 %e1, ptr <slot1>
  ```
  Kısıt SIRASI: **çıktılar → girdiler → bozulanlar** (`"=r,=r,r,~{cc}"`).
  Şablonda satır sonu **`\0D\0A`** olarak kaçışlanır (kaynak CRLF).
  Gereken yan-kanallar (`asm_node` ile paralel): şablon · çıktı kısıtları ·
  çıktı DEĞİŞKEN adları · girdi kısıtları · bozulanlar.

  **ONARIM PLANI (ölçüldü):** parser ŞU AN kritik veriyi ATIYOR —
  `parse_satirici_asm` yalnız `girdi` ifadelerini AST çocuğu yapıyor (C'nin
  `--ast` paritesi için) ve `mimari`yi yan-kanala koyuyor; **şablon, çıktı
  kısıtları, çıktı DEĞİŞKENLERİ ve bozulanlar tümüyle atılıyor**
  (`asm_kisit_atla` sadece tüketiyor). Düğüme alan eklemek `--ast` paritesini
  bozar → 4-5 yeni yan-kanal dizisi gerekir. C'nin hedef biçimi:
  `call { i32, i32 } asm sideeffect "<şablon>", "=r,=r,r,~{cc}"(i32 %6)` +
  `extractvalue` + çıktı işaretçilerine `store`.
- **⚠ ÇOK-KOLLU TESTTE EN AZ ÜÇ VARYANT KULLAN.** D-410'da "hep ilk kola git"
  hatası İKİ varyantla %50 olasılıkla doğru cevabı verir ve test **tesadüfen
  yeşil kalır**. Üç varyant + hepsinin AYRI dönüş değeri şart. (D-393'ün
  "3 parametre yetmez, en az 4" dersinin aynısı.)
- ✓ **D-417: `sabitsüre` codegen'i EKLENDİ + YENİ KAPI `calistir_ct_bariyer`.**
  `check_korpus` 31/32 → **32/32**.
  **🎯 ASIL DERS — DAVRANIŞSAL KAPILAR GÜVENLİK ÖZELLİĞİNİ GÖREMEZ.** Sabotaj
  S159 ile spekülasyon bariyerini sildim: `codegen_diff` **139/139 YEŞİL kaldı**,
  `lfence` sayısı 10 → 0 düştü. Bariyerin yokluğu link hatası vermez, IR geçerli
  kalır, program AYNI SONUCU üretir. **"Program doğru çalıştı" güvenlik
  özelliklerinde YETERSİZ KANITTIR** → yeni kapı davranışı değil YAPIYI
  (bariyer sayısını) C ile karşılaştırır; korpusu `grep` ile tarar ki yeni dosya
  kendiliğinden kapsansın.
  **⚠ D-408'DE KENDİ UYARIMI KENDİM İHLAL ETMİŞİM:** `sabitsüre_olustur`u
  "bariyeri düşürür, yarım yapmıyorum" diye ertelerken **aynı commit'te `ifşa`yı
  tam da o şekilde ekledim** (pass-through, bariyersiz). Bir riski yazmak, onu
  başka bir yerde işlememeyi garanti etmiyor.
- **(TARİHÎ) `sabitsüre` codegen'i — GÜVENLİK-DUYARLI, yarım yapılamaz.** C
  `sabitsüre_olustur`u pass-through yapar AMA yanında
  **`call void @llvm.x86.sse2.lfence()`** spekülasyon bariyeri yayar. Self-host'ta
  ikisi de yok. Naif pass-through link hatasını kapatır ve **bariyeri sessizce
  düşürür** → sabit-süre disiplininde sessiz güvenlik regresyonu. **Link hatası
  gürültülüdür, eksik bariyer değildir.** `test/check_korpus/tc19_02` açık.
### 🎯 D-425: T022 `güvensiz` koşulu + BL001/BL002 — kaçırma 6 → 3
- **⚠ ÖNCE KENDİ ÖLÇÜM HATAMI DÜZELTTİM.** D-420'de `deref_atama_disi` için
  "C=T022, self=G001 — YANLIŞ KOD" yazmıştım; **YANLIŞTI**. C İKİ tanı verir
  (`T022`+`G001`), self yalnız `G001` → **eksik tanı**. Hata özet aracımdaydı:
  `head -1` ile karşılaştırmıştım. **Çok satırlı tanı çıktısını `head -1` ile
  karşılaştırma** — teşhisi tümüyle yanlış yöne çevirir.
- **T022 deref-lvalue gevşetmesi KOŞULLUDUR** (self'te koşulsuzdu → C'nin
  reddettiği deref-write sessizce kabul ediliyordu). Üç şekil ölçüldü:
  `*p` güvensiz DIŞINDA → T022+G001 · İÇİNDE → OK · **`*r` (&değişken) → T022**.
  Kural işaretçi/referans ayrımına DEĞİL, `güvensiz` bağlamına bağlı — üçüncü
  şekli ölçmesem yanlış kuralı kodlayacaktım.
- **BL001/BL002:** `bölge_al` beklenen `*T` bağlamı ister, SESSİZ VARSAYILAN YOK.
  Altyapı D-423'ten BEDAVA geldi: `m4_*` registry'si çeşit yapıcısına ÖZEL
  değil, HER CAGRI'yı beklenen tip düğümüyle işaretler. (M004'ün ikame kısmı
  `tp_yad`/`tp_ad` ister — o `checker.kem`'de yok; BL001 istemediği için oraya
  da portlanabildi. Zincirdeki KONUM da hizalandı — D-407.)
- **Sabotaj 3/3.** S9 (BL001'in pozitif dalını sil) fazladan `BL001_39_37`
  üretti → **pozitif şekiller olmasa toptan sıkılaştırma kapıyı GEÇERDİ.**
- **Kalan 3 kaçırma** (T002 modül · T007 generic bound · T001 generic method)
  ayrı ve büyük kökler.

### ⛔ D-430 (NEGATİF SONUÇ): `cv_*` çapraz taşıma — YAZILDI, GERİ ALINDI
- `surucu_diff`in kalan 5 muafiyetinin kökünü "çeşit varyantları çapraz-dosya
  taşınmıyor" diye kaydetmiştim. **Yanlış kaydetmişim** — eksik tanılar tek tek
  izlenince: **T011** = C `Virtqueue`yi tanımıyor, self D-429 sayesinde TANIYOR
  (self C'den DAHA yetenekli; eşleşmek yetenek silmek olurdu) · **M001** =
  skrutini `sonuç<..>`, `çeşit` DEĞİL. İkisinin de `cv_*` ile ilgisi YOK.
- **AYRIMCI PROBE: C ÇAPRAZ-DOSYA KAPSAYICILIK YAPMIYOR.** `dışa çeşit Renk`
  + eksik varyantlı `eşleş` → C **OK**. Parite `cv_*` taşımasını GEREKTİRMİYOR.
  > Gelecekte "çapraz-dosya çeşit kapsayıcılığı" işine girişmeden önce bunu
  > oku — **C'nin kendisi yapmıyor**, parite hattında yapılacak bir iş DEĞİL.
- **Neden geri alındı:** kod semantik olarak doğru görünüyordu ama **hiçbir
  ölçüm onu ayırt edemedi.** Ayırt edilemeyen kod DOĞRULANMAMIŞ yüzeydir.
  "Doğru görünüyor" bu repoda yeterli değil.
- Yan bulgu (ÖNCEDEN VAR, ölçüldü): tek-segment `kullan` + private `çeşit` →
  self fazladan T011. Ölümcül değil (C zaten T002 ile reddediyor).

### 🎯 D-429: ÇAPRAZ-DOSYA İMZA + YAPI KAYDI TAŞIMA — "check paritesi SIĞ" kökü
- `modul_yukle` yalnız ADLARI hasat ediyordu; İMZALAR (arite, param tipleri,
  param **LİNEERLİĞİ**) ve YAPI kayıtları dosya-yerel kalıyordu → sahte CP005
  (`yetki<MMIO>` param lineerliği bilinmiyor) + sahte T002 (`Virtqueue` yok).
- **MANTIĞI İKİNCİ KEZ YAZMA:** `imza_kaydet`/`yapi_kaydet` `p`'den okur;
  çapraz sürüm tüm yardımcı zincirinin ikizini isterdi (D-407 tuzağı). Çözüm:
  `genel_topla`yı **modülün KENDİ `Ayr`inde** koştur, sonra düz paralel dizileri
  kopyala (`capraz_imza_tasi`, `fn_pbase`/`yapi_abase` yeniden tabanlanır).
- **⚠ DÜĞÜM İNDEKSLERİ TAŞINAMAZ** (`fn_ptn`, `alan_tn` → `-1`): düğüm `mp`'ye
  ait, `p`'de başka şeye denk gelir. Tüketiciler ÖNCEDEN ölçüldü —
  `t14_muaf_isaretle` `tn < 0`u muhafazakâr karşılar → **tanı KAÇIRIR, sahte
  tanı ÜRETMEZ.**
- **ÖLÇÜM COMMIT'TEN ÖNCE, TÜM REPO** (D-427'nin dersi uygulandı):
  **589 dosya · yanlış-pozitif 0 · fark 46** (34'ü lex/parse fikstürü).
  Öncesi/sonrası aynı yüzeyde: **YP 2 → 0, kaçırma 12 → 12** (artmadı).
  İki bağımsız ölçüm uyuştu.
- `surucu_diff` muafiyeti **7 → 5**; kalan 5 hepsi kaçırma yönünde
  (`çeşit` varyantları / bileşik tip temsili hâlâ dosya-yerel — `cv_*`).

### 🎯 D-428: YENİ KAPI `calistir_check_genis` — kapısız kalan 6 yüzey
- **126/126 (7 muaf).** `test/snapshots` (82 dosya!) · `test/ornekler/eski` ·
  `test/stdlib` · `test/asan_matris` · `test/crossfile` · `stdlib/` kökü.
  D-427'nin dersinin genellemesi: **elle taranan ölçüm eskir, kapı eskimez.**
- Muafiyet 3 sınıf (ölçüldü): **E1** C'nin KENDİ sınırlaması (modül-kapsamlı
  yapı alanı · generic bound'da method · tip-paramsız `uygula`) · **E2** PARSER
  katmanı (`tip Ad = HedefTip;` KEMGU'da YOK → C **P001**, self T002/T011) ·
  **E3** `eşleş` desen bağlamında kaçırma.
- Kapsam kararı: codegen/IR bu kapının işi DEĞİL — envanteri bölmemek için
  (D-427'de dönüş tipini `yapi_diff`e bırakmakla aynı gerekçe).
- **⚠ SABOTAJ HEDEFİNİ KORPUSUN GERÇEKTE ÖLÇTÜĞÜ KURALLARDAN SEÇ.** İlk
  sabotajım (S12, D-420 T014 ERISIM dalı) SESSİZ kaldı — ama `grep` uygulandığını
  doğruladı: kapı zayıf değil, **sabotaj yanlış seçilmişti** (bu yüzey
  `k.xs = []` şeklini içermiyor). Kapının tanı-kodu dağılımını saydım
  (`T002` 688 · `T011` 48 · **`T022` 2** …) ve T022'yi hedefledim:
  **126/126 → 124/126**. Bir kuralın var olması HER kapıda görünür olduğu
  anlamına gelmez.

### 🔴 D-427: D-424 SÜRÜCÜ DERLEMESİNİ KIRMIŞ + YENİ KAPI `calistir_surucu_diff`
- **19 dosya HİÇBİR kapının altında değildi** (`drivers/virtio` 10 +
  `tests/drivers/virtio` 9). D-424'ün tip kapısı eklenince self onları
  DERLEYEMEZ oldu: `status` C=11def/SELF=0, `virtio_mmio` 63/0, `virtqueue_bind`
  68/0. Hiçbir kapı görmedi.
- **⚠⚠ D-424'ün ön koşulunu 502 dosyada ölçüp "yanlış-pozitif SIFIR" demiştim;
  o listede `drivers/` YOKTU.** Kendi dersimi ("parite sayısı yalnız ölçülen
  yüzey kadar geniştir", D-420) KENDİ KAPIMA uygulamayı atladım.
  **ÖN KOŞUL ÖLÇÜMÜNÜN KAPSAMI DA AYRICA DOĞRULANMALI** — dersi yazmış olmak
  onu uygulamaya yetmiyor (bu oturumda ikinci kez).
- **Kök:** C `src/ana.c` `kullan_yeni_bicim` = `segment<=1 ∨ seçili ∨ alias`.
  Hiçbiri değilse (`kullan a::b::c;`) **LEGACY DÜZLEŞTİRME**: tüm üst düzey
  adlar görünür, `dışa`/`genel` gerekmez, T041 UYGULANMAZ. Self ayırmıyordu →
  düz `sabit`ler özel sayılıp sahte T002. **Yardımcı (`kullan_yeni_bicim_mi`)
  ZATEN VARDI**, `modul_yukle` çağırmıyordu.
- **Kalan 2 muafiyet (CP005) ÖLÇÜLDÜ:** minimal şekillerde kural C ile BİREBİR;
  gerçek dosyada tüketen işlev BAŞKA DOSYADA ve self çapraz-dosya **imzalarını**
  taşımıyor (yalnız adları) → "check paritesi SIĞ" kökü.

### ✓ D-426: K2 KAPANDI — aggregate payload'ında birim tip `i8`
`sonuç<boş,X>`/`seçimlik<boş>` payload yuvası artık C ile birebir (`{i8,i8,i32}`).
**Onarım İKİ parçalı, tek başına tip eşlemesi YETMEZDİ:** (1) `ll_tip_alan` —
aggregate-alanı bağlamında birim tip `i8`; (2) payload store'unda `int_uydur` —
yuva daralınca REGISTER de daralmalı (C `trunc i32→i8` yayar; `int_uydur`
IMMEDIATE'a dokunmaz, tipler eşitse no-op). D-422'de eşlemeyi tek yere koyup
`{i8, void, i8}` üretmiştim — bu kez üretici ve tüketici birlikte değişti.
`yapi_diff` muafiyeti **26 → 25**.

### 📐 K1 HEDEF BİÇİMİ TAM ÖLÇÜLDÜ (D-430; uygulanmadı — sıfırdan ölçme)
`yapi_diff`in en büyük muafiyet kökü (9 dosya). C ile self yan yana:
```
C   : define i1 @buyuk_mu(ptr %rho, i32 %a, i32 %b)
        %5 = icmp sgt i32 %3, %4
        ret i1 %5                      ← zext YOK, doğrudan
      çağrı yeri: %4 = call i1 @buyuk_mu(...)  +  br i1 %4   (dönüşüm YOK)
      yapı alanı: %K = type { i1 }  ·  store i1 / load i1
      parametre : define i32 @isle(ptr %rho, i1 %b)
SELF: define i32 @buyuk_mu(...)
        %6 = icmp sgt i32 %4, %5
        %7 = zext i1 %6 to i32         ← self i32'ye genişletiyor
        ret i32 %7
```
**Onarım ÜÇ yerde eşgüdüm ister** (D-422'nin `{i8, void, i8}` dersi): dönüş
tipi eşlemesi (`islev_donus_tip`) + `ret` uyarlaması (`int_uydur` ile
`trunc i32→i1`) + ÇAĞRI YERİ (dönüş i1 ise `eğer`/aritmetik bağlamına uyarlama).
Ayrıca yapı alanı/alloca/load/store yolları. `mantıksal` değerleri aritmetiğe
ve karşılaştırmalara aktığı için yüzey GENİŞ.
**Ertelenme gerekçesi:** kazanç YALNIZ yapısal (iki taraf da doğru çalışıyor,
exit/stdout aynı), risk ise çekirdek skaler yol. Bu oturumda tam bu sınıftan
İKİ regresyon çıktı (D-422 `{i8,void,i8}`, D-424 sürücü derlemesi).

### ⚠ K1 (`mantıksal` → i1) ÖLÇÜLDÜ ama BİLİNÇLİ ERTELENDİ
`yapi_diff` muafiyetlerinin en büyük kökü (9 dosya). C `mantıksal`ı **her
yerde** `i1` yapar: `%K = type { i1 }` · `define i1 @f` · `i1 %b` param ·
`store/load i1` · `ret i1` (zext YOK) · çağrı yerinde `br i1` doğrudan.
Self aritmetik bağlamda bilerek `i32` kullanır → değişiklik ÇEKİRDEK SKALER
YOLA dokunur. D-422'de tek satırlık bir tip eşlemesi `{i8, void, i8}` üretip
`codegen_diff`i düşürmüştü; aynı riski daha geniş yüzeyde almadım.
**⚠ K2 (`sonuç<bos,X>` payload) ölçülürken bir C KUSURU bulundu:**
`yapı K { a: boş; }` → C `%K = type { void, i32 }` = **GEÇERSİZ IR**
(`05_yapi`/`Dizi<kesirli64>` sınıfı). Aggregate i8 eşlemesi yalnız
`seçimlik`/`sonuç` payload yuvalarına uygulanmalı — **C'nin kusurunu taklit
etme.** Ölçmesem bir C hatasını self-host'a kopyalayacaktım.

### 🎯🎯 D-424: `--llvm` TİP KAPISI EKLENDİ — 12 atlanan dosya kapandı
- Self-host `--llvm` dalı `kontrol_program`ı **HİÇ ÇAĞIRMIYORDU** → tip hatalı
  programa sessizce IR üretiyordu. Artık C gibi **IR ÜRETMEDEN çıkış 1**.
- **ÜÇ KAPININ 12 ATLAMASI TEK KÖKTENDİ** (`ct_bariyer` 6 + `modul_codegen` 3 +
  `codegen_genis` 3); onikisinde de `--check` zaten BİREBİR paritedeydi.
- **Ön koşul üç adımda kazanıldı:** yanlış-pozitif 5 (D-420) → 3 (D-422 T011) →
  **0** (D-423 M004), 474 dosyalık yüzeyde. D-419'daki "onarım küçük görünüyor"
  değerlendirmem YANLIŞTI — üç ayrı kök gerekti.
- **⚠ TAZE `Ayr` ŞART:** `kontrol_program` `fn_ad`ı doldurur, codegen AYNI
  tabloyu okur → aynı `Ayr`de koşturmak codegen ÇIKTISINI DEĞİŞTİRİR.
- **Bilinçli sınır:** tanı METNİ stdout'a yazılmaz (C stderr'e yazar; self-host'ta
  stderr yerleşiği YOK — eklemek dil yüzeyi değişikliği, **Mehmet'in kararı**).
  Gözlenebilir sözleşme (boş stdout + çıkış 1) korunur; tanılar `--check`ten.
- **⚠ HARNESS SİMETRİSİ:** `codegen_diff` ve `yapi_diff` oracle'a `--tip-atla`
  geçip self'e geçmiyordu → kasıtlı tip-geçersiz korpus dosyaları YANLIŞ
  SEBEPLE kırmızı yapardı. İkisi de simetrik hâle getirildi.
- **🎯 ATLAMALAR POZİTİF ÖLÇÜME DÖNÜŞTÜ — ve kapının TEK GATE'i bu.** Kapıyı
  eklemek YETMEZDİ: üç harness "oracle IR üretemedi → ATLA" diyordu, yani tip
  kapısını kaldırsam **hiçbir kapı kırmızı olmazdı**. Üçünde de o dal POZİTİF
  İDDİAYA çevrildi (oracle reddediyorsa self de reddetmeli):
  `ct_bariyer` 7/7(6 atlandı) → **13/13 (0)** · `modul_codegen` 18/18(3) →
  **21/21 (0)** · `codegen_genis` 67/67(12) → **70/70 (9 — kalanlar MEŞRU
  bare-metal link hataları)**. D-419'un "atlama listesi bir KÖR NOKTA
  ENVANTERİDİR" dersinin uygulaması: listeyi okumak kusuru açtı, BOŞALTMAK
  kilitledi.
- **Sabotaj S6 (tip kapısını devre dışı bırak) YAKALANDI:** 13/13→7/13,
  21/21→18/21, mesaj "C tip hatasıyla REDDEDİYOR, KEMGU IR ÜRETİYOR".

### 🎯 D-423: M004 generic çeşit payload'ı — İKAME, atlama DEĞİL
- **Cazip onarım ("generic param ise atla") ÖLÇÜMLE ÇÜRÜTÜLDÜ:**
  `Secim<metin> + Var(42)` → **M004**, `Secim<tam32> + Var(42)` → OK. Atlamak
  gerçek M004'ü susturur (D-421 tuzağı). C: `substitusyon(..., beklenen)`.
- **BAYRAK DEĞİL DÜĞÜM İŞARETLEME** (`m4_node`/`m4_tn`) — `t14_muaf` ile aynı
  gerekçe: bağlam bayrağı alt-ağaca sızar.
- Hipotezimi kaynaktan çürüttüm: `parse_cesit` `tip_param_kaydet`i ÇAĞIRIYOR
  (satır 1900); kesik okuma yanıltmıştı. Registry hazırdı.
- **SINIRLAR:** iç içe şekil (`Kap<metin>`→`Opt<T>`) ikame EDİLMİYOR — dosya
  geçiyor ama muhafazakâr `"?"` fallback'i sayesinde, "çözüldü" DEĞİL.
  **`checker.kem`'e portlanmadı:** orada `tp_yad`/`tp_ad` registry'si HİÇ YOK
  (ölçüldü) → port tüm parse-yanı altyapıyı ister; korpusta generic çeşit
  bulunmadığı için `checker_diff` etkilenmiyor ama D-407 borcu duruyor.
- Gate: ayrı korpus dosyası eklenemedi (checker.kem portu yok), ama **D-424'ün
  tip kapısı bu kuralı kendiliğinden gate'ler** — sahte M004 self'i abort
  ettirir → `codegen_diff` kırmızı olur.

### 🎯 D-422: `bos` alias + AÇIK void dönüş + YENİ KAPI `calistir_yapi_diff`
- **`bos` (ASCII) oracle'ın KASITLI takma adıdır** (`tip_kontrol.c`:1239 —
  "C2.7: ASCII birim-tip alias 'bos' (Türkçe DNA: ikisi de kabul)"). Self-host
  tanımıyordu → `sonuç<bos,X>` sahte T011. Alias olduğunu ÖLÇEREK doğruladım
  (uydurma ad `Yokk` iki tarafta da reddediliyor). Tek yerden: `tip_bos_mu`.
- **D-418 YARIM KALMIŞ:** açık `-> boş` de `define i32` yayıyordu (D-418 yalnız
  ÖRTÜK dönüşü onarmış). Hiçbir kapı görmedi.
- **🔴 İLK ONARIMIM KIRDI:** eşlemeyi `ll_tip`e koydum → `{i8, void, i8}`
  ("void type only allowed for function results"), `codegen_diff` 139→138.
  **C birim tipi BAĞLAMA GÖRE eşler: SONUÇ'ta `void`, AGGREGATE alanında `i8`.**
  Eşleme `islev_donus_tip`e taşındı. **Kırılma bir bilgi verdi:** öncesinde de
  self `sonuç<bos,X>` payload'ını `i32` yayıyormuş (C `i8`) — `codegen_diff`
  exit'e baktığı için HİÇ görmemiş. Onarım sessiz sapmayı gürültülü yaptı.
- **YENİ KAPI `calistir_yapi_diff` — 116/116, 26 muaf.** `cg_korpus`ta `define`
  kümesini (**ad + DÖNÜŞ TİPİ**) karşılaştırır. Muafiyet listesi = BİLİNEN
  SAPMA ENVANTERİ, 4 kök: K1 `mantıksal`→C `i1`/self `i32` (9 dosya) ·
  K2 `sonuç<bos,X>` payload (1) · K3 lifted lambda daima i64, D-300 (8) ·
  K4 generic BASE gövdesi, D-401 (8). Muaf dosya artık eşleşiyorsa UYARIR.
  > **DAVRANIŞSAL KAPI BU SINIFA KÖRDÜR** — tek oturumda ÜÇ örnek çıktı,
  > üçü de geçerli IR + aynı exit + aynı stdout. `ct_bariyer`/`baremetal_diff`
  > neden YAPI ölçtüğünün üçüncü kanıtı.
- **⚠ SABOTAJ İLK DENEMEDE UYGULANMADI** (`perl` deseni tutmadı, `grep` 0) ve
  kapı YEŞİL kaldı. Sayıyı doğrulamasam "kapı zayıf" diye kaydedecektim —
  **sessizlik önce SABOTAJI şüpheli kılar** (D-402'nin tekrarı). Doğru satırla
  115/116 yakalandı.
- **M004 (kalan 3 yanlış-pozitif) — MEKANİZMA ÖLÇÜLDÜ, cazip onarım YANLIŞ.**
  C **İKAME EDER, ERTELEMEZ**: `Secim<metin> + Var(42)` → **M004**,
  `Secim<tam32> + Var(42)` → OK. "Generic param ise atla" gerçek bir M004'ü
  susturur (D-421 tuzağı). `tp_yad`/`tp_ad` registry'si HAZIR (`parse_cesit`
  DOLDURUYOR — "çağırmıyor" hipotezimi kaynaktan çürüttüm); eksik olan tek şey
  inşa yerindeki BEKLENEN TİP bağlamı.

### 🔴 D-421: D-420'NİN İKİ SOUNDNESS DELİĞİ (yeşil kapılar görmedi)
D-420 TÜM kapıları geçmişti; ön-merge düşmanca denetim iki loud→silent deliği
buldu. **Yeşil kapı "doğru" demek değildir** — bu dersin bu oturumdaki 2. kanıtı.
- **`tn_soy` HAM İŞARETÇİYİ soyuyordu.** Oracle (`src/tip_kontrol.c`
  DUGUM_ERISIM) otomatik deref'i **YALNIZ `TIP_REFERANS`** için yapar; `*K`
  üzerinden alan erişimini **T007 ile REDDEDER**. `TIP_POINTER`ı soymam, C'nin
  İKİ tanıyla reddettiği şekli self'te tamamen sessizleştirdi.
  > **KURAL: bir MUAFİYET kuralı kabul edilebilirliği GENİŞLETMEMELİDİR.**
  > Muafiyet "bu tanı burada yanlış" der, "bu program geçerli" DEMEZ.
- **`bag_tn` KAPSAM-KÖRDÜ.** `yerel` append-only'dir, blok çıkışında KISALMAZ →
  sondan-başa arama BAŞKA bir kardeş bloktaki gölgeyi bulur ve gerçek bir T014'ü
  susturur. Çözüm repoda ZATEN VARDI: `var_tip` disiplini (birden fazla FARKLI
  tip → BELİRSİZ). **Belirsizlikte muhafazakâr taraf TANIYI KORUMAKTIR.**
- **`checker.kem` portu:** `yerel_topla` iki dosyada BİREBİRMİŞ; yardımcı bloğu
  kopyalandı (aynı soruyu iki yerde ayrı yanıtlayan kod ayrışır — D-407).
- **⚠ NEGATİF TEST VAKASI SEÇMEK BİR ÖLÇÜM İŞİDİR.** İki deneme çöptü: somut
  skaler alan ve `Genel<tam32>` ikisi de C'de T014'ün YANINDA T001 üretiyor,
  self T001'i yaymıyor → kapı **YANLIŞ SEBEPLE** kırmızı olurdu. Çalışan tek
  şekil: generic İŞLEV içinde **ÇÖZÜLMEMİŞ `T`** (C erteler → tek T014).
- **Sabotaj 4/4 yakalandı** (S1 ERISIM dalı · S2 toptan muafiyet · S3 naif
  `bag_tn` · S4 sürücü karşılığı). Negatif vaka olmasa S2 kapıyı GEÇERDİ.
- **⚠ KAPIYA KONAMAYAN KÖR NOKTA:** `f(k: *K) { k.xs = []; }` → C `[T007,T014]`,
  self `[T014]`; ayrıştıkları için korpusa konamaz. `tn_soy`un `*T`yi soymaması
  yalnız PROBE ile doğrulanmıştır. T007 boşluğu kapanınca korpusa eklenmeli.
- Kapılar: `checker_diff` **149/149** (0 muaf) · `codegen_diff` 139/139 ·
  `self_driver` 4 mod × 2 sürücü + FIXPOINT · `--check` 117/117.

### 🎯 D-420: T014 alan lvalue + SELF-HOST CHECKER PARİTE ENVANTERİ
- **`k.xs = []` sahte T014 veriyordu** — `ATAMA` muafiyeti yalnız `TANIMLAYICI`
  lvalue'yu tanıyordu; `ERISIM`/`INDEKS` yoktu. **Self-host KENDİ kaynağında
  düşüyordu** (`codegen.kem:1121`, `p.tp_pending = []`). Kural **TİP
  GÜDÜMLÜDÜR** (8 probe ile C'den ölçüldü): `k.n = []` (skaler alan) T014
  ALIR → toptan muafiyet gerçek bir tanıyı susturur. Yeni yan-dizi `yerel_tn`
  (tip **DÜĞÜMÜ**; string yetmez — iç içe `[[]]` alt-ağaç yürüyüşü + INDEKS'te
  ELEMAN tipi gerekir).
- **⚠ `codegen.kem` TEK DOSYA ama İKİ YOL** (checker / codegen) ve bazı tablolar
  yalnız BİR yolda dolar. `fn_node` YALNIZ `imza_topla`da (codegen ön-geçişi)
  dolar; checker'dan okumak **"dizi sınır ihlali"**. Bir tabloyu yeni bir yerden
  okumadan önce **"bu yolda kim dolduruyor?"** diye SOR. Aynı gerekçeyle
  `--llvm` tip kapısı `kontrol_program`ı AYNI `Ayr`de koşturamaz (`fn_ad`
  kirlenir → codegen çıktısı değişir) → **taze `Ayr` şart.**
- **⚠⚠ "131/131 TAM PARİTE" YÜZEY-SINIRLIYDI.** O ölçüm `cg_korpus`,
  `snapshots` ve `selfhost/`i HİÇ kapsamıyordu. Tüm yüzey taranınca **11 sapma**
  çıktı. **Parite sayısı yalnız ölçülen yüzey kadar geniştir.**
- **`--llvm` TİP KAPISININ ÖN KOŞULU TUTMUYOR** (D-419'da "onarım küçük
  görünüyor" demiştim — YANLIŞTI). Bloklayan **5 yanlış-pozitif (≈2 kök)**:
  `cg_cesit_ic_ayirici`+`snapshots/cesit_sonuc` (T011), `cg_generic_cesit`+
  `cg_mono_cesit_metin`+`cg_mono_yapi_field_cesit` (M004). Bugün eklesem
  `codegen_diff` **139 → 135**. Ayrıca **6 kaçırma**: T002/T007/T022/T001/BL001
  ve `deref_atama_disi`. **⚠ DÜZELTME (D-425):** o dosya için "C=T022,
  self=G001 — YANLIŞ KOD" yazmıştım; YANLIŞTI. C **İKİ** tanı verir
  (`T022`+`G001`), self yalnız `G001` → **eksik tanı**, yanlış kod değil.
  Hata benim özet aracımdaydı: karşılaştırmayı `head -1` ile yapmıştım.
  **Çok satırlı tanı çıktısını `head -1` ile karşılaştırma** — ilk satır
  eşleşmese bile fark "yanlış kod" değil "eksik/fazla satır" olabilir.
- **12 ATLANAN DOSYA, ÜÇ KAPI, TEK KÖK:** `ct_bariyer` 6 + `modul_codegen` 3 +
  `codegen_genis` 3 — hepsi "self `--llvm` tip hatasında durmuyor". **Onikisinde
  de `--check` birebir paritede.** Ön koşul kapanınca bu dosyalar "atlandı"dan
  "iki taraf da reddediyor"a döner.
- **Kalan (D-420 kapsamı dışı):** `checker.kem`'de AYNI T014 boşluğu (satır
  4444) — korpusa örnek eklemek `checker_diff`i kırar, port ayrı adım.
  Skaler alan şeklinde eşlik eden T001 yayılmıyor; `f().alan = []` sahte T014.

### ⚠ D-419'DA AÇILAN İKİ YENİ BULGU (ölçüldü, onarılmadı)
Kapıların ATLAMA listeleri tarandı (`codegen_genis` 12 dosya). Üç sınıf çıktı;
7'si bare-metal MMIO (meşru), ama ikisi gerçek bulgu:

**1. `--llvm` TİP HATASINDA DURMUYOR (self-host).** `kem_asm_kernel` T001
içeriyor. `--check` paritede (ikisi de `T001 18 16`), ama:
- C `--llvm`: **durur, IR üretmez**
- self-host `--llvm`: **devam eder, IR yayar** (2 define + 1 asm)
Sebep: self-host sürücüsünde `--llvm` dalı `kontrol_program`ı HİÇ ÇAĞIRMIYOR
(D-399'da görülmüştü). Tip hatası olan programa sessizce IR üretmek "loud →
silent" sınıfıdır. Onarım küçük görünüyor (checker'ı çağır, hata varsa çık) ama
`--llvm` davranışını geniş biçimde değiştirir → ayrı adım.

**2. `05_yapi` — İKİ TARAFTA DA GEÇERSİZ IR** (parite değil, dil kusuru):
- C   : `base element of getelementptr must be sized`
- self: `invalid getelementptr indices`
İkisi de farklı biçimde bozuk. `Dizi<kesirli64>` ile aynı sınıf: gerçek bir
kusur, self-host paritesi işi değil. `codegen_genis` bunu "oracle linklenemedi"
diye atlıyordu — **atlama sebebi bare-metal DEĞİL, C'nin kendi IR'ının geçersiz
olmasıydı.** Atlama listesini okumasaydım bu görünmezdi.

### 🎯 D-419: BİRLEŞİK OS BİRİMİ ÖLÇÜLDÜ — tam KEMGU-OS yapısal paritede
D-418'in kapısı **6 dosyayı atlıyordu** (C oracle onları tek başına derleyemiyor
— T002, birbirlerinin sembollerine bakıyorlar). Meşru bir atlama ama **OS'un
2/3'ü kapısız** demekti. Gerçek derleme birimi Makefile'da yazılıydı (`:1134`):
tüm `runtime/*.kem` + `kem_os.kem` BİRLEŞTİRİLİYOR. Kapı o birimi ölçüyor:
```
4166 satır · define C=240 KEMGU=240 (küme diff BOŞ) · asm C=44 KEMGU=44
üçlü: aarch64-unknown-none-elf (ikisi de)
```
- **⚠ ASM SAYISI DENETİMİ ŞART:** sabotaj S162 ile asm emisyonu silindiğinde
  **işlev sayısı 240=240 AYNI KALDI**, yalnız `asm 44→0` yakalandı. Yalnız
  `define` adı karşılaştıran bir kapı D-416'nın kusurunu KAÇIRIRDI.
- **DERS: kendi kapının ATLADIKLARINI da ölç.** Atlama listesi bir KÖR NOKTA
  ENVANTERİDİR; içinde ne olduğunu bilmeden kapı sayısına güvenme. (D-404'ün
  aynı dersi — bu kez kapıyı BEN yazmıştım.)

### ✓ D-418: `--mimari` + void dönüş + BARE-METAL KAPISI (`runtime/` 0/3 → 3/3)
- **Self-host bare-metal/ARM64 kodu DERLEYEMİYORDU BİLE.** `--mimari` bayrağı
  sürücüde yoktu; SIRALI argüman ayrıştırma `aarch64`ü DOSYA YOLU sanıyor, boş
  girdi okuyup yalnız başlık basıyor, **hata vermiyordu**. Üçlü de sabit kodluydu.
  **Hiçbir kapı görmüyordu** çünkü `runtime/*.kem` host'ta linklenmez → exit
  karşılaştıran kapılar bu yüzeyi atlıyor.
- **Örtük `boş` dönüş `i32` yayılıyordu** (`çıplak işlev free(...) { }` → C
  `define void`, self `define i32`). Gözlenebilir yanlış cevap YOK (LLVM tolere
  ediyor, D-295) — gizli yapısal sapma. Onarım `ret` emisyonunu da zorunlu kıldı
  (`ret void 0` geçersiz).
- **Yeni kapı `calistir_baremetal_diff`:** linklenemeyen yüzeyde DAVRANIŞ değil
  YAPI ölçer — hedef üçlüsü + `define` kümesi (**ad + DÖNÜŞ TİPİ**).
  ⚠ Dönüş tipini kesme: `define void @f` ↔ `define i32 @f` ayrımı kusurun ta
  kendisiydi.
- **⚠ SABOTAJ YANLIŞ SATIRA UYGULANDI (yine).** S161 ilk denemede sessiz kaldı
  çünkü `sed` `builtin_ret`in void dalını değiştirmişti, `islev_donus_tip`i
  değil. Doğru satırla 0/3. **Sessizlik önce sabotajı şüpheli kılar** (D-402).

### ✅ (TARİHÎ) `Dizi<kesirli64>` — **D-451'DE ÇÖZÜLDÜ**. Aşağıdaki ölçümler ESKİMİŞTİR
(o gün C exit 7 / self exit 1 idi; D-451 öncesinde ikisi de LINK-RED olmuştu).
**Ölçüldü:** `değişken t = xs[0] + xs[1]` → C **exit 7**, self-host **exit 1**
(doğrusu 42). İkisi de yanlış ve **farklı** biçimde yanlış — yani bu bir parite
işi DEĞİL, gerçek bir dil kusuru.

**Kök (ölçüldü):** erişimci SONEKİ yanlış. C `Dizi<kesirli64>` için 32-bitlik
`_tam` çeşidini kullanıyor:
```
call ptr @kdl_dizi_olustur(ptr %3, i32 8)          ← eleman boyutu DOĞRU (8)
call void @kdl_dizi_ekle_tam(ptr, ptr, double %5)  ← ama 32-bit erişimci
call double @kdl_dizi_al_tam(ptr %7, i32 %8)       ← declare i32 ile UYUŞMAZ
```
**Eleman boyutu 8 olduğu için BELLEK GÜVENLİĞİ sorunu YOK** — yalnız değerler
yanlış. (Bu ayrımı ölçmeden varsaymak yanlış olurdu.)

**Onarım şekli — RUNTIME DEĞİŞİKLİĞİ GEREKMİYOR:** `kdl_dizi_al_tam64` 8 baytı
`int64_t` olarak okur, yani BİTLER doğrudur. Derleyici tarafında:
- oku : `%i = call i64 @kdl_dizi_al_tam64(...)` + `bitcast i64 → double`
- yaz : `bitcast double → i64` + `kdl_dizi_yaz_tam64` / `_ekle_tam64`
- `kesirli32` için aynısı `_tam` + `float↔i32` ile.

**⚠ SCOPE: bu C'Yİ (ORACLE'I) DEĞİŞTİRİR.** Bu oturumun 20+ artımı "self-host'u
C'ye uydur" hattındaydı; oracle'ı değiştirmek TÜM parite kapılarını yeniden
tabanlar. Kusur tartışmasız ve onarım bounded, ama hangi hatta yürüneceği
**Mehmet'in kararı** — sessizce kapsam genişletmedim.

- **(ESKİ NOT) `Dizi<kesirli64>` OKUMA yolu İKİ TARAFTA DA bozuk.** `kdl_dizi_al_tam` i32
  döndürüyor; geri okuyan bir test **C oracle'da da** yanlış sonuç verdi. Yazma
  yolu D-408'de düzeldi, okuma açık.
- ~~**Sıradaki kapısız yüzeyler:** `test/snapshots` (81), `test/ornekler/eski`
  (16), `test/asan_matris` (12), `test/stdlib` (9), `test/crossfile` (2)~~
  ✓ **D-428: HEPSİ KAPI ALTINDA** (`calistir_check_genis`, 126/126, 7 muaf).
  Ayrıca D-427: `drivers/virtio` + `tests/drivers/virtio` (19 dosya) →
  `calistir_surucu_diff`. **Bilinen kapısız `.kem` yüzeyi KALMADI**
  (`lex_korpus`/`parse_korpus` kendi kapılarında: `--token`/`parser_diff`).

- 🎯🎯 **D-407: `test/moduller` 18/18, MUAFİYET LİSTESİ BOŞ.** Nitelikli
  (üç segmentli) çeşit yapıcısı: codegen'in kolu solu yalnız `TANIMLAYICI`
  kabul ediyordu, `ifd::Ifade::Sayi` biçiminde sol bir **YOL**'dur. Tek koşul
  genişletmesi. **Checker'ın `yol_cesit_adi`si zaten ikisini de kabul
  ediyordu** — iki tarafın ayrı davranması kusurun kendisiydi.
  **DERS: aynı soruyu iki yerde ayrı yanıtlayan kod er ya da geç ayrışır;
  böyle bir kusuru ararken "diğer taraf ne yapıyor?" İLK soru olmalı.**
- 🎯 **D-406: `test/moduller` 11/18 → 16/18, muafiyet 7 → 2.** Ham işaretçi
  indekslemesi (`*T` içinde `d[i]`) heap-dizi yoluna düşüyordu; `*T` düz bellek,
  KdlDizi **başlığı yok** → hem yanlış lowering hem tip hatası. C aynası:
  `getelementptr <pointee>, ptr, i64` + load/store. Ayrım POZİTİF bilgiyle
  (`cg_apointee`); kayıt yoksa eski yola düş. **Okuma ve yazma kolları AYRI
  yerlerde** → iki onarım.
- **⚠⚠ MUAFİYET LİSTESİNE YAZDIĞIN GEREKÇE DE BİR İDDİADIR — ÖLÇ.** O 7 dosyayı
  "hepsi tek kök: dönüş-tipi-güdümlü çıkarsama" diye kaydetmiştim. Hata satırını
  izleyince **ÜÇ AYRI kök** çıktı ve **hiçbiri o değildi** (D-404 `yetki<R>`,
  D-405 `bölge_al`, D-406 `*T` indeksleme). Doğrudan o özelliği yazsaydım büyük
  bir işi YANLIŞ YERE yapardım. "Kalanların hepsi aynı kök" en cazip ve en test
  edilmemiş varsayımdır.
- ✓ **D-405: `bölge_al` yerleşiği.** C **argümanları YOK SAYAR**, parametresiz
  `@kdl_global_bolge_al()` çağırır → arite farklı, salt ad eşlemesi YETMEZ.
  **Bu kök "dönüş-tipi-güdümlü çıkarsama" sandığım şeyin ALTINDA duruyordu**:
  kalan 7 modül dosyası `dizi.kem`in `oluştur`unda takılıyordu ve o `bölge_al`
  çağırıyor. **Kökü ölçmeden büyük bir özellik yazsaydım yanlış yeri onarırdım.**
  Hata satırını tek tek izlemek bu turda iki kök açtı.
- **KALAN TAKOZ (ölçüldü):** `%44 = load i64` → `call i32 @kdl_dizi_al_tam(ptr, i32 %44)`
  — heap dizi erişiminde `tam64` indeks i32'ye DARALTILMIYOR. Ayrı ve bounded.
- ✓ **D-404: `yetki<R>` self-host codegen'de** (`%kdl_yetki`, OUT-PTR ABI).
  **⚠ ÖRTÜK KAPSAM DELİĞİ:** `yetki` kullanan tek dosya `kem_heap.kem` ve onun
  C oracle'ı host'ta LİNKLENMİYOR (`kdl_mmio_oku32` bare-metal) → `codegen_genis`
  atlıyor. **Kapı 67/67 yeşilken bu boşluk sessizce duruyordu.** "Oracle
  kurulamadı → atla" politikası doğru ama KÖR NOKTA yaratır: atlanan dosyaların
  TEK kullanıcısı olduğu bir özellik hiç ölçülmemiş olur. Saf-yetki programı
  host'ta linkleniyor — gate'lenebilirmiş, kimse denememişti.
- ✓ **D-403: MODÜL generic'leri artık specialize ediliyor** (`@dizi.al$i64`,
  `@cgmodul_mat.esle$double` — C ile aynı adlar). D-402'de reddedilen D-401b'nin
  İKİ kusuru vardı: (1) specialization modül bağlamını kaybediyordu; (2) ilk
  onarımım bağlamı ÇAĞRI YERİNDEN alıyordu — önek **BİLDİRİMİN** adından
  (`nokta_onek`) gelmeli. **Doğru mekanizmayı seçmek yetmiyor, DOĞRU KAYNAKTAN
  beslemek gerekiyor.** `test/moduller` 11/18'de sabit; mangling artık doğru,
  kalan iş dönüş-tipi-güdümlü çıkarsama.
- ✓ **D-402: artık KAPI var — `make calistir_modul_codegen`** (11/11, 7 muaf).
  Öncesinde bu yüzey hiçbir kapının altında değildi ve **D-401b regresyonunu
  (11/18→9/18, biri SESSİZ YANLIŞ CEVAP: `ana_golge_jenerik` C=1≠KEMGU=100)
  `codegen_diff` de `codegen_genis` de GÖRMEDİ** — ikisi de yeşildi. Elle
  ölçtüğüm için geri aldım; ölçmeseydim gönderirdim.
- **⚠ SABOTAJIN SESSİZLİĞİ ÖNCE SABOTAJI ŞÜPHELİ KILAR.** D-402'de S138 sessiz
  kaldı çünkü `son_segment` (`::` ile böler) kullanmıştım, nokta ile bölen bir
  şey değil → sabotaj **uygulanmamıştı**. "Kapı zayıf" diye kaydetmek yanlış
  olurdu. D-356'nın "korpusu düzelt" dersinin varyantı: **sabotajı düzelt.**
- **⚠ UZUN KOŞUM SÜRERKEN KAYNAĞI DEĞİŞTİRME:** `test_tumu` koşarken
  `selfhost/codegen.kem`i düzenledim → kapı yarı-bozuk kaynaktan `codegen.exe`
  kurdu, `codegen_genis` **66/67** raporladı. Temiz kaynakta 67/67.

### ⚠⚠ (TARİHÎ) SELF-HOST'TA GENERIC İŞLEV MONOMORFİZASYONU YOK (ölçüldü 2026-08-07)
`ADIM 23`'teki "LLVM monomorphization" **C derleyiciye aittir**; self-host'ta
generic İŞLEV mono'su **hiç yoktur**. Ölçüm (`işlev kimlik<T>(x: T) -> T`):
```
C:    define i32 @kimlik$i32(ptr, i32)  +  define i64 @kimlik$i64(ptr, i64)
SELF: define i32 @kimlik(ptr, i32)      ← TEK gövde, T daima i32 varsayılıyor
      call i32 @kimlik(ptr %2, i64 %5)  ← İMZA UYUŞMAZLIĞI
```
- **Tamsayı T'de SESSİZ geçiyor** — LLVM `define`/`call` uyuşmazlığını kabul
  eder (D-295 dersi) ve x86-64 ABI'sinde değer register'da hayatta kalıyor.
  `2^33+42` ile denedim: **kırpılmadı**, ikisi de doğru. Yani bu, tamsayıda
  gözlenebilir bir kusur ÜRETMİYOR — ama sağlamlığı ABI tesadüfüne dayanıyor.
- **`kesirli64`'te LOUD:** `'%4' i32 but expected 'double'` → LLVM reddeder.
  Register sınıfı değiştiği an tesadüf biter (D-294'ün aynı dersi).
- **Bu, `test/moduller`de kalan 7 dosyanın KÖKÜYLE AYNI** — `dizi::Liste<tam64>`
  gibi çapraz-modül generic'ler `@dizi.al$i64` ister, self-host `@dizi.al` yayar.
- **Mevcut malzeme:** self-host'ta `mono_mangle` / `mono_ir_sanitize` /
  `mono_gerek_yapi` / `mono_gerek_cesit` VAR — ama YAPI/ÇEŞİT için. İşlev yolu
  yok. `gp_ad` (D-370) generic param adlarını zaten tutuyor. C karşılığı:
  `src/llvm.c` `BekleyenSpec` + bekleyenler worklist'i (~2891, ~2987).
- **Sıradaki iş bu.** Ad çözümü (D-398/400) bitti; kalan tek büyük parça bu.

  - **Kalan 7 — TEK sınıf: çapraz-modül GENERIC MONOMORFİZASYONU** (`ana_ifd`,
    `ana_kap`, `ana_kap_coklu`, `dizi_*`), hepsi `'%N' i32 but expected 'ptr'`.
    Ad çözümü DEĞİL. Ölçüldü (`dizi_kullan`):
    ```
    C:    define i64 @dizi.al$i64(ptr %rho, ptr %l, i64 %i, i64 %varsayilan)
          define void @dizi.ekle$i64(ptr, ptr, %kdl_yetki, i64)
    SELF: define i32 @dizi.al(ptr %rho, ptr %a0, i64 %a1, i32 %a2)
    ```
    İki eksik: (1) çapraz-modül **monomorfizasyon** — nitelikli tip annotation'ı
    (`değişken l: dizi::Liste<tam64>`) T'yi RHS çağrısına besler ve `$i64` soneki
    üretir; self-host tek bir jenerik-olmayan gövde yayıp T'yi `i32` sanıyor.
    (2) `yetki<R>` parametresi `%kdl_yetki` taşınmalı, `i32` değil.
    **Ayrı ve büyük alt-sistem** — ad-mangling işiyle (D-398/400) karıştırma.
  - **`modul_yukle` (--check) aynı transitif-dizin kusurunu taşıyor ama MASKELİ:**
    check yalnız isim topluyor, `ana_zincir` `mat`in adlarına ihtiyaç duymuyor.
- **YAN BULGU — check paritesi SIĞ:** `mat::topla(20)` (yanlış arite) C'de
  `T010`, self'te `OK`. 131/131 mevcut korpusta doğru ama isim düzeyinde;
  imza/tip yok. **Parite sayısı mekanizma derinliğini KANITLAMAZ.**
- **`--parse` paritesi de kanıt değil:** C `--ast` yolunda da `modulleri_yukle`
  çağrılmıyor → 13/13 sıfır-diff modül yükleme hakkında hiçbir şey söylemez.

**TEKRARLAYAN DERS — kapı, YANLIŞ uygulamanın GÖZLENEBİLİR olduğu şekli ister:**
sabotaj üç kez SESSİZ kaldı ve her seferinde korpus düzeltildi, kural değil:
(a) `sonuç<sonuç<..>,..>` yetmedi çünkü yanlış ayrıştırma da uyumsuz tip üretip
AYNI tanıyı veriyordu → doğru davranışın SESSİZ olduğu şekil gerekti (D-385);
(b) sabit değeri i32'ye SIĞIYORDU → 2^33 gerekti (D-391); (c) 3 parametreli
çağrı yetmedi çünkü ilk 3 argüman register'a sığıyor → EN AZ 4 gerekti (D-393).

**ÖLÇÜM ARACI DA YANLIŞ OLABİLİR:** D-391'de "ham işaretçi deref bozuk" sanıp
`&x olarak *tam32` probe'u yazdım — C onu T001 ile REDDEDİYOR, yani probe
geçersizdi. Şekli kaynaktan BİREBİR al, kendin uydurma.

**KAPI SEÇİMİ (bu seride iki kez ısırdı):** `checker_diff` yeşilken
`calistir_parser_diff` KIRMIZI kalabilir — üç ayrı uygulama var
(`selfhost/parser.kem` referans parser, `selfhost/checker.kem` referans checker,
`selfhost/codegen.kem` birleşik sürücü). Korpusa dosya eklerken **hangi
uygulamaların o şekli görmesi gerektiğini** ayrıca düşün ve İLGİLİ TÜM kapıları
koş (D-387: p7 eklenmiş ama `parser.kem` hiç güncellenmemişti).

### ✓ ÇÖZÜLDÜ — D-415 (aşağıdaki D-414 notu ARTIK TARİHÎ)
İki kök vardı, **ikisi de benim önceki kararlarımın hatasıydı**:
- **🔴 D-405 YANLIŞTI: `bölge_al` GERÇEK TAHSİSTİR** — `malloc(n*sizeof(T))`.
  D-405'te gördüğüm `@kdl_global_bolge_al()` `bölge_al`ın karşılığı DEĞİL, **her
  işlevin ρ-EDİNME PROLOGU**ydu (tüm C çıktısında yalnız BİR kez, `main`de).
  Küresel bölge işaretçisi tampon diye dönünce yazılan her eleman bölgeyi
  bozuyordu. **D-405'in korpusu kaçırdı çünkü belleğe HİÇ YAZMIYORDU** — kendi
  notu "adres kararsız, denetlemiyoruz" diyordu.
  **DERS: tahsis yerleşiğini test ederken tahsis edilen belleğe YAZ ve GERİ OKU.**
- **🔴 D-411'in "fallback güvenlidir" iddiası YANLIŞTI.** `büyü<T>(l: &Liste<T>)`
  T'yi iç içe konumda taşıyor → `i32` fallback'i `büyü$i32` üretiyor, eleman
  kopyası 4 bayt genişliğinde. **Define ve çağrı i32'de ANLAŞTIĞI için LLVM
  kabul ediyor** — "hata gürültülü kalır" dediğim şey olmadı, kusur çalışma
  anına kaydı. Onarım: **aktif ikameden devral** (`subst_bul`) — sıra (a) çıplak
  parametre → (b) aktif ikame → (c) i32.
- **exit 139 asla artefakt sayılmaz** (aşağıdaki not doğruydu, kanıtlandı).

### 🔴🔴 (TARİHÎ) AÇIK: `dizi_kullan` SELF-HOST İKİLİSİNDE ARALIKLI SEGFAULT (D-414)
**Bu GERÇEK bir kusur, kapı artefaktı DEĞİL.** Aynı ikili art arda koşulunca
`42 42 139 42 …` veriyor; C oracle aynı dosyada **8/8 kararlı 42**.
`git stash` ile D-414 geri alınıp ölçüldü: **segfault D-414'ten ÖNCE de var** —
yani o değişiklik sebebi değil, kusur önceden oradaydı ve `modul_codegen`
kapısını ŞANSLA geçiyordu.

> **⚠ ÖNCEKİ TEŞHİSİM KISMEN YANLIŞTI.** D-413'te aralıklı kırmızıları
> "tek bash çağrısında art arda `make` hedefleri → Windows dosya kilidi" diye
> açıklamıştım. O etken GERÇEK (127'ler Defender exec yarışı, ölçüldü) **ama
> 139'ları açıklamıyor.** En az bir kısmı bu segfault'tu. **"Aralıklı = artefakt"
> en cazip ve en tehlikeli varsayım**: exit 127 ortamsaldır, **exit 139 DEĞİLDİR.**
> Kodu ayır: 127 → yeniden koş; 139 → BELLEK HATASI, teşhis et.

**KURAL (güncellendi): kapı kırmızıysa temiz yeniden koş ve en az İKİ yeşil
koşum gör — AMA çıkış kodu 139 ise bunu artefakt sayma, ayrı bir kusur olarak
aç.** Aralıklılık UB'nin normal görüntüsüdür.

**⚠ ZAMAN AŞIMINA UĞRAYAN `make` ÖLMEZ — ORPHAN OLARAK KOŞMAYA DEVAM EDER.**
D-411'de bir `calistir_codegen_diff` 10 dk sınırında "timeout" verdi; ben devam
edip **arka planda ikinci bir kapı koşumu başlattım**. İki `make` aynı anda
`build/codegen.exe`i yazdı → `codegen_diff` **134/135 SAHTE KIRMIZI** ve sürücü
başarısız. Kaynak DEĞİŞMEDEN yeniden koşunca **135/135 yeşil**. Bu, D-297'nin
("aynı testin iki eş zamanlı koşumu birbirini ezer") yeni bir biçimi.
**Kural: bir kapı koşumu zaman aşımına uğrarsa, YENİSİNİ BAŞLATMADAN ÖNCE
bittiğinden emin ol.** Sahte kırmızıyı gerçek regresyon sanıp geri almaya
kalkmak, gerçek bir kusuru geri almaktan daha ucuz değildir.

**⚠ EKSİK ARTEFAKT — `build/codegen.exe` (D-398'de İKİ KEZ ısırdı):** sabotaj
döngüsünde `rm -f build/codegen.exe` yapıp sonra ELLE ölçüm koşarsan her dosya
"IR üretemedi" der ve bu **sahte bir kök** gibi görünür (bir kez "modül desteği
yok" sandım, bir kez "kaynak-splice çalışmıyor"). `calistir_self_driver`
`kemgu_self.exe` kurar, `codegen.exe`'yi KURMAZ. **Elle ölçümden önce daima:**
```
build/kemgu.exe --llvm selfhost/codegen.kem > build/codegen.ll && \
  clang -x ir build/codegen.ll -x none build/kdl_runtime.o -o build/codegen.exe
```
Kapılar (`calistir_codegen_diff/genis`) bunu kendileri yapar — elle koşum yapmaz.

**BAYAT ARTEFAKT (yine yaşandı):** `git checkout origin/main` ile ölçüm yapıp geri
dönünce `build/kemgu.exe` ESKİ kaynaktan kalır ve tüm parite kapıları sahte kırmızı
verir (148→144 gözlendi). Dal değiştirdiysen `rm -f build/kemgu.exe build/kdl_runtime.o`
+ `make` ŞART.

**Bootstrap kapıları (`calistir_lexer_bootstrap`, `calistir_parser_bootstrap`)
`origin/main`'de DE exit 2 verir** — oran raporlarlar (%96-99), yeşil/kırmızı kapı
değildirler. Bunu regresyon sanma.

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

## ⚠⚠ ÖLÇÜM ARACI KONTROL LİSTESİ (D-500 — en sık tekrarlayan hata sınıfı)

Bu depoda **kusurdan çok ölçüm aracı yanıldı.** Tek bir oturumda **sekiz** vaka
sayıldı ve biri **beş tur** kaybettirdi. Sonuç sürekli aynı: *olmayan bir kusuru
kovalamak* ya da *gerçek bir kusuru yeşil sanmak.*

**KURAL: Beklenmedik bir sonuç önce ARACI şüpheli kılar, kodu değil.**
Sessiz sabotaj, sıfır bulgu, "kanca ateşlenmiyor" — hepsinin ilk açıklaması
ölçümün kendisidir.

### Koşmadan önce
- [ ] **İkiliyi nasıl çağırıyorum?** Harness'ın çağrımını **oku**, taklit etme.
      *(D-499: harness `kemcheck.exe "$f"` — BAYRAKSIZ. Ben `--checkdump`
      geçtim, ikili bayrağı dosya adı sandı, boş çıktı verdi.)*
- [ ] **Yeni kuralı sınamadan önce ESKİ bir kural aynı yoldan çıkıyor mu?**
      *(T020 de sessizdi → sorun kancada değil çağrımdaydı.)*
- [ ] **Başarı kodu ne?** `cmd || fail++` yalnız 0'ı başarı sayar.
      *(D-489: bu testin başarı kodu 42 → 300/300 "başarısız" bastım.)*
- [ ] **`&&` zincirli sessiz kurulum YOK.** Her adımın rc'sini ayrı yazdır.
      *(D-499: ikili hiç oluşmamıştı, "OK" bayat binaridan geliyordu.)*
- [ ] **Boruya bağlarken `${PIPESTATUS[0]}`.** *(D-444: `make | tail` → exit
      kodu `tail`ın.)*

### Yamadan sonra
- [ ] **Yama gerçekten uygulandı mı?** `grep -c` ile SAY.
      *(D-490/D-402: `perl`/`sed` deseni sessizce tutmadı, kapı yeşil kaldı.)*
- [ ] **`sed`/`awk` Türkçe UTF-8 `.kem`/`.c` dosyasında KULLANMA** — Edit aracı.
      *(D-461: `awk`ta `&` "tüm eşleşme" demek → 17 satır bozuldu.
      D-490: `sed` `\n`'i gerçek satır sonuna çevirip C dizgisini kırdı.)*
- [ ] **Sabotaj döngüsünden sonra `.o` VE ikiliyi `rm -f`.**
      *(D-457: `cp` mtime'ı bozar, make objeyi güncel sanır.)*

### Sonucu okurken
- [ ] **Çok satırlı tanı çıktısını `head -1` ile karşılaştırma.** *(D-420/D-425:
      "yanlış kod" sandığım şey "eksik satır"dı.)*
- [ ] **Filtre çıktı satırına mı, içeriğe mi uygulanıyor?** *(D-491:
      `grep -v "^\s*--"` dosya:satır önekine takıldı → yorum filtresi hiç
      çalışmadı.)*
- [ ] **Türkçe kaynakta düz `grep '"[a-z]+"'` hex-escape'li adları KAÇIRIR.**
      *(D-492: 41 anahtar kelimenin 25'ini buldum, eksik listesi yanlış çıktı.)*
- [ ] **`exit 127` ortamsaldır, `exit 139` DEĞİLDİR.** 139 → bellek hatası, aç.
- [ ] **WSL `/tmp` ÇAĞRILAR ARASINDA SİLİNİR.** Bir koşumun logunu `/tmp`e
      yazıp SONRAKİ çağrıda okumaya kalkma — dosya YOK olur ve denetim
      sessizce boş döner (D-508 sonrası: 12 atlama izi ölçülmeden kaldı,
      `grep` hiçbir şey bulmadı ve bir an *"atlama yok"* diye okunacaktı).
      Kalıcı ölçüm logunu iki tarafın da gördüğü bir yola yaz (`/mnt/c/...`).
- [ ] **⚠⚠ BİR CHECKER KURALINI PORTLARKEN ÜÇ UYGULAMAYI DA GÜNCELLE.**
      Bu oturumda **ÜÇ KEZ** ısırdı (D-503'te zaten kayıtlıyken):
      ```
      checker_diff              -> selfhost/checker.kem
      self_driver --check       -> selfhost/codegen.kem
      ct_bariyer / --llvm yolu  -> selfhost/codegen.kem
      ```
      **Hedefli kapının yeşil olması portun tam olduğunu KANITLAMAZ**:
      `checker_diff` 166/166 ve 167/167 yeşilken tam takım sürücüde ve
      `ct_bariyer`de kırmızı verdi (*"C REDDEDİYOR, KEMGU IR ÜRETİYOR
      (loud→silent)"*). Yeni kuraldan sonra **`grep -c "D-NNN" selfhost/*.kem`
      ile İKİ dosyada da doğrula.**
- [ ] **İKİ AĞAÇLI KURULUMDA SABOTAJ GERİ ALMASI SENKRONLANMALI.** Windows
      worktree git'i tutuyor, WSL kopyası derleniyor. Bir sabotajı Windows'ta
      geri alıp WSL'e **kopyalamazsan** kapı kırmızı kalır ve bu **bayat ikili
      gibi görünür** — harness kaynaktan yeniden kurduğu için `rm -f` +
      yeniden kurmak "artefakt değilmiş" dedirtir ve seni yanlış köke
      yönlendirir. *(D-517: `checker_diff` 165/166 kaldı; ikiliyi zorla
      kurdum, hâlâ kırmızıydı — WSL'deki KAYNAK hâlâ S95 taşıyordu.)*
      **Sabotaj döngüsünden sonra kaynağı İKİ TARAFTA da `grep`le doğrula.**
- [ ] **`--include="*.kem"` GÖMÜLÜ KAYNAKLARI GÖRMEZ.** Etki alanı ölçerken
      `test/*.c` içindeki C DİZGİSİNE gömülü KEMGU kaynakları ayrı bir
      yüzeydir. *(D-517: `bellek_*` etki alanını "4 dosya, 3'ü zaten uyumlu"
      diye ölçtüm; `test_llvm.c`deki gömülü kaynak taramanın dışındaydı ve
      tam takım orada kırmızı verdi. D-427'nin **"ön koşul ölçümünün KAPSAMI
      da ayrıca doğrulanmalı"** dersinin ikinci tekrarı.)*
- [ ] **`rc=0` bir SONUÇ değil bir İDDİADIR.** Kapı sayısını ve atlama izlerini
      AYRICA ölç. *(D-486: sekiz kapı sessizce atlanıyordu, make yine 0 döndü.)*

## Bu Oturumun Çalışma Kuralları

- Her dosya yazıldıktan sonra `make` çalıştır, sıfır uyarı hedefi (`-Wall -Wextra -Wpedantic`)
- Test binary'leri için ASan derleme bayrakları: `-fsanitize=address,undefined -g -fno-omit-frame-pointer`
- Bellek alan modüller için her test çalıştırmasında ASan aktif olmalı; `ERROR: AddressSanitizer` çıkarsa adım onaylanmaz
- Her mantıksal birim bitince Türkçe küçük git commit
- Türkçe UTF-8 hex escape kuralı: ayrıntı için yukarıdaki **"Türkçe UTF-8 Dikkat Noktası"** bölümüne bak (kural: her Türkçe karakter escape'inden sonra 0-9 / a-f / A-F geliyorsa concatenation şart)
- **D-numara tahsisi (DECISIONS_LOG):** D-NNN'i **merge anında, güncel `origin/main`'deki en yüksek D'ye bakıp** ver — branch açarken DEĞİL. Paralel oturumlar eski main'den dallanıp aynı numarayı kapar (yaşandı: D-076→082→086→088→092 zinciri + D-094 G004/güvensiz çakışması). PR açarken main ilerlemişse numarayı güncelle.
- **Seri ilerleme (dizi/codegen güvenlik işi):** Aynı dosyaya dokunan (özellikle `src/llvm.c`, `selfhost/codegen.kem`) dizi/bellek-güvenliği işini **tek daldan, seri** yürüt; main'e sık merge et. Paralel dallar aynı handler'da çakışır.
