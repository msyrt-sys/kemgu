> **⚠ GEÇERSİZ — hatalı test kurgusu (2026-10-11).** Bu test, eğitim verisinde bulunmayan bir dil hakkında bağlamsız EZBER sordu; hiçbir model `çıplak`ı soğuktan bilemez. `deepseek-v4.1-flash` sonucu ayrıca bütçe kesintisi artefaktıdır (`reasoning_budget_exhausted`). Aşağıdaki KALDI kararları geçersizdir; geçerli ölçüm için `kalibrasyon2_<model>.md` dosyalarına bak.

# EVREN kalibrasyonu — `deepseek-v4.1-flash`

- Tarih: 2026-10-11 20:31:04
- HTTP: 200 · süre: 221.3 sn
- Kaynak: `runtime/kem_heap.kem` satır 16–19, 28–65
- finish_reason: `length` · usage: `{"prompt_tokens": 957, "total_tokens": 5053, "completion_tokens": 4096, "prompt_tokens_details": {"cached_tokens": 0, "created_cache_tokens": 896, "multimodal_tokens": null}, "completion_tokens_details": {"reasoning_tokens": 4096}, "evren": {"credits_held_cr": "0.0000", "credits_remaining_cr": "1010.0000", "routed_model": "deepseek/deepseek-v4.1-flash", "reasoning_budget_exhausted": true}}`
- Parametreler: `max_tokens: 4096`, `stream: false`

## Yanıt

_(boş)_

## Düşünme (reasoning)

We need answer Turkish. Need parse. User asks about KEMGU code. Need answer 3 questions. Need likely they expect explain malloc, naked qualifier meaning, difference tekkez<T> vs yetki<R>. But KEMGU language? We don't know. Need be honest. Need analyze code.

We need answer in Turkish. Need address code. We can explain malloc based on code. For çıplak: likely "naked function" akin no prologue/epilogue? In Kemgu? Need infer. Code says "çıplak işlev malloc(n: tam64) -> *tam8". "çıplak" Turkish for naked. In systems programming, naked function means compiler doesn't generate prologue/epilogue, used for low-level ABI, interrupt handlers, assembly stubs. But here C-ABI malloc implemented in high-level? It has local variables, so if truly naked, cannot use normal locals without setup? Hmm in C, __attribute__((naked)) means no prologue/epilogue, body limited to inline asm; compiler doesn't generate return. But code uses variables and return. Maybe in KEMGU, "çıplak" means no runtime support, freestanding? Or "bare function" exported with C ABI? Need investigate.

KEMGU maybe a fictional/constructed language? Terms: küresel değişken, tam64, *tam8, çıplak işlev, tekkez<T>, yetki<R>. Could be from some language spec? We need identify. KEMGU? Turkish keywords. "tekkez<T>" and "yetki<R>"? Sounds like "once<T>" and "authority<R>"? Maybe Kemgu is a language? I don't know. Need not hallucinate. We can say I don't know exact semantics; in general "çıplak" likely naked function. For tekkez<T> vs yetki<R>, I don't know KEMGU. Need maybe answer based on names? "tekkez" = once? "yetki" = permission/capability. Could be concurrency primitives? Need infer from Turkish: tekkez<T> maybe "tek kez" = one-time/once, like std::sync::Once<T>? yetki<R> = authority? In Rust? "yetki<R>" maybe "permission<R>"? Hm.

Let's parse code. It defines malloc. Global variables: kem_bump next bump address aligned 16, kem_son heap upper bound exclusive, kem_bos free-list head (block address; 0 empty), kem_global_bolge lazy single global region address.

Function malloc(n: tam64) -> *tam8:
- request = n; if 0 set 1 to allocate unique valid ptr.
- total = ((request + 31)/16)*16. Wait header 16 + request, 16-aligned. If request + 31 /16 floor *16? Let's calculate: ceil((request+16)/16)*16? They use ((istek + 31) / 16) * 16. This equals ceil(istek/16)*16? Let's test request=1: (32)/16=2*16=32. But header(16)+1=17, aligned to 16 => 32. request=16: (47)/16=2*16=32; header+16=32 yes. request=17:48/16=3*16=48; header+17=33 aligned 48. So total includes header and padding to 16. Actually header 16 + request, round up to multiple of 16. Good.
- Free list first fit: previous=0, cur=kem_bos. While cur !=0:
  hsize_p = cur as *tam64; bsize = *hsize_p; header size at +0.
  hnext_p = (cur+8) as *tam64; bnext=*hnext_p.
  if bsize >= total:
    if previous==0: kem_bos = bnext; else op_p=(prev+8) as *tam64; *op_p=bnext;
    return (cur+16) as *tam8; // skip header
  prev=cur; cur=bnext.
- Bump allocation: p=kem_bump; if p+total > kem_son return 0 null. kem_bump=p+total; set header size at p = total; next at p+8=0; return p+16.

So function implements simple malloc with free-list first-fit and bump allocator fallback. It doesn't split blocks or coalesce. Free list nodes are in header of free blocks, with size and next. Allocated block header stores size and next=0? Actually in bump allocation, header next=0. In free-list allocation, it returns block without modifying header? It leaves header as it was, likely size >= total and next maybe old. But it removes from free list. It doesn't mark allocated vs free; no free function shown. It doesn't shrink. If bsize > total, internal fragmentation. It doesn't set kem_bump? No. It returns cur+16. It doesn't set header next for allocated from free list, but not needed? Maybe free() later uses size. But no free. It preserves header? Comment "header'ı KORU → böl/küçült yok, bloğu bütün ver" means keep header, no split/shrink, give whole block. So allocated block's header still says original bsize, maybe free() can use it? But if allocated, header next may point to list, dangerous? Maybe free() later sets. But no free.

Question 2: çıplak niteleyicisi. Need answer. In C-like systems languages, naked function means compiler does not emit function prologue/epilogue, so function body must manage stack/registers and return itself. Used for ABI-critical, interrupt handlers, assembly trampolines, or to define symbols like malloc without runtime overhead. However code has local variables and return; if KEMGU's "çıplak" doesn't mean that, exact semantics uncertain. We can note in many languages it's like __attribute__((naked)); in KEMGU spec? I don't know exact. We can say based on name and context, it likely marks function as exposed with C ABI / no runtime wrapper? Wait code comment: "// C-ABI malloc: void* malloc(size_t) → @malloc(i64)->ptr." and "çıplak işlev malloc(n: tam64) -> *tam8". Maybe "çıplak" specifically means function has C ABI, no mangling? Hmm.

Let's examine syntax: "çıplak işlev malloc(n: tam64) -> *tam8" No `@` in signature? Comment says C-ABI malloc: void* malloc(size_t) → @malloc(i64)->ptr. Maybe `çıplak` means "bare function" exported as C symbol? In languages, "naked" could mean no name mangling? Actually "bare function" in D? `extern(C)`? In Zig, `export fn`? In Rust, `extern "C" fn`? "çıplak işlev" maybe "naked function" in Turkish, but naked in C means no prologue. The comment C-ABI malloc maybe indicates it must match C ABI. But "çıplak" could be "bare" as in bare-metal? Hmm.

Question 3: `tekkez<T>` ile `yetki<R>` arasındaki fark nedir? These look like types with angle brackets. Maybe from KEMGU language features. "tekkez<T>" maybe "once<T>" type? "yetki<R>" maybe "authority<R>"? Could be memory allocation? Need know KEMGU. It might be an esoteric language from a previous conversation? We need maybe search memory? I as AI no internet. But maybe terms:
- `tekkez<T>`: Turkish "tekkez" = "once" maybe a one-time initialization cell? Like `Once<T>` in Rust, for lazy global region? Code has `kem_global_bolge: tam64 = 0; // K4b: lazy tek global bölge (adres)`. That comment mentions lazy single global region. Maybe `tekkez<T>` is a type for a single global region? "tekkez" could be "once" or "single"?
- `yetki<R>`: "authority<R>" maybe a capability/rights type in language for permissions. Could be linear/affine type? Need infer from Turkish: "yetki" = authorization/permission. `<R>` maybe region? `yetki<R>` might represent a capability to access region R? In memory management, `tekkez<T>` vs `yetki<R>`? Hmm.

Maybe KEMGU has types `tekkez<T>` (unique<T>?) and `yetki<R>` (authority<R>?). Turkish: "tekkez" maybe "unique"? Wait "tekkez" could be "tek kez" = "single time" not "unique". "tekkez<T>" maybe "one-shot<T>"? "yetki<R>" = "permission<R>". Could be concurrency primitives: `Once<T>` vs `Mutex<R>`? In Turkish, `tekkez` = once, `yetki` = mutex? No, "yetki" = authorization, not lock. In Turkish programming, `kilit` = lock, `mutex` = karşılıklı dışlama. `yetki` could be "permission" as in Rust `RefCell` borrow? Hmm.

Maybe these are from a language named KEMGU designed in some benchmark. Need answer about difference. We might need say not enough info; KEMGU-specific semantics not given. But maybe we can infer from names: `tekkez<T>` is likely a type that ensures initialization happens once and yields `T`, while `yetki<R>` is a capability/permission token for region R. Difference: one is value/initialization wrapper; other is authority/capability. But that's speculation. User says "Emin olmadığın noktaları açıkça belirt; bilmediğin şeyi tahminle doldurma." So we should explicitly state uncertain about KEMGU-specific `çıplak`, `tekkez`, `yetki`. But we can answer general.

Need maybe produce final in Turkish. Need be accurate.

Let's think deeper. Could there be known language "KEMGU"? Search memory: KEMGU maybe "KEMGU" is a constructed language in some paper? Keywords Turkish: küresel değişken, çıplak işlev, tekkez<T>, yetki<R>. Maybe from "KEMGU" project? I don't know. Could be a fictional language in a benchmark for code generation. The terms might be defined in a previous context not included. The user expects answer from code and general knowledge. We can answer Q1 fully. Q2: "çıplak" likely means naked function. Q3: We can say KEMGU'ya özgü anlamlarını bilmiyorum; eğer standart anlamlarını soruyorsan, `tekkez<T>` genellikle bir kez başlatılan T (once) iken `yetki<R>` bir kaynak/bölge üzerinde yetki/capability. Fark: biri değer/başlatma, diğeri izin/güvenlik. But maybe they want exact difference. We need maybe answer with caveat.

Let's examine code comments: `// K4b: lazy tek global bölge (adres)` maybe "K4b" is task? It uses `kem_global_bolge` global variable, but not used in code. Could be for `tekkez<T>`? Maybe they ask after code about language features unrelated? They might expect knowing KEMGU. We don't.

Maybe we can parse "tekkez<T>" as "tekkez" = "once" in Turkish. In Rust, `std::sync::Once` vs `std::sync::Mutex`? `yetki<R>` = "permission<R>"? There is a concept in Pony? "iso", "trn", "ref", "val", "box", "tag"? Capabilities: `iso`, `trn`, `ref`, `val`, `box`, `tag`. "yetki" could be "capability". "tekkez<T>" maybe "Once<T>" and "yetki<R>" maybe "Authority<R>" for capabilities. Hmm.

Could be from language "KEMGU" where `tekkez<T>` is "once<T>" for one-time initialization, `yetki<R>` is "permission<R>" for region-based memory. Difference: `tekkez<T>` stores a value initialized once; `yetki<R>` represents a capability to access region R, not a value. But uncertain.

Let's focus Q1. Need step by step. Include details:
- `istek` handles zero.
- `toplam` calculation: header + request round up to 16. Explain formula.
- Free list first-fit: walk singly linked list through block headers. Each free block header has size at +0, next at +8. If block size >= total, unlink from list by updating head or previous next. Return user pointer at block+16. No splitting, so if larger block, whole block is consumed; internal fragmentation possible.
- Bump allocation: if no free block, take from bump area. Check overflow: p + toplam > kem_son. If not enough return null. Advance bump, write size and next=0 in header, return p+16.
- Note no free/coalesce/split, no memset, no thread safety, no checking alignment of free list? But free list blocks presumably aligned. `kem_bump` 16-aligned; `toplam` multiple 16 so returned ptr p+16 aligned 16? If header 16, p 16 aligned, p+16 aligned. Free list cur maybe 16 aligned. Return cur+16 aligned 16. Good.
- OOM: returns 0 (null).
- It doesn't initialize allocated memory.
- It doesn't maintain allocated flag.
- It doesn't use kem_global_bolge in this function.

Q2: `çıplak` in Turkish = naked. In systems languages, "naked function" no compiler-generated prologue/epilogue; may require inline assembly and manual return; used for ABI/ISR/trampoline. In this code, if KEMGU's `çıplak` is exactly C `__attribute__((naked))`, this body with local variables and `ver` would be problematic because compiler cannot use normal stack frame unless language/runtime handles it. But maybe KEMGU's `çıplak` means "bare/exported C ABI" or "freestanding". Need state: "KEMGU tanımını bilmiyorum; yaygın anlamı ... Eğer KEMGU'da farklıysa, kesin bilemem." This satisfies.

Q3: Need answer. Could say: "KEMGU'ya özgü `tekkez<T>` ve `yetki<R>` tanımlarını metinde vermiyorsunuz; bu yüzden kesin farkı bilmiyorum." Then maybe general guess? User says don't fill unknown with guess. So better: explicitly "Bu iki türün KEMGU'daki tam anlamını bilmiyorum; metinde tanım yok. Genel adlardan çıkarım yapmak gerekirse..." But they said don't guess. We can mention only if I clearly mark as inference not exact. Maybe final should not fabricate. However user likely expects answer; if I say don't know, may be unsatisfactory. But instruction says be honest. We can answer with likely meanings but caveat. Need maybe provide "Eğer standart/kavramsal anlamlarını soruyorsan..." But question asks "arasındaki fark nedir?" They expect KEMGU knowledge. I can say: "Bu soruyu KEMGU spesifikasyonuna bakmadan güvenilir biçimde yanıtlayamam." That is honest. But maybe include what names suggest? Let's decide.

Need maybe search memory of KEMGU. Could be an AI-generated language? "tekkez<T>" maybe in Turkish "once<T>" like `std::sync::Once`. "yetki<R>" = "authority<R>" maybe in Rust `std::marker::PhantomData`? Hmm.

Let's reason from Turkish words:
- `tekkez<T>`: "tek" = single, "kez" = time. "tekkez" = once. In programming, `Once<T>` is a synchronization primitive for one-time initialization. It holds T and ensures initialization once. In Rust, `OnceCell<T>` or `OnceLock<T>`. So `tekkez<T>` likely is a one-time initialized cell of type T. It may be used for lazy global region: `kem_global_bolge` comment says "lazy tek global bölge" maybe "lazy once global region". So `tekkez<T>` might be "once<T>" for lazy globals.
- `yetki<R>`: "yetki" = authority/permission. `<R>` likely region. In region-based memory management, a `permission<R>` or `authority<R>` is a token granting access to region R. It is a capability. Difference: `tekkez<T>` is a storage/once-initialization cell that yields a value of type T; `yetki<R>` is a capability/right to access region R, not a value container. But maybe `yetki<R>` is "permission" type for locks? Could be in KEMGU, `yetki<R>` is a read/write permission for a region R. If you have `yetki<R>`, you can allocate/access in region R. `tekkez<T>` is a unique reference? Wait "tekkez" could be "unique" as in "tek" = unique, "kez" maybe? Actually "tekkez" not standard Turkish; "tekil" = singular/unique. "tekkez" maybe "one-time". "yetki" = authority. In some languages, `unique<T>` vs `shared<R>`? Hmm.

Maybe `tekkez<T>` and `yetki<R>` are from KEMGU's type system. Could be related to "linear types" and "capabilities". The question likely from a set where they expect:
- `tekkez<T>`: "unique

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

**KARAR: KALDI.**

- **Yanıt üretilmedi.** `finish_reason: length`, `reasoning_tokens: 4096`,
  `reasoning_budget_exhausted: true` — tüm bütçe düşünmeye gitti, `content` boş.
  4096 bu model için yetersiz (model kataloğu 65536'ya kadar izin veriyor).
- **Düşünme metninden okunabilen:** S1 (malloc algoritması) koddan DOĞRU
  çözülmüş (ilk-uyum serbest liste + bump, 16 bayt başlık, bölme/birleştirme yok).
- **S2 (`çıplak`):** GCC `__attribute__((naked))` analojisi; yerel değişken
  kullanımıyla çeliştiğini fark ediyor ama çözmüyor. Region-prologue (ρ
  parametresi + `kdl_bolge_olustur` → `malloc` öz-tetiklemesi) hiç yakalanmadı.
- **S3:** "tek kez" / "yetki" sözlük anlamından tahmin; KEMGU bilgisi yok.

Not: tekrar ölçüm için `max_tokens` ≥ 16384 ya da düşük `reasoning_effort`
gerekir; aksi halde ölçülen şey modelin bilgisi değil bütçe kesintisidir.
