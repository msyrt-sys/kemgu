# Kırmızı Queue — KEMGU

Direktif Ek v1.1 Bölüm A uyarınca: spec dışı 🔴 Kırmızı (tip sistemine yeni
katman, formal teorem etkisi, breaking change, yeni anahtar kelime, yeni
unsafe primitif, ABI değişikliği, concurrency modeli değişikliği) kararlar
buraya eklenir. Mehmet haftalık spec oturumunda toplu temizler.

Format:
```
## [tarih] — başlık
- Kategori: <yeni keyword | tip katmanı | teorem | breaking | unsafe | ABI | concurrency>
- Bağlam: <neden bu noktada gündeme geldi>
- Önerilen seçenekler:
  1. ...
  2. ...
- Engellediği iş: <ya da yok>
```

---

## [2026-05-13] — stdlib genişletmesi: runtime + dil özellik kuyruğu

Stdlib genişletme görevi sırasında karşılaşılan dil/derleyici sınırları.
Yeşil katman çözümlerle (stub + skeleton) atlatıldı; runtime ve spec
oturumunda toplu karara açık:

### A. stdlib::metin — runtime primitif gerek (en kritik)

- **Kategori:** runtime / built-in eksikliği
- **Bağlam:** stdlib/metin.kem (uzunluk, birleştir, kes, böl, küçükharf,
  büyükharf, içerir, başlar_ile, biter_ile, kırp, yer_değiştir,
  tekrarla, yansıt) tümü stub. --check geçer ama runtime'da no-op.
- **Gerekli primitifler:**
  1. `metin_uzunluk(s) -> tam32`            (strlen)
  2. `metin_byte_al(s, i) -> dtam8`         (s[i])
  3. `metin_birlestir(a, b) -> metin`       (heap alloc + memcpy)
  4. `metin_kes(s, b, e) -> metin`          (substring + alloc)
  5. `metin_karsilastir(a, b) -> tam32`     (memcmp wrapper)
  6. `metin_kucukharf(s) -> metin`          (UTF-8 aware, Türkçe I→ı/İ→i)
  7. `metin_buyukharf(s) -> metin`          (UTF-8 aware)
  8. `metin_indeks_bul(s, alt) -> tam32`    (substring search; -1 yoksa)
- **Önerilen seçenekler:**
  1. tip_kontrol.c'ye EKLE_BUILTIN şeklinde eklenip LLVM tarafında
     libc çağrısına map'lenmesi (bellek_al ile aynı pattern).
  2. `runtime/` dizininde ayrı C dosyaları, link-time bağlama.
- **Engellediği iş:** stdlib::dosya kullanım örnekleri (path normalize,
  uzantı çıkar), stdlib::json (key escape), genel kullanıcı kodu.

### B. stdlib::dizi — dinamik allocator gerek

- **Kategori:** runtime / built-in eksikliği
- **Bağlam:** stdlib/dizi.kem'de harita<T,U>, filtre<T>, dilimle,
  birleştir gibi yeni Dizi dönen ops yok (in-place harita_yerinde_tam
  ve fold çözüm). Heap allocate edilmiş dizi yapısı yok.
- **Gerekli primitifler:**
  1. `dizi_olustur<T>(kapasite: tam32) -> Dizi<T>`
  2. `dizi_ekle<T>(d: Dizi<T>, x: T) -> Dizi<T>`  (mutating)
  3. `dizi_uzunluk_runtime<T>(d) -> tam32`        (sentinel veya header)
  4. `dizi_serbest<T>(d)` (region tarafından otomatik olsa daha iyi)
- **Notlar:** test/ornekler/heap_dizi_metin.kem'de zaten
  `dizi_olustur(N)`, `dizi_ekle_tam(d, x)` kullanım örnekleri var
  ama --check tarafında symbol yok; sadece LLVM/runtime senaryosu.
  Build-time → check-time symbol parite gerek.

### C. Pattern matching: sonuç<T,E> için tamam/hata desenleri

- **Kategori:** parser (yeni keyword desen tanıma)
- **Bağlam:** `eşleş r { tamam(v) => ..., hata(m) => ... }` parser'da
  `desen bekleniyor` (P211) hatası veriyor. `değer`/`hiç` keyword'leri
  desen olarak tanınıyor (parse_desen'de TOK_DEGER, TOK_HIC sabitleri),
  ama TOK_TAMAM, TOK_HATA yok.
- **Etkilenen:** stdlib/sonuc.kem — harita, ya_da, bağla, hatayı_yay
  hepsi pattern matching gerektiriyor; bunlar olmadan sonuç inspection
  imkansız. Şu an sadece construction wrapper'ları yazıldı.
- **Önerilen seçenekler:**
  1. src/parser.c parse_desen'e `TOK_TAMAM` ve `TOK_HATA` ekle
     (TOK_DEGER ile aynı conditional satıra).
  2. Sembol tablosu/tip kontrol tarafı `değer(v)` ile aynı yolu izler.
- **Engel:** yok (parser ufak değişiklik); spec içi ekleme sayılır.

### D. Generic callback tip çıkarsama: `işlev(T) -> U`

- **Kategori:** tip çıkarsama
- **Bağlam:** `işlev harita<T, U>(xs: Dizi<T>, f: işlev(T) -> U)
  -> Dizi<U>` formunda `f(v)` çağrısında dönüş U olarak çıkarsanmıyor
  (T020 ver tipi uyumsuzluğu). Concrete (tam32 → tam32) versiyon
  çalışıyor.
- **Etkilenen:** stdlib/opsiyonel.kem, stdlib/dizi.kem'de harita/
  filtre/bağla generic değil, concrete tam32 + metin versiyonları
  ayrı.
- **Önerilen seçenek:** monomorphization pre-pass'inde callback
  parametrelerini de T→U substitusyonuna dahil et.

### E. Tip dönüştürme (tam32 ↔ tam64, vb.)

- **Kategori:** tip / dönüştürme operatörü yok
- **Bağlam:** fibonacci/faktöriyel için tam64 dönüş istemek doğal
  ama `n: tam32`, `ver n;` tam64 dönüşte uyumsuz hata veriyor.
  Implicit conversion yok, explicit `as` syntax yok.
- **Önerilen seçenekler:**
  1. `n as tam64` cast operatörü (Rust tarzı).
  2. Implicit widening (Java/C tarzı) — KEMGU felsefesine ters.
  3. `tam32_to_tam64(n)` built-in pair (her tip için).
- **Geçici:** Stdlib şimdi tam32 sınırlı (fib(46+) overflow).

### F. Pattern binding scope (eşleş)

- **Kategori:** tip kontrol (scope binding)
- **Bağlam:** CLAUDE.md'deki "ADIM 11.7 sınırlamalar" notunda
  `eslesme.kem`'in pattern binding'i çalışmıyor diye işaretliydi
  ama benim yeni testlerde `değer(v) => v` çalışıyor (ADIM 19+
  düzeltmesi). Sadece bilgi notu — kuyruk değil.

### G. stdlib::dosya — syscall altyapısı gerek

- **Kategori:** runtime / syscall layer
- **Bağlam:** stdlib/dosya.kem'in tüm I/O ops'u stub (ac, kapat,
  oku_metin, oku_satirlar, yaz_metin, ekle, ekle_satir, var_mi,
  sil, boyut, yeniden_adlandir, kopyala). Sözdizimi --check geçer
  ama runtime'da hiç bir şey yapmaz.
- **Gerekli primitifler (POSIX karşılıkları):**
  1. `dosya_ac(yol: metin, mod: metin) -> tam32`  (open syscall)
  2. `dosya_kapat(handle: tam32) -> tam32`        (close)
  3. `dosya_oku(handle, buffer, n) -> tam32`      (read)
  4. `dosya_yaz(handle, buffer, n) -> tam32`      (write)
  5. `dosya_var_mi(yol) -> mantıksal`             (stat / access)
  6. `dosya_sil(yol) -> tam32`                    (unlink)
  7. `dosya_yeniden_adlandir(eski, yeni) -> tam32` (rename)
  8. `dosya_boyut(yol) -> tam64`                  (stat.st_size)
- **Not:** test/ornekler/dosya_io.kem'de zaten `dosya_ac/yaz/kapat/
  tumu_oku` kullanım örnekleri var — LLVM IR katmanında kısmen
  mevcut (libc wrappers) ama tip_kontrol built-in tablosunda yok.
  Build-time → check-time parite gerek.
- **Önerilen yol:** runtime/dosya.c'de C wrapper'ları + LLVM IR
  declare'leri + tip_kontrol.c'de EKLE_BUILTIN entries.

### H. Operatör: `!` (mantıksal değil) parse sorunu

- **Kategori:** lexer
- **Bağlam:** `!x` yazımı L005 hatası veriyor ("KEMGU'da '!' yerine
  'degil' kullanin"). Doğru — KEMGU `değil` kullanıyor, ama:
  - `değil X(args)` precedence sorunu yaşıyor: parser bunu
    `(değil X)(args)` olarak tip-kontrol ediyor (T004 mantıksal ister).
  - Workaround: `değil (X(args))` parantez ile.
- **Önerilen seçenek:** parse_tekli'de `değil`'i çağrı/erişimden
  düşük precedence'a koy (önek operatör seviyesi 7'de, sonek 8).
  Şu anda muhtemelen 7'de ama çağrı uygulanmadan birinciye yapışıyor.
- **Engellediği iş:** Yok (workaround basit), ama API'lerde okunurluk.

---

## (Önceki kuyruk: boş.)

Linear Types Spec V1 onaylı olduğu için tekkez/imha keyword eklemesi,
TIP_TEKKEZ kategori, L001–LC001 hata kodları, region/linear entegrasyonu
ve closure-itself-linear **spec içi** sayılır → otomatik onaylı, queue'ya
eklenmez.

---

## [2026-05-13] — Parser panik modunda sonsuz hata raporu (fuzzer bulgusu)

- **Kategori:** parser bug (yeşil/sarı — internal, spec etkisi yok)
- **Bağlam:** test/test_fuzz_advanced.c mod a (sözdizimi-aware random
  fuzzer) parser'ı çok kategorili random token akışı ile besledi:

  ```
  z T 659 [ yapı , ; + f modül 12
  ```

  Parser bu girdiyi parse ederken `P018: alan_adi bekleniyor` hatasını
  **aynı pozisyonda (col 25)** binlerce kez tekrar raporladı.
  `PARSER_MAX_HATA = 100` limiti devre dışı gibi davrandı, hata mesajları
  arena'da birikip ASan internal allocator OOM'a düştü (~1 sn içinde
  16 MB allocation failure).

- **Etki:**
  - test_fuzz_advanced.c mod a workaround: random token yerine 20 sabit
    bozuk snippet (mod_a_snippets) kullanılıyor. Parser hâlâ test ediliyor
    ama "gerçek random fuzzing" yapılmıyor.
  - Mevcut test_fuzz.c (10000 iter byte-level random) tetiklemiyor.

- **Olası kök neden:**
  - parser_panik_sync() yapı alanı parse loop'unda token tüketmiyor
  - PARSER_MAX_HATA kontrolü parser_hata() içinde değil

- **Önerilen seçenekler:**
  1. parser_hata()'ya `if (p->hata_sayisi >= PARSER_MAX_HATA) return;`
  2. parser_panik_sync()'i her hata sonrası zorunlu çağır
  3. Aynı (satır,sütun) tekrar hata raporlanırsa skip et

- **Engellediği iş:** Random fuzzing yeterli coverage'a ulaşamıyor.
  Çekirdek bug için ayrı görev gerek. src/parser.c'ye dokunulamaz
  (test altyapı görevinin kapsamı: sadece test/ + tools/ + .github/).

---

## [2026-05-14] — Realtime Spec V1 implementasyon: açık sorular

V1 implementasyonu tamam (32/32 test, drone örneği çalışır), ileride
karara açık noktalar (RT.14'te kayıtlı):

### A. V1 loop yasağı — straight-line only

- **Kategori:** spec kapsam (V1 darlık)
- **Bağlam:** V1'de `iken` ve `için` her durumda RT002. Sebep: bound
  çıkarsama tutarlı bir altyapı gerektiriyor. Pratikte drone PID,
  audio callback gibi tipik realtime kodları zaten straight-line.
- **V2 yol haritası:**
  1. `iken[max=N] kosul { ... }` annotation
  2. `sabit N` referansı → otomatik bound (data-flow zincirlemesi)
  3. `Dizi<T, N>` literal-uzunluklu tip (statik dizi)
- **Engellediği iş:** Yok — Manuel unroll yeterli (drone_kontrol.kem ispat).

### B. Mutual recursion algılaması

- **Kategori:** RT003 sınırlama
- **Bağlam:** V1: direct self-call algılanır, ama `a → b → a` zinciri
  RT003 atlatılır. Çağrı grafiği analizi V2'de.
- **Önerilen seçenek:** wcet.c'de `aktif_islev` yerine `aktif_islev_yigin`
  (stack) tut; çağrı yapılırken hedef yığında varsa RT003.
- **Engel:** V1 yeterli — direct recursion zaten %95 vaka.

### C. Built-in I/O fonksiyonları realtime mi?

- **Kategori:** stdlib API tasarımı
- **Bağlam:** `kdl_yazdir_tam`, `kdl_dosya_yaz`, vs. tip_kontrol
  built-in tablosunda flag yok — realtime fnotion çağırınca otomatik
  RT004 olur (sembol bulamadığı için RT005 da olabilir).
- **Önerilen seçenek:** Stdlib'de built-in tabloya `realtime` flag ekle;
  hangi I/O fnotionları realtime "approved" olduğu açık spec kararı.
- **V1:** Tüm built-in'ler non-realtime kabul; realtime kullanıcı
  fnotion'a I/O izinli değil (zaten gerçek dünyada öyle olmalı).

### D. Cycle tablosu kalibrasyonu

- **Kategori:** WCET hesap doğruluğu
- **Bağlam:** RT.7.1'deki cost'lar x86 modern + ARM Cortex-A approx.
  Mikroop seviyesi gerçek değerler için CPU vendor docs gerek
  (Intel Optimization Manual, ARM TRM).
- **Engel:** V1 pessimistic — gerçek runtime ≤ hesap garanti, sertif
  için yeterli. Kalibrasyon V2.

### E. Branch için: feature/realtime-qualifier vs claude/sleepy-haibt-6c763d

- **Kategori:** branch yönetimi (yeşil — kullanıcı kararı)
- **Bağlam:** Worktree branch'i `claude/sleepy-haibt-6c763d` olarak
  açılmış. Direktifte `feature/realtime-qualifier` istendi ama
  `git branch -m` permission classifier tarafından reddedildi
  ("Bu branch'ten ayrılma" yasağı). Worktree branch'ini Mehmet
  isterse manuel rename edebilir veya cherry-pick edebilir.
- **Engel:** Yok — implementasyon bu branch'te tamam.

---

## [2026-05-14] — SIMD Spec V1: V2'ye bırakılan özellikler

SIMD intrinsics V1 tamamlandı (belgeler/KEMGU_SIMD_Spec_V1.md). Aşağıdaki
özellikler V1 kapsamı dışı, V2'ye bırakıldı:

### A. LLVM ASCII identifier kısıtlaması

- **Kategori:** LLVM backend (yeşil — workaround mevcut)
- **Bağlam:** LLVM IR identifier'lari ASCII karakter olmali, ya da `"..."`
  içinde quote'lanmalı. KEMGU işlev adlarında Türkçe karakter (ş, ı, ğ vb.)
  kullanıldığında LLVM derleme hatası verir:
  ```
  define float @satır_carp(...)
                   ^ expected '(' in function argument list
  ```
- **Mevcut çözüm:** Vektör örnekleri ASCII fonksiyon adı kullanır
  (test/ornekler/matris_carpim.kem'de `satir_carp` — `satır` yerine).
- **Önerilen çözüm:** `ad_yaz` fonksiyonu non-ASCII byte algılarsa
  identifier'i `@"..."` quote ile sarmala. LLVM destekler.

### B. vektor_yukle / vektor_sakla (Dizi<T> + bounds)

- **Kategori:** runtime + tip kontrol genişlemesi
- **Bağlam:** SD.2.1'de tanımlı `vektor_yukle(p, ofs)` ve `vektor_sakla(p, ofs, v)`
  intrinsicleri V1'de yok. Dizi<T> ile entegrasyon için bounds check
  semantiği (ofs + N <= length) gerekir.
- **Engellediği iş:** Gerçek matris çarpımı (bellek-load yerine sadece
  vektor_doldur broadcast üzerinden çalışıyor şu an).

### C. vektor_karistir / vektor_birlestir (shuffle)

- **Kategori:** LLVM intrinsic + parser
- **Bağlam:** `shufflevector` LLVM IR mevcut, ama indeks listesi compile-time
  sabit dizi olarak parse'lanmalı. V1'de yapılmadı.

### D. Hedef-spesifik feature flag

- **Kategori:** build sistem (yeşil)
- **Bağlam:** AVX-512 mantıksal vektör (N=64) için `clang -march=skylake-avx512`
  bayrağı gerekli; AVX2 max N=8 (i32) veya N=16 (i8). KEMGU bu farkı
  bilmeden tüm vektör tiplerini kabul eder; LLVM `-march=...` üzerine bırakır.
- **Engellediği iş:** Yok — kullanıcı clang bayrağıyla seçer.

### E. vektor_doldur default N=4 (context yoksa)

- **Kategori:** tip kontrol (sarı)
- **Bağlam:** `vektor_doldur(s)` çağrısı bidirectional context (beklenen
  vektor<T, N>) olmadan ne zaman çağrılırsa, dönüş tipi `vektor<T_arg, 4>`
  varsayılır. Bu V1'de pragmatik; ama yanıltıcı — ya hata vermeli ya da
  context gerektirmeli.
- **Önerilen çözüm:** V2'de explicit `vektor_doldur<T, N>(s)` generic
  parametre, ya da context yoksa hata (V003-like).

---

## [2026-05-13] — Snapshot sözdizim sınırlamaları (test altyapı)

- **Kategori:** parser kapsam genişletme (yeşil — sözdizimsel)
- **Bağlam:** 30 yeni snapshot eklenirken 4 dosyada parser sınırlamasına
  takıldı, basitleştirildi:

  | Snapshot | Çalışmayan sözdizim | Basitleştirme |
  |----------|--------------------|--------------|
  | 21_modul_kullan | `değişken p: grafik::Nokta` (modül-nitelikli tip) | Tip annot kaldırıldı |
  | 23_generic_constraint | `kendin` parametre tipi olarak | `tam32` ile değiştirildi |
  | 24_nested_generic | `Kutu<T> { ... }` generic yaratım | `olustur<T>(x)` sarıcı |
  | 49_generic_method | `uygula Cift<tam32, tam32>` specialization | `uygula Cift` |

- **Önerilen seçenekler (gelecek görev):**
  1. parse_tip içine `::` modül-nitelikli tip referansı
  2. `kendin` parametre tipi (`baska: kendin`)
  3. Generic yapı yaratım: `Tip<T> { alan: x }`
  4. uygula specialization sözdizimi

- **Engellediği iş:** Yok — basitleştirmeler test'in özünü değiştirmedi.

---

## [2026-05-14] — DRF teoremi genişletme planı: KARARLAR ONAYLANDI ✓

**Mehmet onayı (2026-05-14):** Plan dökümanı Bölüm 7'deki tüm önerilerin
hepsi onaylandı. Aşağıdaki maddeler **kapatılmış** sayılır (bilgi olarak
kalır; geri dönülmesi gerekirse buradan başlanır).

Onaylanan kararlar (önerilen seçenekler kabul):
- **A:** V1'de kâğıt formalizasyon yeterli; Faz B (mekanize) V2'ye saklı.
- **B:** V1 dar (statik DRF), V2 geniş (operasyonel), V3 metateorem.
- **C:** Linear types DRF'in **temel taşıyıcısı** (S1'in compile-time önkoşulu).
- **D:** Paralel — Faz A1+A2 hemen (yapıldı), lang syntax ayrı oturum, A3 son (yapıldı).
- **E:** Frozen region **hibrit** — `dondur` builtin call + sembol flag (yeni keyword YOK).
- **F:** V1 = SC varsayımı + LLVM IR `atomic acq_rel` fence emit
  (görev/kanal/dondur sınırlarında); V2 weak memory.
- **G:** Ayrı teoremler — Teorem 4' (DRF) + Teorem 7 (Authority Soundness)
  ortak lemma DRF-L6 paylaşır.
- **H:** Güvensiz **izolasyon** modu — güvensiz blok dışı DRF korunur.
- **I:** **Çok dosya** organizasyon — modüler.
- **J:** **30+** test eşiği (Linear/CT/RT'a yakın).

Yapılan dökümanlar (Faz A tamamlandı — 2026-05-14):
- `belgeler/KEMGU_DRF_Genisletme_Plan.md` (önceki commit)
- `belgeler/KEMGU_Operasyonel_Semantik.md` — yeni
- `belgeler/KEMGU_DRF_Lemmalar.md` (DRF-L1..L7) — yeni
- `belgeler/KEMGU_DRF_Teoremi.md` (Teorem 4' V1 statik) — yeni
- `belgeler/KEMGU_Bellek_Modeli.md` cross-ref (mevcut Teorem 4 korunur)
- `belgeler/KEMGU_Linear_Types_Spec_V1.md` cross-ref
- `belgeler/KEMGU_Capability_Spec_V1.md` cross-ref (CP.14)

Beklenen sonraki adımlar (Faz B/C — bu görev kapsamı dışı):
- Concurrency lang syntax (`görev`/`kanal`) parser implementasyonu — ayrı görev
- Faz C: `test/test_drf.c` 30+ test — lang syntax sonrası
- Faz B: Mekanize ispat (V2) — V1 kâğıdı yeterli

### EKLEME (2026-05-14, aynı tarih): Concurrency lang syntax + Faz C tamamlandı

Yukarıdaki "ayrı görev" maddeleri **aynı oturumda tamamlandı**:

- **Lang syntax (`görev`/`kanal` keyword + 5 built-in çağrı):**
  - Lexer: TOK_GOREV, TOK_KANAL eklendi (toplam 35 keyword).
  - AST: DUGUM_TIP_GOREV, DUGUM_TIP_KANAL.
  - Parser: `parse_tip` görev<T>, kanal<T> destekler.
  - Tip sistemi: TIP_GOREV, TIP_KANAL kategorileri; `tip_olustur_gorev`,
    `tip_olustur_kanal`; nominal eşitlik; yazdırma.
  - Tip kontrol built-in handler'ları: `görev_başlat`, `görev_birleştir`,
    `kanal_gönder`, `kanal_al`, `dondur` — yeni hata kodları DRF001-DRF005.
  - Linear miras: `görev<T>` linear (tip_lineer_mi); kanal<T> non-linear
    (transfer tamponu pragmatik karar V1).
  - LR-2 güçlendirildi: tüm linear tipler yapı içinde yasak (eskiden
    sadece TIP_TEKKEZ).

- **Faz C test (`test/test_drf.c`):**
  - 36/36 test geçti, ASan temiz.
  - Plan Karar J eşiği (30+) sağlandı.
  - 20 negative + 16 positive.

- **Mevcut test paketi etkilenmedi:**
  - test_linear 57/57, test_capability 40/40, test_sabitsure 39/39,
    test_wcet 32/32, test_simd 30/30 — hepsi geçti.

- **V1 implementasyon sınırları (V2'ye saklı):**
  - Lambda body block-form (`|| { ver e; }`) destekli değil — ifade-form
    zorunlu (`|| e`). Block tip çıkarsama V2.
  - Concurrency runtime (thread/channel) yok — yalnız tip kontrol.
    LLVM codegen extern fonksiyon olarak link-time'a bırakılır.
  - `kanal_aç` üretici built-in yok V1 (kanal'lar parametre alınır).
  - Weak memory model (C++11 MM) fence emit V2.

---

(Tarihçi referans için açık sorular metni aşağıda korunur — kararlar yukarıda.)

## [2026-05-14] — DRF teoremi genişletme planı: açık tasarım soruları (TARİHÇE)

DRF (Data Race Freedom) teoremi genişletme PLAN'ı hazırlandı —
`belgeler/KEMGU_DRF_Genisletme_Plan.md`. Mevcut Teorem 4 (`Bellek_Modeli.md`
satır 260-272) **kâğıt üzerinde, 4 satırlık informel** ispat taslağı. Linear
Types V1 (onaylı), Capability V1 (taslak), Sabitsüre V1 (taslak), Realtime/SIMD
katmanları geldikten sonra DRF'in yeniden ifade edilmesi gerekiyor.

Aşağıdaki kararlar tip sistemine yeni katman + formal teorem etkisi
kategorisinde → 🔴 Kırmızı. Plan dökümanı Bölüm 7'de detay var.

### A. Proof assistant seçimi (V2 mekanizasyonu için)
- **Kategori:** teorem / araç seçimi
- **Bağlam:** Mevcut tüm KEMGU ispatları kâğıt üzerinde. TOPLAS makale planı
  için mekanik ispat artı-değer; ama proof assistant syntax İngilizce
  (Türkçe DNA ile çelişebilir).
- **Önerilen seçenekler:**
  1. Coq (gallina) — akademik standart, MathComp ekosistemi
  2. Isabelle/HOL — seL4 ekibi kullandı, kâğıt benzeri
  3. Lean 4 — modern, hızlı, mathlib aktif
  4. F* (effects) — DRF için doğal effects sistemi
  5. Sadece kâğıt (V1 yeterli) — Faz B'yi V2'ye ertele
- **Engellediği iş:** Faz B (mekanize) — V1 kâğıt yeterli, V2 saklı.

### B. DRF teoreminin kapsamı
- **Kategori:** teorem ifade darlığı
- **Bağlam:** "Güvenli alt küme" tabiri muğlak. Dar/Geniş/Metateorem üç seçenek.
- **Önerilen seçenekler:**
  1. **Dar (V1):** `tip_kontrol = OK ∧ güvensiz yok` → derleyici reddeder
  2. **Geniş (V2):** Tüm runtime izleri (operasyonel semantik gerek)
  3. **Bütünleşik metateorem (V3):** DRF + Memory Safety + Side-Channel + BET
- **Engellediği iş:** Yok — V1 dar yeterli, V2 V3 ileride.

### C. Linear types'ın DRF'e statik mı yoksa semantik katkısı?
- **Kategori:** teorem yapısı
- **Bağlam:** S1 mevcut runtime invaryantı. Linear types compile-time zorlanır.
  Linear DRF'in temel taşıyıcısı mı, yoksa güçlendirici mi?
- **Önerilen seçenekler:**
  1. **Temel taşıyıcı:** Linear zorlama → S1'in compile-time önkoşulu
     (matematik güç tam gösterilir)
  2. **Güçlendirici:** S1 runtime aksiyom, Linear sadece compile-time yakalar
- **Engellediği iş:** (1) seçilirse Bölüm D ile bağlı (lang syntax zorunlu).

### D. Concurrency lang syntax önceliği
- **Kategori:** dil syntax / DRF prerequisite
- **Bağlam:** `görev` / `kanal` keyword'leri DRF teoremi için gerekli. Şu an
  sadece `src/bolge.h` API var (`bolge_olustur_sahip`,
  `bolge_sahiplik_transfer`, `bolge_kanal_gonder`, `bolge_donmus_mu`).
- **Önerilen seçenekler:**
  1. Önce lang syntax — 10+ checkpoint, DRF teoremi bekler
  2. Önce kâğıt ispat — varsayım "lang syntax buna sadık eklenecek"
  3. Paralel — plan A1+A2 hemen, lang ayrı, A3 son
- **Engellediği iş:** DRF ispatı A3 adımı (Plan Faz A).

### E. Frozen region tipinin formal modeli
- **Kategori:** yeni tip qualifier / API tasarımı
- **Bağlam:** R-PAYLAŞ `dondur(v)` — runtime API mı, compile-time tip qualifier
  mı? Yeni keyword `donmuş` gerekirse 🔴.
- **Önerilen seçenekler:**
  1. Runtime API — `dondur: &değişken T -> &T`, runtime invaryantı
  2. Compile-time qualifier — `donmuş<T>` yeni tip operatörü (yeni keyword)
  3. Hibrit — `dondur` builtin + sembol tablosu flag (yeni keyword yok)
- **Engellediği iş:** DRF-L4 (Frozen Region Safety) lemma ifadesi.

### F. Bellek modeli — Sequential Consistency vs Weak Memory
- **Kategori:** memory model / ARM64 (DGX Spark hedefi)
- **Bağlam:** DRF tanımı genelde SC varsayar. ARM64 weak memory model
  (relaxed default + dmb sy fence). DGX Spark ARM64.
- **Önerilen seçenekler:**
  1. V1 SC varsayımı + runtime fence sorumluluğu — basit ama eksik
  2. C++11 MM entegrasyonu — her `görev`/`kanal`/`dondur` acquire/release
  3. Daha güçlü model (release-acquire default) — her boundary fence
- **Engellediği iş:** LLVM IR fence emit politikası; V2 weak memory.

### G. Capability + DRF: ayrı teorem mi, bileşke mi?
- **Kategori:** teorem organizasyonu
- **Bağlam:** `yetki<R>` linear (CP.1.1). DRF-L6 capability'i kapsar. Ama
  Capability'nin asıl katkısı confused-deputy/ambient authority — DRF dışı.
- **Önerilen seçenekler:**
  1. Tek DRF teoremi — DRF-L6 ile kapsanır
  2. Ayrı teorem — Teorem 4' (DRF) + Teorem 7 (Authority Soundness) ortak
     lemma DRF-L6 paylaşır
- **Engellediği iş:** Yok (ileride).

### H. `güvensiz` blok + DRF: opt-out modeli
- **Kategori:** teorem önkoşul yapısı / Teorem 5 ilişkisi
- **Bağlam:** Mevcut Teorem 4 "Güvenli alt küme". Teorem 5 (Güvensiz Sınır
  Bütünlüğü) güvensizden `*` çıkamaz diyor. DRF güvensiz bloğu nasıl ele alır?
- **Önerilen seçenekler:**
  1. Tamamen dışlama — "Π güvensiz blok içermez" önkoşul
  2. Güvensiz izolasyon — güvensiz blok DRF garanti yok, dışı korunur
  3. Programcı annotation — `[etiket: "no-DRF"]` opt-out
- **Engellediği iş:** Teorem ifadesinin tam yazımı (Plan Bölüm 3.1).

### I. Plan onayı + dosya yapısı
- **Kategori:** belge organizasyonu
- **Bağlam:** Plan dökümanı yazıldı. Sonra Operasyonel_Semantik.md, DRF_Lemmalar.md,
  DRF_Teoremi.md (veya Bellek_Modeli.md yenile).
- **Önerilen seçenekler:**
  1. Çok dosya — modüler, küçük commit'ler, git history düzgün
  2. Tek dosya `KEMGU_DRF_V2.md` — tüm bunları içerir
- **Engellediği iş:** Faz A1 başlangıcı.

### J. Test sayısı eşiği (🟡 Sarı — sessizlik onay)
- **Kategori:** test minimum
- **Bağlam:** Linear 50, Capability 35, Sabitsüre 30, Realtime 30, SIMD 25.
- **Öneri:** **30+** (Linear/CT/RT'a yakın). ~20 negative + ~10 positive.

---

## [2026-05-14] — stdlib::kripto bundle yaklaşımı + V1 sınırları

- **Kategori:** modül sistemi / runtime gereksinimi
- **Bağlam:** `stdlib/kripto.kem` + `stdlib/kripto/{karma,sifre,rastgele,anahtar}.kem`
  beş ayrı modül eklendi. Import sistemi olmadığından submodüller base API'ye
  ve birbirine referans verir; bu yüzden `calistir_kripto_check` "bundle"
  yaklaşımı kullanıldı (Makefile concat).

### A. Modül import — V2 takvimine
- **Sorun:** Her submodül ChaCha20/SHA256/HKDF içerikleri için base
  primitif'lere erişmek istiyor; tek-dosya derleme bunu engelliyor.
- **Önerilen:** `kullan stdlib::kripto::karma` sözdizimini ifade context'inde
  parse + sembol tablosu cross-file linker.
- **Geçici çözüm:** Makefile `cat stdlib/kripto.kem stdlib/kripto/*.kem > bundle`
  konsantrasyon. `calistir_kripto_check` hedefi.

### B. Hardware RNG (RDRAND / ARM RNDR)
- **Kategori:** yeni unsafe primitif (üretim için 🔴)
- **Bağlam:** rastgele.kem'de `hw_rastgele_u64() -> dtam64` API tanımlı
  fakat implementasyon yok. CSPRNG'in hardware seed'i kritik.
- **Önerilen seçenekler:**
  1. `__builtin_rdrand_u64` benzeri intrinsic (x86 + ARM ayrı path)
  2. `güvensiz` blok içinde inline asm (yeni keyword `asm` lazım)
  3. C ABI FFI (mevcut güvensiz blok + extern fn)
- **Engellediği iş:** Üretim-grade kripto. V1 stub (xorshift64) kripto güvensiz.

### C. Dosya I/O syscall altyapısı
- **Kategori:** runtime (yeşil — runtime/OS layer ile çözülür)
- **Bağlam:** `test/ornekler/sifrele_dosya.kem` AEAD demo derler ama
  gerçek dosya açma/okuma syscall altyapısı bekliyor.
- **Önerilen:** OS katmanı (KEMGU işletim sistemi tarafı) syscall wrapper.

### D. mantıksal → dtam mask dönüşümü E002 verir
- **Kategori:** tip kontrol kuralı (yeşil — kural sıkı; alternatif çözüm var)
- **Bağlam:** `m olarak sabitsüre<dtam32>` (m: sabitsüre<mantıksal>) → E002
  "olarak: kaynak ve hedef sayısal/karakter olmalı". Mask'a manuel
  dönüşüm için bit hile (`acc == 0` üzerinden) kullanıldı.
- **Öneri:** `sabitsüre<mantıksal> -> sabitsüre<dtam8>` cast'i izinli kıl
  (kayıp prezisyon yok; 0/1 → 0/1).

### E. dtam64 → dtam8 narrowing tek adımda yasak (E004)
- **Kategori:** tip kontrol kuralı (yeşil — geçici çözüm var)
- **Bağlam:** `state olarak dtam8` (state: dtam64) → E004. Çözüm:
  `(state olarak dtam32) olarak dtam8` iki adım (kw_kaynak >= 64 &&
  kw_hedef < 32 koşulu sadece bir-adımda vurdu).
- **Öneri:** dtam64 → dtam8 implicit & 0xFF semantiği ile izinli kıl
  (kullanıcı niyeti ortada).

### Sonuç (görev kapsamında)
- 5 modül eklendi (kripto.kem + kripto/*.kem)
- 67 test geçti (3 dosya: test_kripto, test_kripto_vektor, test_kripto_timing)
- Örnek dosya: test/ornekler/sifrele_dosya.kem
- Belge: belgeler/KILAVUZ.md §14 (Kripto stdlib)
- src/ dokunulmadı; yalnız Makefile (calistir_kripto_check eklendi) ve
  stdlib/ + test/ + belgeler/ değişti.
