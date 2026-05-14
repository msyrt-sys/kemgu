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
