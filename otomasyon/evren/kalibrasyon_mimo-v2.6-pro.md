> **⚠ GEÇERSİZ — hatalı test kurgusu (2026-10-11).** Bu test, eğitim verisinde bulunmayan bir dil hakkında bağlamsız EZBER sordu; hiçbir model `çıplak`ı soğuktan bilemez. `deepseek-v4.1-flash` sonucu ayrıca bütçe kesintisi artefaktıdır (`reasoning_budget_exhausted`). Aşağıdaki KALDI kararları geçersizdir; geçerli ölçüm için `kalibrasyon2_<model>.md` dosyalarına bak.

# EVREN kalibrasyonu — `mimo-v2.6-pro`

- Tarih: 2026-10-11 20:37:25
- HTTP: 200 · süre: 381.0 sn
- Kaynak: `runtime/kem_heap.kem` satır 16–19, 28–65
- finish_reason: `stop` · usage: `{"prompt_tokens": 853, "total_tokens": 4665, "completion_tokens": 3812, "prompt_tokens_details": {"cached_tokens": 0, "created_cache_tokens": 848, "multimodal_tokens": null}, "completion_tokens_details": {"reasoning_tokens": 952}, "evren": {"credits_held_cr": "0.0000", "credits_remaining_cr": "1010.0000", "routed_model": "xiaomi/mimo-v2.6-pro"}}`
- Parametreler: `max_tokens: 4096`, `stream: false`

## Yanıt

# KEMGU `malloc` Kodunun Analizi

> **Önemli dürüstlük beyanı:** "KEMGU" adlı dil hakkında kaynaklarımda spesifik, doğrulanmış bir dokümantasyon bulamıyorum. Dilin Türkçe anahtar kelimeleri kullanması (`küresel`, `değişken`, `eğer`, `iken`, `değilse`, `ver`, `olarak`) ve tür adları (`tam64`, `tam8`, `çıplak`, `tekkez`, `yetki`) üzerinden çıkarım yapıyorum. Aşağıda kesin bildiklerimi ve bilmediklerimi açıkça ayırıyorum.

---

## 1. Fonksiyon Ne Yapıyor?

Bu fonksiyon, **C-ABI uyumlu bir `malloc` implementasyonudur**: heap bellekte, istenen boyutta bir blok tahsis eder ve kullanıcıya göstergeci (pointer) döndürür. İki aşamalı bir tahsis stratejisi kullanır.

### Genel Mimari

Her tahsis edilmiş blok, kullanıcı verisinden önce **16 baytlık bir gizli başlık (header)** tutar:

| Offset | İçerik |
|--------|--------|
| `+0`   | `boyut` (tam64): toplam blok uzunluğu (header dahil) |
| `+8`   | `sonraki` (tam64): serbest-listede bir sonraki bloğa işaretçi (`0` = yok) |

Tüm bloklar **16 bayt hizalıdır**. Küresel durum üç değerle izlenir:

- **`kem_bump`** — bir sonraki bump-tahsis adresi (uygun olduğunda 16 hizalı)
- **`kem_son`** — heap'in üst sınırı (dışlayıcı: bu adresten sonrası kullanılamaz)
- **`kem_bos`** — serbest-liste başlangıcı (serbest blokların oluşturduğu tek bağlı liste)

### Adım Adım İşleyiş

```
girdi: n (istenilen bayt sayısı)
```

**Adım 0 — Normalizasyon:**
```
eğer n == 0 ise, istek = 1 (benzersiz, geçerli bir göstergeci garanti etmek için)
```

**Adım 1 — Boyut hesabı:**
```
toplam = ((istek + 31) / 16) * 16
```
Header (16 bayt) + kullanıcı verisi (`istek`) hesaba katılır, sonuç 16'nın katına yuvarlanır. Böylece hem kullanıcı verisi hem de bloğun kendisi 16 hizalı kalır.

**Adım 2 — Serbest-listeden arama (First-Fit / İlk-Uyum):**

Liste baştan sona taranır:

```
bir önceki = 0;
cur = kem_bos;
```

Her düğüm için:
- Header'ın `boyut` alanı okunur (`bsize`)
- Header'ın `sonraki` alanı okunur (`bnext`)
- Eğer `bsize >= toplam` ise → **bulundu**:
  - Blok listeden çıkarılır (ilk düğümse baş güncellenir; değilse önceki düğümün `sonraki` bağlantısı `bnext`'e yönlendirilir)
  - Kullanıcıya `cur + 16` adresi (bloğun veri bölümü) döndürülür
  - ⚠️ **Header korunur, bölme/küçültme yapılmaz** — yani blok gereğinden büyük olsa bile olduğu gibi verilir (bellek israfı riski var, ama sadelik kazanılıyor)

Uygun blok bulunamazsa (`bsize < toplam` her seferinde), sondaki boş işaretcıye (`bnext == 0`) kadar devam edilir.

**Adım 3 — Bump tahsis (serbest listesi boşken veya uygun blok yokken):**

```
p = kem_bump
```

Sınır kontrolü:
```
eğer p + toplam > kem_son ise → OOM → null (0) döndür
```

Başarılıysa:
```
kem_bump = p + toplam        // bir sonraki serbest bump adresi
header[p].boyut = toplam      // header'a boyut yaz
header[p].sonraki = 0         // serbest listede değil, sonraki = 0
dönüş: p + 16                  // header atlanarak veri adresi döndür
```

### Özet Akış Şeması

```
malloc(n)
  │
  ├─ n==0 → istek=1
  ├─ toplam = round_up16(istek + 31)   [≈ header(16) + istek, 16 hizalı]
  │
  ├─ ── Faz 1: Serbest listesi (first-fit) ──
  │     Kem_bos'tan başla, bs ≥ toplam olan blok ara
  │     Bulursan: listeden çıkar, (blok+16) dön  ✅
  │
  └─ ── Faz 2: Bump tahsis ──
        p = kem_bump
        p+toplam > kem_son → 0 dön (OOM)  ❌
        header yaz, kem_bump güncelle, (p+16) dön  ✅
```

### Dikkat Çeken Tasarım Kararları

| Karar | Etki |
|-------|------|
| Böl/küçült yok, blok bütün verilir | Sadeliği artırır, ama iç parçalanma (internal fragmentation) olabilir |
| First-fit stratejisi | Hızlıdır, ama dış parçalanmaya yol açabilir |
| Sadece bump + serbest liste | Basit bir heap yöneticisi; coalescing (bitişik serbest blokları birleştirme) ve defragmentation **yok** |
| `çıplak` (naked) fonksiyon | Derleyici eklemesi (prologue/epilogue) olmadan saf ABI-uyumlu kod üretimi |

---

## 2. `çıplak` Niteleyicisi

**Ne bildiğim kesin:** Koddaki yorum satırı şunu söylüyor:

```
// C-ABI malloc: void* malloc(size_t) → @malloc(i64)->ptr.
```

Bu, fonksiyonun bir **sistem/mallok sembolü** olarak C çağrı uyumluluğu (C-ABI) ile dış dünyaya açılması gerektiğini işaret ediyor.

**Çıkarımım:** İngilizcede *"naked function"* kavramı vardır (LLVM'de [`naked`](https://llvm.org/docs/LangRef.html#function-attributes), GCC'de `__attribute__((naked))`, ARM/RISC-V'de çok yaygındır). Türkçede **"çıplak"** kelimesi birebir karşılığıdır.

Bir **çıplak fonksiyon**, derleyicinin otomatik olarak ürettiği **prologue ve epilogue kodu olmadan** derlenir:

| Normal fonksiyon | Çıplak fonksiyon |
|-----------------|-----------------|
| Stack frame kurulur (`push rbp; mov rbp, rsp`) | Frame kurulmaz |
| Yerel değişkenler stack'e ayrılır | Derleyici stack yönetmez |
| `ret` öncesi frame yıkılır (`pop rbp`) | Sadece kodunuzdaki gerçek dönüşler çalışır |

**Amacı:** Derleyiciden gelen herhangi bir ek komutun **ABI'yi bozmamasını** garantilemek. Özellikle:
- `@malloc` gibi C-ABI sembolü olarak export edilen, doğrudan çağrılan sistem fonksiyonlarında
- Çıktı sözleşmesinin (return convention, register kullanımı) byte-level kontrol gerektirdiği yerlerde
- Interrupt handler, bootloader, runtime bootstrap gibi yerlerde

> ⚠️ **Emin olmadığım nokta:** KEMGU'nun `çıplak` sözcüğünün tam bunu mu kastettiğini %100 doğrulayamıyorum. Ancak C-ABI + inline pointer arithmetic + stack quảniminin elle yapılması bağlamı bu anlamı çok güçlü destekliyor. KEMGU'ya özgü farklı bir anlamı varsa bunu bilmiyorum.

---

## 3. `tekkez<T>` ile `yetki<R>` Arasındaki Fark

> **⚠️ Açıkça belirtmem gerekiyor:** Bu iki tipin KEMGU'daki kesin semantiklerini **bilmiyorum**. KEMGU'ya ait dokümantasyona erişimim yok ve bu iki isim literatürde genel olarak bilinen standart programlama kavramlarıyla birebir örtüşmüyor. Aşağıdakiler **etimolojik/dilsel çıkarımlardır**, kesin bilgi değildir.

### İsim Analizi (Türkçe sözlük anlamı üzerinden)

| Terim | Sözlük anlamı | Tahmini programlama karşılığı |
|-------|--------------|-------------------------------|
| **`tekkez<T>`** | "tek" (tekil/tek seferlik) + "kez" (sefer/zaman) → "tek seferlik" | **Tek kez başlatılabilen / tek kez atanabilir tipler** (İngilizce *"once"* kavramının karşılığı) |
| **`yetki<R>`** | "yetki" (izin, hak, yetki belgesi) | **Yetki/izin tabanlı tipler** (İngilizce *"capability/permission"* kavramının karşılığı) |

### Olası Anlamları (TAHMİN, ONAYLANMADI)

**`tekkez<T>` — Tek-seferlik sahiplenme tipi:**
```
tekkez<T> ≈ T değerinin yalnızca bir kez kurulup bir kez tüketelebildiği bir kap
```
- Rust'taki `OnceCell<T>`, `OnceLock<T>`, `Box::into_inner`, veya *linear type* / *affine type* mantığına benzeyebilir
- "Bir kez yaz, sonsuza kadar oku" ya da "bir kez tüketildikten sonra erişilemez"
- `malloc` bağlamında: "bu heap bloğu bir kez tahsis edildi, ikinci kez verilmeyecek"

**`yetki<R>` — Yetki/izin tipi:**
```
yetki<R> ≈ Belirli bir kaynağa R erişim yetkisinin kanıtı / tokeni
```
- Capability-based güvenlik modeli: `yetki<Yazma>`, `yetki<Okuma>`, `yetki<Silme>` gibi alt türler
- Rust'taki `PhantomData` ile marker trait'ler, veya `unsafe` token'ları gibi
- Kaynak tabanlı erişim kontrolü: `yetki<R>` tipli bir değeri elinde bulunduran kod, `R` kaynağına bu yetkiye uygun işlem yapabilir

### Farkın Özeti (Tahmine dayalı)

```
tekkez<T>   → "KAÇ KEZ?"  sorusuna cevap verir (tek sefer mi?)
yetki<R>    → "NEYİ, NASIL?" sorusuna cevap verir (hangi kaynağı, hangi yetkiyle?)
```

İki boyutturlar: biri **ömrü/tüketimi** disipline eder (lineer tip boyutu), diğeri **erişim yetkisini** kısıtlar (güvenlik/erk boyutu).

> **Son bir kez netleştireyim:** `tekkez<T>` ve `yetki<R>` hakkındaki yukarıdaki bölüm **bilmediğim bir konuda yapılan eğitimli bir çıkarımdır**. KEMGU dilinin resmî tanımına ulaşsam bu kısmı kesin olarak düzeltebilirim veya genişletebilirim. Şu anda kesin bildiklerimle bilmediklerimi ayırmayı tercih ediyorum.

## Düşünme (reasoning)

The user is asking about a program written in "KEMGU" programming language. Let me analyze the code and answer three questions:

1. What does this function do? Explain step by step.
2. What does the `çıplak` qualifier mean and what purpose does it serve?
3. What is the difference between `tekkez<T>` and `yetki<R>`?

Let me be honest here - I don't have specific knowledge of a programming language called "KEMGU". This appears to be either:
- A very niche/novel programming language
- An educational/hypothetical programming language created for a course or exam
- A language I simply don't have information about

Looking at the syntax carefully though, it seems to use Turkish keywords:
- `küresel değişken` = global variable
- `tam64` = int64
- `tam8` = int8
- `çıplak işlev` = naked function
- `değişken` = variable
- `eğer` = if
- `iken` = while
- `değilse` = else
- `ver` = return
- `olarak` = as (cast)

So it's a Turkish-keyworded systems programming language. The function itself is clearly a `malloc` implementation with free-list management and bump allocation.

For question 1, I can explain the malloc algorithm clearly based on the code.

For question 2, `çıplak` literally means "naked" in Turkish. In systems programming contexts (like Rust's `#[no_mangle]`, C's `__attribute__((naked))`, or assembly-related qualifiers), "naked" typically means the function has no prologue/epilogue generated by the compiler - it's meant to map directly to raw machine code or assembly. Looking at the comment "// C-ABI malloc: void* malloc(size_t) → @malloc(i64)->ptr", this function needs to conform to the C ABI for the `malloc` symbol. So `çıplak` likely means it's a naked function that must adhere to C calling conventions and has no compiler-generated preamble/postlude.

For question 3, `tekkez<T>` and `yetki<R>` are types/generics that I cannot identify. I should be honest about not knowing these.

Wait, let me think more carefully. Could this be related to some known concepts?

- `tekkez` could relate to "teker teker" (one by one) or some unique/singleton concept... or maybe "tek kez" meaning "one time"... Hmm, `tekkez` might mean something like "unique" or "once" type - perhaps a linear/affine type or a once-initialized value.
- `yetki` means "permission/authority" in Turkish. `yetki<R>` could be some kind of permission or capability type parameterized by R.

But I really don't know what KEMGU language specifics are. These could be:
- Ownership/capability types
- Threading primitives
- Memory management types

I should be honest that I don't know the exact semantics of `tekkez<T>` and `yetki<R>` in KEMGU.

Actually, wait. Let me reconsider. Given the context - this is a memory allocator, and the language has raw pointer operations with `olarak` (as) casts... 

Could `tekkez<T>` be "once<T>" - like a value that can only be set once (similar to `OnceCell` or `Lazy<T>`)?
Could `yetki<R>` be "permission<R>" or "capability<R>" - like a capability-based access control token?

These are educated guesses but I shouldn't present them as facts.

Let me structure my answer honestly, being clear about what I can determine from the code vs. what I'm uncertain about.

Actually, thinking again more about `tekkez<T>` - "tek" means "single/one" and "kez" means "time/occurrence" in Turkish. So "tekkez" = "one-time" = probably a "once" or "run-once" primitive. Like `std::once` or `std::sync::Once`.

And `yetki<R>` - "yetki" means "permission/authority/right" in Turkish. This could be a capability or permission type.

But I genuinely don't know the specific semantics in KEMGU. I should state this clearly.

Let me write a thorough answer covering all three questions, being transparent about uncertainties.

## Gönderilen istem

````
Aşağıdaki kod KEMGU adlı bir programlama dilinde yazılmıştır.

```kemgu
küresel değişken kem_bump: tam64 = 0;   // bir sonraki bump adresi (16-hizalı)
küresel değişken kem_son: tam64  = 0;   // heap üst sınırı (dışlayıcı)
küresel değişken kem_bos: tam64  = 0;   // serbest-liste başı (blok adresi; 0=boş)
küresel değişken kem_global_bolge: tam64 = 0;   // K4b: lazy tek global bölge (adres)

// C-ABI malloc: void* malloc(size_t) → @malloc(i64)->ptr.
çıplak işlev malloc(n: tam64) -> *tam8 {
    değişken istek: tam64 = n;
    eğer istek == 0 { istek = 1; }               // 0-bayt → benzersiz geçerli ptr
    değişken toplam: tam64 = ((istek + 31) / 16) * 16;   // header(16)+istek, 16-hizalı

    // 1) Serbest-liste ilk-uyum (header'ı KORU → böl/küçült yok, bloğu bütün ver).
    değişken onceki: tam64 = 0;
    değişken cur: tam64 = kem_bos;
    iken cur != 0 {
        değişken hsize_p: *tam64 = cur olarak *tam64;             // header.boyut @ +0
        değişken bsize: tam64 = *hsize_p;
        değişken hnext_p: *tam64 = (cur + 8) olarak *tam64;       // header.sonraki @ +8
        değişken bnext: tam64 = *hnext_p;
        eğer bsize >= toplam {
            eğer onceki == 0 {
                kem_bos = bnext;                                  // liste başından çıkar
            }
            değilse {
                değişken op_p: *tam64 = (onceki + 8) olarak *tam64;
                *op_p = bnext;                                    // önceki.sonraki = bnext
            }
            ver (cur + 16) olarak *tam8;                          // blok + header
        }
        onceki = cur;
        cur = bnext;
    }

    // 2) Bump tahsis (sınır + OOM korumalı; bump zaten 16-hizalı, toplam 16-katı).
    değişken p: tam64 = kem_bump;
    eğer p + toplam > kem_son { ver 0 olarak *tam8; }             // OOM → null
    kem_bump = p + toplam;
    değişken hs_p: *tam64 = p olarak *tam64;
    *hs_p = toplam;                                               // header.boyut
    değişken hn_p: *tam64 = (p + 8) olarak *tam64;
    *hn_p = 0;                                                    // header.sonraki
    ver (p + 16) olarak *tam8;
}
```

Üç soruyu yanıtla:
1. Bu fonksiyon ne yapıyor? Adım adım açıkla.
2. `çıplak` niteleyicisi ne anlama geliyor, hangi amaca hizmet ediyor?
3. `tekkez<T>` ile `yetki<R>` arasındaki fark nedir?

Emin olmadığın noktaları açıkça belirt; bilmediğin şeyi tahminle doldurma.
````

## Değerlendirme (Claude, 2026-10-11)

**KARAR: KALDI** (KEMGU'ya özgü anlamı okuyamıyor; kod düzeyinde okuma iyi).

- **S1 — DOĞRU.** Algoritma koddan eksiksiz çözülmüş: `((n+31)/16)*16`
  hesabı, ilk-uyum serbest liste, başlığın korunması, bump + OOM → null.
- **S2 — YANLIŞ (C analojisinden uydurma).** `çıplak` = GCC/LLVM `naked`
  saymış: "stack frame kurulmaz, derleyici yığını yönetmez". Kodun kendisi
  bunu çürütüyor (yerel değişkenler, döngüler). Gerçek anlam (derleyiciden
  ölçüldü): normal işlev `@g(ptr %rho, …)` alır ve prologda
  `kdl_bolge_olustur()` çağırır; `çıplak` işlev `@f(i64)` — ρ yok, bölge
  prologu yok, C-ABI imzası, ham deref `volatile`. `malloc` çıplak olmasaydı
  prologu `kdl_bolge_olustur` → `malloc` → … ile kendini tetiklerdi. C-ABI
  kısmını yalnızca koddaki yorumdan aldı; region-prologue hiç geçmiyor.
- **S3 — tahmin, dürüstçe etiketlenmiş.** Sözlük anlamından "lineer/tek
  seferlik" ve "capability" çıkarımı genel yönde doğru ama kanıta değil
  etimolojiye dayalı; `geri_al`, CP005, OUT-PTR ABI gibi KEMGU ayrıntısı yok.
- **Olumlu:** Bilmediğini açıkça ayırıyor; uydurmayı "tahmin" diye işaretliyor.
