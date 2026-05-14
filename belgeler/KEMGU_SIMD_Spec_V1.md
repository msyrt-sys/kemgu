# KEMGU SIMD Intrinsics Spec V1

**Durum:** TASLAK (Direktif Hedef 2 — Maksimum Performans altında).
**Spec içi alt-adımlar otomatik onaylı; Direktif Ek v1.3 ile MERGE bekler.**

---

## SD.0 — Motivasyon ve Üç Stratejik Hedef Bağlantısı

KEMGU `vektör<T, N>` tipi, **Single Instruction Multiple Data** (SIMD)
disiplinini tip sistemine taşır. `N` adet `T` tipi elemanı **tek mantıksal değer**
olarak gören, donanım vektör register'larıyla (x86_64 XMM/YMM/ZMM, ARM64 NEON
Q-register, ARM SVE2 scalable) doğrudan eşlenen aritmetik birimidir.

### Stratejik Hedef Bağlantısı

- **HEDEF 1 (Kırılamaz Güvenlik):** SIMD operasyonları sabit-süre disiplinine
  doğal uyum sağlar — vektör add/xor/and gibi `lane`-paralel işlemler veriden
  bağımsız sabit süredir. Constant-time kriptografi (AES-NI, ChaCha20, Curve25519
  field arithmetic) için `vektör<sabitsüre<dtam8>, 16>` ile kombinlenebilir.
- **HEDEF 2 (Maksimum Performans — Bu spec'in birincil amacı):** Önemli
  iş yükleri:
  - **AI inference:** GEMM (matris çarpımı), conv2d, attention dot product —
    `f32` / `bf16` / `i8` tensor operations.
  - **Sinyal işleme:** FFT, FIR filter, convolution — `f32` / `f64`.
  - **Oyun motoru:** Vector math (3D transform, dot/cross), particle systems,
    physics broadphase — `f32x4`.
  - **Kripto:** AES round, ChaCha20 quarter-round, Curve25519 field op —
    `dtam8x16`, `dtam32x8`.
  - **Metin işleme:** `kdl_metin_eslestir` (vektörize edilmiş string search),
    UTF-8 doğrulama — `dtam8x16`/`dtam8x32`.
- **HEDEF 3 (Evrensel OS):** Aynı KEMGU kaynak kodu hem x86_64 AVX2/AVX-512
  hem ARM64 NEON üzerinde **LLVM IR generic vector type** (`<N x T>`) sayesinde
  taşınabilir; LLVM backend hedefe özel intrinsic'e indirir.

### ASLA Listesi Hatırlatması

- ASLA implicit auto-vectorization: KEMGU **explicit `vektör<T, N>` tipi
  gerektirir**. Scalar loop'lar otomatik vektorize edilmez (deterministik
  performans → predictable). Auto-vectorization hint olabilir; semantic değil.
- ASLA hedef-spesifik intrinsic syntax: `_mm_add_epi32`, `vaddq_s32` yasak —
  KEMGU sadece `+` operatörünü `vektör` üzerinde tanımlar. LLVM IR'e generic
  `<N x T>` IR çıkar; hedef seçimi link-time `clang -march=...` ile yapılır.
- ASLA implicit conversion: `vektör<tam32, 4>` ile `vektör<tam32, 8>` farklı
  tiptir; `vektör_genişlet`/`vektör_daralt` explicit operasyonlar gerekir.
- ASLA exception: Yanlış lane sayısı, yanlış element tipi → derleme zamanı
  hatası (V001-V010).

---

## SD.1 — Tip Tanımı

```
vektör<T, N> : tip      (T : vektör-yetenekli skaler tip, N : sabit lane sayısı)
```

`vektör` bir tip kurucusudur; runtime temsili `[N x T]` LLVM vektör'üne (yani
N adet T elemanın bitişik, hizalı bellek bloğuna) eşlenir.

### SD.1.1 Lane Sayısı `N` (Sabit Compile-Time Sayı)

`N` literal compile-time bir tam sayı olmalıdır. İzin verilen değerler:

```
N ∈ {2, 4, 8, 16, 32, 64}   (2^1 .. 2^6)
```

`N = 1` yasak (skalerden farkı yok); `N = 128, 256+` yasak (V1 — V2'de
scalable vector için ayrı yapı).

### SD.1.2 Element Tipi `T` (Vektör-Yetenekli)

V1'de izinli `T` tipleri:

| Kategori | T | Sebep |
|----------|---|-------|
| Tamsayı (işaretli) | tam8, tam16, tam32, tam64 | AVX2/AVX-512/NEON tüm width'ler |
| Tamsayı (işaretsiz) | dtam8, dtam16, dtam32, dtam64 | Aynı |
| Kesirli | kesirli32 (`f32`), kesirli64 (`f64`) | SSE/AVX/NEON FP unit |
| Mantıksal | mantıksal | Mask register (AVX-512 `k0..k7`, NEON predicate) |

V1'de **YASAK** (V001 VEKTOR_ELEMENT_INVALID):
- `karakter` — UTF-8 kod noktası, byte stream. Yerine `dtam32`.
- `metin` — değişken uzunluk (UTF-8). Yerine `vektör<dtam8, 16>` byte-wise.
- `yapı` — V2'de "vektör of struct" (SoA gather/scatter). V1'de yasak.
- `referans`/`pointer` — pointer'lar vektörize edilemez (V2'de scatter/gather).
- `seçimlik`/`sonuç`/`işlev`/`tekkez`/`sabitsüre` — V1: vektör katmanı dışı.

V2'de düşünülen: `sabitsüre<vektör<T, N>>` (sabit-süre vektör), `tekkez<vektör<T, N>>`
(linear ownership), `vektör<sabitsüre<T>, N>` (lane-wise secret).

### SD.1.3 Pratik Aliasler (V1 Öneri — Optional)

```
v4tam32     = vektör<tam32, 4>        (SSE/NEON 128-bit i32x4)
v8tam32     = vektör<tam32, 8>        (AVX2 i32x8)
v16tam32    = vektör<tam32, 16>       (AVX-512 i32x16)
v4kesirli32 = vektör<kesirli32, 4>    (f32x4 — 3D vector math)
v8kesirli32 = vektör<kesirli32, 8>    (AVX2 f32x8)
v16kesirli32= vektör<kesirli32, 16>   (AVX-512 f32x16)
v2kesirli64 = vektör<kesirli64, 2>    (f64x2)
v4kesirli64 = vektör<kesirli64, 4>    (AVX2 f64x4)
v8kesirli64 = vektör<kesirli64, 8>    (AVX-512 f64x8)
v16dtam8    = vektör<dtam8, 16>       (16-byte AES block, NEON Q)
v32dtam8    = vektör<dtam8, 32>       (AVX2 u8x32)
v64dtam8    = vektör<dtam8, 64>       (AVX-512 u8x64)
```

V1'de aliasler **stdlib `simd.kem` modülünde** type alias olarak tanımlanır
(eğer KEMGU type alias desteklerse); aksi takdirde manuel tip yazımı zorunlu.

---

## SD.2 — Operasyonlar

### SD.2.1 Yükle / Sakla (Load / Store)

```
vektör_yükle<T, N>(p: &Dizi<T>, ofs: tam32) -> vektör<T, N>
vektör_sakla<T, N>(p: &değişken Dizi<T>, ofs: tam32, v: vektör<T, N>)
```

`p[ofs..ofs+N]` aralığını **vektör register'a** yükler. `ofs + N <= uzunluk(p)`
runtime kontrol değil — kullanıcının sorumluluğu (`güvensiz` bloktan çıkmayan
KEMGU kontrolü ile, V2'de length-aware Dizi tipi).

Hizalama: `vektör_yükle_hizalı` (aligned) ve `vektör_yükle` (unaligned) iki form.
V1'de tek form (unaligned, taşınabilir).

### SD.2.2 Aritmetik (Elementwise)

```
vektör + vektör   →  lane-wise add
vektör - vektör   →  lane-wise sub
vektör * vektör   →  lane-wise mul
vektör / vektör   →  lane-wise div (FP), sdiv/udiv (int)
vektör % vektör   →  lane-wise rem  (int — FP'de YASAK V005)
```

Sözdizim: standart `+ - * / %` operatörleri tip sistemi tarafından `vektör<T, N>
+ vektör<T, N>` → `vektör<T, N>` olarak override edilir. Skaler `+` ile aynı
tokenler; tip kontrol disambiguates.

### SD.2.3 Skaler Broadcast

```
vektör_doldur<T, N>(s: T) -> vektör<T, N>      // s, s, s, ..., s (N kez)
```

V1'de skaler `*` operatörü de override edilebilir:
```
v * s      →  v * vektör_doldur(s)            (s broadcast then mul)
```
ama V1'de **explicit `vektör_doldur` gerekir** — implicit broadcast belirsiz
(`v * s` veya `v * vektör[s,s,...]`?).

### SD.2.4 Karşılaştırma (Mask Üretir)

```
vektör_eşit<T, N>(a, b: vektör<T, N>) -> vektör<mantıksal, N>
vektör_küçük<T, N>(a, b: vektör<T, N>) -> vektör<mantıksal, N>
vektör_büyük, vektör_küçük_eşit, vektör_büyük_eşit  (analog)
```

`vektör<mantıksal, N>` — N lane'lik mask vektörü. AVX-512'de `kN` register,
AVX2'de `<N x i1>` (LLVM IR-level), NEON'da predicate register.

### SD.2.5 Mantıksal/Bit Operatörler

```
vektör & vektör   →  bitwise AND (tamsayı vektörler)
vektör | vektör   →  bitwise OR
vektör ^ vektör   →  bitwise XOR
~vektör           →  bitwise NOT
```

Mantıksal vektörler için:
```
vektör_ve, vektör_veya, vektör_değil   (mask vektörü üzerinde)
```

### SD.2.6 Karıştırma (Shuffle / Permute)

```
vektör_karıştır<T, N>(v: vektör<T, N>, indeksler: [N tam32]) -> vektör<T, N>
```

`indeksler[i]` `0..N-1` aralığında — yeni `v'[i] = v[indeksler[i]]`. Index
compile-time sabit dizi olmalı (LLVM `shufflevector` instruction).

```
vektör_birleştir<T, N>(a, b: vektör<T, N>, indeksler: [N tam32]) -> vektör<T, N>
```

`indeksler[i] < N` → `a[indeksler[i]]`; `indeksler[i] ∈ [N, 2N)` → `b[indeksler[i]-N]`
(LLVM 2-source shufflevector).

### SD.2.7 Azaltma (Horizontal Reduction)

```
vektör_topla<T, N>(v: vektör<T, N>) -> T          // sum(v[0..N])
vektör_çarp<T, N>(v: vektör<T, N>) -> T           // product(v[0..N])
vektör_min<T, N>(v: vektör<T, N>) -> T            // min(v[0..N])
vektör_max<T, N>(v: vektör<T, N>) -> T            // max(v[0..N])
vektör_ve_azalt<N>(v: vektör<mantıksal, N>) -> mantıksal     // all(v)
vektör_veya_azalt<N>(v: vektör<mantıksal, N>) -> mantıksal   // any(v)
```

LLVM intrinsic: `llvm.vector.reduce.add.v<N>i<W>` vs.

### SD.2.8 Maskelenmiş Operasyonlar

```
vektör_maskeli_yükle<T, N>(p, ofs, mask: vektör<mantıksal, N>)
    -> vektör<T, N>                       // mask[i]=1 ise yükle, değilse 0
vektör_maskeli_sakla<T, N>(p, ofs, v, mask: vektör<mantıksal, N>)
    -> boş                                // mask[i]=1 ise yaz
vektör_seç<T, N>(mask: vektör<mantıksal, N>,
                  a, b: vektör<T, N>) -> vektör<T, N>     // ternary lane-wise
```

LLVM: `llvm.masked.load`, `llvm.masked.store`, `select` instruction.

### SD.2.9 Element Çıkarma / Yerleştirme

```
vektör_eleman<T, N>(v: vektör<T, N>, i: tam32) -> T            // v[i]
vektör_eleman_yerleştir<T, N>(v: vektör<T, N>, i: tam32, s: T) -> vektör<T, N>
```

LLVM `extractelement`, `insertelement`. `i` runtime olabilir (V1).

### SD.2.10 Tip Dönüşümleri (Vektör Boyutu Sabit, Element Tipi Değişir)

```
vektör_olarak_kesirli32<N>(v: vektör<tam32, N>) -> vektör<kesirli32, N>
vektör_olarak_tam32<N>(v: vektör<kesirli32, N>) -> vektör<tam32, N>    // truncate
```

V1'de **küçük bir built-in set**: yaygın cast'ler (int↔float, signed↔unsigned).
V2'de generic `vektör_olarak<T2>(v) -> vektör<T2, N>` çıkarsama.

---

## SD.3 — Hizalama (Alignment)

Vektör tipler aşağıdaki minimum hizalama gereksinimine sahiptir:

| `vektör<T, N>` toplam boyut | Hizalama | Donanım |
|---|---|---|
| 16 byte | 16 | SSE/NEON 128-bit |
| 32 byte | 32 | AVX2 256-bit |
| 64 byte | 64 | AVX-512 512-bit |

Runtime alloc API:
```
kdl_bellek_hizali_al(boyut: tam64, hizalama: tam64) -> *dtam8
kdl_bellek_hizali_serbest(p: *dtam8)
```

Bölge tabanlı tahsis: arena pointer'larının vektör-hizalı olduğu garanti
edilmez; vektör bölgesi için ayrı `kdl_bellek_hizali_al` yolu gerekli.

LLVM IR `alloca` `align N` ile emit edilir (otomatik).

---

## SD.4 — Tip Kontrol Kuralları

### V001 — VEKTOR_ELEMENT_INVALID
```
vektör<T, N> tipinde T vektör-yetenekli olmalı (tamsayı, kesirli, mantıksal)
```

### V002 — VEKTOR_LANE_INVALID
```
vektör<T, N> tipinde N {2, 4, 8, 16, 32, 64} kümesinde olmalı
```

### V003 — VEKTOR_ARITMETIK_TIP_UYUMSUZ
```
vektör<T1, N1> + vektör<T2, N2>: T1 == T2 ve N1 == N2 gerekli
```

### V004 — VEKTOR_SKALER_KARMA
```
vektör<T, N> + T yasak (V1) — explicit vektör_doldur(s) kullan
```

### V005 — VEKTOR_FP_MOD
```
vektör<kesirli32/64, N> % vektör — kesirli vektörde `%` yasak
```

### V006 — VEKTOR_BIT_FP
```
vektör<kesirli, N> & vektör — bit op kesirli vektörde yasak
```

### V007 — VEKTOR_KARISTIR_INDEKS_RANGE
```
vektör_karıştır(v, [i1, i2, ...]) — i_k ∈ [0, N) olmalı (compile-time check)
```

### V008 — VEKTOR_KARISTIR_INDEKS_NONCONST
```
vektör_karıştır indeksleri compile-time sabit dizi olmalı
```

### V009 — VEKTOR_AZALT_TIP
```
vektör_topla operandı vektör<T, N>, T sayısal olmalı
vektör_ve_azalt operandı vektör<mantıksal, N> olmalı
```

### V010 — VEKTOR_MASK_TIP
```
vektör_seç maskesi vektör<mantıksal, N>, a/b tipi vektör<T, N> aynı N
```

---

## SD.5 — LLVM IR Çıkarımı

### Tip Eşleme

```
vektör<tam32, 4>      →  <4 x i32>
vektör<kesirli32, 8>  →  <8 x float>
vektör<mantıksal, 16> →  <16 x i1>
vektör<dtam8, 16>     →  <16 x i8>
```

### Operasyon Eşleme

| KEMGU | LLVM IR |
|---|---|
| `a + b` (int) | `add <N x iW> a, b` |
| `a + b` (FP) | `fadd <N x fW> a, b` |
| `a * b` (int) | `mul <N x iW> a, b` |
| `a / b` (signed int) | `sdiv <N x iW>` |
| `a / b` (unsigned int) | `udiv <N x iW>` |
| `a / b` (FP) | `fdiv <N x fW>` |
| `vektör_eşit` (int) | `icmp eq <N x iW> a, b → <N x i1>` |
| `vektör_küçük` (int signed) | `icmp slt` |
| `vektör_küçük` (FP) | `fcmp olt` |
| `vektör_topla` (int) | `call iW @llvm.vector.reduce.add.vNiW(...)` |
| `vektör_topla` (FP) | `call fW @llvm.vector.reduce.fadd.vNfW(fW 0.0, ...)` |
| `vektör_min` (int signed) | `llvm.vector.reduce.smin` |
| `vektör_max` (int unsigned) | `llvm.vector.reduce.umax` |
| `vektör_doldur(s)` | `insertelement` + `shufflevector splat` |
| `vektör_karıştır(v, idx)` | `shufflevector <N x T> v, undef, <N x i32> <idx...>` |
| `vektör_birleştir(a, b, idx)` | `shufflevector <N x T> a, b, <N x i32> <idx...>` |
| `vektör_eleman(v, i)` | `extractelement <N x T> v, i32 i` |
| `vektör_eleman_yerleştir` | `insertelement` |
| `vektör_seç(mask, a, b)` | `select <N x i1> mask, <N x T> a, <N x T> b` |
| `vektör_maskeli_yükle` | `call <N x T> @llvm.masked.load.vNT.p0(...)` |

### Hedef Bazlı Codegen (LLVM Off-Tree)

LLVM kendisi `<N x T>` → AVX2 `vpaddd`, AVX-512 `vpaddd zmm0`, NEON `add v0.4s, v0.4s, v0.4s`,
SVE2 `add z0.s, z0.s, z0.s` indirimini yapar. KEMGU **hedef-spesifik intrinsic
yazmaz** — taşınabilirlik garanti.

### Hizalama Annotation

```llvm
%v = load <4 x i32>, ptr %p, align 16
store <4 x i32> %v, ptr %q, align 16
```

`align` LLVM optimize için kullanır; runtime check değil.

---

## SD.6 — Auto-Vectorization

V1'de **kapalı** — KEMGU loop'ları otomatik vektörize etmez. Kullanıcı explicit
`vektör<T, N>` tipi yazmalı.

Sebep:
- Deterministik performans (geliştirici bilir vektör kodu yazdığını).
- Constant-time disiplini bozmaz (auto-vec çıkarımı tahmin edilemez).
- Debug edilebilirlik (LLVM IR'de `<N x T>` görmek kolay).

V2'de düşünülen: `vektorize` annotation (`#[vektorize(N=8)] iken ...`)
KEMGU-tarafı `vektör` koduna transform — bu da explicit kalır.

---

## SD.7 — İlk Kullanıcılar

### kdl_metin_eslestir — Vektörize String Search

Aho-Corasick yerine basit `memchr`-style aramada SIMD:
```kemgu
işlev kdl_metin_indeks_bul_simd(hedef: &Dizi<dtam8>, harf: dtam8) -> tam32 {
    değişken n: tam32 = uzunluk(hedef);
    değişken hedef_v: vektör<dtam8, 16> = vektör_doldur(harf);
    değişken i: tam32 = 0;
    iken i + 16 <= n {
        değişken blok: vektör<dtam8, 16> = vektör_yükle(hedef, i);
        değişken mask: vektör<mantıksal, 16> = vektör_eşit(blok, hedef_v);
        eğer vektör_veya_azalt(mask) {
            // İlk eşleşme bit-scan
            için j: tam32 in 0..16 {
                eğer vektör_eleman(mask, j) {
                    ver i + j;
                }
            }
        }
        i = i + 16;
    }
    // Kalan byte'lar scalar
    iken i < n {
        eğer hedef[i] == harf { ver i; }
        i = i + 1;
    }
    ver -1;
}
```

### Matris Çarpımı (test/ornekler/matris_carpim.kem)

```kemgu
işlev matris_carp_simd(A, B: &Dizi<kesirli32>, C: &değişken Dizi<kesirli32>,
                        M, N, K: tam32) {
    için i: tam32 in 0..M {
        için j: tam32 in 0..N adim 8 {
            değişken acc: vektör<kesirli32, 8> = vektör_doldur(0.0);
            için k: tam32 in 0..K {
                değişken a_skal: vektör<kesirli32, 8> = vektör_doldur(A[i*K + k]);
                değişken b_vec: vektör<kesirli32, 8> = vektör_yükle(B, k*N + j);
                acc = acc + a_skal * b_vec;
            }
            vektör_sakla(C, i*N + j, acc);
        }
    }
}
```

(Not: `adim` syntax V2 — V1'de `i = i + 8` manuel.)

---

## SD.8 — Test Stratejisi

`test/test_simd.c` — en az **25 test**, ASan + UBSan temiz:

1. Lexer: `vektör` keyword tanır.
2. Parser: `vektör<tam32, 4>` AST düğüm.
3. Parser: `vektör<vektör<tam32, 4>, 2>` (nested) hata (V010).
4. Tip: TIP_VEKTOR kategorisi oluşur.
5. Tip: equality nominal (`vektör<tam32, 4> == vektör<tam32, 4>`).
6. Tip kontrol: V001 (yapı element olamaz).
7. Tip kontrol: V002 (N=3 yasak).
8. Tip kontrol: V003 (farklı N).
9. Tip kontrol: V003 (farklı T).
10. Tip kontrol: V004 (skaler + vektör).
11. Tip kontrol: V005 (kesirli vektörde %).
12. Tip kontrol: V006 (kesirli vektörde &).
13. Aritmetik: `v + v` tip uyumlu.
14. Aritmetik: `v * v` tip uyumlu.
15. Karşılaştırma: `vektör_eşit` mask döner.
16. Karıştırma: `vektör_karıştır` indeks range kontrol.
17. Azaltma: `vektör_topla` (int).
18. Azaltma: `vektör_topla` (FP).
19. Maskelenmiş: `vektör_seç` üç vektör.
20. Element: `vektör_eleman` int dönüş.
21. Element: `vektör_eleman_yerleştir`.
22. Yükle/sakla: tip kontrolü.
23. Broadcast: `vektör_doldur(s)`.
24. LLVM: `<4 x i32>` IR çıkar (--llvm modu).
25. LLVM: `vektör_topla` → `llvm.vector.reduce.add.v4i32` çağrısı.
26. Runtime: matris çarpımı entegrasyon (bench).
27. Bench: SIMD vs scalar speedup ölçer (`test_bench.c`).

---

## SD.9 — V2'ye Bırakılanlar

- **Scalable vector** (`vektör_ölçeklenebilir<T>`, SVE2 / RVV) — runtime
  `vscale` değişkeni gereklidir.
- **Vector of struct (VoS)** — gather/scatter primitives.
- **`vektör<sabitsüre<T>, N>`** — lane-wise constant-time.
- **`tekkez<vektör<T, N>>`** — linear ownership için.
- **Tip alias / generic aliases** (`v4tam32 = vektör<tam32, 4>`) — KEMGU
  type alias özelliği gerekirse.
- **FMA (fused multiply-add)** intrinsic — `vektör_fma(a, b, c) = a*b + c`.
- **Vektör length-aware Dizi** — `DiziN<T, N>` ile yükle/sakla bounds check.
- **Auto-vectorize hint** — `#[vektorize]` annotation.

---

## SD.10 — Direktif İlişkisi

- **Hedef 1 (Güvenlik):** SIMD constant-time disiplinine doğal uyum.
  `vektör<sabitsüre<dtam8>, 16>` V2'de hedeflenir.
- **Hedef 2 (Performans):** Birincil motivasyon. AI/oyun/kripto/sinyal işleme.
  Generic LLVM IR `<N x T>` ile x86_64 ve ARM64 taşınabilir.
- **Hedef 3 (Evrensel OS):** ARM64 öncelik (DGX Spark, Android NDK).
  NEON 128-bit minimum, SVE2 V2.

---

**V1 onay durumu:** Spec içi alt-adımlar (sözdizim + parser + tip + LLVM
+ test + bench) otomatik onaylı. Direktif Ek v1.3'e MERGE bekler.

**Versiyon:** V1.0 — 2026-05-14
