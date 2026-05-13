# KEMGU Derleyici Mimarisi

Bu belge KEMGU derleyicisinin iç yapısını anlatır — kaynaktan ELF/native ikiliye
giden yolun her aşamasını ve hangi dosyaların hangi görevi üstlendiğini.

> Tutarsızlık varsa kod tek doğrudur. Yapılacak işlerle güncel için
> [`../CLAUDE.md`](../CLAUDE.md) "Mevcut Durum ve Yapılacaklar" bölümüne bak.

---

## 1. Derleme Hattının Üst Görünümü

```
.kem kaynak (UTF-8)
       │
       ▼
   ┌───────┐
   │ Lexer │   src/lexer.c, src/utf8.c, src/anahtar_kelime.c
   └───┬───┘
       │  Token akışı (TokenTipi, lexeme, satır, sütun)
       ▼
   ┌────────┐
   │ Parser │   src/parser.c, src/ifade.c (Pratt)
   └───┬────┘
       │  AST (DugumTipi, arena allocated)
       ▼
   ┌────────────┐
   │ Tip Kontrol│   src/tip.c, src/sembol.c, src/tip_kontrol.c
   └─────┬──────┘
       │  Tip-anote edilmiş AST + sembol tablosu
       ▼
   ┌────────────────────────┐
   │ Bölge + Escape Analizi │   src/escape.c, src/bolge.c, src/bolge_atama.c
   └────────────┬───────────┘
       │  Her tahsis için ρ_yerel / ρ_çağıran / ρ_iterasyon
       ▼
   ┌─────────────────────────┐
   │ Linear Type Analizi     │   src/tip_kontrol.c (lineer hooks)
   └────────────┬────────────┘
       │  L001..L008 + LR002 kontrolleri
       ▼
   ┌─────────────┐
   │ LLVM IR Üret │   src/llvm.c
   └─────┬───────┘
       │  Text-tabanlı LLVM IR (.ll)
       ▼
       clang -x ir prog.ll -o prog.exe
       │  veya cross: clang -target aarch64-unknown-none -x ir prog.ll -c
       ▼
   Native x86_64 / ARM64 ikilisi
```

LSP server alternatif giriş noktasıdır — aynı pipeline'ı dosya yerine
JSON-RPC mesajları üzerinden çalıştırır.

---

## 2. Aşamalar ve Sorumlu Dosyalar

### 2.1 Lexer

| Dosya                         | Görev                                      |
|-------------------------------|--------------------------------------------|
| [`src/utf8.c`](../src/utf8.c) | UTF-8 byte sequence tanıma, Türkçe karakter sınıflandırma |
| [`src/anahtar_kelime.c`](../src/anahtar_kelime.c) | 33 anahtar kelime tablosu — `tanımlayıcı` mı `anahtar` mi |
| [`src/lexer.c`](../src/lexer.c) | Tokenizer ana — sayı, metin, karakter, ham string, yorum |
| [`src/hata.c`](../src/hata.c) | Konum bilgili hata raporlama (callback hook ile LSP'ye gider) |

Lexer **stream**'dir (token başına bir çağrı). Parser kendi 1+lazy-2-token
lookahead tamponunu tutar.

> Spec: [`KEMGU_Lexer_Spesifikasyonu.md`](KEMGU_Lexer_Spesifikasyonu.md).

### 2.2 Parser (recursive descent + Pratt)

| Dosya                              | Görev                              |
|------------------------------------|------------------------------------|
| [`src/arena.c`](../src/arena.c)    | Arena allocator — AST tahsisleri için tek serbestleme |
| [`src/ast.c`](../src/ast.c)        | AST düğüm yapıları (Tagged union) |
| [`src/ast_yazdir.c`](../src/ast_yazdir.c) | Debug yazdırma (`--parse` modu) |
| [`src/parser.c`](../src/parser.c)  | Üst düzey + deyimler — recursive descent |
| [`src/ifade.c`](../src/ifade.c)    | İfade parser — Pratt + tip parser  |

**Strateji:**
- Deyimler ve tanımlar: recursive descent (okunabilir, hata mesajı yakın).
- İfadeler: Pratt parser (8 öncelik seviyesi, sol/sağ birleşme tablosu).
- AST: tagged union + arena → tek `arena_serbest()` ile temizlik.
- Hata kurtarma: panik modu — `;` / `}` / üst düzey anahtar kelime'de sync.
- `>>` token bölme: `Dizi<seçimlik<T>>` gibi generic kapanışlar için
  `parser_buyuk_ayir` ile sağa-kaydır `>>` iki `>` token'ına bölünür.

> Spec: [`KEMGU_Grammar_EBNF.md`](KEMGU_Grammar_EBNF.md).

### 2.3 Tip sistemi

| Dosya                                       | Görev |
|---------------------------------------------|-------|
| [`src/tip.c`](../src/tip.c)                 | `TipBilgisi` — kategori + ad + args; nominal eşitlik; yazdırma |
| [`src/sembol.c`](../src/sembol.c)           | Sembol tablosu — 8 kategori, 5 scope; parent-pointer linked list |
| [`src/tip_kontrol.c`](../src/tip_kontrol.c) | AST visitor — ifade + deyim + tanım tip kontrolü; bidirectional çıkarsama; generic monomorphization; constraint enforcement; linear hooks |

**Tasarım kararları:**
- Çıkarsama: lokal + bidirectional (Rust/Swift tarzı).
- Generic: monomorphization (LLVM aşamasında her tip args için ayrı fonksiyon).
- Eşitlik: nominal (ad + args; structural değil).
- Sayı literal: context-dependent, default `tam32`.

Hata kodları: `T001` (tip uyumsuzluğu) … `T031` (bilinmeyen özellik).
Linear hata kodları: `L001` … `L008`, `LR002`.

### 2.4 Bölge sistemi

| Dosya                                       | Görev |
|---------------------------------------------|-------|
| [`src/bolge.c`](../src/bolge.c)             | Bölge temsili — 9 kategori, ömür sıralama, LCA hesabı |
| [`src/escape.c`](../src/escape.c)           | DFA + fixed-point escape analizi |
| [`src/bolge_atama.c`](../src/bolge_atama.c) | R-LIT, R-YEREL, R-VER, R-İTERASYON, R-KOŞUL — escape ile entegre |

**Escape analizi:**
- Forward DFA, fixed-point iterasyon (max 16 pass).
- Her AST düğümü için escape kategorisi: `ESC_YEREL`, `ESC_ITERASYON`, `ESC_CAGIRAN`.
- Değişken bağlama izleme: scope stack, append-only on assign (MAY-flow konservatif).
- `ver` ile dönen değer + tanımlayıcı zinciri → tüm alt-tahsis bölgeleri `CAGIRAN`'a terfi.

**Bölge atama:**
- Escape NULL → eski syntax tabanlı davranış (geriye uyumlu).
- Escape bağlı → AST düğümünden direkt kategori, daha keskin.

> Formal: [`KEMGU_Bellek_Modeli.md`](KEMGU_Bellek_Modeli.md) — Katman 1.

### 2.5 Linear types analizi

`src/tip_kontrol.c` içinde lineer-spesifik hook'lar:
- `lineer_tuketildi` ve `lineer_scope_seviyesi` sembol flag'leri.
- `sembol_bul_yazilabilir` — tüketim işaretleme için mutable lookup.
- Bölge (blok/işlev/için/eşleş) kapanışında tüketilmemiş bağlar → L001.
- Lambda gövdesi içinde lineer yakalama → closure tipi otomatik `tekkez<işlev(...)>` (LC-2).
- Tüketim noktaları: `kullan`, `imha`, çağrı arg, `ver`, yapı alan değeri, değişken atama (move).

> Spec: [`KEMGU_Linear_Types_Spec_V1.md`](KEMGU_Linear_Types_Spec_V1.md).

### 2.6 LLVM IR backend

| Dosya                           | Görev                              |
|---------------------------------|------------------------------------|
| [`src/llvm.c`](../src/llvm.c)   | Text-based IR üretici — libLLVM yok, sadece string output |

**Mimarisi:**
- `LlvmGen` state + SSA register sayacı + lineer isim tablosu.
- Pre-pass'ler:
  1. Metin literalleri → `@.str.N = private constant [K x i8] c"..."`.
  2. Yapı tanımları → `%YapiAdi = type { tip1, ... }`.
  3. İşlev imza tablosu → çağrı dönüş tipini bilmek için.
- Tip tracking: her ifade `(reg, llvm_tip)` döner.
- Multi-int: `tam8`/`dtam8` → `i8`, `tam16`/`dtam16` → `i16`, vb.
- Float: `kesirli32` → `float`, `kesirli64` → `double` (fadd/fsub/fmul/fdiv/frem).
- Yapı: by-value param + dönüş (`alloca + store + load`).
- Dizi: `alloca [N x T]` + GEP+store + load T.
- Generic monomorphization: çağrıda arg'dan T çıkarsanır, mangled name (`kimlik$i32`), worklist sonrası emit. Duplicate emission `MonoKayit` ile engellenir.

**Yapı izlenimi:**
```kemgu
işlev topla(c: Cift) -> tam32 { ver c.a + c.b; }
// %Cift = type { i32, i32 }
// define i32 @topla(%Cift %c) { ... }
```

**ARM64 cross:** `clang -target aarch64-unknown-none -x ir prog.ll -c` —
mevcut IR portable; ARM64 ELF object üretir. Test:
`make calistir_arm64_test` ([`Makefile`](../Makefile) satır 309+).

### 2.7 LSP server

| Dosya                           | Görev                              |
|---------------------------------|------------------------------------|
| [`src/json.c`](../src/json.c)   | Minimal JSON parser + yazıcı (arena-tabanlı) |
| [`src/lsp.c`](../src/lsp.c)     | JSON-RPC 2.0, Content-Length framing, capabilities |

**Mevcut yetenekler (v2):**
- `initialize` → capabilities (textDocumentSync=1, hover, completion, definition).
- `didOpen`/`didChange`/`didClose` — belge state'inde AST cache.
- `publishDiagnostics` — `hata.c` callback hook'undan beslenir.
- `textDocument/hover` — tanımlayıcı üzerine markdown (ad + kategori).
- `textDocument/definition` — tanımın konumu.
- `textDocument/completion` — keyword + üst düzey semboller.

**Sınırlar:** incremental sync yok, workspace mesajları yok, lokal değişken hover yok.

### 2.8 KDL Runtime

| Dosya                                   | Görev |
|-----------------------------------------|-------|
| [`runtime/kdl_runtime.c`](../runtime/kdl_runtime.c) | KEMGU runtime primitif şablonu — link entegrasyonu test edilmekte |

KDL runtime, gelecekte allocator + syscall layer + thread runtime için ana
bağlanma noktası. Şu an `make calistir_runtime_link_test` ile compile + link
doğrulaması yapılır. KIRMIZI_QUEUE A/G karara bağlı genişletilecek.

---

## 3. Test Mimarisi

```
test/
├── test_<modül>.c        # Birim testleri (modül başına bir dosya)
├── test_snapshot.c       # Snapshot framework
├── test_fuzz.c           # Parser fuzzer (10k iter)
├── test_runtime_link.c   # Runtime link doğrulama
├── snapshots/            # AST/IR snapshot referansları
├── stdlib/               # Stdlib --check test dosyaları (.kem)
└── ornekler/             # End-to-end .kem örnekler
```

### 3.1 Birim testleri (modül başına)

Her modülün kendi test dosyası ([`test/test_<modül>.c`](../test/)).
Format:

```c
TEST("açıklama", { ... });
ASSERT(koşul);
```

Çıktı: `[N] açıklama ... ✓` veya `✗`, sonunda `=== X/Y test geçti ===`.

| Modül              | Test sayısı | ASan |
|--------------------|-------------|------|
| Lexer              | 103/103     | n/a (malloc lexer'da) |
| Arena              | 19/19       | ✓ |
| AST                | 31/31       | ✓ |
| Parser             | 90/90       | ✓ |
| Tip                | 26/26       | ✓ |
| Sembol             | 18/18       | ✓ |
| Tip kontrol        | 97/97       | ✓ |
| Bölge              | 17/17       | ✓ |
| Bölge atama        | 13/13       | ✓ |
| Escape             | 17/17       | ✓ |
| JSON               | 21/21       | ✓ |
| LSP                | 6/6         | ✓ |
| LLVM (E2E)         | 30/30       | n/a (clang + run) |
| Linear             | 54/54       | ✓ |

> Sayılar [`../CLAUDE.md`](../CLAUDE.md) "Tamamlanan" bölümünden alınmıştır;
> tek doğru kaynak `make test_tumu` çıktısıdır.

### 3.2 Snapshot testleri

[`test/test_snapshot.c`](../test/test_snapshot.c). Bir `.kem` dosyasını
parser'dan geçirir, AST'yi serileştirir, [`test/snapshots/`](../test/snapshots/)
altındaki referansla diff alır. Sapma varsa test fail.

### 3.3 Parser fuzzer

[`test/test_fuzz.c`](../test/test_fuzz.c). 10000 iterasyon rastgele token
sekansı parser'a verir; segfault / ASan hatası / sonsuz döngü olmaması
beklenir. Parser robustness için.

### 3.4 Stdlib --check testleri

[`Makefile`](../Makefile) `calistir_stdlib_check` hedefi her stdlib modülünü
karşılık gelen `test/stdlib/test_<modül>.kem` ile birleştirip `--check`'ten
geçirir (import sistemi yok → tek dosya derleme).

### 3.5 LLVM end-to-end

[`test/test_llvm.c`](../test/test_llvm.c). Her test:
1. `.kem` dosyasını `kemgu --llvm` ile IR'ye çevir.
2. IR'yi `clang -x ir prog.ll -o prog.exe` ile derle.
3. `./prog.exe` çalıştır, exit code'u beklenenle karşılaştır.

Gerçek programları çalıştırarak compiler'ın ucundan ucuna doğrulanması.

### 3.6 ARM64 cross-compile

[`Makefile`](../Makefile) `calistir_arm64_test`:
```bash
kemgu --llvm kernel.kem | clang -target aarch64-unknown-none -x ir -c -o kernel_aarch64.o
file kernel_aarch64.o    # → ELF 64-bit LSB relocatable, ARM aarch64
```

DGX Spark + Android NDK altyapısı. Host ARM64 olmadığı için çalıştırma yok —
yalnız derleme + section header doğrulama.

---

## 4. Cross-Platform Yapı

Makefile platform/mimari tespiti:

```makefile
ifeq ($(OS),Windows_NT)
    EXE := .exe
    PLATFORM := windows
    ARCH := x86_64
else
    UNAME_S := $(shell uname -s)   # Linux, Darwin
    UNAME_M := $(shell uname -m)   # x86_64, aarch64, arm64
    ...
endif
```

**Birincil hedef:** Windows x86_64 (geliştirme).
**Hedefler:** Linux x86_64/ARM64, macOS arm64, DGX Spark (ARM64), Android NDK.

### 4.1 Dual-compiler

Win11 26200'de ASan/UBSan runtime'ı için Clang64 gerekli — UCRT64 GCC bu
runtime'ı içermiyor. Makefile bu yüzden:

| Hedef                | Derleyici  | Sebep                        |
|----------------------|------------|------------------------------|
| `build/kemgu.exe`    | UCRT64 GCC | Prod ikilisi; ASan'sız hızlı |
| `build/test_lexer.exe` | UCRT64 GCC | Lexer malloc kullanmıyor    |
| `build/test_*.exe` (diğer) | Clang64 | ASan + UBSan runtime tam destek |

### 4.2 Standart ve uyarılar

- C11 (`-std=c11`). Platform-özel uzantı yok.
- `-Wall -Wextra -Wpedantic` — sıfır uyarı hedefi.
- Sabit boyutlu tipler: `int64_t`, `uint32_t`, vb. (çıplak `int`/`long` YASAK).
- UTF-8 kaynak kodu; Türkçe karakter escape kuralı (CLAUDE.md "Türkçe UTF-8 Dikkat Noktası").

### 4.3 Bağımlılıklar

KEMGU derleyicisi C11 standart kütüphanesi dışında **hiçbir** harici
bağımlılık kullanmaz. LLVM tarafı yalnız text IR üretir; `libLLVM` ile link
yok. Cross-compile için `clang` ihtiyacı kullanıcı tarafında.

---

## 5. Modül Bağımlılıkları

```
ana.c ───────────┬──────────────────────────────────────────────────────┐
                 │                                                         │
                 ▼                                                         ▼
              lsp.c ──→ json.c                                         llvm.c
                 │         │                                              │
                 ▼         │                                              ▼
            tip_kontrol.c ──→ tip.c                                    bolge.c
                 │            sembol.c   ◄─── escape.c ◄─── bolge_atama.c
                 ▼            (kategori)        (DFA)        (R-* aksiyom)
              parser.c ───→ ifade.c
                 │            │
                 ▼            ▼
              ast.c ←──── ast_yazdir.c
                 │
                 ▼
              arena.c
                 │
                 ▼
              lexer.c
                 ▼
              utf8.c
              anahtar_kelime.c
              hata.c (her yerden çağrılır, LSP callback ile)
```

Header `-MMD -MP` ile otomatik dependency tracking; `.h` değişikliğinde
ilgili `.o` rebuild edilir.

---

## 6. Sürüm Etiketleri

KEMGU şu an sürüm-öncesi geliştirme aşamasında. Önemli kilometre taşları
git tag olarak işaretlenmiş değil (yol haritada). Yakın gelecek için
beklenen tag isimleri (Mehmet onayı ile):

- `v0.1-derleyici` — Lexer→LLVM IR ucu uca, mevcut özelliklerle.
- `v0.2-concurrency` — `görev` / `kanal` syntax + Katman 2 uygulaması.
- `v0.3-self-host` — KEMGU ile yazılmış bootstrap derleyici.
- `v1.0-os-mvp` — Minimal işletim sistemi (kernel + birkaç sürücü).

> Yol haritası kararlı değil — direktif Ek v1.1 Bölüm 3'e tabi.

---

## Devam

- Dil rehberi: [`KILAVUZ.md`](KILAVUZ.md).
- Onboarding: [`BASLAMAK.md`](BASLAMAK.md).
- Geliştirici notları: [`../CLAUDE.md`](../CLAUDE.md) (her oturumda otomatik okunur).
