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
  paritesi + FIXPOINT. **Bedel (V1 known-limit, T18):** `sonuç` içindeki lineer `görev<T>`
  için L001 leak uyarısı tetiklenmez (`tip_lineer_mi` sonuç'a özyinelemez) — eşleş'siz
  düşen görev join edilmez; bellek-güvenliği değil liveness kaybı. V2: lineer-yayılım +
  eşleş lineer-tüketimi. **Ergonomi:** her görev_başlat bir `eşleş` ister (`?`-yayılımı V2).
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
  - Semaforlar / bariyerler (Plan Karar F V2)
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
  **Bilinçli sınır:** `seçimlik<T>` ve `sonuç<T,H>` dalları YOK — self-host'ta
  bileşik tip temsili yok; kapanması tip çıkarsaması işine bağlı.
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
- **LSP v3** (incremental sync, workspace, semanticTokens, references)
- ~~**LLVM v4** (dizi param/return, dizi length, generic islev codegen)~~ ✓ **ZATEN YAPILMIŞ**
  (2026-07-17 ölçümü — bu madde ESKİMİŞTİ, sonraki işlerde D-085/D-088 vb. ile kapanmış ama
  roadmap güncellenmemiş). Ampirik doğrulama (derle+çalıştır+exit): dizi param `topla(xs:
  Dizi<tam32>)`→42 ✓, dizi dönüş `yap() -> Dizi<tam32>`→42 ✓, `dizi_boyut`→3 ✓, generic
  `kimlik<T>(x:T)->T` → 42 ✓, generic+metin → 5 ✓. **DERS:** roadmap maddelerini başlamadan
  ölç — eskimiş olabilir.
- **Stdlib network/JSON/regex** (runtime altyapı sonra)
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

### 🎯 SELF-HOST CHECKER PARİTESİ TAMAMLANDI (D-350..D-387, 2026-08-06)
**Ölçülmüş durum — yukarıdaki "26 tanı kodu / kalan ~48" notu ARTIK GEÇERSİZ:**
- **Self-host tanı kodu: 70** (D-350'de 24). Kalan `T015`/`T023` **ÖLÜ** (C parser'ı
  o şekilleri zaten reddediyor) → **portlanacak tanı kodu KALMADI.**
- **GENİŞ ÖLÇÜM 131/131 TAM PARİTE:** `stdlib` + `stdlib/temel` + `test/ornekler` +
  `kütüphane` + `test/moduller` yüzeyinde C oracle ile **sıfır fark** — ne
  yanlış-pozitif ne eksik tanı (`kem_os.kem` dâhil).
- **Kapılar:** `checker_diff` **148/148** (0 muaf) · `parser_diff` 13/13 ·
  `codegen_diff` 113/113 · sürücü 4 mod × 2 sürücü + FIXPOINT · `check_kapisi`
  210/217 (0 RED) · C birim **903 test**.
- **Bileşik tip temsili (D-377..D-386):** `Dizi<E>` · `seçimlik<T>` · `sonuç<T,H>` ·
  `&T`/`&değişken T`/`*T` · `tekkez<T>`/`yetki<R>` · `görev<T>`/`kanal<T>` ·
  `olarak` ifadesinin tipi · yapı ALAN tipleri · `eşleş` desen-bağlama tipleri ·
  Katman 2 intrinsik dönüşleri. **KALAN:** `işlev(..)->T` — temsil biçimi bir
  TASARIM kararıdır, **Mehmet'e sorulmadan sabitlenmeyecek**.
- **`görev<T>` LİNEERDİR** (`kanal<T>` değil) — D-384.

### 🎯 SELF-HOST CODEGEN PARİTESİ TAMAMLANDI (D-388..D-394, 2026-08-06)
Checker paritesi doygunlaşınca ölçüm CODEGEN'e çevrildi. `codegen_diff` yalnız
`cg_korpus` üzerinde koşuyordu; `test/ornekler` + `stdlib/temel`e karşı ölçünce
**31 sapma** çıktı. Yedi partide kapatıldı:
- **⚠ DÜZELTME (D-395): "GERÇEK SAPMA SIFIR" İDDİAM YANLIŞTI.** Elle koşturduğum
  ölçüm döngüsünde link başarısızlıkları **sessizce `fark` sayılıp yazdırılmıyordu**;
  ben o iki dosyayı "beklenen bare-metal segfault" sandım. Kapıyı Makefile'a bağlayıp
  koşunca gerçek yüz çıktı: `gorev_temel` ve `matris_carpim` **GERÇEKTEN
  başarısız** (aşağıda). Doğru sayı 65/67'dir, 65/65 değil.
  **DERS: elle koşturulan ölçüm döngüsü kapı DEĞİLDİR** — kapı sessiz düşen dalı
  olmayacak biçimde yazılır ve `[ "$fail" -eq 0 ]` ile biter. Ölçümü kapıya
  bağlamadan "sıfır sapma" DEME.
- Bare-metal keşif dosyaları (`kem_mmio_ham`, `kem_pointer`) host'ta eşlenmemiş
  MMIO adresi okuyor → **C DE segfault ediyor**, self-host BİREBİR aynı → parite
  doğru, kusur değil, muafiyet gerekmez. (`03_kontrol.kem` exit 151 de ÇÖKME
  DEĞİL: kaynağın kendisi `120+30+1=151` yazıyor — "exit>128 = çökme" yanıltıcı.)
- **`codegen_diff` 113 → 119** (0 muaf) + **YENİ KAPI `calistir_codegen_genis`**
  (D-395): `test/ornekler` + `stdlib/temel` üzerinde exit koduna EK OLARAK
  **stdout** karşılaştırır. `codegen_diff`in dar korpusu bu 31 sapmanın HİÇBİRİNİ
  görmüyordu; `bignum_selfhost` iki tarafta da exit 0 verirken stdout'ta `0`
  yerine yığın adresi basıyordu — yalnız exit'e bakan kapı bunu KAÇIRIR.
  Durum: **65/67, 2 muaf** (aşağıdaki iki kök).
- **🎯 MUAFİYET LİSTESİ BOŞALDI — kapı 67/67.** Kurulduğunda 2 satır vardı;
  ikisi de kapatıldı (D-396, D-397). Tasarlandığı gibi: muafiyet listesi
  küçülmek içindir.
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

### 🔶 `Dizi<kesirli64>` — DE-RİSK EDİLDİ, ama ORACLE DEĞİŞİKLİĞİ İSTİYOR (D-417 sonrası)
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
- **Sıradaki kapısız yüzeyler:** `test/snapshots` (81 dosya), `test/ornekler/eski`
  (16), `test/asan_matris` (12), `test/stdlib` (9), `test/crossfile` (2).
  `test/check_korpus` D-408'de ölçüldü (31/32).

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

## Bu Oturumun Çalışma Kuralları

- Her dosya yazıldıktan sonra `make` çalıştır, sıfır uyarı hedefi (`-Wall -Wextra -Wpedantic`)
- Test binary'leri için ASan derleme bayrakları: `-fsanitize=address,undefined -g -fno-omit-frame-pointer`
- Bellek alan modüller için her test çalıştırmasında ASan aktif olmalı; `ERROR: AddressSanitizer` çıkarsa adım onaylanmaz
- Her mantıksal birim bitince Türkçe küçük git commit
- Türkçe UTF-8 hex escape kuralı: ayrıntı için yukarıdaki **"Türkçe UTF-8 Dikkat Noktası"** bölümüne bak (kural: her Türkçe karakter escape'inden sonra 0-9 / a-f / A-F geliyorsa concatenation şart)
- **D-numara tahsisi (DECISIONS_LOG):** D-NNN'i **merge anında, güncel `origin/main`'deki en yüksek D'ye bakıp** ver — branch açarken DEĞİL. Paralel oturumlar eski main'den dallanıp aynı numarayı kapar (yaşandı: D-076→082→086→088→092 zinciri + D-094 G004/güvensiz çakışması). PR açarken main ilerlemişse numarayı güncelle.
- **Seri ilerleme (dizi/codegen güvenlik işi):** Aynı dosyaya dokunan (özellikle `src/llvm.c`, `selfhost/codegen.kem`) dizi/bellek-güvenliği işini **tek daldan, seri** yürüt; main'e sık merge et. Paralel dallar aynı handler'da çakışır.
