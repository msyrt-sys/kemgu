# KEMGU Mekanizasyon Çekirdek Onarım Planı (v2)

**Tarih:** 2026-05-18 (revize)
**Durum:** TASLAK PLAN v2 — iki bağımsız değerlendirme (Gemini + ChatGPT) tarafından önerilen 6 modifikasyonla güncellendi
**Önceki versiyon:** v1 (`db9ac1f`) — yapı doğru ama 6 noktada modifikasyon gerek
**Branch hedefi:** `feature/drf-mekanize-ve-v3-metateorem` (mevcut)
**Tetikleyici:** İki bağımsız değerlendirme Lean mekanizasyondaki yapısal sorunları aynı yerlerden işaret etti; planın ilk versiyonu doğru bulundu ama 6 önemli modifikasyon önerildi.

**Kapsam (v2):**
Bu doküman v1'in kabul edilen analizini (§1-§2) korur ve 6 modifikasyon uygular:
1. §3: **Layered typing judgment** (monolitik yerine HasType + LinearOK + RegionOK)
2. §4: **Runtime guard reinterpretation** + dual constructor (Ok + Fault) — preconditions ÇIKARMAK YANLIŞ, **yeniden konumlandır**
3. §5 (yeni merkez): **ConfigTyped köprüsü** — discharge lemmaların asıl temeli
4. §6: Discharge lemmalar yeniden formüle edilir (ConfigTyped üzerinden)
5. §7: Onarım sıralaması **aşamalı inşa** (8 adım, ConfigTyped önce, layered build-up)
6. §8-§11: Maliyet, kapsam, isimlendirme ve akademik strateji güncellenir

**Karar noktası:** Mehmet bu v2 planı onaylarsa onarım başlar.

---

## 1. Yapısal Sorun Analizi (v1'den korunur)

### 1.1 Sorun: Vacuous Predicates (Placeholder True)

**Nerede:** [`proofs/drf-v2-lean/Kemgu/Sem/Core.lean`](../proofs/drf-v2-lean/Kemgu/Sem/Core.lean) §11

```lean
def TipKontrolOk     (_Pi : Program) : Prop := True
def LineerKontrolOk  (_Pi : Program) : Prop := True
def CapabilityKontrolOk (_Pi : Program) : Prop := True
def SabitsureKontrolOk  (_Pi : Program) : Prop := True
def BolgeAtamaOk     (_Pi : Program) : Prop := True
def RealtimeKontrolOk   (_Pi : Program) : Prop := True
```

**Akademik kategori:**
- **Vacuous quantification** (Pierce TAPL §8.3)
- **Semantic self-validation**: tip sistemi Lean'de hiç tanımlanmadı
- **Trivial soundness**: `IyiTipli → P` ≡ `True → P` = `P`

### 1.2 Sorun: Step Constructor Preconditions (Circular Smuggling)

**Nerede:** [`SmallStep.lean`](../proofs/drf-v2-lean/Kemgu/Sem/SmallStep.lean)

```lean
| sAtama ...
    (h_not_frozen : ¬ isFrozen S k.bolge)
    (h_owner : sahiplikGet S.sahiplik (k.bolge, S.zaman)
                = some (Sahip.thread ctx.tid))
| cGorevBaslat ...
    (h_lineer_caller : ∃ ctx' ∈ S'.thread, ...)
```

**Akademik kategori:**
- **Type system smuggling** (Harper §4)
- **Circular reasoning**: Step well-typed by construction → her Step tip-sound
- **Loss of generality**: hatalı runtime davranış (frozen yazma, un-owned modify) Step'te ifade edilemiyor

### 1.3 Sorun: V3 Overclaiming (Placeholder Conjuncts)

**Nerede:** [`Soundness/Main.lean`](../proofs/drf-v2-lean/Kemgu/Soundness/Main.lean)

```lean
def SideChannelResistant_v2_placeholder (_Pi : Program) : Prop := True
def BET_v2_placeholder (_Pi : Program) : Prop := True

theorem kemgu_soundness_v3 ... :
    DrfHolds S ∧ MemSafe_perStep S
    ∧ SideChannelResistant_v2_placeholder Pi      -- = True
    ∧ BET_v2_placeholder Pi                        -- = True
```

**Akademik kategori:**
- **Overclaiming**: "V3 soundness" iddiası DRF + MemSafe'a indirgenir
- **False generalization**: 4-conjunct claim, 2-conjunct conclusion

### 1.4 Üç Sorunun Birleşik Sonucu

V1 bundled mekanize akademik olarak savunulamaz. Onarım gerek. Mevcut commit'ler iyi yapısal iskelet sağlıyor — temel restoration.

---

## 2. Wright-Felleisen Yaklaşımı (v1'den korunur)

### 2.1 Klasik Type Soundness (Wright & Felleisen 1994)

```
Progress:       ∀ e τ, (∅ ⊢ e : τ) → (e is value) ∨ (∃ e', e ⟶ e')
Preservation:   ∀ e e' τ, (∅ ⊢ e : τ) ∧ (e ⟶ e') → (∅ ⊢ e' : τ)
Soundness Cor.: Well-typed programs never get stuck
```

**Referanslar:**
- Pierce **TAPL** §8.3
- Wright & Felleisen 1994 (I&C 115(1))
- Harper **PFPL** §4-7
- Felleisen & Hieb 1992 (TCS 103(2))

### 2.2 KEMGU'ya Uygulama (Aşamalı)

Klasik soundness yaklaşımı KEMGU'ya **aşamalı** uygulanır (v2 değişim §6'da detaylı):
1. Önce klasik HasType (lineerlik+region OLMADAN) için Progress + Preservation
2. Sonra LinearOK katmanı eklenir, lemmalar güncellenir
3. Sonra RegionOK katmanı eklenir, lemmalar güncellenir
4. Tam soundness `Typed = HasType ∧ LinearOK ∧ RegionOK` için sentez

---

## 3. Katmanlı Typing Judgment Tasarımı (MODIFIYE — v1→v2)

### 3.1 Tasarım Felsefesi: Layered, Mono Değil

**v1 öneri** (geri çekildi): Monolitik `Γ; Λ; Ρ ⊢ e : τ ⊣ Λ'; Ρ'` — tek inductive judgment, üç boyutu (tip + lineerlik + bölge) birleştiriyor.

**v2 öneri** (kabul edilen): Üç ayrı judgment + birleştirici structure.

**Niye katmanlı:**
- **Modülerlik**: her boyutun (tip, lineerlik, bölge) kendi kuralları, kendi Progress/Preservation
- **Aşamalı inşa**: önce HasType, sonra LinearOK, sonra RegionOK — her katman tek başına valide edilebilir
- **Discharge ayrılığı**: typing'den Step önkoşullarına 3 ayrı lemma ailesi (her katmandan)
- **V2 hazırlığı**: Capability/Sabitsure/Realtime ileride 4., 5., 6. katman olarak eklenebilir (orthogonality)
- **Akademik norm**: TAPL §15 (subtyping) ve seL4 mekanizasyonu katmanlı judgment kullanır

### 3.2 Katman 1: HasType (Klasik Tip Sistemi)

```lean
inductive HasType : TipOrtam → Ifade → Tip → Prop where
  | t_var : Γ x = some τ → HasType Γ (Ifade.tanim x) τ
  | t_lit : DegerTipi v = τ → HasType Γ (Ifade.sabit v) τ
  | t_atama : Γ x = some τ → HasType Γ e τ → HasType Γ (Ifade.atama x e) Tip.bos
  | t_seq : HasType Γ a τ_a → HasType Γ b τ_b → HasType Γ (Ifade.seq a b) τ_b
  | t_kullan : Γ x = some (Tip.tekkez τ) → HasType Γ (Ifade.kullanIf x) τ
  | t_gorev_baslat : ∀ v ∈ yd, Γ v ≠ none → HasType (Γ_inner yd) kod τ_donus →
                     HasType Γ (Ifade.gorevBaslat yd kod) (Tip.gorev τ_donus)
  | t_kanal_gonder : Γ v = some τ → HasType Γ (Ifade.kanalGonderIf k v) Tip.bos
  | t_kanal_al : HasType Γ (Ifade.kanalAlIf k) τ
  | t_dondur : HasType Γ (Ifade.dondurIf b) Tip.bos
  | t_imha : Γ x = some (Tip.tekkez τ) → HasType Γ (Ifade.imhaIf x) Tip.bos
```

Klasik judgment — sadece tip uyumu, lineerlik veya bölge YOK. Pierce TAPL §8 STLC formatı.

### 3.3 Katman 2: LinearOK (Lineer Geçişler)

```lean
inductive LinearOK : TipOrtam → LineerOrtam → Ifade → LineerOrtam → Prop where
  | l_var_nonlinear : Γ x = some τ → ¬ Tip.lineerMi τ →
                       LinearOK Γ Λ (Ifade.tanim x) Λ
  | l_var_move : Γ x = some τ → Tip.lineerMi τ → Λ x = some Lineerlik.aktif →
                  LinearOK Γ Λ (Ifade.tanim x) (Λ.update x Lineerlik.tuketildi)
  | l_kullan : Γ x = some (Tip.tekkez τ) → Λ x = some Lineerlik.aktif →
                LinearOK Γ Λ (Ifade.kullanIf x) (Λ.update x Lineerlik.tuketildi)
  | l_gorev_baslat : LinearOK Γ Λ_inner kod Λ_inner' →
                      Λ' = Λ \ (yd ∩ {v | Tip.lineerMi (Γ v)}) →
                      LinearOK Γ Λ (Ifade.gorevBaslat yd kod) Λ'
  -- ... diğer kurallar
```

Lineer Lambda durumunun nasıl değiştiğini izler. T-VAR'ın **mode-aware** olduğunu burada açık: aynı `Ifade.tanim x` ifadesi linear ise consume, non-linear ise read.

### 3.4 Katman 3: RegionOK (Bölge Geçişleri)

```lean
inductive RegionOK : TipOrtam → BolgeOrtam → Ifade → BolgeOrtam → Prop where
  | r_atama : Ρ x = some b → b.kategori ≠ BolgeKategorisi.donmus →
              -- ctx erişebilir bölge (sahip/lit/global)
              RegionOK Γ Ρ (Ifade.atama x e) Ρ
  | r_gorev_baslat : Ρ' = Ρ ∪ {bolge(v) ↦ ρ_sahip(tYeni) : v ∈ yd} →
                      RegionOK Γ Ρ (Ifade.gorevBaslat yd kod) Ρ'
  | r_kanal_gonder : Ρ' = Ρ.update b (ρ_kanal k) →
                      RegionOK Γ Ρ (Ifade.kanalGonderIf k v) Ρ'
  | r_dondur : Ρ' = Ρ.update b ρ_donmus →
                RegionOK Γ Ρ (Ifade.dondurIf b) Ρ'
  -- ...
```

Bölge atama + sahiplik geçişlerini izler. T-ATAMA için "frozen değil" ve "sahip" koşulları burada.

### 3.5 AccessMode (T-VAR Mode-Aware)

```lean
inductive AccessMode : Type where
  | read   -- okuma (non-linear, paylaşılabilir)
  | move   -- linear move (consume)
  | write  -- atama hedefi (mutate)
deriving Repr, DecidableEq

def varAccessMode (Γ : TipOrtam) (e : Ifade) (x : VarId) : AccessMode :=
  match e with
  | Ifade.atama y _ => if x = y then AccessMode.write else AccessMode.read
  | Ifade.kullanIf y => if x = y then AccessMode.move else AccessMode.read
  | Ifade.imhaIf y => if x = y then AccessMode.move else AccessMode.read
  | _ => AccessMode.read  -- default
```

AccessMode T-VAR'ın hangi tip kuralının uygulanacağını belirler — read/move ayrımı linear vs non-linear semantiğin tabanı.

### 3.6 Birleştirici Structure

```lean
structure Typed (Γ : TipOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam)
                (e : Ifade) (τ : Tip)
                (Λ' : LineerOrtam) (Ρ' : BolgeOrtam) : Prop where
  hasType    : HasType Γ e τ
  linearOK   : LinearOK Γ Λ e Λ'
  regionOK   : RegionOK Γ Ρ e Ρ'
```

`Typed` üç katmanın conjunction'ı. Discharge lemmaları `Typed`'i hipotez alır.

**Avantaj:** Aşamalı inşa boyunca sadece o ana kadar tanımlı katmanlar kullanılır. Adım 4'te (Progress + Preservation for HasType) sadece HasType lazım, LinearOK eklenmemiş. Bu **decoupling** ispatların sırasını kolaylaştırır.

### 3.7 V1 Onarım Subset Kapsamı

V1 onarım için bu üç katman yeterli:
- ✓ HasType: temel tip uyumu (TAPL §8 minimal)
- ✓ LinearOK: Linear Types Spec V1 (tekkez, kullan, imha)
- ✓ RegionOK: Bölge sistemi V1 (R-LIT/R-YEREL/R-VER + sahiplik)
- ✗ CapabilityOK (V2.6 — Capability spec V2'de eklenirse)
- ✗ SabitsureOK (V2.4 NI ile birlikte)
- ✗ RealtimeOK (V2.3 BET ile birlikte)

§9'da V2 ilişkisi daha detaylı: V1 onarım **bilinçli olarak dar tutulur** (Linear + Region only).

### 3.8 Decidability ve tipCheck Function

Inductive `Typed` decidable değil; ek olarak:

```lean
def tipCheck : TipOrtam → LineerOrtam → BolgeOrtam → Ifade →
               Option (Tip × LineerOrtam × BolgeOrtam)

theorem tipCheck_soundness :
    tipCheck Γ Λ Ρ e = some (τ, Λ', Ρ') →
    Typed Γ Λ Ρ e τ Λ' Ρ'

theorem tipCheck_completeness :
    Typed Γ Λ Ρ e τ Λ' Ρ' →
    tipCheck Γ Λ Ρ e = some (τ, Λ', Ρ')
```

Bu ek ~200 satır — `tipCheck` her ifade için decision procedure. Onarım sonrası eklenir (zorunlu değil ama akademik tamlığa katkı).

---

## 4. SmallStep Refactor — Runtime Guard Reinterpretation (KRİTİK DÜZELTME)

### 4.1 v1'in Hatası

v1 planımda yazmıştım: "preconditions kaldır". **Bu yanlış.** Modeller bunu işaret etti:

> Preconditions Step constructor'ından **çıkarılmamalı**, **yeniden konumlandırılmalı**. Mevcut precondition'lar zaten "runtime guard" — yani "iyi tipli adımın hangi koşullar altında alınabileceğini" tanımlıyor. Eksik olan **alternatif** Fault constructor'ları.

### 4.2 Doğru Refactor: Dual Constructors (Ok + Fault)

Mevcut `sAtama` constructor'ı **YERINDE KALIR** ama anlamı değişir:
- **Eski yorum**: "sAtama tek geçerli atama Step'i, h_owner/h_not_frozen zorunlu" (circular)
- **Yeni yorum**: "sAtamaOk yalnız iyi davranan atama Step'i; sAtamaFault kötü davranan alternatifler"

```lean
inductive Step : Konfigurasyon → Konfigurasyon → Prop where
  | sAtamaOk
      (S S' : Konfigurasyon)
      (ctx : ThreadCtx) (x : VarId) (v : Deger) (k : Konum)
      (h_in : ctx ∈ S.thread)
      (h_ifade : ctx.ifade = .atama x (.sabit v))
      -- Runtime guard'lar (KORUNUR, ama Ok constructor'ına ait):
      (h_not_frozen : ¬ isFrozen S k.bolge)
      (h_owner : sahiplikGet S.sahiplik (k.bolge, S.zaman)
                  = some (Sahip.thread ctx.tid))
      (h_store : S'.store = (k, v) :: S.store)
      (h_iz : S'.iz = .memYaz ctx.tid k v :: S.iz)
      (h_zaman : S'.zaman = S.zaman + 1)
      (h_sahip : S'.sahiplik = S.sahiplik)
      (h_kanal : S'.kanal = S.kanal)
      : Step S S'

  | sAtamaFaultFrozen
      (S S' : Konfigurasyon)
      (ctx : ThreadCtx) (x : VarId) (v : Deger) (k : Konum)
      (h_in : ctx ∈ S.thread)
      (h_ifade : ctx.ifade = .atama x (.sabit v))
      (h_frozen : isFrozen S k.bolge)               -- NEGATION of Ok's h_not_frozen
      (h_fault_state : S'.iz = .memYaz ctx.tid k v :: S.iz)  -- iz kaydedilir
      -- fault flag (yeni Konfigurasyon alanı: fault : Option FaultSebep)
      (h_fault : S'.fault = some (FaultSebep.frozenWrite k.bolge))
      : Step S S'

  | sAtamaFaultUnowned
      (S S' : Konfigurasyon)
      (ctx : ThreadCtx) (x : VarId) (v : Deger) (k : Konum)
      (h_in : ctx ∈ S.thread)
      (h_ifade : ctx.ifade = .atama x (.sabit v))
      (h_not_owner : sahiplikGet S.sahiplik (k.bolge, S.zaman)
                      ≠ some (Sahip.thread ctx.tid))
      (h_fault : S'.fault = some (FaultSebep.notOwner k.bolge ctx.tid))
      : Step S S'
```

### 4.3 Disjoint + Deterministik

**Önemli:** Ok ve Fault constructor'ları **disjoint** olmalı:
- `sAtamaOk` aktif olabilir ⟺ `h_not_frozen ∧ h_owner` doğru
- `sAtamaFaultFrozen` aktif olabilir ⟺ `h_frozen` doğru
- `sAtamaFaultUnowned` aktif olabilir ⟺ `h_not_owner` doğru

Bu üç durum **mutually exclusive + exhaustive** (excluded middle on isFrozen ∧ sahiplikGet outcome). Yani belirli bir `(S, atama x v)` için **tam bir** Step constructor uygulanabilir.

**Deterministik (V1 SC altında):** Aynı S → aynı S' (her constructor tek bir resultan üretir). Non-deterministic concurrency YOK (V1).

### 4.4 Konfigurasyon'a Fault Field Eklenir

```lean
structure Konfigurasyon where
  thread      : List ThreadCtx
  store       : Store
  sahiplik    : Sahiplik
  kanal       : List KanalDurumu
  zaman       : Zaman
  iz          : Iz
  fault       : Option FaultSebep  -- YENİ: nil = normal, some sebep = fault state
```

Fault state'e gidildiğinde:
- `S.fault = some sebep` (fault olduğunu gösterir)
- Sonraki Step'ler bu state'ten alınamaz (Progress: fault → no further reduction)

### 4.5 Diğer Constructor'lar İçin Aynı Pattern

- **cGorevBaslatOk** + **cGorevBaslatFaultLinearViolation** (`h_lineer_caller` ihlal)
- **cKanalGonderOk** + **cKanalGonderFaultLinearConsume** (linear v already consumed)
- **cKanalAlOk** + (genelde fault yok — alım için precondition zayıf)
- **cDondurOk** + **cDondurFaultAlreadyFrozen** (zaten frozen)
- **sLinKullanOk** + **sLinKullanFaultAlreadyConsumed** (`Λ(x) ≠ aktif`)
- **sLinImhaOk** + **sLinImhaFaultAlreadyConsumed**

Her constructor için **Ok + Fault** dual. Toplam:
- Eski 8 constructor → yeni ~16 constructor (8 Ok + 8 Fault)
- SmallStep boyutu: ~250 satır (mevcut 227 → ~400-450)

### 4.6 Mevcut L0-L7 Adaptasyonu

L4 (FrozenRegionRead) örneği:

```lean
theorem drf_l4_a_step
    (Γ Λ Ρ) (h_typed : Typed Γ Λ Ρ ifade τ Λ' Ρ')
    (S S' : Konfigurasyon) (h_step : Step S S')
    (h_config : ConfigTyped Γ Λ Ρ S)  -- yeni: state-typing köprüsü
    ...
    : Olay.memYaz t k v ∈ S.iz ∨ k.bolge ≠ b := by
  cases h_step with
  | sAtamaOk _ _ _ _ _ h_in h_ifade h_not_frozen h_owner _ h_iz _ _ _ =>
    -- h_not_frozen artık constructor'dan; mevcut akış aynı
    ...
  | sAtamaFaultFrozen _ _ _ k_x _ _ _ h_frozen _ _ =>
    -- Fault case: typed program bu fault'a gitmez
    exfalso
    -- discharge lemma: typing + config → no fault
    exact typing_excludes_frozen_fault h_typed h_config h_frozen
```

Yani L4:
1. `sAtamaOk` case: mevcut akış (h_not_frozen constructor'da)
2. `sAtamaFault*` case: typing'in fault'a gitmediğini gösteren discharge lemma

Her DRF lemma 8 constructor case yerine 16 case'e çıkar — ama Ok case'leri MEVCUT akış, Fault case'leri **typing exclusion** (discharge lemma application).

### 4.7 Tahmini Değişim

- SmallStep.lean: ~250 satır (mevcut 227 → ~400-450)
- Fault sebebi enum: ~30 satır
- Konfigurasyon.fault field: ~5 satır
- L0BolgeKorunumu: ~40 satır (8 Fault case ekler)
- L4FrozenRegionRead: ~80 satır
- L7BellekErisimTipSoundness: ~70 satır
- L2/L3/L5/L6: ~50 satır × 4 = ~200 satır
- Drf.lean (Teorem 4'): ~100 satır
- T1 (MemSafety): ~80 satır

**SmallStep + L0-L7 + Drf + T1 toplam değişim: ~850 satır.**

---

## 5. ConfigTyped Köprüsü (YENİ — v2'nin Merkezi)

### 5.1 v1'in Eksiği

v1 planında discharge lemmaları doğrudan "typing → Step precondition" formundaydı. **Bu yetmez** — typing yalnız ifade hakkında bilgi verir, **state hakkında değil**. Discharge lemmaları state'in typing'e uyumlu olduğunu da bilmeli.

**v2 öneri:** `ConfigTyped` — runtime state'in static type'a uyumunun **merkezi predikatı**.

### 5.2 ConfigTyped Tanımı (Kavramsal)

```lean
def ConfigTyped (Γ : TipOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam)
                (S : Konfigurasyon) : Prop :=
  StoreTyped Γ Ρ S.store
  ∧ ThreadsTyped Γ Λ Ρ S.thread
  ∧ SahiplikConsistent Ρ S.sahiplik S.zaman
  ∧ KanalConsistent Γ Ρ S.kanal
  ∧ S.fault = none  -- fault state ConfigTyped değil
```

5 alt-yapı (her biri ayrı predikat):

#### 5.2.1 ValueTyped

```lean
inductive ValueTyped (Γ : TipOrtam) (Ρ : BolgeOrtam) : Deger → Tip → Prop where
  | vt_skaler : ValueTyped Γ Ρ (Deger.skaler n) Tip.scalar
  | vt_metin : ValueTyped Γ Ρ (Deger.metinDeg b uz) Tip.metin
  | vt_yapi : -- alanlar typed
              ValueTyped Γ Ρ (Deger.yapiVal b alanlar) (Tip.yapi name)
  | vt_dizi : ValueTyped Γ Ρ (Deger.diziVal b uz) (Tip.dizi τ)
  | vt_closure : -- closure body typed
                 ValueTyped Γ Ρ (Deger.closureVal kodId yd) (Tip.islev args ret)
  | vt_yetki : ValueTyped Γ Ρ (Deger.yetkiTok id kaynak) (Tip.yetki kaynak)
  | vt_birim : ValueTyped Γ Ρ Deger.birim Tip.bos
```

Her runtime değer için tip uyumu.

#### 5.2.2 StoreTyped

```lean
def StoreTyped (Γ : TipOrtam) (Ρ : BolgeOrtam) (store : Store) : Prop :=
  ∀ (k : Konum) (v : Deger), (k, v) ∈ store →
    ∃ τ : Tip,
      ValueTyped Γ Ρ v τ
      -- konum bölgesi typed (k.bolge ∈ tanımlı bölgeler)
      ∧ ∃ x, Ρ x = some k.bolge
```

Store'daki her (k, v) için: değer tip-uyumlu, konum bilinen bölgede.

#### 5.2.3 ThreadsTyped

```lean
def ThreadsTyped (Γ : TipOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam)
                 (threads : List ThreadCtx) : Prop :=
  ∀ ctx ∈ threads,
    -- thread'in ifadesi tipli (some Λ_ctx, Ρ_ctx ile)
    ∃ Λ_ctx Ρ_ctx τ Λ' Ρ',
      Typed Γ Λ_ctx Ρ_ctx ctx.ifade τ Λ' Ρ'
      -- thread'in lineer durumu tutarlı (her tüketilmiş var Λ'da TUKETILDI)
      ∧ ctx.lineer ≈ Λ_ctx
```

Her thread'in ifadesi tipli; her thread'in lineer durumu Λ'ya uyumlu.

#### 5.2.4 SahiplikConsistent

```lean
def SahiplikConsistent (Ρ : BolgeOrtam) (sahiplik : Sahiplik) (zaman : Zaman) : Prop :=
  -- Tüm bölgeler için sahiplik tutarlı
  ∀ b z, z ≤ zaman →
    sahiplikGet sahiplik (b, z) = some (Sahip.thread t) →
    -- t bilinen thread (Ρ'da)
    ∃ x, Ρ x = some b
  ∧
  -- Frozen bölgeler kalıcı: bir kez frozen, sonraki tüm z'lerde frozen
  ∀ b z₀ z, z₀ ≤ z ∧ z ≤ zaman →
    sahiplikGet sahiplik (b, z₀) = some Sahip.donmus →
    sahiplikGet sahiplik (b, z) = some Sahip.donmus
```

Sahiplik haritası bilinen bölgelere refer eder; frozen persistence garantili.

#### 5.2.5 KanalConsistent

```lean
def KanalConsistent (Γ : TipOrtam) (Ρ : BolgeOrtam)
                    (kanal : List KanalDurumu) : Prop :=
  ∀ kd ∈ kanal,
    -- her kanaldaki tüm değerler tip-uyumlu
    ∀ v ∈ kd.gonderKuyrugu,
      ∃ τ, ValueTyped Γ Ρ v τ
```

Kanal kuyruğundaki her değer tipli.

### 5.3 ConfigTyped İmplementasyon Notları

- 5 alt-yapı bağımsız tanımlanabilir
- ConfigTyped = bunların `∧` birleşimi
- Her alt-yapının ayrı korunum lemması (Preservation_StoreTyped, vs.)
- ConfigTyped korunumu (Step → ConfigTyped korunur) **Preservation theorem'in inducktif çekirdeği**

### 5.4 Niye Bu Köprü Şart

Discharge lemmaları örneği:
```lean
theorem typing_implies_not_frozen
    (Γ Λ Ρ) (h_typed : Typed Γ Λ Ρ (Ifade.atama x e) τ Λ' Ρ')
    (S : Konfigurasyon) (h_config : ConfigTyped Γ Λ Ρ S)
    (k : Konum) (h_k : Ρ x = some k.bolge) :
    ¬ isFrozen S k.bolge
```

Bu lemma:
- `h_typed`: typing'den `Ρ' x` frozen değil (T-ATAMA kuralı)
- `h_config`: state ve typing tutarlı (Ρ x = k.bolge → S.sahiplik'te tutarlı)
- Sonuç: `¬ isFrozen S k.bolge`

`h_config` olmadan typing soyut kalır; `h_config` ile state-typing **bağlantısı kurulur**.

### 5.5 Tahmini Boyut

- ValueTyped: 80 satır
- StoreTyped: 30 satır
- ThreadsTyped: 50 satır
- SahiplikConsistent: 40 satır
- KanalConsistent: 20 satır
- ConfigTyped birleşim + 5 korunum lemması: ~150 satır

**ConfigTyped toplam: ~370 satır.** v1 planımda bu yoktu — v2'nin en büyük ekleme.

---

## 6. Discharge Lemmaları — Yeniden Formüle (MODIFIYE)

### 6.1 v1'in Eksiği

v1 planımda yazmıştım: "typing → Step precondition". **Bu ifade yetersiz** — state-typing bağlantısı yok.

**v2 öneri:** Her discharge lemmasının iki çeşidi:
1. **Normal guard holds**: `Typed + ConfigTyped → Step'in Ok constructor'ı uygulanır`
2. **Fault impossible**: `Typed + ConfigTyped → Step'in Fault constructor'ı uygulanamaz` (`exfalso`)

### 6.2 Discharge Lemma Aileleri

**Aile 1 — Normal Guards (HasType + ConfigTyped → Step.Ok guard):**

```lean
theorem typing_implies_sAtamaOk_guards
    (Γ Λ Ρ) (h_typed : Typed Γ Λ Ρ (Ifade.atama x e) Tip.bos Λ' Ρ')
    (S : Konfigurasyon) (h_config : ConfigTyped Γ Λ Ρ S)
    (ctx : ThreadCtx) (h_ctx : ctx ∈ S.thread)
    (h_active : ctx.ifade = Ifade.atama x e) :
    ∃ k : Konum,
      (¬ isFrozen S k.bolge)                                    -- h_not_frozen
      ∧ (sahiplikGet S.sahiplik (k.bolge, S.zaman)
          = some (Sahip.thread ctx.tid))                         -- h_owner
```

Yani: typed program + consistent state → sAtamaOk'in her iki guard'ı sağlanır.

**Aile 2 — Fault Impossibility (Typed + ConfigTyped → Fault constructor'ı uygulanamaz):**

```lean
theorem typing_excludes_sAtamaFault_frozen
    (Γ Λ Ρ) (h_typed : Typed Γ Λ Ρ (Ifade.atama x e) Tip.bos Λ' Ρ')
    (S : Konfigurasyon) (h_config : ConfigTyped Γ Λ Ρ S)
    (ctx : ThreadCtx) (h_ctx : ctx ∈ S.thread)
    (k : Konum) (h_k : k = ... )
    (h_frozen : isFrozen S k.bolge) :
    False
```

İspat: typing'den `Ρ x ∉ frozen`; config'ten state Ρ'ya uyumlu; h_frozen contradiction.

**Aile 3 — Linear Discharge (LinearOK → linear guards):**

```lean
theorem typing_implies_lineer_caller
    (Γ Λ Ρ) (h_typed : Typed Γ Λ Ρ (Ifade.gorevBaslat yd kod) τ Λ' Ρ')
    (S : Konfigurasyon) (h_config : ConfigTyped Γ Λ Ρ S) :
    ∃ ctx' ∈ S'.thread, ctx'.tid = ctx.tid ∧
      ∀ v ∈ linearYakalananlar, (v, Lineerlik.tuketildi) ∈ ctx'.lineer
```

LinearOK katmanından çıkarsanır.

**Aile 4 — Region Discharge (RegionOK → bolge guards):**

```lean
theorem typing_implies_bolge_transferred
    (Γ Λ Ρ) (h_typed : Typed Γ Λ Ρ (Ifade.gorevBaslat yd kod) τ Λ' Ρ')
    (S S' : Konfigurasyon) (h_config : ConfigTyped Γ Λ Ρ S) :
    S'.sahiplik = sahiplikSetMany S.sahiplik transferredBolgeler S.zaman ...
```

RegionOK katmanından.

### 6.3 No-Fault Theorem (Discharge'in Çatısı)

```lean
theorem typed_well_typed_no_fault
    (Pi : Program) (Γ Λ Ρ) (h_typed_program : ProgramTyped Γ Λ Ρ Pi)
    (S₀ S : Konfigurasyon) (h_init : ConfigTyped Γ Λ Ρ S₀)
    (h_run : StepStar S₀ S) :
    S.fault = none  -- Hiçbir reachable state fault'a gitmez
```

İspat: StepStar induction + ConfigTyped korunumu (Preservation) + fault impossibility lemmaları.

Bu **çatı teorem** her Fault constructor'ının typed program için ulaşılamaz olduğunu gösterir — DRF lemmalar bu teoremi kullanarak Fault case'lerini `exfalso` ile geçer.

### 6.4 Tahmini Boyut

- Aile 1 (Normal Guards): 6-8 lemma × 30-50 satır = ~250-400 satır
- Aile 2 (Fault Impossibility): 8-10 lemma × 30-50 satır = ~300-500 satır
- Aile 3 (Linear): 4-5 lemma × 40-60 satır = ~200-300 satır
- Aile 4 (Region): 4-5 lemma × 40-60 satır = ~200-300 satır
- No-Fault çatı teorem: ~100 satır

**Discharge lemmaları toplam: ~1050-1600 satır.** v1 planımda ~200-250 demiştim — UNDERESTIMATE. v2 daha gerçekçi.

---

## 7. Onarım Sıralaması — Aşamalı İnşa (MODIFIYE)

### 7.1 v1'in Sırası (Geri Çekildi)

v1 plan sırası:
1. Typing judgment → 2. Progress → 3. Preservation → 4. Discharge → 5. Step refactor → 6. L0-L7

**Sorun:** Tek monolitik typing judgment + Progress/Preservation tek aşamada → Lean uzmanı için bile karmaşık.

### 7.2 v2 Yeni Sıralama (Aşamalı İnşa, 8 Adım)

**Felsefe:** Önce iskelet, sonra katmanlı zenginleştirme. Her adım kendi içinde valide edilebilir.

```
┌──────────────────────────────────────────┐
│  1. StepResult + Fault Constructor       │  ~250 satır
│  Tasarım  (en başta, foundation)         │  ~1 hafta
└──────────────┬───────────────────────────┘
               ↓
┌──────────────────────────────────────────┐
│  2. ConfigTyped İskeleti                 │  ~370 satır
│  (placeholder DEĞİL, gerçek alt-yapılar) │  ~2-3 hafta
└──────────────┬───────────────────────────┘
               ↓
┌──────────────────────────────────────────┐
│  3. Minimal HasType (Klasik)             │  ~200 satır
│  (lineerlik + region OLMADAN, TAPL §8)   │  ~1.5 hafta
└──────────────┬───────────────────────────┘
               ↓
┌──────────────────────────────────────────┐
│  4. Progress + Preservation for HasType  │  ~300 satır
│  (klasik soundness, lineer/region YOK)   │  ~2 hafta
└──────────────┬───────────────────────────┘
               ↓
┌──────────────────────────────────────────┐
│  5. LinearOK Katmanı + Progress/Pres     │  ~250 satır
│  Güncellemesi                             │  ~2 hafta
└──────────────┬───────────────────────────┘
               ↓
┌──────────────────────────────────────────┐
│  6. RegionOK Katmanı + Progress/Pres     │  ~300 satır
│  Güncellemesi                             │  ~2 hafta
└──────────────┬───────────────────────────┘
               ↓
┌──────────────────────────────────────────┐
│  7. Discharge Lemmaları + No-Fault       │  ~1050-1600 satır
│  Çatı Teoremi                             │  ~4-5 hafta
└──────────────┬───────────────────────────┘
               ↓
┌──────────────────────────────────────────┐
│  8. L0-L7 + T1 + Teorem 4' Adaptasyonu   │  ~850 satır
│  (yeni temele oturt)                      │  ~3 hafta
└──────────────────────────────────────────┘
```

**Toplam:** ~3,570-4,120 satır + ~17-19 hafta.

### 7.3 Aşamalı Avantajlar

**Adım 3'ten sonra:** Minimal HasType + Progress/Preservation **çalışan bir mini-system**. STLC-benzeri Lean projesi — kendi başına sunulabilir.

**Adım 5'ten sonra:** LinearOK eklenmiş — Linear Lambda Calculus mekanize edilmiş. Mevcut literatürde (Linear Haskell, ATS) referans noktası.

**Adım 6'dan sonra:** RegionOK eklenmiş — region calculus mekanize. MLKit/Tofte-Talpin geleneğine yakın.

**Adım 7'den sonra:** Discharge ailesi — typed semantik bridge tam.

**Adım 8'den sonra:** Mevcut DRF lemmaları + T1 yeni temele oturtulmuş. Full V1 onarım.

### 7.4 Paralel Çalışma

- Adım 1 (StepResult) ve Adım 2 (ConfigTyped iskeleti) PARALEL yapılabilir
- Adım 5 ve 6 (LinearOK, RegionOK) PARALEL yapılabilir (eğer ayrı kişi)
- Adım 7 (Discharge) Adım 6 bittikten sonra
- Adım 8 Adım 7 bittikten sonra

### 7.5 Checkpoint'ler

Her adımdan sonra commit + lake build doğrulaması:
- C1: StepResult + Fault — build OK
- C2: ConfigTyped 5 alt-yapı — build OK
- C3: HasType + Progress/Preservation (klasik) — build OK
- C4: HasType + Linear/Region — build OK
- C5: Discharge lemma ailesi — build OK
- C6: No-Fault çatı teoremi — build OK
- C7: L0-L7 adapt — build OK + 0 sorry

7 ara checkpoint + final.

---

## 8. Tahmini Maliyet ve Zaman (MODIFIYE — Gerçekçi)

### 8.1 v1'in Hatası

v1: "6-10 hafta, ~1,400-1,950 satır" — **çok iyimser**.

### 8.2 v2 Gerçekçi Tahmin

| Adım | Lean Satır | Hafta Tahmini | Risk |
|------|-----------|---------------|------|
| 1. StepResult + Fault | 250 | 1 | Düşük — yapısal |
| 2. ConfigTyped iskeleti | 370 | 2-3 | Yüksek — yeni concept |
| 3. Minimal HasType | 200 | 1.5 | Düşük — TAPL §8 |
| 4. Progress + Preservation (HasType) | 300 | 2 | Orta-Yüksek — case analysis |
| 5. LinearOK + update | 250 | 2 | Orta — lineerlik induction |
| 6. RegionOK + update | 300 | 2 | Orta-Yüksek — sahiplik geçiş |
| 7. Discharge + No-Fault | 1050-1600 | 4-5 | Yüksek — büyük lemma ailesi |
| 8. L0-L7 + T1 + Drf adapt | 850 | 3 | Orta — mevcut + yeni dual |
| **Toplam (v2)** | **3,570-4,120** | **17-19 hafta (≈4-5 ay)** | |

**v1 → v2 farkı:** ~2 katı satır, ~2 katı zaman. v1 underestimate idi.

### 8.3 Tahmin Notu

Tahminler **deneyimli Lean kullanıcısı** içindir. KEMGU bağlamı + kâğıt belge cross-ref + insan review döngüleri ek zaman.

Auto mode + agent + insan review hibrit modelde **paralelleştirme + iterasyonla 3-4 ay** (12-16 hafta) mümkün — ama düşük risk varsayımı altında.

### 8.4 Tıkanma Riski Sırası

En yüksek tıkanma riski:
1. **Adım 7 (Discharge ailesi)**: ~1500 satır, 10+ lemma — 300 satır tıkanma kuralı her lemmada test edilir
2. **Adım 4 (Preservation HasType)**: TAPL §8'in mekanize karmaşıklığı KEMGU 5x
3. **Adım 2 (ConfigTyped)**: 5 alt-yapı bağımsız tasarım kararları
4. **Adım 6 (RegionOK)**: bölge geçişleri çoklu Step constructor üzerinde

Her tıkanma noktasında DUR + rapor + Mehmet kararı.

---

## 9. V2 Turları ile İlişki (MODIFIYE — V1 Scope Daraltıldı)

### 9.1 v1'in Hatası

v1 plan IyiTipli'de tüm 7 alt-koşulu (TipKontrol, Lineer, Capability, Sabitsure, Bolge, Realtime, NoGuvensiz) güncelleyecekti. **Bu çok geniş.**

### 9.2 v2 Önerisi: V1 Onarım = Linear + Region Only

**V1 onarım kapsamı:**
- ✓ HasType (klasik tip)
- ✓ LinearOK (Linear Types Spec V1)
- ✓ RegionOK (Bölge sistemi V1)
- ✗ CapabilityOK — kapsam dışı
- ✗ SabitsureOK — kapsam dışı
- ✗ RealtimeOK — kapsam dışı
- ✗ NoGuvensiz tam — minimal (mevcut hali yeterli)

**Akademik söylem:** "Çekirdek hesaplama bellek güvenliğine (Memory Safety + DRF) odaklanır. Capability, Constant-Time ve Real-Time özellikleri **gelecekteki genişletmelere bırakılır** (V2.2-V2.5)."

Bu daraltma:
- V1 onarım kapsamını **yönetilebilir** kılar
- Akademik olarak savunulabilir ("focused contribution")
- TOPLAS reviewer'a "we mechanize the linear region core; cap/ct/rt are orthogonal extensions" denilebilir

### 9.3 DRF-L Lemmaları Kapsamı

| Lemma | V1 onarım | Açıklama |
|-------|-----------|----------|
| L0 BolgeKorunumu | ✓ Linear+Region | Region core'da |
| L1 BolgeThreadTekilligi | ✓ Linear+Region | Region core'da |
| L2 LinearMoveCrossThread | ✓ Linear | Linear core'da |
| L3 LinearClosureSoundness | ✓ Linear+Region | Linear closure |
| L4 FrozenRegionRead | ✓ Region | Region donmus |
| L5 KanalAtomikTransfer | ✓ Region (b+c) | Channel sahiplik |
| **L6 CapabilityLinear** | ✗ **DIŞ** | Capability V2.6'da |
| L7 BellekErisimTipSoundness | ✓ HasType+Region | Memory access typed |

**L6 V1 onarım kapsamı dışı.** Gerekçe: Capability spec V2 (zaten daha eksik) — Linear ile birlikte ileride mekanize.

DRF-L lemmaların onarım sonrası 6-7'si Linear+Region yeni temelde; L6 V2'de tamamlanır.

### 9.4 V2 Turları Sonrası Plan

Onarım bittikten sonra:
- **V2.1 Cross-Step HB**: yeni temel üzerinde, daha düzgün argument (~200 satır azalma)
- **V2.2 T2/T3**: bölge lifecycle, ConfigTyped'a `yaratilmis_bolgeler` alanı eklenir (~250 satır)
- **V2.3 BET**: yeni katman RealtimeOK (~350 satır)
- **V2.4 NI**: yeni katman SabitsureOK + two-execution (~400 satır)
- **V2.5 Cross-Step DRF tam**: V2.1'in tamamlanması (~200 satır)
- **V2.6 Capability**: yeni katman CapabilityOK + L6 mekanize (~200 satır)

V2 toplam (onarım sonrası): ~1,600 satır, 4-6 hafta (ek).

---

## 10. V3 Theorem Adlandırma (YENİ)

### 10.1 v1'in Hatası: kemgu_soundness_v3

v1 plan "kemgu_soundness_v3" adıyla bundled teorem yazdı. **Bu isim geri çekilmeli** — SCR + BET formal olmadan "V3" iddiası overclaiming.

### 10.2 Yeni Adlandırma

| Eski (geri çek) | Yeni (kullan) | Anlam |
|----------------|---------------|-------|
| `kemgu_soundness_v3` (placeholder True ile) | `kemgu_soundness_v3_skeleton` | Eski hali, placeholder işaretli |
| (yok) | `kemgu_drf_memsafe_v1` | Yalnız DRF + T1 bundled (onarım sonrası) |
| (yok) | `kemgu_core_soundness_v1` | Onarım sonrası tam (Linear+Region+DRF+T1) |
| (yok) | `kemgu_soundness_v3` (ileride) | NI + BET formal eklendikten SONRA |

### 10.3 Geçiş Süreci

**Şimdi (mevcut commit `0304705`):**
- `kemgu_soundness_v3` → rename `kemgu_soundness_v3_skeleton` (placeholder'lı olduğu açık)
- Doküman güncellemesi: V3 ismi "iskelet" olarak işaretli

**Onarım sonrası (~4-5 ay):**
- Yeni: `kemgu_drf_memsafe_v1` (Linear+Region+DRF+T1 bundled, real proof)
- Yeni: `kemgu_core_soundness_v1` (Wright-Felleisen Soundness sentezi)

**V2.3 + V2.4 sonrası (BET + NI formal):**
- Yeni: `kemgu_soundness_v3` (artık gerçek 4-conjunct, V3 ismi haklı)

### 10.4 Akademik Dürüstlük

V3 ismi **uzun süreli rezerve edilir** — sadece tüm 4 boyut (Memory Safety + DRF + Side-Channel + BET) formal mekanize olduğunda kullanılır. TOPLAS reviewer "your V1/V2/V3 nomenclature is consistent" der.

---

## 11. Akademik Strateji (YENİ)

### 11.1 v1'in Hatası: TOPLAS Doğrudan

v1 plan TOPLAS submission'ı doğrudan hedefliyordu. **Bu çok agresif** — V1 onarım bile 4-5 ay; V2 turları + iyileşme döngüleri ile TOPLAS-ready 1.5-2 yıl.

### 11.2 v2 Önerisi: Konferans → Journal

**Aşama 1: Konferans (6-9 ay)**
Hedefler:
- **OOPSLA** (ACM SIGPLAN, Bahar) — KEMGU dilinin kendisi + V1 mekanize sunum
- **ICFP** (Eylül) — Linear/Region calculus mekanize odak
- **CPP** (Ocak) — Certified Programming and Proofs, Lean topluluk
- **ITP** (Temmuz) — Interactive Theorem Proving, mathematician kitlesi

**Artifact:** Lean 4 kod + tezimi + ~12-page paper. Tipik conference accept rate %20-30.

**Aşama 2: Journal Extended (Konferans'tan 1-2 yıl sonra)**
- TOPLAS extended version — 40+ page, full proofs, V2 turları dahil
- Konferans paper'ın 2-3 katı malzeme
- Reviewer cycle 6-18 ay

### 11.3 Metodolojik Katkı

KEMGU mekanizasyonun ek katkısı: **"LLM-Assisted Mechanization of Linear Region Calculus"**.

- Lean 4 mekanizasyon sürecinde Claude/Gemini/ChatGPT kullanımı
- Plan formulation + sorun tespit + iterasyon hız analizi
- İnsan-LLM hybrid productivity ölçümü
- Bu metodolojik katkı **ayrı bir paper** olabilir (PLATEAU workshop, ICSE NIER, vs.)

İki paper farklı katki:
1. **Teknik paper**: KEMGU dilinin Linear Region Calculus + V1 mekanize (OOPSLA/ICFP)
2. **Metodolojik paper**: LLM-assisted mekanizasyon workflow (PLATEAU/SE)

### 11.4 Riskler ve Mitigasyon

- **Konferans rejection**: yaygın; revize + farklı konferans
- **V2 tamamlanmazsa**: V1 paper'da "V1 core, V2 extensions in progress" denir
- **LLM görünür mü?**: Methodology section'da açıkça belgelenir

---

## 12. Mevcut V2.1 Commit'in Kaderi (eski §9)

V2.1 commit `08ee17e` (HappensBefore.lean + DrfCrossStep.lean) **silinmez**. Onarım sonrası **yeni temele entegre edilir**:

- HappensBefore.lean: HB types değişmez, tip-system bağlanması ek lemma'larla
- DrfCrossStep.lean §1-2: değişmez, typing hipotezi eklenir
- DrfCrossStep.lean §3 (cross-Step DRF tam iskelet): onarım sonrası tam ispatla doldurulur

V2.1 iyi başlangıç, onarımdan sonra tamamlanır.

---

## 13. Karar Noktası (Mehmet)

Bu v2 plan üç seçenek sunar:

### (A) Onayla — onarım v2 başlasın
- **17-19 hafta (~4-5 ay)** süre
- **~3,570-4,120 satır** Lean kod değişimi
- Çıktı: Wright-Felleisen soundness + Linear+Region katmanlı temel
- Konferans paper hazır

### (B) Reddet — V2'ye devam (modelleri ignore)
- Mevcut V1 bundled + V2 turları üzerinden ilerle
- Risk: konferans reviewer aynı uyarıyı bulur

### (C) Modifiye — kapsam azalt/genişlet
- Sadece HasType + Progress/Preservation (Linear/Region yok) — daha küçük V1
- Veya direkt full mechanization (V2 dahil) — daha büyük

### Öneri

**(A) v2 Onarım** — modellerin uyarıları kabul edildi, plan modifikasyonları içinde. Wright-Felleisen yaklaşımı standart, katmanlı build-up risk minimize ediyor.

**4-5 ay süre kabul edilebilir** çünkü:
- TOPLAS yerine konferans hedefi (§11) — zaman baskısı az
- Aşamalı checkpoint'ler (her adımda commit + Mehmet review)
- Tıkanma politikasıyla risk yönetimi

---

## 14. Yasaklar ve Kapsam

Bu v2 plan **doküman**dır. **Hiçbir Lean kodu değişikliği YAPILMADI**.

V2.1 commit `08ee17e` korunur — onarım sonrası entegre.

Mevcut feature branch `feature/drf-mekanize-ve-v3-metateorem` üzerinde herhangi bir değişiklik için **Mehmet onayı bekler**.

main'e dokunulmaz. Force-push yok.

---

## 15. Çapraz Referanslar

**KEMGU Belgeler:**
- [`KEMGU_DRF_Mekanize_Spec.md`](KEMGU_DRF_Mekanize_Spec.md) — Faz A/B/C planı (mevcut)
- [`KEMGU_Metateorem_V3.md`](KEMGU_Metateorem_V3.md) — V3 V1 bundled (overclaiming kabul ediliyor)
- [`KEMGU_Operasyonel_Semantik.md`](KEMGU_Operasyonel_Semantik.md) — Op.Sem §7 IyiTipli
- [`KEMGU_DRF_Lemmalar.md`](KEMGU_DRF_Lemmalar.md) — DRF-L0..L7
- [`KEMGU_Linear_Types_Spec_V1.md`](KEMGU_Linear_Types_Spec_V1.md) — Linear Types Spec V1
- [`KEMGU_Bellek_Modeli.md`](KEMGU_Bellek_Modeli.md) — Bölge sistemi

**Dış Kaynaklar:**
- **Pierce, B. C. (2002)** *Types and Programming Languages.* MIT Press. §8.3 (STLC), §15 (Subtyping).
- **Wright, A. K. & Felleisen, M. (1994)** "A Syntactic Approach to Type Soundness." *Information and Computation* 115(1): 38-94.
- **Harper, R. (2016)** *Practical Foundations for Programming Languages.* 2nd ed., Cambridge University Press. §4-7.
- **Felleisen, M. & Hieb, R. (1992)** "The Revised Report on the Syntactic Theories of Sequential Control and State." *Theoretical Computer Science* 103(2): 235-271.
- **Tofte, M. & Talpin, J.-P. (1997)** "Region-Based Memory Management." *Information and Computation* 132(2): 109-176.
- **Jung, R. et al. (2018)** "RustBelt: Securing the Foundations of the Rust Programming Language." *POPL 2018.*
- **Leroy, X. (2009)** "Formal Verification of a Realistic Compiler." *CACM* 52(7): 107-115.
- **Klein, G. et al. (2009)** "seL4: Formal Verification of an OS Kernel." *SOSP 2009.*

**Konferans Referansları (§11):**
- OOPSLA (ACM SIGPLAN), ICFP, CPP, ITP, PLATEAU workshop

---

## 16. Değişiklik Özeti (v1 → v2)

| Bölüm | v1 | v2 | Sebep |
|-------|----|----|-------|
| §3 | Monolitik typing judgment | Layered (HasType + LinearOK + RegionOK + Typed) | Modülerlik, aşamalı build-up |
| §4 | "Preconditions kaldır" | "Runtime guard reinterpretation" + dual constructors | Modeller circular yorumu uyarısı yanlış — preconditions zaten guard, eksik olan Fault alternatif |
| §5 (yeni) | Yoktu | ConfigTyped köprüsü merkezde | Discharge lemma typing'i state'e bağlamalı |
| §6 | 6 adım sıralı | 8 adım aşamalı + paralel imkân | Daha küçük artımlı checkpoint'ler |
| §7 | 6-10 hafta, ~1,400-1,950 satır | 17-19 hafta (~4-5 ay), ~3,570-4,120 satır | v1 underestimate; v2 ConfigTyped + Discharge ailesi gerçek tahmin |
| §8 | Capability + Sabitsure + Realtime dahil | Linear + Region only (V1 onarım dar) | "Focused contribution" — akademik savunulabilir |
| §10 (yeni) | Yoktu | V3 isim rezerve, ara isimler | Akademik dürüstlük — V3 4-conjunct olmadan kullanılmaz |
| §11 (yeni) | TOPLAS doğrudan | Konferans → Journal | Daha gerçekçi timeline; ek metodolojik paper imkanı |

---

**END KEMGU Mekanizasyon Çekirdek Onarım Planı v2 (2026-05-18)**
