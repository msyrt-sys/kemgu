# KEMGU Mekanizasyon Çekirdek Onarım Planı

**Tarih:** 2026-05-18
**Durum:** TASLAK PLAN — sadece doküman, hiçbir Lean kodu değişikliği YOK
**Branch hedefi:** `feature/drf-mekanize-ve-v3-metateorem` (mevcut)
**Tetikleyici:** İki bağımsız değerlendirme (Gemini + ChatGPT) Lean mekanizasyondaki yapısal sorunları aynı yerlerden işaret etti.

**Kapsam:**
Bu doküman mevcut Lean mekanizasyonunda (commit'ler `253717c..08ee17e`, ~2257 satır Lean) tespit edilen üç yapısal sorunu analiz eder ve Wright-Felleisen Type Soundness yaklaşımıyla **çekirdek onarımın** planını verir.

**Karar noktası:** Mehmet bu planı onaylarsa onarım başlar. Reddederse V2 turlarına devam edilir (modellerin uyarıları kabul edilmez). Modifiye ederse plan revize edilir.

---

## 1. Yapısal Sorun Analizi

### 1.1 Sorun: Vacuous Predicates (Placeholder True)

**Nerede:** [`proofs/drf-v2-lean/Kemgu/Sem/Core.lean`](../proofs/drf-v2-lean/Kemgu/Sem/Core.lean) §11

```lean
def TipKontrolOk     (_Pi : Program) : Prop := True
def LineerKontrolOk  (_Pi : Program) : Prop := True
def CapabilityKontrolOk (_Pi : Program) : Prop := True
def SabitsureKontrolOk  (_Pi : Program) : Prop := True
def BolgeAtamaOk     (_Pi : Program) : Prop := True
def RealtimeKontrolOk   (_Pi : Program) : Prop := True

structure IyiTipli (Pi : Program) : Prop where
  tipOk           : TipKontrolOk Pi          -- = True
  lineerOk        : LineerKontrolOk Pi        -- = True
  capabilityOk    : CapabilityKontrolOk Pi    -- = True
  sabitsureOk     : SabitsureKontrolOk Pi     -- = True
  bolgeOk         : BolgeAtamaOk Pi           -- = True
  realtimeOk      : RealtimeKontrolOk Pi      -- = True
  noGuvensiz      : NoGuvensiz Pi             -- (etkin)
```

**Ne:** IyiTipli'nin 7 alt-koşulundan 6'sı `True` predikatı, sadece `NoGuvensiz` etkin yapısal kontrol. IyiTipli pratik olarak `NoGuvensiz`'e eşdeğer.

**Niye sorun:**
- DRF lemmaları (L0-L7) hipotez olarak `IyiTipli Pi` alır
- `IyiTipli Pi` ≈ `NoGuvensiz Pi` (diğer 6 trivial)
- Lemmalar IyiTipli'den **hiçbir yapısal kısıt çıkaramaz**
- Tüm DRF garantilerinin "tip sistemi sağlar" varsayımı havada
- Lemma'ların substantive içeriği SmallStep precondition'larından geliyor (bkz. §1.2), IyiTipli'den DEĞİL

**Akademik kategori:**
- **Vacuous quantification** (Pierce TAPL §8.3): premise "her zaman doğru", iddianın gerçek içeriği yok
- **Trivial soundness**: teorem `IyiTipli → P` formundadır ama IyiTipli her programa uyduğu için aslında `True → P` = `P` halini alır
- **Semantic self-validation**: tip sistem (paper'da inductive) **Lean'de hiç tanımlanmadı** ama "kabul ediyoruz tanımlanmış" varsayılıyor

**Etki:** V1 mekanize'nin "tip-sound" iddiası DEFENDABLE değil; reviewer kolaylıkla "where's the type system?" diye sorar.

---

### 1.2 Sorun: Step Constructor Preconditions (Semantic Self-Validation)

**Nerede:** [`proofs/drf-v2-lean/Kemgu/Sem/SmallStep.lean`](../proofs/drf-v2-lean/Kemgu/Sem/SmallStep.lean)

```lean
inductive Step : Konfigurasyon → Konfigurasyon → Prop where
  | sAtama
      (S S' : Konfigurasyon)
      (ctx : ThreadCtx) (x : VarId) (v : Deger) (k : Konum)
      (h_in : ctx ∈ S.thread)
      (h_ifade : ctx.ifade = .atama x (.sabit v))
      (h_not_frozen : ¬ isFrozen S k.bolge)                                      -- ← A3.0''
      (h_owner : sahiplikGet S.sahiplik (k.bolge, S.zaman)
                  = some (Sahip.thread ctx.tid))                                  -- ← A3.0''''
      (h_store : ...)
      ...
      : Step S S'
  | cGorevBaslat
      ...
      (h_lineer_caller : ∃ ctx' ∈ S'.thread, ctx'.tid = ctx.tid ∧
                          ∀ v ∈ linearYakalananlar,
                            (v, Lineerlik.tuketildi) ∈ ctx'.lineer)              -- ← A3.0'''
      ...
      : Step S S'
```

**Ne:** SmallStep'in 8 constructor'ından bazılarına (sAtama, cGorevBaslat) tip-sistem-benzeri precondition'lar eklenmiş (`h_owner`, `h_not_frozen`, `h_lineer_caller`).

Bu precondition'lar **A3.0''/A3.0'''/A3.0'''' refactor turlarıyla** eklendi — her birinde DRF lemmaları (L4, L2, Teorem 4', T1) "provable" hale getirmek için.

**Niye sorun:**
- **Step relation'ı kısıtlanmış**: yalnız "iyi davranan" adımları içeriyor. Çağrıda `Step S S'` derecek olan kişi h_owner gibi koşulları sağlamak zorunda.
- **Operasyonel semantiğin görevi değildir**: tip sistem hangi adımların alınabileceğini kısıtlar; operasyonel semantik **tüm olası adımları** açıklamalı (iyi/kötü ayırt etmeden).
- **Sonuç circular**: "iyi-tipli program iyi adımlar atar" iddiası SmallStep'in "yalnız iyi adımlar tanımlı" yapısıyla tautolojik hale gelir.
- **Standart yaklaşımdan sapma**: TAPL (Pierce 2002), Wright-Felleisen 1994, CompCert, RustBelt hepsinde Step kısıtsız, tip system aktiviteyi yargılar.

**Akademik kategori:**
- **Semantic self-validation**: operasyonel semantik tip-sistem koşullarını **kendi içine gömüyor**
- **Circular reasoning**: "Step is well-typed by construction" → her Step tip-sound
- **Type system smuggling** (Harper "Practical Foundations" §4): tip kontrol Step'in içine sızdırılmış
- **Loss of generality**: gerçek runtime hatalı adım denemeleri (frozen yazma, un-owned modify) Step'te ifade EDİLEMİYOR; semantik eksik

**Etki:** Lemmaların ispatları kâğıt üstünde "tip sistem garantileri" diyor ama Lean'de "Step zaten bu garantileri içeriyor" diyor — iki söylem çelişkili.

---

### 1.3 Sorun: V3 Overclaiming (Placeholder Conjunct)

**Nerede:** [`proofs/drf-v2-lean/Kemgu/Soundness/Main.lean`](../proofs/drf-v2-lean/Kemgu/Soundness/Main.lean)

```lean
def SideChannelResistant_v2_placeholder (_Pi : Program) : Prop := True
def BET_v2_placeholder (_Pi : Program) : Prop := True

theorem kemgu_soundness_v3 ... :
    DrfHolds S
    ∧ MemSafe_perStep S
    ∧ SideChannelResistant_v2_placeholder Pi      -- = True
    ∧ BET_v2_placeholder Pi                        -- = True
  := by ...
```

**Ne:** V3 "bütünleşik metateorem" 4 conjunct'tan oluşuyor; ama Side-Channel ve BET conjunct'ları `True` predikatı.

**Niye sorun:**
- Teorem ifadesi okuyucuya "V3 KEMGU Soundness" gibi sunuluyor
- Aslında ispatlanan: DRF + MemSafe + True + True ≡ DRF + MemSafe
- Side-Channel ve BET hakkında **HİÇBİR ŞEY** ispatlanmıyor
- "Bundled form" demek bunu meşrulaştırmaz; placeholder True teknik olarak doğru ama anlamsızca doğru

**Akademik kategori:**
- **Overclaiming**: teorem ismi/ifadesi gerçekte ispatlanandan fazlasını söylüyor
- **False generalization**: 4-conjunct claim ile 2-conjunct conclusion
- **Scientific dishonesty** (zayıf form): reviewer "True conjunct nedir?" diye sorduğunda cevap zayıf

**Etki:** TOPLAS reviewer'ı "your 'V3 metatheorem' is just DRF + MemSafe with two True clauses — why call it V3?" der.

---

### 1.4 Üç Sorunun Birleşik Sonucu

Üç sorun birlikte düşünüldüğünde mevcut V1 bundled mekanize **akademik olarak savunulamaz**:

1. IyiTipli vacuous → tip-sound iddia havada
2. Step preconditions tip-system'i smuggling → operasyonel semantik eksik
3. V3 overclaiming → metateorem aslında 2-conjunct

**Pozitif yan:** Mevcut commit'ler **iyi yapısal iskelet** sağlıyor (Op.Sem altyapı, HB ordering, lemma şablonları). Onarım sıfırdan yazma değil, **temel restoration**.

**Negatif yan:** Onarım kapsamlı — gerçek tip sistem tanımı + Progress + Preservation + L0-L7 adaptasyonu gerek. Tahmini ~1,400-1,950 satır, 6-10 hafta.

---

## 2. Wright-Felleisen Yaklaşımı

### 2.1 Klasik Type Soundness (Wright & Felleisen 1994)

Wright & Felleisen "A Syntactic Approach to Type Soundness" (Information & Computation 115(1), 1994) **Type Soundness = Progress + Preservation**:

```
Progress:
  ∀ e τ, (∅ ⊢ e : τ) → (e is value) ∨ (∃ e', e ⟶ e')

Preservation (Subject Reduction):
  ∀ e e' τ, (∅ ⊢ e : τ) ∧ (e ⟶ e') → (∅ ⊢ e' : τ)

Soundness Corollary:
  Well-typed programs never get stuck (don't reach an irreducible non-value).
```

Bu yaklaşımın avantajı:
- Step relation **tip-sistem hakkında hiçbir şey bilmez**
- Tip sistem **ayrı bir judgment** olarak tanımlanır
- İki teorem ile "tip-sound" gösterilir
- Stuck state ≠ Fault state; tipli programlar stuck'a gitmez

**Referanslar:**
- Pierce **TAPL** §8.3 (Type Soundness for STLC) — minimal STLC örneği
- Pierce **TAPL** §15 (Subtyping & Type Soundness)
- Harper **"Practical Foundations for Programming Languages"** §4-7
- Wright & Felleisen 1994 (Information & Computation 115(1))
- Felleisen & Hieb 1992 (TCS 103(2)) — reduction contexts

### 2.2 KEMGU'ya Uygulama

**Adım 1**: Tip sistemini explicit judgment olarak tanımla:
```
Γ; Λ; Ρ ⊢ e : τ ⊣ Λ'; Ρ'
```

Burada:
- `Γ` : VarId → Tip (geleneksel tip ortamı)
- `Λ` : Lineerlik durumu (linear var'ların aktif/tüketildi durumu, Linear Types Spec V1)
- `Ρ` : Bölge ortamı (var → bölge, Region-based memory)
- `Λ'` : `e`'nin değerlendirilmesinden sonra Λ
- `Ρ'` : `e`'nin değerlendirilmesinden sonra Ρ

**Adım 2**: SmallStep'i precondition'lardan ARINDIRR:
- sAtama her zaman alınabilir
- Hatalı durum (frozen target, un-owned, vs.) için `Step.sAtamaFault` constructor ekle
- "Stuck" yerine "Fault" state'i kullan

**Adım 3**: Progress + Preservation ispatla:

```
Progress (KEMGU):
  Γ; Λ; Ρ ⊢ e : τ ⊣ Λ'; Ρ'  ∧  S well-formed
  → ∃ S', Step S S' ∧ S' ≠ Fault

Preservation (KEMGU):
  Γ; Λ; Ρ ⊢ e : τ ⊣ Λ'; Ρ'  ∧  Step S S'  ∧  S well-formed
  → ∃ Γ' Λ'' Ρ'', Γ'; Λ''; Ρ'' ⊢ e' : τ
```

**Adım 4**: DRF lemmaları (L0-L7) typing judgment'ı hipotez olarak alır:
```lean
theorem drf_l4_a_step
    (Γ Λ Ρ) (Pi) (h_typed : Γ; Λ; Ρ ⊢ Pi.body : τ ⊣ Λ'; Ρ')
    (S S' : Konfigurasyon) (h_step : Step S S')
    ...
```

Yani DRF garantileri **gerçek typing judgment'tan** çıkarsanır, vacuous predicate'ten DEĞİL.

**Sonuç:** Step relation kısıtsız + ayrı tip sistem + Progress/Preservation ile "tip-sound" iddiası **akademik olarak savunulabilir** form'a girer.

---

## 3. Minimal Typing Judgment Tasarımı

### 3.1 Judgment Formu

```
Γ; Λ; Ρ ⊢ e : τ ⊣ Λ'; Ρ'
```

**Bileşenler:**
- `Γ : VarId → Tip` — geleneksel tip ortamı, immutable
- `Λ : VarId → Lineerlik` — linear durumu (sadece linear-tracked var'lar için)
- `Ρ : VarId → Bolge` — bölge atama (mevcut Core.lean'in implicit Ρ_t'sinin ilk hali)
- `e : Ifade`, `τ : Tip`
- `Λ'` : `e` değerlendirildikten sonra Λ (linear consumption izlenir)
- `Ρ'` : `e` değerlendirildikten sonra Ρ (region promotion / new allocation)

**Lean 4 sözdizimi:**
```lean
inductive Typed : TipOrtam → LineerOrtam → BolgeOrtam → 
                  Ifade → Tip → LineerOrtam → BolgeOrtam → Prop where
  -- (kurallar aşağıda)
```

### 3.2 Tip Sistem Kuralları (Minimal Subset — V1 Onarım)

**T-VAR (Değişken referansı):**
```
Γ(x) = τ    Λ(x) = aktif (eğer linear ise)
─────────────────────────────────────────
Γ; Λ; Ρ ⊢ x : τ ⊣ Λ[x ↦ tüketildi (eğer linear)]; Ρ
```

**T-LIT (Literal):**
```
─────────────────────────────────
Γ; Λ; Ρ ⊢ sabit n : skaler ⊣ Λ; Ρ
```

**T-ATAMA (Atama):**
```
Γ; Λ; Ρ ⊢ e : τ ⊣ Λ'; Ρ'
Γ(x) = τ          (tip uyumu)
Ρ'(x) = b         (x'in bölgesi)
b ∉ ρ_donmus      (frozen değil — typing yargısı)
b ∈ ρ_sahip(t) ∨ b ∈ ρ_yerel(f) ∨ b ∈ ρ_global   (ctx'in erişebileceği)
─────────────────────────────────────────────────
Γ; Λ; Ρ ⊢ atama x e : boş ⊣ Λ'; Ρ'
```

**T-CGOREV-BASLAT (Görev başlat):**
```
yd ⊆ dom(Γ)                                       (yakalama var'lar tanımlı)
∀ v ∈ yd, Γ(v) = τ_v
∀ v ∈ yd ∩ Linear, Λ(v) = aktif                   (linear yakalananlar aktif)
Γ; Λ_inner; Ρ_inner ⊢ kod : τ_dönüş ⊣ ...        (closure body tipli)
Λ' = Λ \ (yd ∩ Linear)                            (caller'da tüketildi)
Ρ' = Ρ ∪ {bolge(v_i) ↦ ρ_sahip(t_yeni) : v_i ∈ yd}   (sahiplik transferi)
─────────────────────────────────────────────────────────
Γ; Λ; Ρ ⊢ gorevBaslat yd kod : gorev<τ_dönüş> ⊣ Λ'; Ρ'
```

**T-KANAL-GONDER:**
```
Γ(k) = kanal<τ>
Γ(v) = τ
Λ(v) = aktif (eğer linear)
Ρ(v) = b
─────────────────────────────────────────────────────
Γ; Λ; Ρ ⊢ kanalGonderIf k v : boş ⊣ Λ \ {v}; Ρ[b ↦ ρ_kanal(k)]
```

**T-DONDUR:**
```
Γ(b) = bolge
─────────────────────────────────────────────────
Γ; Λ; Ρ ⊢ dondurIf b : boş ⊣ Λ; Ρ[b ↦ ρ_donmus]
```

**T-KULLAN (Linear consume):**
```
Γ(x) = tekkez<τ>
Λ(x) = aktif
─────────────────────────────────
Γ; Λ; Ρ ⊢ kullanIf x : τ ⊣ Λ[x ↦ tüketildi]; Ρ
```

**T-IMHA:** Benzer.

**T-SEQ (Sıralı):**
```
Γ; Λ; Ρ ⊢ a : τ_a ⊣ Λ_1; Ρ_1
Γ; Λ_1; Ρ_1 ⊢ b : τ_b ⊣ Λ'; Ρ'
─────────────────────────────────
Γ; Λ; Ρ ⊢ seq a b : τ_b ⊣ Λ'; Ρ'
```

### 3.3 V1 Onarım Kapsamı

Bu typing judgment minimal subset kapsar:
- ✓ Değişken, literal, atama, sıralı (klasik)
- ✓ Görev başlat (Linear capture + region transfer)
- ✓ Kanal gönder/al (channel transfer)
- ✓ Dondur (frozen region)
- ✓ Linear kullan/imha
- ✗ Birleştir (görev_birlestir) — V1 onarım sonrası
- ✗ Capability tip kontrolü — Sabitsure ve Capability ayrı spec
- ✗ Realtime annotation — V2.3 BET ile
- ✗ Sabitsure tag — V2.4 NI ile

Bu kapsam DRF L0-L7 + T1 için yeterli; V2 turlarının gerek olduğu özellikler typing genişletilerek eklenir.

### 3.4 Decidability

Typing judgment **decidable** olabilmeli ki gerçek programlar mekanize kontrol edilebilsin. Inductive form decidable değil; ek olarak **type checking function** (sözdiziminden tipi türeten) gerek:

```lean
def tipCheck : TipOrtam → LineerOrtam → BolgeOrtam → Ifade → 
               Option (Tip × LineerOrtam × BolgeOrtam)
```

`tipCheck` ↔ `Typed` arasında **soundness + completeness** lemmaları:
- `tipCheck Γ Λ Ρ e = some (τ, Λ', Ρ') → Typed Γ Λ Ρ e τ Λ' Ρ'`
- `Typed Γ Λ Ρ e τ Λ' Ρ' → tipCheck Γ Λ Ρ e = some (τ, Λ', Ρ')`

Bu ek mekanizasyon olmadan **typed predicate sadece soyut** kalır — yine vacuous benzeri risk.

---

## 4. SmallStep Refactor Planı

### 4.1 Mevcut sAtama (Onarım Öncesi)

```lean
| sAtama
    (S S' : Konfigurasyon)
    (ctx : ThreadCtx) (x : VarId) (v : Deger) (k : Konum)
    (h_in : ctx ∈ S.thread)
    (h_ifade : ctx.ifade = .atama x (.sabit v))
    (h_not_frozen : ¬ isFrozen S k.bolge)                  -- KALDIRILACAK
    (h_owner : sahiplikGet S.sahiplik (k.bolge, S.zaman)
                = some (Sahip.thread ctx.tid))              -- KALDIRILACAK
    (h_store : ...)
    (h_iz : ...)
    (h_zaman : ...)
    (h_sahip : ...)
    (h_kanal : ...)
    : Step S S'
```

### 4.2 Onarılmış sAtama + sAtamaFault

```lean
inductive StepResult : Type where
  | normal (S' : Konfigurasyon)
  | fault  (sebep : FaultSebep) (S' : Konfigurasyon)  -- son state, faulted

inductive FaultSebep : Type where
  | frozenWrite (b : Bolge)
  | notOwner (b : Bolge) (writer : ThreadId) (actualOwner : Option ThreadId)
  | linearDoubleConsume (x : VarId)
  | linearMissedConsume (x : VarId)
  -- ...

inductive Step : Konfigurasyon → StepResult → Prop where
  | sAtamaOk
      (S S' : Konfigurasyon)
      (ctx : ThreadCtx) (x : VarId) (v : Deger) (k : Konum)
      (h_in : ctx ∈ S.thread)
      (h_ifade : ctx.ifade = .atama x (.sabit v))
      -- h_not_frozen, h_owner KALDIRILDI
      (h_store : S'.store = (k, v) :: S.store)
      (h_iz : S'.iz = .memYaz ctx.tid k v :: S.iz)
      (h_zaman : S'.zaman = S.zaman + 1)
      (h_sahip : S'.sahiplik = S.sahiplik)
      (h_kanal : S'.kanal = S.kanal)
      : Step S (StepResult.normal S')

  | sAtamaFaultFrozen
      (S S' : Konfigurasyon)
      (ctx : ThreadCtx) (x : VarId) (v : Deger) (k : Konum)
      (h_in : ctx ∈ S.thread)
      (h_ifade : ctx.ifade = .atama x (.sabit v))
      (h_frozen : isFrozen S k.bolge)        -- frozen yazma denendi
      : Step S (StepResult.fault (FaultSebep.frozenWrite k.bolge) S')

  | sAtamaFaultUnowned
      (S S' : Konfigurasyon)
      (ctx : ThreadCtx) (x : VarId) (v : Deger) (k : Konum)
      (h_in : ctx ∈ S.thread)
      (h_ifade : ctx.ifade = .atama x (.sabit v))
      (h_unowned : sahiplikGet S.sahiplik (k.bolge, S.zaman)
                    ≠ some (Sahip.thread ctx.tid))
      : Step S (StepResult.fault (FaultSebep.notOwner k.bolge ctx.tid _) S')
```

### 4.3 Diğer Constructor'lar Benzer

- **cGorevBaslat**: `h_lineer_caller` kaldırılır; `cGorevBaslatFault` (linear capture violation) eklenir
- **cDondur**: Frozen geçişi ok, fault yok
- **cKanalGonder/Al**: Linear consume durumları için Fault constructor'ları
- **sLinKullan/Imha**: Already aktif olmayan var consume için Fault

### 4.4 Mevcut L0-L7 İspatlarına Etki

Mevcut L4 ispatı:
```lean
| sAtama _ _ _ k_x _ _ h_not_frozen _ _ h_iz _ _ _ =>
  ...
  rw [h_eq] at h_not_frozen
  exact h_not_frozen h_frozen
```

Onarılmış L4 ispatı:
```lean
| sAtamaOk _ _ _ k_x _ _ _ h_iz _ _ _ _ =>
  -- h_not_frozen artık precondition değil; Progress/Preservation ile typing'tan elde edilir
  have h_typed : ... ⊢ atama x (sabit v) : boş ⊣ ... := h_typed_input
  have h_not_frozen : ¬ isFrozen S k_x.bolge :=
    typing_implies_not_frozen h_typed
  ...
| sAtamaFaultFrozen _ _ _ k_x _ _ _ h_frozen =>
  -- typed program bu fault'a gitmez; Progress ile fail
  exfalso
  exact typing_excludes_frozen_fault h_typed h_frozen
```

Yani L4 ispatı:
1. `sAtamaOk` case: typing'tan h_not_frozen türetilir (discharge lemma)
2. `sAtamaFaultFrozen` case: typing programın fault'a gitmemesini garantiler

Her case **typing hipotezini KULLANIR**, vacuous IyiTipli yerine.

### 4.5 Tahmini Değişim

- SmallStep.lean: ~250 satır (mevcut 227 → ~480, Fault constructor'lar + sebep enum)
- L0BolgeKorunumu: minimal değişim (~10 satır)
- L4FrozenRegionRead: orta (~60 satır — Fault case eklenir)
- L7BellekErisimTipSoundness: orta (~60 satır)
- L2/L3/L5/L6: küçük-orta (~40 satır × 4 = 160 satır)
- Drf.lean (Teorem 4'): büyük (~80 satır — typing hipotezi tüm yere eklenir)
- T1 (MemSafety): orta (~60 satır)

**SmallStep + L0-L7 + Drf + T1 toplam değişim: ~700 satır.**

---

## 5. Precondition Discharge Lemmaları

### 5.1 Tip Sisteminden Step Precondition'larını Türetme

Refactor sonrası, eski Step preconditionları typing judgment'tan **lemmas** olarak türetilir:

**Lemma: typing_implies_owner**
```lean
lemma typing_implies_owner
    (Γ : TipOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam)
    (x : VarId) (v : Deger)
    (h_typed : Typed Γ Λ Ρ (Ifade.atama x (Ifade.sabit v)) Tip.boş Λ' Ρ')
    (S : Konfigurasyon) (ctx : ThreadCtx) (k : Konum)
    (h_state : ...)  -- state-typing connection
    : sahiplikGet S.sahiplik (k.bolge, S.zaman) = some (Sahip.thread ctx.tid)
```

İspat: typing judgment'ın T-ATAMA kuralının (b ∈ ρ_sahip(t)) önkoşulundan çıkarsanır.

**Lemma: typing_implies_not_frozen**
```lean
lemma typing_implies_not_frozen
    ...
    (h_typed : Typed Γ Λ Ρ (Ifade.atama x (Ifade.sabit v)) ...)
    : ¬ isFrozen S k.bolge
```

T-ATAMA'nın `b ∉ ρ_donmus` koşulundan.

**Lemma: typing_implies_lineer_caller**
```lean
lemma typing_implies_lineer_caller
    ...
    (h_typed : Typed Γ Λ Ρ (Ifade.gorevBaslat yd kod) (Tip.gorev τ) Λ' Ρ')
    : ∀ v ∈ linearYakalananlar (Λ),
        (v, Lineerlik.tuketildi) ∈ Λ'
```

T-CGOREV-BASLAT'ın `Λ' = Λ \ (yd ∩ Linear)` çıkışından.

### 5.2 Kanalsız Precondition Lemmaları

Her Step constructor için ayrı discharge lemma. Toplam ~6-8 lemma, her biri ~30-50 satır = ~200-400 satır.

### 5.3 Discharge Lemmaları → DRF Lemmaları

DRF lemmaları artık şöyle yazılır:

```lean
theorem drf_l4_a_step
    (Γ Λ Ρ) (h_typed : Typed Γ Λ Ρ Pi.body ...)         -- typing hipotezi
    (S S' : Konfigurasyon) (h_step : Step S (StepResult.normal S'))
    ...
    : Olay.memYaz t k v ∈ S.iz ∨ k.bolge ≠ b := by
  cases h_step with
  | sAtamaOk _ _ _ _ _ _ _ _ h_iz _ _ _ _ =>
    -- typing'tan h_not_frozen türet
    have h_not_frozen := typing_implies_not_frozen h_typed ...
    -- mevcut ispat akışı
    ...
```

Yani DRF lemma'lar typing'i hipotez alır, discharge lemmalar ile Step koşullarını türetir, sonra mevcut akışı izler.

---

## 6. Onarım Sıralama (Bağımlılık Grafı)

```
┌──────────────────────────────────────┐
│  1. Minimal typing judgment kur      │  300-400 satır
│  (Inductive Typed Γ Λ Ρ e τ Λ' Ρ')   │  ~2 hafta
└──────────────┬───────────────────────┘
               ↓
┌──────────────────────────────────────┐
│  2. Progress lemma                   │  150-200 satır
│  (typing → step var veya value)      │  ~1 hafta
└──────────────┬───────────────────────┘
               ↓
┌──────────────────────────────────────┐
│  3. Preservation lemma               │  200-300 satır
│  (typing + step → typing korunur)    │  ~1.5 hafta
└──────────────┬───────────────────────┘
               ↓
┌──────────────────────────────────────┐
│  4. Discharge lemmaları              │  200-250 satır
│  (typing → Step preconditions)       │  ~1 hafta
└──────────────┬───────────────────────┘
               ↓
┌──────────────────────────────────────┐
│  5. SmallStep refactor               │  100-150 satır
│  (preconditions kaldır + Fault)      │  ~0.5 hafta
└──────────────┬───────────────────────┘
               ↓
┌──────────────────────────────────────┐
│  6. L0-L7 + T1 + Teorem 4' adapt     │  350-500 satır
│  (typing hipotez, discharge kullan)  │  ~2 hafta
└──────────────────────────────────────┘
```

**Toplam:** ~1,300-1,800 satır + IyiTipli güncellemesi + V3 metateorem revize ~100-150 satır.

**Grand total:** ~1,400-1,950 satır, 6-10 hafta (Lean uzmanlık + kâğıt cross-ref).

### 6.1 Paralel Çalışma İmkanı

Adım 1'den sonra Adım 4 (discharge) ve Adım 5 (SmallStep) PARALEL yapılabilir. Discharge lemmaları typed predicate üzerinden, SmallStep Fault constructor'ları typed-bağımsız.

Adım 6 (L0-L7 adapt) hem 4 hem 5'in çıktısını ister; en sona.

### 6.2 Checkpoint'ler

Her büyük adımdan sonra commit + lake build doğrulaması:
- C1: Typing judgment + tipCheck function — build OK
- C2: Progress + Preservation ispatlandı — build OK
- C3: Discharge lemmaları — build OK
- C4: SmallStep refactor + Fault — build OK
- C5: L0-L7 + T1 + Teorem 4' adaptasyon — build OK

5 ara checkpoint + final.

---

## 7. Tahmini Maliyet ve Zaman

| Adım | Lean Satır | Hafta Tahmini | Risk |
|------|-----------|---------------|------|
| 1. Minimal typing judgment | 300-400 | 2 | Orta — kapsam kararı (subset) |
| 2. Progress lemma | 150-200 | 1 | Düşük — klasik form |
| 3. Preservation lemma | 200-300 | 1.5 | Orta-Yüksek — case analysis büyük |
| 4. Discharge lemmaları | 200-250 | 1 | Düşük — Step'ten typing'e |
| 5. SmallStep refactor | 100-150 | 0.5 | Düşük — yapısal değişim |
| 6. L0-L7 + T1 + Teorem 4' | 350-500 | 2 | Orta — mevcut + ek typing |
| V3 metateorem revize | 100-150 | 0.5 | Düşük — SCR/BET True yerine açık ifade |
| **Toplam** | **1,400-1,950** | **6-10 hafta** | |

### 7.1 Hafta Maliyeti Notu

Tahminler **deneyimli Lean kullanıcısı** içindir. KEMGU bağlamı + kâğıt belge cross-ref + Mehmet review döngüleri ek zaman.

Auto mode + agent + insan review hibrit modelde, **paralelleştirme + iterasyonla 4-6 hafta** mümkün.

### 7.2 Tıkanma Riski

En yüksek tıkanma riski:
- Adım 1: Typing judgment kapsam kararı (V1 onarım için neresi yeterli?)
- Adım 3: Preservation ispatı (Pierce TAPL §8.3'te bile 50+ satır STLC için; KEMGU 5x karmaşıklık)
- Adım 6: L0-L7 adaptasyonu (8 lemma + Teorem 4' + T1, her birinde typing hipotez eklemek)

Her tıkanma noktasında **DUR + rapor + Mehmet kararı** politikası uygulanır (mevcut tıkanma politikası).

---

## 8. V2 Turları ile İlişki

Onarım bittikten sonra V2 hedefler **yeni temel üzerinde** çalışır:

### 8.1 V2.1 Cross-Step HB Ordering

**Etki:** Pozitif.
- Mevcut HappensBefore.lean (commit `08ee17e`) korunur
- DrfCrossStep.lean'in cross-Step ispatı typing'i hipotez aldığında daha düzgün argument
- Trajectory analysis Step.normal vs Step.fault ayırt eder (fault'a gitmeyen iz garantili tipli)
- Tahmini kazanç: cross-Step ispatın 300+ satırlık tıkanmasının ~200 satıra düşmesi

### 8.2 V2.2 T2/T3 Bölge Lifecycle

**Etki:** Çok pozitif.
- Typing judgment'a `T-BOLGE-YARAT` ve `T-BOLGE-SERBEST` kuralları eklenir
- Step constructor'ları lifecycle event'leri için yeni Step kuralları
- T2 counting argument typing seviyesinde direkt çıkar (her yarat 1 kez)
- T3 reachability typed scope semantics ile düzgün ifade edilir
- Tahmini ek maliyet: ~250 satır (typing genişletme + yeni Step kuralları)

### 8.3 V2.3 BET (Realtime + WCET)

**Etki:** Pozitif.
- Typing judgment'a `gerçekzamanlı` annotation eklenir
- WCET function typing'e bağlı (cycle counting per Step type)
- BET teoremi typed function'lar için, Progress ile birleşik
- Tahmini ek maliyet: ~350 satır

### 8.4 V2.4 NI (Sabitsure + Two-Execution)

**Etki:** Pozitif (en çok).
- Two-execution simulation typing judgment üzerinde tanımlanır
- sabitsure tag typing'e dahil, leak prevention typing'in görevi
- NI ispat typing-based information flow
- Tahmini ek maliyet: ~400 satır

### 8.5 V2.5 Cross-Step DRF Tam İspat

**Etki:** Pozitif.
- Onarım sonrası Step.normal vs Step.fault ayrımı sayesinde "well-typed program'lar fault'a gitmez" lemması cross-Step DRF ispatına temel sağlar
- Sahiplik transfer trace artık typing-based
- 300 satır tıkanmasından ~200 satıra düşer

### 8.6 Genel V2 Maliyet Revizyonu

| V2 Tour | Onarımsız Tahmin | Onarımlı Tahmin |
|---------|-----------------|-----------------|
| V2.1 Cross-Step HB | 300 satır (tıkanma) | 200 satır |
| V2.2 T2/T3 | 250 satır | 250 satır |
| V2.3 BET | 350 satır | 350 satır |
| V2.4 NI | 400 satır | 400 satır |
| V2.5 Cross-Step DRF tam | 300 satır (tıkanma) | 200 satır |
| **V2 toplam** | **1,600 satır** | **1,400 satır** |

V2'nin kendi maliyeti **biraz düşer** (~200 satır), ama daha önemli olan: V2 garantileri **savunulabilir** hale gelir.

---

## 9. Mevcut V2.1 Commit'in Kaderi

V2.1 commit `08ee17e` (HappensBefore.lean + DrfCrossStep.lean) **silinmez**. Onarım sonrası **yeni temele entegre edilir**:

- HappensBefore.lean: HB types değişmez, tip-system bağlanması ek lemma'larla
- DrfCrossStep.lean §1-2 (data_race_tam_implies_candidate, drf_v2_same_step_via_hb): değişmez, typing hipotezi eklenir
- DrfCrossStep.lean §3 (cross-Step DRF tam iskelet): onarım sonrası tam ispatla doldurulur

Yani V2.1 **iyi başlangıç**, onarımdan sonra **tamamlanır**.

---

## 10. Karar Noktası (Mehmet)

Bu plan üç seçenek sunar:

### (A) Onayla — onarım başlasın
- 6-10 hafta süre
- ~1,400-1,950 satır Lean kod değişimi
- Çıktı: akademik olarak savunulabilir V1 mekanize + V2 turlarının düzgün temeli
- TOPLAS makalesi için "we mechanize using Wright-Felleisen Type Soundness" diyebilen versiyonu

### (B) Reddet — V2'ye devam
- Modellerin uyarılarını kabul etmiyor
- Mevcut V1 bundled form + V2 turları üzerinden ilerle
- Risk: reviewer "your IyiTipli is vacuous, your Step preconditions smuggle type system, your V3 has placeholder True conjuncts" der
- Cevap: "our V1 is structural skeleton; full mechanization deferred" — savunulabilir ama zayıf

### (C) Modifiye et — onarım planını değiştir
- Kapsam azalt: sadece IyiTipli'yi gerçek inductive yap, Step refactor sonra
- Veya kapsam genişlet: V2 typed-aware'i hemen dahil et
- Veya farklı yaklaşım: typing yerine "specification monad" (F* gibi)

### Öneri

**(A) Onarım** — modellerin uyarısı mekanik olarak doğru, akademik dürüstlük için onarım gerekli. TOPLAS submission'ından önce yapılması en doğru.

(B) reddetme seçeneği **TOPLAS'ta savunulamaz**; reviewer aynı uyarıyı 30 dakikada bulur.

(C) "F* monad" gibi farklı yaklaşımlar Lean projesinden uzaklaşmak; mevcut altyapı kaybedilir.

---

## 11. Yasaklar ve Kapsam

Bu plan **doküman**dır. **Hiçbir Lean kodu değişikliği YAPILMADI**.

V2.1 commit `08ee17e` korunur — onarım sonrası entegre.

Mevcut feature branch `feature/drf-mekanize-ve-v3-metateorem` üzerinde herhangi bir değişiklik için **Mehmet onayı bekler**.

---

## 12. Çapraz Referanslar

**KEMGU Belgeler:**
- [`KEMGU_DRF_Mekanize_Spec.md`](KEMGU_DRF_Mekanize_Spec.md) — Faz A/B/C planı (mevcut)
- [`KEMGU_Metateorem_V3.md`](KEMGU_Metateorem_V3.md) — V3 V1 bundled (mevcut, overclaiming kabul ediliyor)
- [`KEMGU_Operasyonel_Semantik.md`](KEMGU_Operasyonel_Semantik.md) — Op.Sem §7 IyiTipli tanımı (kâğıt, doğru)
- [`KEMGU_DRF_Lemmalar.md`](KEMGU_DRF_Lemmalar.md) — DRF-L0..L7 (kâğıt, doğru)

**Dış Kaynaklar:**
- **Pierce, B. C. (2002)** *Types and Programming Languages.* MIT Press. §8.3.
- **Wright, A. K. & Felleisen, M. (1994)** "A Syntactic Approach to Type Soundness." *Information and Computation* 115(1): 38-94.
- **Harper, R. (2016)** *Practical Foundations for Programming Languages.* 2nd ed., Cambridge University Press. §4-7.
- **Felleisen, M. & Hieb, R. (1992)** "The Revised Report on the Syntactic Theories of Sequential Control and State." *Theoretical Computer Science* 103(2): 235-271.
- **Jung, R. et al. (2018)** "RustBelt: Securing the Foundations of the Rust Programming Language." *POPL 2018.*
- **Leroy, X. (2009)** "Formal Verification of a Realistic Compiler." *CACM* 52(7): 107-115.

---

**END KEMGU Mekanizasyon Çekirdek Onarım Planı (2026-05-18)**
