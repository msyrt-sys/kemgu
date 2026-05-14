# KEMGU Operasyonel Semantik V1 (DRF Önkoşulu)

**Tarih:** 2026-05-14
**Durum:** TASLAK FORMALİZASYON — kâğıt üzerinde (mekanizasyon V2 saklı)
**Amaç:** DRF (Data Race Freedom) teoreminin genişletilmiş ifadesi için
gereken operasyonel altyapıyı (küçük-adım reduksiyon, izler, happens-before,
data race) formel olarak tanımlamak.
**Plan referansı:** `belgeler/KEMGU_DRF_Genisletme_Plan.md`, Faz A Adım 0.

---

## 0. Niye Bu Belge?

Mevcut DRF Teoremi (`KEMGU_Bellek_Modeli.md` satır 260-272) "Güvenli alt
kümede data race imkansız" şeklinde **doğal Türkçe + 4 satırlık ispat
taslağı**. Bu form:

- "Güvenli alt küme" tanımsız → formel ön-koşul yok.
- "Eşzamanlı erişim" tanımsız → küçük-adım modeli yok.
- "Data race" tanımsız → happens-before bağıntısı yok.

Bu belge yukarıdaki üç eksikliği kapatır. Notation tarzı: small-step
operasyonel semantik (Plotkin 1981, Felleisen–Hieb 1992 reduction context),
"izler" (Java MM, C++11 MM tarzı).

V1 kapsamı **kâğıt formel**. Mekanize ispat (Coq/Lean) Faz B'ye saklı
(Direktif Ek v1.1 spec-içi onay).

---

## 1. Sözdizim Soyutu (Kısa)

KEMGU'nun tam grammar'ı `belgeler/KEMGU_Grammar_EBNF.md`'de. Bu belge
**operasyonel semantik için gerekli minimum sözdizim subset'i** sayar:

```
τ  ::= tam_w | dtam_w | kesirli_w | mantıksal | karakter | boş
     | yapı_adı | Dizi<τ> | seçimlik<τ> | sonuç<τ,τ>
     | &τ | &değişken τ | *τ
     | tekkez<τ>                                  -- Linear V1
     | yetki<R>                                   -- Capability V1
     | sabitsüre<τ>                               -- Sabitsüre V1
     | işlev(τ⃗) -> τ

e  ::= x | n | f(e⃗) | e.l | e[e] | e₁ op e₂ | -e | !e
     | &e | &değişken e | *e
     | yapı_adı{l: e, ...} | [e₁, ..., eₙ]
     | |x⃗: τ⃗| e                                  -- lambda
     | eğer e { s⃗ } değilse { s⃗ }
     | eşleş e { desen => e, ... }
     | kullan(e) | imha(e)                       -- Linear V1 consumer
     | tekkez_yarat(e)                            -- Linear V1 producer
     | yetki_olustur(kt, izin) | delege(e,e) | geri_al(e)   -- Capability V1
     | sabitsüre_yarat(e) | ifşa(e)              -- Sabitsüre V1
     | görev_başlat(e) | birleştir(e)            -- Concurrency (V2 syntax)
     | gönder(g,e) | al(a) | dondur(e)           -- Concurrency

s  ::= değişken x: τ = e | x = e | ver e | { s⃗ } | e
     | (yukarıdaki control-flow ifadeleri deyim olarak)

d  ::= [gerçekzamanlı?] işlev f(x⃗: τ⃗) -> τ { s⃗ }
     | yapı T { l: τ; ... }
     | özellik P<τ⃗> { ... }
     | uygula P için T { d⃗ }
     | sabit C: τ = e
     | modül M { d⃗ }
     | kullan path

Π  ::= d⃗            -- program = üst düzey tanımların listesi
```

> **NOT — `görev_başlat`, `kanal_aç`, `gönder`, `al`, `dondur`** şu an
> KEMGU dilinde HENÜZ PARSE EDİLMİYOR (sadece `src/bolge.h` runtime API).
> Bu belge **varsayım**la yazılır: lang syntax ileride bu adlandırma ile
> eklenir. Plan Bölüm 7.D referansı.

---

## 2. Tip Ortamı, Bölge Bilgisi, Lineerlik Durumu

### 2.1 Tip Ortamı (Γ)

```
Γ ::= ∅ | Γ, x : τ
```

Tip kontrol fazı (`src/tip_kontrol.c`) `Γ ⊢ e : τ` türetmesini üretir.
Bu belge tip kontrol kurallarını **varsayım** alır (`belgeler/Tip_Sistemi.md`
özetinde — bu dosya henüz yok; kuralları Linear_Types_Spec_V1.md,
Capability_Spec_V1.md, Sabitsure_Spec_V1.md içinde dağılı).

### 2.2 Lineerlik Durumu (Λ)

Linear V1'in `Lineer Bağlama Kayıtları` sistematik versiyonu:

```
Λ : Var → {AKTIF, TUKETILDI}
```

Her `x : tekkez<τ>` (veya `x : yetki<R>`) için Λ takip eder. Tüketim
sonrası `Λ(x) = TUKETILDI`. `Λ \ {x}` → `x`'i çıkarır.

### 2.3 Bölge Ortamı (Ρ)

Mevcut bölge atama (`src/bolge_atama.c`) çıktısı:

```
Ρ : AST_düğüm → BolgeBilgisi
```

`Ρ(e) = ρ` → ifade `e`'nin tahsis edildiği bölge. Bölgeler `KEMGU_Bellek_Modeli.md`
Katman 1 + 2 kategorilerinden biri:

```
ρ ::= ρ_lit | ρ_yerel(f) | ρ_çağıran(f) | ρ_iterasyon(d) | ρ_global
    | ρ_sahip(t) | ρ_kanal(k) | ρ_donmuş   -- Katman 2
```

> **NOT — `ρ_donmuş`** kategorisi mevcut `BolgeKategorisi` enum'unda
> ayrı yok; Plan Bölüm 7.E "hibrit" kararına göre runtime flag (sembol
> tablosunda) olarak takip edilir. Bu belgede formel ayrı kategori kabul
> edilir (kavramsal netlik).

### 2.4 Sahiplik Haritası (Σ)

Katman 2 için:

```
Σ : Bölge × Zaman → Thread ∪ {⊥, DONMUŞ}
```

`Σ(ρ, z) = t` → `ρ` zamanında `t` thread'inin sahip olduğunu söyler.
`⊥` → henüz hiçbir thread sahibi yok. `DONMUŞ` → `dondur` çağrıldı,
çoklu okuyucu izinli, yazma yok.

**Aksiyom (S1 — Tekil Sahiplik):**
```
∀ ρ ∉ ρ_donmuş, ∀ z : Σ(ρ, z) ∈ Thread ⊎ {⊥}    (singleton veya yok)
```

---

## 3. Store ve Bellek Modeli

### 3.1 Store (σ)

```
σ : Konum → Değer
Konum ::= ρ × ofset      -- bölge ve içindeki ofset (bayt-bazlı)
```

Bellek erişimleri her zaman bir `(ρ, ofs)` çiftine yapılır. Bu sayede
**bölge** bilgisi runtime'da da takip edilir (KEMGU'da gerçek implementasyon
arena offset'i + region pointer).

### 3.2 Değerler (v)

```
v ::= n              -- skaler (tam/dtam/kesirli/mantıksal/karakter)
    | (n, ρ_str)     -- metin (ptr, bölge)
    | (ρ_yapı, l⃗)   -- yapı (bölge, alan haritası)
    | (ρ_dizi, n_uz) -- dizi (bölge, uzunluk)
    | closure(c, YD⃗) -- closure (kod + yakalama)
    | yetki_token(id, R, izin, iptal)             -- yetki<R>
    | (linear veya sabitsüre wrapper'lar runtime'da T ile aynı)
```

### 3.3 Memory Model (V1: Sequential Consistency)

Plan Bölüm 7.F kararı: **V1 = SC varsayımı**. Tüm bellek operasyonları
global bir sıralı izde gerçekleşir; weak memory (ARM64 relaxed) için
runtime fence emit (LLVM `atomic acq_rel`) — Plan Karar F.

V2'de **C++11 MM** entegrasyonu (acquire/release fence'leri görev/kanal/
dondur boundary'lerinde).

---

## 4. Küçük-Adım Reduksiyon — Tek Thread

### 4.1 Konfigürasyon (Tek Thread)

```
K ::= ⟨e, σ, Λ, Ρ⟩
```

Reduksiyon: `⟨e, σ, Λ, Ρ⟩ ⟶ ⟨e', σ', Λ', Ρ'⟩`.

### 4.2 Reduksiyon Kuralları (Temsili Subset)

> Tam kural listesi `belgeler/KEMGU_Operasyonel_Semantik_Tam.md`'de
> olacak (V1.1). Burada DRF için kritik olanlar.

**S-VAR:**
```
σ(Ρ(x), 0) = v
─────────────────────────────────
⟨x, σ, Λ, Ρ⟩ ⟶ ⟨v, σ, Λ, Ρ⟩
```

**S-ATAMA:**
```
⟨e, σ, Λ, Ρ⟩ ⟶* ⟨v, σ', Λ', Ρ'⟩
σ'' = σ'[Ρ'(x), 0 ↦ v]
──────────────────────────────────────────────────
⟨x = e, σ, Λ, Ρ⟩ ⟶ ⟨(), σ'', Λ', Ρ'⟩
```

**S-LIN-KULLAN (Linear consume):**
```
Λ(x) = AKTIF      σ(Ρ(x), 0) = v
─────────────────────────────────────────────
⟨kullan(x), σ, Λ, Ρ⟩ ⟶ ⟨v, σ, Λ[x ↦ TUKETILDI], Ρ⟩
```

**S-LIN-İMHA:**
```
Λ(x) = AKTIF
─────────────────────────────────────────────
⟨imha(x), σ, Λ, Ρ⟩ ⟶ ⟨(), σ, Λ[x ↦ TUKETILDI], Ρ⟩
```

**S-LIN-DOUBLE-USE (Hata — bu reduksiyon GERÇEKLEŞMEZ):**
```
Λ(x) = TUKETILDI
─────────────────────────────────────────────
⟨kullan(x), σ, Λ, Ρ⟩ ⟶ HATA L002         (runtime panic değil — derleme zaten reddetti)
```

**Yorum:** Tip kontrol fazı bu durumun derlemesini reddeder
(L002 LINEAR_DOUBLE_USE). Runtime'da bu reduksiyon **gerçekleşmez** çünkü
program ana derleme aşamasını bile geçemez. Operasyonel semantik için
"hata reduksiyonu" sadece **olasılığın olmadığını** göstermek için yazılır.

**S-SABİTSÜRE-WRAP, S-SABİTSÜRE-IFSA, S-DELEGE, ...** benzer şekilde
(detaylar `Sabitsure_Spec_V1.md` ve `Capability_Spec_V1.md` ile uyumlu).

---

## 5. Çoklu-Thread Konfigürasyonu

### 5.1 Thread Bağlamı

```
T_id ∈ Thread = {t₀, t₁, ...}    (sayılabilir küme)

T ::= ⟨t_id, e, Λ_t, Ρ_t⟩        -- bir thread bağlamı
```

`Λ_t` ve `Ρ_t` o thread'in **yerel** linear durumu ve bölge ortamı.
Paylaşılan: σ (store), Σ (sahiplik haritası).

### 5.2 Tüm Sistem Konfigürasyonu

```
S ::= ⟨T⃗, σ, Σ, K⃗⟩
```

- `T⃗` aktif thread'lerin listesi.
- `σ` global store.
- `Σ` sahiplik haritası (zamana göre değişken; bu konfigürasyonda **şimdi**ki sahiplik).
- `K⃗` kanal durumları haritası (her kanal için bekleyen mesaj sıraları).

### 5.3 Sistem Reduksiyonu

```
⟨T⃗, σ, Σ, K⃗⟩ ⟹ ⟨T⃗', σ', Σ', K⃗'⟩
```

Tek bir adım = bir thread'in tek bir küçük-adım reduksiyonu **veya** thread
spawn / kanal işlemi / dondurma.

### 5.4 Concurrency Kuralları

**C-GÖREV-BAŞLAT (R-GÖREV uygulaması):**
```
T = ⟨t₁, görev_başlat(c) ∷ E[•], Λ₁, Ρ₁⟩ ∈ T⃗
c = closure(kod, YD⃗)
t_yeni = fresh(Thread)
ρ_yeni = ρ_sahip(t_yeni)
∀ v_i ∈ YD⃗ : Σ' = Σ[bölge(v_i) ↦ t_yeni, z]   -- sahiplik transferi (R-YAKALAMA-THREAD)
Λ₁' = Λ₁ \ {YD⃗ ∩ Lineer}                    -- linear yakalananlar t₁'den silinir
T_yeni = ⟨t_yeni, kod, ∅, ρ_yeni⟩
T⃗' = (T⃗ \ T) ∪ {⟨t₁, E[görev<T>(t_yeni)], Λ₁', Ρ₁⟩, T_yeni}
──────────────────────────────────────────────────────────────────────────
⟨T⃗, σ, Σ, K⃗⟩ ⟹ ⟨T⃗', σ, Σ', K⃗⟩
```

Çağıran thread `t₁` `görev<T>` handle alır; yeni thread `t_yeni` kendi
ρ_sahip bölgesiyle başlar.

**C-KANAL-GÖNDER:**
```
T = ⟨t, gönder(g_id, v) ∷ E[•], Λ, Ρ⟩
K⃗(g_id).gönder_kuyruğu += v
Σ' = Σ[bölge(v) ↦ ρ_kanal(g_id)]        -- S3 atomik transfer
Λ' = Λ \ {v eğer linear}
──────────────────────────────────────────────────────────────────────────
⟨T⃗, σ, Σ, K⃗⟩ ⟹ ⟨T⃗[t ↦ ⟨t, E[()], Λ', Ρ⟩], σ, Σ', K⃗.gönderildi(g_id, v)⟩
```

**C-KANAL-AL:**
```
T = ⟨t, al(a_id) ∷ E[•], Λ, Ρ⟩
K⃗(a_id).gönder_kuyruğu = v ∷ rest   (boş değil)
Σ' = Σ[bölge(v) ↦ ρ_sahip(t)]       -- t'ye geri transfer
──────────────────────────────────────────────────────────────────────────
⟨T⃗, σ, Σ, K⃗⟩ ⟹ ⟨T⃗[t ↦ ⟨t, E[v], Λ ∪ {v}, Ρ⟩], σ, Σ', K⃗.alındı(a_id)⟩
```

**C-DONDUR:**
```
T = ⟨t, dondur(v) ∷ E[•], Λ, Ρ⟩
Σ' = Σ[bölge(v) ↦ DONMUŞ]            -- R-PAYLAŞ: bölge donmuş
──────────────────────────────────────────────────────────────────────────
⟨T⃗, σ, Σ, K⃗⟩ ⟹ ⟨T⃗[t ↦ ⟨t, E[v_donmuş], Λ, Ρ⟩], σ, Σ', K⃗⟩
```

(Plan Karar E "hibrit": `dondur` builtin call + sembol flag. Operasyonel
açıdan `v_donmuş` tipinin yazma izni runtime'da yok.)

**C-BİRLEŞTİR (R-BİRLEŞTİR uygulaması):**
```
T₁ = ⟨t₁, birleştir(g) ∷ E[•], ...⟩
T_hedef ∈ T⃗ ile T_hedef.id = g.thread, T_hedef bitti, dönüş = r
Σ' = Σ[bölge(r) ↦ ρ_çağıran(t₁)]   -- terfi
T⃗' = (T⃗ \ T_hedef) ile t_hedef'in ρ_sahip bölgeleri (r dışında) serbest
──────────────────────────────────────────────────────────────────────────
⟨T⃗, σ, Σ, K⃗⟩ ⟹ ⟨T⃗'[t₁ ↦ ⟨t₁, E[r], ...⟩], σ', Σ', K⃗⟩
```

---

## 6. İzler ve Happens-Before

### 6.1 İz (Trace) Tanımı

```
τ : Trace = (Konfigürasyon × Olay) sonlu liste
```

Bir iz, sistem konfigürasyonunun arka arkaya gelişmesinin **gözlenebilir
olaylar dizisi**.

```
Olay ::= mem_oku(t, ρ, ofs, v)
       | mem_yaz(t, ρ, ofs, v)
       | thread_başlat(t)
       | thread_bitir(t)
       | kanal_gönder(t, k, v)
       | kanal_al(t, k, v)
       | dondur(t, ρ)
```

### 6.2 Sıralı Sıra (Sequenced-Before)

Aynı thread içindeki iki olay arasında program-sırası gelir:

```
e₁ ≺_pl e₂   ⟺   e₁ ve e₂ aynı thread'de ve e₁ τ'da daha önce
```

### 6.3 Senkronize-Eder (Synchronizes-With)

```
e₁ ≺_sw e₂  ⟺  (e₁ = kanal_gönder(t_a, k, v) ∧ e₂ = kanal_al(t_b, k, v))
            ∨  (e₁ = thread_başlat(t_b) ∧ e₂ ∈ t_b'nin ilk olayı)
            ∨  (e₁ ∈ t_b'nin son olayı ∧ e₂ = birleştir(t_a, t_b))
            ∨  (e₁ = dondur(t_a, ρ) ∧ e₂ = mem_oku(t_b ≠ t_a, ρ, _, _))
```

(C++11 MM'ın `synchronizes-with` ilişkisinin KEMGU karşılığı.)

### 6.4 Happens-Before (Kapanış)

```
≺_hb := transitive_closure(≺_pl ∪ ≺_sw)
```

### 6.5 Data Race

```
data_race(τ) ⟺
  ∃ e₁ = mem_op(t₁, ρ, ofs, _),
  ∃ e₂ = mem_op(t₂, ρ, ofs, _) ∈ τ
  ∧ t₁ ≠ t₂
  ∧ (e₁ veya e₂ bir mem_yaz)
  ∧ ¬(e₁ ≺_hb e₂)
  ∧ ¬(e₂ ≺_hb e₁)
```

(C++11 MM tarzı: yarış = bir çift bellek operasyonu, en az biri yazma,
happens-before ile sıralanmamış.)

---

## 7. İyi-Tipli Program (Well-Typed)

```
İyiTipli(Π) ⟺
  1. tip_kontrol_program(Π) = OK                          (T001..T031 yok)
  2. lineer_kontrol_program(Π) = OK                       (L001..LR002 yok)
  3. capability_kontrol_program(Π) = OK                   (CP001..CP005 yok — Capability V1)
  4. sabitsüre_kontrol_program(Π) = OK                    (CT001..CT008 yok)
  5. bölge_atama_program(Π) = OK                          (escape + R-* aksiyom geçti)
  6. realtime_kontrol_program(Π) = OK (eğer var)          (RT001..RT007 yok)
  7. Π hiçbir `güvensiz` blok içermez                     (Plan Karar H, izolasyon modu)
```

V1'de yedi koşul birlikte sağlanmalı. (7) `güvensiz` ile ilgili gevşetme
Plan Karar H'da "güvensiz izolasyon" — güvensiz blok dışı koruma.

### 7.1 Tip Kontrol Türetilebilirliği

DRF teoremi için **tek koşul**: `İyiTipli(Π) = doğru`. Bu durumda
**aşağıdaki üç önerme** garanti edilir:

- **Tip Güvenliği (Type Soundness):** Reduksiyon "stuck" olmaz veya bilinen
  tip hatası vermez (Wright–Felleisen 1994).
- **Bölge Bütünlüğü (Region Soundness):** Her bellek operasyonu canlı
  bölgeye yapılır (Tofte–Talpin 1997 tarzı).
- **DRF (Bu belgenin amacı):** Hiçbir iz data race içermez.

---

## 8. Anahtar Aksiyomlar (Bu Belgenin Sonuçları)

Aşağıdakiler **bu belgede aksiyomdur**; Bölüm 1-7'nin sonucudur. DRF
lemmaları (`KEMGU_DRF_Lemmalar.md`) bunlardan türetilir.

### A1 — Tip Korunumu

```
İyiTipli(Π) ∧ Π ⟹* S' ∧ S' = ⟨T⃗, σ, Σ, K⃗⟩
  ⟹ ∀ T = ⟨t, e, Λ, Ρ⟩ ∈ T⃗ : Γ' ⊢ e : τ'        (uygun Γ', τ')
```

Reduksiyon tip değişikliğine sebep olmaz.

### A2 — Bölge Korunumu

```
İyiTipli(Π) ∧ Π ⟹* S' = ⟨T⃗, σ, Σ, K⃗⟩
  ⟹ Σ S1'i karşılar     (her ρ ∉ ρ_donmuş için |sahip| ≤ 1)
```

Reduksiyon sahiplik singleton'unu kırmaz.

### A3 — Lineer Korunumu

```
İyiTipli(Π) ∧ Π ⟹* S' = ⟨T⃗, σ, Σ, K⃗⟩
  ⟹ ∀ T = ⟨t, e, Λ, Ρ⟩ : Λ tutarlı       (her tüketim ≤ 1)
```

Reduksiyon linear tüketim sayısını >1 yapmaz.

### A4 — Bellek Bütünlüğü

```
İyiTipli(Π) ∧ Π ⟹* S' ∧ mem_op(t, ρ, ofs, _) ∈ S'.kullanım
  ⟹ Σ(ρ, z) ∈ {t, DONMUŞ}   (t bu ρ'nun sahibi ya da ρ donmuş)
```

Bir thread ancak kendi sahip olduğu veya donmuş bölgeye erişir.

---

## 9. V1 Sınırları (V2'ye Bırakılanlar)

- **Weak memory model** (ARM64 relaxed) — V1 SC; V2 C++11 MM.
- **Inter-procedural escape soundness** — V1 yerel; V2 callee summary.
- **`güvensiz` blok soundness** — V1 dışlama; V2 izolasyon proof.
- **Interrupt / signal handler** — V1 thread modeli synchronous; V2 async.
- **Mekanize ispat** (Coq/Isabelle/Lean) — V1 kâğıt; V2 saklı.
- **Tip-driven optimizasyonun semantik korunumu** — V1 kapsamı dışı.

---

## 10. Referanslar

- **Plotkin 1981** "A Structural Approach to Operational Semantics"
- **Wright & Felleisen 1994** "A Syntactic Approach to Type Soundness"
- **Tofte & Talpin 1997** "Region-Based Memory Management"
- **Boehm & Adve 2008** "Foundations of the C++ Concurrency Memory Model"
- **Sevcik & Aspinall 2008** "On Validity of Program Transformations in
  the Java Memory Model"
- **O'Hearn 2007** "Resources, Concurrency and Local Reasoning" (CSL)
- **Klein et al. 2009** "seL4: Formal Verification of an OS Kernel"
- KEMGU Bellek Modeli — `belgeler/KEMGU_Bellek_Modeli.md` (Katman 1+2 aksiyomları)
- KEMGU Linear Types V1 — `belgeler/KEMGU_Linear_Types_Spec_V1.md`
- KEMGU DRF Plan — `belgeler/KEMGU_DRF_Genisletme_Plan.md`

---

**END Operasyonel Semantik V1**
