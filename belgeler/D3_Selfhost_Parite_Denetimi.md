# Self-Host Codegen Parite Denetimi (D3)

> Otomatik denetim · C `llvm.c` (5724 satır) ↔ self-host `codegen.kem` (3860 satır)
> feature-coverage farkı. Amaç: tam self-hosting (gate #1) önündeki **gerçek**
> boşlukları, tasarımsal-kasıtlı eksiklerden ve bölge/F4 işinden ayırmak.

> **DURUM GÜNCELLEMESİ (2026-06-21, branch `claude/laughing-jackson-9f2295`):** Aşağıdaki
> Öncelik-1 + bazı Öncelik-2 boşlukları **codegen.kem'e PORTLANDI** (her biri codegen_diff +
> bootstrap fixpoint birebir; korpus 58→66/66; self_driver 4-mod yeşil):
> - ✅ bit/kaydırma (& \| ^ << >> ~) · ✅ pointer deref `*x` · ✅ float/double aritmetik +
>   karşılaştırma + **cast (sitofp/fptosi/fpext/fptrunc)** · ✅ eşleş (skaler **+ tagged-union
>   sonuç/seçimlik** yapıcı+destructuring+bind) · ✅ için (for-loop, heap dizi) ·
>   ✅ **çeşit ADT TAM** (enum: cv_* yan-kanal + ll_tip→i8 + YOL disc + eşleş DESEN_YOL;
>   **payload**: cc_* node-registry + {i8,payloadlar} anonim struct + CAGRI-YOL yapıcı +
>   eşleş payload-extract+ofset, tek/çoklu/payloadsuz varyant; --ast parite korundu). korpus 58→68/68.
>
> **Kalan (her biri ayrı dedicated pas):** MODUL/DISA/UYGULA/YOL (modül mangling + method dispatch),
> ve/veya short-circuit (hot-path, codegen.kem ağır kullanır → büyük IR diff, düşük fayda),
> non-ASCII işlev adı `@"böl"` (codegen.kem'de `\"` hiç yok → self-host `\"` lexing riski),
> kesirli32 bidirectional çıkarsama.

## 1. Özet

Self-host `codegen.kem`, C `llvm.c`'nin **çekirdek imperatif altkümesini** parite
ile karşılıyor: tamsayı/mantıksal aritmetik, struct-by-value, referans parametreleri,
heap-uniform diziler, kontrol akışı (eğer/iken), atama lvalue'leri (tanımlayıcı/
erişim/indeks), F4.2a ρ-ABI bölge geçişi, alloca-hoist, `olarak` cast'i ve birleşik
sürücü (`--token/--parse/--check/--llvm`). Bu, self-host fixpoint bootstrap'in
(lexer/parser/checker/codegen byte-eşdeğer) zaten kanıtladığı düzeydir — yani
codegen.kem **kendi kaynağını** (bu altküme ile yazılmış) derleyebiliyor.

Ancak C tarafı, self-host'un **lower etmediği** geniş bir özellik kümesini destekler:
**float/double tamamen yok**, **eşleş/çeşit/lambda parse edilip codegen'e inmiyor**,
**generic monomorphization yok** (heap-uniform + checker `?`-erteleme ile telafi),
bit/shift/xor lowering eksik, pointer deref/`~` placeholder, concurrency/MMIO/SIMD/
yetki/sabitsüre yalnız `--check`. Manşet boşluklar: **float**, **eşleş/çeşit**,
**bit-shift/xor**, **pointer deref** — bunlar "self-host *gerçek dili* derleyebilir"
eşiğinin önündeki gerçek to-do'lar. Lambda/closure ise heap-uniform self-host'ta
tasarımsal olarak kasıtlı yok.

## 2. Parite Tablosu

### AST düğüm türleri
| Özellik | C llvm.c | self-host |
|---|---|---|
| TAM / MANTIKSAL / METIN / KARAKTER | ✓ | ✓ (KARAKTER kısmi: i32) |
| KESIRLI (float/double literal) | ✓ | ✗ (parse bile yok, ~satır 1026 HATA) |
| TANIMLAYICI / IFADE_DEYIMI | ✓ | ✓ |
| IKILI (arith + cmp) | ✓ | ✓ |
| IKILI (bit/shift/xor) | ✓ (and/or/xor/shl/lshr/ashr) | kısmi (yalnız & →and, \| →or) |
| IKILI (ve/veya short-circuit) | ✓ (branch) | kısmi (non-short-circuit) |
| TEKLI neg/değil | ✓ | ✓ |
| TEKLI &/&değişken (address-of) | ✓ | kısmi (yalnız TANIMLAYICI operand) |
| TEKLI *x deref / ~ (bit değil) | ✓ | ✗ (placeholder '0') |
| YAPI_OLUSTUR / ERISIM | ✓ | ✓ |
| DIZI_OLUSTUR / INDEKS | ✓ (heap+stack, D-069 bound) | ✓ (yalnız heap, runtime bound) |
| TIP_DONUSTUR (olarak) | ✓ (zext/sext/trunc/sitofp/fptosi/...) | kısmi (sext/trunc; float yok) |
| BLOK / DEGISKEN / ATAMA / VER | ✓ | ✓ |
| EGER / IKEN | ✓ | ✓ |
| ICIN (for) | ✓ (heap KdlDizi*) | ✗ (codegen'de yok) |
| GUVENSIZ | ✓ | ✓ |
| ESLES / ESLES_KOLU | ✓ (destructuring+binding) | ✗ (parse var, lower yok) |
| CESIT (çeşit/enum/ADT) | ✓ (tagged-union) | ✗ (parse var, lower yok) |
| LAMBDA | ✓ (fat-value+capture) | ✗ (tasarım: yok) |
| YOL (çeşit/modül) | ✓ | ✗ |
| MODUL / DISA | ✓ | ✗ |
| UYGULA (method dispatch) | ✓ | ✗ |
| SATIRICI_ASM (inline asm) | ✓ | ✗ |
| KULLAN_IFADE / IMHA_IFADE (linear) | ✓ | ✗ |

### Builtin aileleri
| Aile | C llvm.c | self-host |
|---|---|---|
| metin_* / dosya_* / yaz_* / yazdir_* / arg_* | ✓ | ✓ |
| dizi_* (element-aware) | ✓ (tam/tam64/ptr/yapi) | kısmi (yapı varyantı yok) |
| bellek_al/serbest/kopyala | ✓ | kısmi (decl var, routing sınırlı) |
| otp_* / tekkez_* / sabitsüre_* / yetki_* | ✓ | ✗ (yalnız --check) |
| mmio_oku/yaz / vektor_* (SIMD) | ✓ | ✗ (yalnız --check) |
| bölge_al | ✓ (malloc-proxy) | ✗ |

### Generic / sonuç-seçimlik-çeşit / struct-float-ref
| Özellik | C llvm.c | self-host |
|---|---|---|
| Monomorphization (worklist, mangle$T) | ✓ | ✗ (tip-param parse+discard) |
| sonuç<T,H> / seçimlik<T> tagged-union | ✓ | ✗ |
| tamam/hata/değer/hiç ctor + eşleş bind | ✓ | ✗ (yalnız --check) |
| çeşit/enum disc + payload | ✓ | ✗ |
| Struct-by-value / &T / multi-int | ✓ | ✓ |
| *T deref + pointee width | ✓ | ✗ (placeholder) |
| float/double + fadd/fcmp/sitofp | ✓ | ✗ |
| F4.2a ρ-ABI (non-main fn ptr %rho + main seed) | ✓ | ✓ |
| D-069 stack array bound-check | ✓ | yok (heap-uniform → runtime; **tasarım**) |
| --hedef flag / ARM64 | ✗ | ✗ |

## 3. Gerçek Boşluklar (yalnız C'de olup self-host'ta olmayanlar)

**(a) TASARIM — heap-uniform self-host'ta kasıtlı yok (parite hedefi DEĞİL):**
- LAMBDA / closure / fat-value ABI — fat-pointer + closure capture yok.
- D-069 inline stack-OOB kontrolü — stack `[N×T]` yolu yok; tüm diziler heap →
  runtime sınır-kontrolü (`kdl_runtime.c`). CLAUDE.md invaryantı: stack dizi
  eklenirse inline kontrol aynı commit'te zorunlu.
- Stack DIZI_OLUSTUR length-metadata — stack dizi olmadığı için konu dışı.

**(b) BÖLGE — B1/F4.2b işi, ertelenmiş:**
- `bölge_al` gerçek arena (C'de malloc-proxy; self-host'ta hiç yok) — F4 sonrası.
- Closure/dizi/metin env free (region-dealloc) — C'de bile leak; F4'e ertelenmiş.

**(c) GERÇEK BOŞLUK — taşınması gereken ama taşınmamış:**
- **float/double (kesirli32/64)** — KESIRLI literal parse bile edilmiyor; `ll_ikili`
  yalnız add/sub/mul/sdiv/srem/and/or. fadd/fsub/fmul/fdiv/frem/fcmp/sitofp/fptosi
  yok. **En kritik boşluk.**
- **eşleş (ESLES/DESEN_*) lowering** — parser üretiyor, codegen'de branch yok →
  default ret/0. sonuç/seçimlik kullanan her gerçek program kırılır.
- **çeşit/enum/ADT lowering** — parse var, ctor/disc emission yok.
- **sonuç<T,H>/seçimlik<T> tagged-union + tamam/hata/değer/hiç ctor codegen** —
  yalnız --check (eşleş ile aynı bağımlılık zinciri).
- **bit/shift/xor lowering** — `^ << >>` checker+parse destekli ama `ll_ikili`
  shl/lshr/ashr/xor emit etmez. Düşük efor.
- **pointer deref `*x` ve `~`** — '0' placeholder; güvensiz/sistem kodu için.
- **TANIMLAYICI dışı &address-of** (`&yapı.alan`, `&dizi[i]`).
- **için (for-loop)** — C'de heap KdlDizi* üzerinde var; self-host'ta yok.
- **dizi_*_yapi (struct-element dizi, D-087)** — yapı varyantı yok.
- **MODUL/DISA/UYGULA/YOL emit** — modül mangling, method dispatch, çeşit-yol.
- **ve/veya short-circuit** — self-host non-short-circuit (yan etkili operandda fark).
- **olarak float cast'leri** (sitofp/fptosi/fpext/fptrunc) — float boşluğuna bağlı.
- **inline asm (satirici_asm)** — yok.

## 4. Öneri (öncelikli to-do — gate #1: self-host gerçek dili derler)

**Öncelik 1 — kritik yol (bunlar olmadan self-host gerçek program derleyemez):**
1. **float/double** — KESIRLI literal parse + `ll_ikili`'ye fadd/fsub/fmul/fdiv/frem
   + fcmp ordered + sitofp/fptosi/fpext/fptrunc. Tek başına en büyük kapsam açığı.
2. **eşleş + sonuç/seçimlik + tamam/hata/değer/hiç codegen** — tagged-union
   `{i8 tag, payload}` emit + ctor + eşleş destructuring/binding. Tek bağımlılık
   zinciri olarak birlikte taşınmalı (parser zaten üretiyor).
3. **bit/shift/xor lowering** — `ll_ikili`'ye shl/lshr/ashr/xor; düşük efor.
4. **pointer deref `*x` + `~`** — placeholder'ı gerçek load/xor ile değiştir.

**Öncelik 2 — dil bütünlüğü:**
5. çeşit/enum lowering (eşleş ile birlikte test).
6. için (for-loop) — heap KdlDizi* üzerinde kdl_dizi_boyut/al döngüsü (C ile birebir).
7. MODUL/DISA/UYGULA/YOL — modül mangling + method dispatch.
8. TANIMLAYICI dışı &address-of + dizi_*_yapi varyantı + ve/veya short-circuit.

**Tasarımsal olarak boşluk DEĞİL (taşınmayacak):** lambda/closure/fat-value;
stack-dizi inline bound-check; bölge_al arena + region-dealloc (BÖLGE/F4 track);
generic monomorphization (heap-uniform + checker `?`-erteleme ile telafi — gate #1
için zorunlu değil).

**Ayrı spec track'leri (gate #1 dışı):** concurrency (DRF V2), MMIO, SIMD, yetki,
sabitsüre, otp, tekkez — C'de codegen var ama her biri kendi PR/track'inde.

**KRİTİK — D1/D2-sınıfı routing aynası:** `llvm.c`'ye inen builtin/dispatch routing
düzeltmeleri (örn. `dizi_*` element-tipi yönlendirme, builtin prefix routing,
yapıcı dispatch) self-host `builtin_kdl_ad` prefix-routing + `dizi_*` özel-durumları
ile **AYNA güncelleme** gerektirebilir — aksi halde fixpoint kayar. Her routing
PR'ında codegen.kem'in karşılık gelen tablosunu aynı anda güncelle ve
`calistir_codegen_bootstrap` ile stage1==stage2 doğrula. (D2 metin_/dosya_ `!ik`
guard'ı codegen.kem'i ETKİLEMEDİ — çünkü codegen.kem kendi kaynağında builtin-adlı
kullanıcı işlevi gölgelemiyor; fixpoint birebir kaldı. Ama dizi_* element-routing
veya gelecekteki routing işi ayna gerektirebilir.)
