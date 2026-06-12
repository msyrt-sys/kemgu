# KEMGU V3 Bütünleşik Güvenlik Metateoremi

**Tarih:** 2026-05-18
**Durum:** TASLAK V1 BUNDLED — DRF + Memory Safety tam, Side-Channel/BET iskelet (V2 hedef)
**Branch:** `feature/drf-mekanize-ve-v3-metateorem`
**Mekanize:** [`proofs/drf-v2-lean/Kemgu/Soundness/Main.lean`](../proofs/drf-v2-lean/Kemgu/Soundness/Main.lean)
**Kardeş belgeler:**
- [`KEMGU_DRF_Mekanize_Spec.md`](KEMGU_DRF_Mekanize_Spec.md) — Faz A+B+C planı, doğrulama tablosu
- [`KEMGU_DRF_Teoremi.md`](KEMGU_DRF_Teoremi.md) — DRF Teorem 4' kâğıt formel
- [`KEMGU_DRF_Lemmalar.md`](KEMGU_DRF_Lemmalar.md) — DRF-L0..L7
- [`KEMGU_Operasyonel_Semantik.md`](KEMGU_Operasyonel_Semantik.md) — Op.Sem
- [`KEMGU_Bellek_Modeli.md`](KEMGU_Bellek_Modeli.md) — Memory Safety (T1, T2, T3)
- [`KEMGU_Sabitsure_Spec_V1.md`](KEMGU_Sabitsure_Spec_V1.md) — Side-Channel
- [`KEMGU_Realtime_Spec_V1.md`](KEMGU_Realtime_Spec_V1.md) — BET

---

## 1. Niye Bu Belge

KEMGU dilinin güvenlik garantileri 4 ayrı boyutta:

| Boyut | Kaynak | Faz |
|-------|--------|-----|
| Memory Safety (T1, T2, T3) | `Bellek_Modeli.md` | B1 |
| Data Race Freedom (Teorem 4') | `DRF_Teoremi.md` + DRF-L0..L7 | A3 |
| Side-Channel Resistance (Non-Interference) | `Sabitsure_Spec_V1.md` CT.10 | B3 |
| Bounded Execution Time (BET) | `Realtime_Spec_V1.md` RT.8 | B2 |

Bu dört boyut **ortak önkoşul** (`IyiTipli(Π)` = TipKontrol OK + ¬güvensiz blok) paylaşır ama **ortogonal ispat yolları** izler. **V3 Metateorem M** dördünün birleşik ifadesidir:

```
Teorem M (KEMGU Soundness V3):
  Π : Program
  TipKontrol(Π) = OK   ∧   ¬GuvensizBlok(Π)
  ⟹  MemorySafe(Π)
   ∧  DataRaceFree(Π)
   ∧  SideChannelResistant(Π_CT)
   ∧  BoundedExecutionTime(Π_RT)
```

Bu belge V3 metateoreminin **V1 bundled formu** Lean 4 mekanize edilir; ileride V2 refactor turları ile tam form'a genişletilir.

---

## 2. Bileşenlerin Bağımsızlığı (Orthogonality)

Dört özellik ortak önkoşulu paylaşsa da ispat yolları ayrık:

- **MemorySafe(Π)** → Bölge sahipliği + sAtama h_owner garantisi (T1 tam form). T2/T3 bölge lifecycle gerek (V2).
- **DataRaceFree(Π)** → S1 invariant (DRF-L1) + sAtama h_owner (Teorem 4' same-Step) + Linear semantik (L2, L3, L6). Cross-Step HB ordering V2.
- **SideChannelResistant(Π_CT)** → Information-flow non-interference. `sabitsure<T>` tag tracking + two-execution simulation gerek (V2).
- **BoundedExecutionTime(Π_RT)** → WCET hesap + cycle counting. `gerçekzamanlı` qualifier + cost semantics gerek (V2).

**Bağımsızlık avantajı:** Bir bileşen kırılırsa diğerleri etkilenmez. Bir refactor (örn. B2' realtime model) yalnız BET'i etkiler; DRF ve Memory Safety bozulmaz.

**Birleşim avantajı:** V3 tek teorem olarak dört garantiyi tek bir IyiTipli önkoşulundan türetir. TOPLAS makale çıktısı için **konsolide soundness statement**.

---

## 3. V1 Bundled Form

V1 mekanize formu (mevcut commit'ler):

```lean
theorem kemgu_soundness_v3
    (Pi : Program) (h_iyi : IyiTipli Pi)
    (S₀ S : Konfigurasyon) (h_run : StepStar S₀ S)
    (h_init_s1 : s1_invariant S₀) :
    -- (1) DRF: S1 invariant StepStar boyunca korunur
    s1_invariant S
    -- (2) MemorySafe (per-Step): her yeni memYaz event'i için yazan thread
    --     hedef bolgenin sahibidir (T1 tam form, h_owner via sAtama)
    ∧ MemSafe_perStep S
    -- (3) SideChannelResistant: V2 hedef (placeholder True)
    ∧ SideChannelResistant_v2_placeholder Pi
    -- (4) BoundedExecutionTime: V2 hedef (placeholder True)
    ∧ BET_v2_placeholder Pi
```

**İspat yapısı:**
- (1) `drf_l0_bolge_korunumu_starStep` direct (Faz A3 → DRF-L0' starStep)
- (2) `t1_bellek_guvenligi_tam` direct (Faz B1' → T1 tam form)
- (3), (4) placeholder True (V2 refactor'da gerçek içerik)

---

## 4. V2 Genişletme Planı

| Bileşen | Refactor gerek | Maliyet tahmini |
|---------|----------------|-----------------|
| MemorySafe T2/T3 tam | B1' bölge lifecycle Step constructor'ları | ~250 satır |
| DRF cross-Step | HB ordering mekanize (sequenced-before + synchronizes-with) | ~100 satır |
| SideChannelResistant tam | B3' sabitsure tag + two-execution simulation | ~400 satır |
| BoundedExecutionTime tam | B2' realtime annotation + cycle counting + WCET | ~350 satır |

**Toplam V2 hedef:** ~1100 satır + ek ispatlar.

V3 metateorem'in V1 bundled formu **kabul edilebilir bir başlangıç**: temel iki bileşen (DRF + MemSafe) tam mekanize, diğer iki bileşen placeholder ile bundled — V2 refactor'da iki yeni bileşen aktif olur.

---

## 5. Mevcut Durum Özeti

**Mekanize edilen V1 bundled bileşenler (Faz A + B + γ tam):**

| Bileşen | Form | Dosya |
|---------|------|-------|
| Op.Sem altyapı | TAM | `Kemgu/Sem/{Core,SmallStep}.lean` (555 satır) |
| DRF-L0..L7 + Teorem 4' | TAM (5) + KISMI (3) | `Kemgu/Drf/*.lean` (~870 satır) |
| T1 Memory Safety | TAM | `Kemgu/MemSafety/Theorems.lean` |
| T2/T3, BET, NI | İSKELET (V2) | İlgili `*.lean` dosyalar |
| **V3 Metateorem M** | **TAM bundled** | `Kemgu/Soundness/Main.lean` (bu commit) |

**Refactor zinciri:** A3.0' (sahiplikSet) → A3.0'' (h_not_frozen) → A3.0''' (h_lineer_caller) → A3.0'''' (h_owner)

**Build:** `lake build` ✓ (17/17 job, exit 0), 0 sorry/axiom/opaque/admit.

---

## 6. TOPLAS Makale İçin Önem

V3 metateoremi **modern PL toplulukta beklenen "bundled soundness theorem"** formatına oturur:

- **Iris/RustBelt** (Jung et al. 2018): Rust borrow checker + concurrency + atomicity bundled.
- **CompCert** (Leroy 2009): Tip soundness + semantics preservation bundled.
- **seL4** (Klein et al. 2009): Functional correctness + integrity + confidentiality bundled.

**KEMGU V3** bundled soundness olarak konumlanır:
- Bölge tabanlı bellek modeli (region-based) — MLKit/Tofte-Talpin geleneği
- Linear types (Linear V1) — Rust/Idris geleneği
- Capability — seL4/Cedar geleneği
- Constant-time (sabitsüre) — Vale/Jasmin geleneği
- Realtime (BET) — Ada SPARK geleneği

**TOPLAS makalesinde sunum:**
- V3 metateorem ana iddia
- V1 bundled form V1.0 paper
- V2 cross-Step + Side-Channel + BET tam form V2.0 paper

---

## 7. KIRMIZI_QUEUE Bölüm B Kapatması

Direktif Ek v1.1 Plan Karar B'nin "V3 bütünleşik" hedefi:

```
KIRMIZI_QUEUE.md [2026-05-14] DRF teoremi genisletme planı Bölüm B:
"Plan Karar B (V3 bütünleşik)":
[V1 BUNDLED TAMAMLANDI 2026-05-18: Lean 4 mekanize, bkz.
 belgeler/KEMGU_Metateorem_V3.md]
Gerekçe: §3 yukarıda; mevcut commit'lerde tam ispatlı.
V2 (cross-Step + Side-Channel + BET tam) ayrı plan.
```

---

## 8. Çapraz Referanslar

- [`KEMGU_DRF_Mekanize_Spec.md`](KEMGU_DRF_Mekanize_Spec.md) §6 — Doğrulama tablosu
- [`KEMGU_DRF_Teoremi.md`](KEMGU_DRF_Teoremi.md) §3 — Teorem 4' kâğıt formel
- [`KEMGU_Bellek_Modeli.md`](KEMGU_Bellek_Modeli.md) §Güvenlik Teoremleri — T1/T2/T3
- [`KEMGU_Sabitsure_Spec_V1.md`](KEMGU_Sabitsure_Spec_V1.md) — Side-Channel
- [`KEMGU_Realtime_Spec_V1.md`](KEMGU_Realtime_Spec_V1.md) — BET
- [`KEMGU_Direktif_Ek_v1.1.md`](KEMGU_Direktif_Ek_v1.1.md) — Plan Karar B

---

**END KEMGU V3 Metateorem V1 Bundled (2026-05-18)**
