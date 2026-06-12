# KEMGU DRF Mekanize İspatları (Lean 4)

**Durum:** Faz A2 tamamlandı (2026-05-17). Lake projesi + operasyonel
semantik altyapısı (Core + SmallStep) lake build temiz, 0 sorry, 0 axiom.

**Spec dökümanı:** [`belgeler/KEMGU_DRF_Mekanize_Spec.md`](../../belgeler/KEMGU_DRF_Mekanize_Spec.md)

---

## Hızlı Başlangıç

### Kurulum (ilk sefer)

1. **elan** (Lean toolchain yöneticisi) kur:
   - Windows / MSYS2: kurulum komutu `curl -sSf https://raw.githubusercontent.com/leanprover/elan/master/elan-init.sh | sh -s -- -y`
   - PATH'e ekle: `$HOME/.elan/bin`
2. Bu dizinde:
   ```bash
   cd proofs/drf-v2-lean
   lake update            # mathlib4 v4.29.0 + dependencies (~30-60 dk ilk sefer)
   lake build             # ~1-3 dk artımlı
   ```

### Build doğrulama

```bash
# Bu dizinden:
lake build

# Veya proje kökünden:
mingw32-make calistir_drf_lean_proof
```

Beklenen çıktı:
```
✔ [N/M] Built Kemgu.Sem.Core
✔ [N/M] Built Kemgu.Sem.SmallStep
✔ [N/M] Built Kemgu
Build completed successfully (N jobs).
```

Exit kodu 0 = tüm tip kontrol + ispat terimi kontrol geçti.

---

## Proje Yapısı

```
proofs/drf-v2-lean/
├── lakefile.lean             Lake build betiği (mathlib4 require)
├── lean-toolchain            leanprover/lean4:v4.29.0 (pinned)
├── lake-manifest.json        Bağımlılık versiyonları (otomatik üretilen, commit edilir)
├── Kemgu.lean                Root modül — alt-modülleri import eder
├── Kemgu/
│   └── Sem/                   Operasyonel semantik altyapısı (Faz A2)
│       ├── Core.lean          Temel tipler (~270 satır)
│       └── SmallStep.lean     Küçük-adım reduksiyon (~190 satır)
└── (gelecek)
    └── Kemgu/
        ├── Drf/                Faz A3-A5: DRF lemmalar + Teorem 4'
        ├── MemSafety/          Faz B1: Memory Safety
        ├── SideChannel/        Faz B2: Side-Channel
        ├── BET/                Faz B3: BET
        └── Soundness/          Faz C: V3 metateorem
```

`.lake/` dizini (build artifakları + mathlib cache) **gitignore**'da; lake update
ile reproducible.

---

## Faz Durumu

| Faz | Tanım | Durum | Spec referansı |
|-----|-------|-------|----------------|
| A1  | Spec dökümanı | ✓ Tamam (2026-05-17) | `belgeler/KEMGU_DRF_Mekanize_Spec.md` |
| A2  | Lake projesi + Op.Sem (Core + SmallStep) | ✓ Tamam (2026-05-17) | Bu dizin |
| A3  | DRF-L0..L7 mekanize | ⏳ Bekler (Mehmet onayı) | Spec §3.1 satır 7-14 |
| A4  | Teorem 4' mekanize | ⏳ | Spec §3.1 satır 15 |
| A5  | Makefile + final commit | ⏳ | Spec §5.1 |
| B   | Memory Safety + Side-Channel + BET | ⏳ Faz A sonrası | Spec §3.2 |
| C   | V3 bütünleşik metateorem | ⏳ Faz B sonrası | Spec §3.3 |

---

## Politika

- **Türkçe DNA:** Yorumlar Türkçe, identifier'lar ASCII transliterasyon
  (`bolge`, `iyitipli`, `drf_l1_bolge_thread_tekilligi`).
- **No sorry, no axiom, no opaque:** Her teorem/lemma tam ispatlı; kâğıt
  ispatı (`belgeler/KEMGU_DRF_*.md`) source-of-truth.
- **Kaynak (kâğıt) belgeler:**
  - `belgeler/KEMGU_Operasyonel_Semantik.md` (Op.Sem §1-8)
  - `belgeler/KEMGU_DRF_Lemmalar.md` (DRF-L0..L7)
  - `belgeler/KEMGU_DRF_Teoremi.md` (Teorem 4')
  - `belgeler/KEMGU_Bellek_Modeli.md` (Memory Safety teoremleri)
  - `belgeler/KEMGU_Sabitsure_Spec_V1.md` (Side-Channel)
  - `belgeler/KEMGU_Realtime_Spec_V1.md` (BET)

---

## Bağımlılıklar (lake-manifest.json'da pinned)

- **mathlib** `v4.29.0` — Lean 4 matematik kütüphanesi
- **batteries** master — standart kütüphane uzantısı
- **aesop** master — otomatik ispat tactic
- **plausible, LeanSearchClient, importGraph, ProofWidgets4, Qq, Cli** — mathlib transitive bağımlılıkları

---

## C Test Paketi ile İlişki

Bu Lean ispat sistemi KEMGU C derleyici test paketinden **bağımsızdır**.
`mingw32-make test_tumu` (C tarafı, 505+ test) çalıştığında Lean kısmı
etkilenmez. Lean ispatı bozulursa C derleyici etkilenmez.

İki sistemi ayrı CI job olarak çalıştırmak önerilir:
- Job 1: C tarafı (`mingw32-make test_tumu`)
- Job 2: Lean tarafı (`mingw32-make calistir_drf_lean_proof`)
