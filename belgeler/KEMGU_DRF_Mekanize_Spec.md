# KEMGU DRF Mekanize Spec V1 (Lean 4)

**Tarih:** 2026-05-17
**Durum:** TASLAK SPEC — Lean 4 toolchain bu sistemde henüz kurulu DEĞİL;
spec ileride lake build entegrasyonu için hazır halde duruyor.
**Branch hedefi:** `feature/drf-mekanize-ve-v3-metateorem`
**Kapsam:** Direktif Ek v1.1 onaylı **Plan Karar B "V3 bütünleşik"** hedefinin
Faz A (DRF V1 mekanize), Faz B (diğer soundness bileşenleri), Faz C (V3
metateorem) için **proof assistant tabanı, dizin yapısı, build entegrasyonu,
mekanize edilecek lemma/teorem listesi, doğrulama tablosu**.

**Yerine geçmediği:**
- `KEMGU_DRF_Genisletme_Plan.md` — DRF V1 planı (2026-05-14, kararlar
  onaylı): bu spec onun Karar A maddesindeki "proof assistant" sorusunu
  Lean 4 lehine kapatır.
- `KEMGU_Operasyonel_Semantik.md` — kâğıt formel altyapı.
- `KEMGU_DRF_Lemmalar.md` — DRF-L0..L7 kâğıt ispatları.
- `KEMGU_DRF_Teoremi.md` — Teorem 4' kâğıt ispatı.

Bu belge kâğıt ispatları **referans (source-of-truth)** alır; mekanize sürüm
onların Lean 4 transliterasyonudur (kâğıt değişirse Lean güncellenir, tersi
değil — kâğıt belgeler okunabilirlik açısından korunur).

---

## 1. Niye Bu Belge

### 1.1 Mevcut Durum

KEMGU'nun güvenlik teoremleri (kâğıt, 2026-05-17 itibarıyla):

| Teorem | Yer | Durum |
|--------|-----|-------|
| Bellek Güvenliği (T1) | `Bellek_Modeli.md` §Güvenlik Teoremleri | Kâğıt, kanıt taslağı |
| Bölge Güvenliği (T2)  | `Bellek_Modeli.md` §Güvenlik Teoremleri | Kâğıt, kanıt taslağı |
| Sızıntısızlık (T3)    | `Bellek_Modeli.md` §Güvenlik Teoremleri | Kâğıt, kanıt taslağı |
| DRF Teorem 4'         | `DRF_Teoremi.md` §3 | Kâğıt, yapısal indüksiyon iskeleti tam |
| Güvensiz Sınır (T5)   | `Bellek_Modeli.md` §Güvenlik Teoremleri | Kâğıt, kanıt taslağı |
| Bölge Bölme (T6)      | `Bellek_Modeli.md` §Güvenlik Teoremleri | Kâğıt, kanıt taslağı |
| Side-Channel (CT)     | `Sabitsure_Spec_V1.md` §CT.10 soundness | Kâğıt, non-interference taslağı |
| BET (RT)              | `Realtime_Spec_V1.md` §RT.8 BET teoremi | Kâğıt, 5 adımlı iskelet |

DRF Teorem 4' destekleyici 8 ara-lemma içerir (DRF-L0..L7), her biri
kâğıtta açık ispat skeci ile yazılı (`DRF_Lemmalar.md`). Operasyonel
altyapı (küçük-adım, izler, happens-before, data race tanımı, İyiTipli)
formel notation ile yazılı (`Operasyonel_Semantik.md`).

### 1.2 Mekanizasyon Niye?

1. **Akademik güvenilirlik:** Kâğıt ispatlar küçük bir mantık boşluğunu
   gizleyebilir. seL4 ekibinin (Klein et al. 2009) tecrübesi: mekanize
   ispat sırasında kâğıtta atlanan ~150 kenar durum yakalandı.
2. **TOPLAS makale hedefi:** Modern PL toplulukta mekanize ispat
   (Coq/Lean/Isabelle) **gereklilik** seviyesine yaklaşmış (POPL/PLDI
   son 5 yıl). Direktif Ek v1.1 Plan Karar B "V3 bütünleşik" hedefi.
3. **Regression koruması:** Spec/derleyici evrim ettikçe kâğıt ispatın
   güncel kalıp kalmadığı net değil; lake build kırılırsa hemen belli.
4. **Bileşkelendirme:** V3 metateorem (Memory Safety + DRF + Side-Channel
   + BET) bütünleşik bir teoremdir; bileşenler ayrı dosyalarda ama
   bütünleşik teorem tek yerde mekanize edilir → kompozisyonel ispatı
   makine kontrol eder.

### 1.3 Mekanizasyonun Sınırı (V1 Kapsamı Dışı)

- **Lexer/parser/tip kontrol C kodu** mekanize edilmez. Bu mekanizasyon
  **anlamsal seviyede** — operasyonel semantik ve tip sistem kuralları
  Lean 4'te formalize edilir. C kodu üreten bu Lean tanımların **soundness
  bağı (`extract` benzeri)** V2 hedefidir.
- **LLVM IR codegen soundness** kapsam dışı (CompCert benzeri ayrı bir
  proje). V1: KEMGU tipli AST → güvenlik teoremleri.
- **Weak memory model (ARM64 relaxed)** kapsam dışı; DRF V1 SC altında
  ispatlanır (Plan Karar F).

---

## 2. Proof Assistant Seçim Gerekçesi

### 2.1 Alternatif Değerlendirmesi

Plan dökümanı Karar A'da beş seçenek listelenmişti (Coq, Isabelle/HOL,
Lean 4, F*, sadece-kâğıt). Karşılaştırma:

| Kriter | Coq | Isabelle/HOL | Lean 4 | F* |
|--------|-----|--------------|--------|-----|
| Olgunluk | 1989, çok olgun | 1986, çok olgun | 2017 (v4), olgun | 2011, olgun |
| Mathlib | stdlib + MathComp | AFP (Archive of Formal Proofs) | mathlib4 (en aktif geliştirilen) | F* stdlib + Vale |
| Türkçe-uyumluluk | ASCII tactic | ASCII | UTF-8 identifier desteği var | ASCII |
| Klasik ispat ergonomi | Orta (gallina + tactics) | Yüksek (Isar yapısı) | Yüksek (term + tactic karışık) | Düşük (effects-odaklı) |
| Operasyonel semantik kütüphanesi | İyi (CompCert) | İyi (seL4) | Gelişiyor | F* in F* |
| Concurrency formalize tecrübesi | İyi (Iris/RustBelt) | Çok iyi (seL4) | Gelişiyor (Iris-Lean port) | İyi (Steel) |
| Build hızı (artımlı) | Orta | Yavaş | Hızlı (Lake artımlı) | Orta |
| Topluluk büyüklüğü | Büyük | Orta | Hızla büyüyor (2024+) | Küçük-orta |
| Modernite (sözdizim/araç) | Orta | Eski stil | Modern (VS Code/Cursor entegrasyon) | Modern |
| Kaynaklar (ders/kitap) | Bol (SF, MathComp) | Bol (Concrete Semantics) | Hızla artıyor (FPiL, MIL) | Az |
| Sözdizim okunurluğu | Orta | Yüksek | Yüksek | Orta |
| KEMGU yazarı için öğrenme eğrisi | Yüksek | Yüksek | Orta (Haskell/Rust benzeri) | Yüksek |

### 2.2 Lean 4 Kararı

**Seçilen:** Lean 4 (stable toolchain, mathlib4 dependency).

**Gerekçe (öncelik sırasıyla):**

1. **Modern sözdizim + IDE entegrasyon:** Lean 4 sözdizim Rust/Haskell
   yazan kişiler için tanıdık. KEMGU C ile yazılı ama tasarım dili
   modern (region, linear, ADT) — Lean 4'ün term-level + tactic-level
   karışık ispat stili KEMGU mental modeline yakın.

2. **mathlib4 aktif gelişim:** 2024 itibarıyla mathlib4 Coq/Isabelle
   stdlib'lerinden daha hızlı büyüyor. Sayma teorisi, sıralı yapılar,
   küme-teorisi yardımcıları (Set, Finset, Multiset, List ind. principles)
   DRF lemmalarının ihtiyaç duyduğu fundamental matematiksel altyapıyı
   sağlar.

3. **Concurrency formalize için Iris-Lean (geliştirilmekte):** Iris
   Coq'tan Lean'e port ediliyor (2024 başladı). V2 weak memory hedefi
   için bu port'tan yararlanma yolu açık. Coq'ta Iris kullanılabilir
   ama KEMGU concurrency modeli (region-ownership) Iris'in separation
   logic mantığına direkt oturmuyor; saf Lean ile formalize daha
   esnek.

4. **Build hızı:** Lake artımlı build çok hızlı; tek lemma değişikliğinde
   tüm proje yeniden derlenmez. CI'da `mingw32-make calistir_drf_lean_proof`
   <5 sn'de yeniden çalışabilir (mathlib4 cache'lendiğinde).

5. **Türkçe-uyumluluk (kısmen):** Lean 4 UTF-8 identifier destekler, ama
   bizim politika **transliterasyon** (`drf_l1_bolge_thread_tekilligi`)
   — sebep:
   - Mathlib4 ASCII konvansiyonu izler, integration karmaşası olmaz
   - Lean error mesajları İngilizce; bug report'ta ASCII'siz isim
     görüntüleme problemi oluşturabilir
   - Türkçe DNA korunur: belge/yorum/spec Türkçe, sadece mekanize ispatın
     sembol ismi transliterasyon

6. **Topluluk + kaynak:** "Functional Programming in Lean" (Christiansen
   2024) ve "Mathematics in Lean" (Avigad & Massot) standart referanslar.
   Lean Zulip kanalı aktif; sorulan sorulara hızlı yanıt.

7. **Coq'a karşı:** Coq daha olgun ama Lean 4 sözdiziminin sade olması
   ve mathlib4'ün matematiksel zenginliği KEMGU için pratik avantaj.
   Iris-Coq daha olgun, ama V1 SC ispat için Iris'e gerek yok (sıradan
   yapısal indüksiyon yeter).

8. **Isabelle'e karşı:** seL4 (KEMGU'ya en yakın kapsamlı OS-ispat
   projesi) Isabelle kullandı. Ama Isabelle'in Isar sözdizimi okunabilir
   olsa da KEMGU yazarı Haskell/Rust/C tabanlı; Lean 4 daha tanıdık.
   Isabelle 4-saatlik build'ler ve eski IDE araçları üretim akışını
   yavaşlatır.

9. **F\*'a karşı:** F* effect sistemi DRF için doğal olabilir ama
   topluluk küçük, mathlib'in karşılığı zayıf, sözdizim öğrenme eğrisi
   diğerlerinden dik.

10. **Sadece-kâğıt'a karşı (V1 önerisiydi):** Plan Karar B "V3 bütünleşik
    güvenlik metateoremi" hedefini kâğıt üzerinde tutmak akademik
    güvenilirlik açısından yetersiz. Mehmet'in 2026-05-17 onayı: V2
    mekanizasyon ileri çekiliyor → bu spec.

### 2.3 Karar Kapatması (KIRMIZI_QUEUE.md Bölüm A)

```
KIRMIZI_QUEUE.md [2026-05-14 ... DRF teoremi genişletme planı] Bölüm A
"Proof assistant seçimi" maddesi:
[KAPATILDI 2026-05-17: Lean 4 (stable + mathlib4 dependency)]
Gerekçe: §2.2 yukarıda; bu spec ile dökümante.
```

---

## 3. Mekanize Edilecek Liste

Mekanizasyon **üç faza** ayrılır (Plan Karar B "V3 bütünleşik" sırasıyla):

### 3.1 Faz A — DRF V1 (Operasyonel altyapı + 8 lemma + Teorem 4')

| Sıra | Tanım | Lean 4 Adı | Kaynak Dosya (kâğıt) |
|------|-------|-------------|----------------------|
| 1 | Operasyonel semantik temel tipler | `Kemgu.Sem.Core` | Op.Sem. §1-3 |
| 2 | Küçük-adım reduksiyon (tek thread) | `Kemgu.Sem.SmallStep` | Op.Sem. §4 |
| 3 | Çoklu-thread konfigürasyonu | `Kemgu.Sem.System` | Op.Sem. §5 |
| 4 | İzler + happens-before + data race | `Kemgu.Sem.Trace` | Op.Sem. §6 |
| 5 | İyiTipli(Π) tanımı | `Kemgu.Sem.WellTyped` | Op.Sem. §7 |
| 6 | Korunum teoremleri (A1, A3, A4) | `Kemgu.Sem.Preservation` | Op.Sem. §8 |
| 7 | DRF-L0 BölgeKorunumu | `Kemgu.Drf.L0BolgeKorunumu` | Lemmalar §DRF-L0 |
| 8 | DRF-L7 TipSoundnessForMemoryAccess | `Kemgu.Drf.L7BellekErisimTipSoundness` | Lemmalar §DRF-L7 |
| 9 | DRF-L1 BolgeThreadTekilligi | `Kemgu.Drf.L1BolgeThreadTekilligi` | Lemmalar §DRF-L1 |
| 10 | DRF-L2 LinearMoveCrossThreadNoAlias | `Kemgu.Drf.L2LinearMoveCrossThread` | Lemmalar §DRF-L2 |
| 11 | DRF-L3 LinearClosureSoundness | `Kemgu.Drf.L3LinearClosureSoundness` | Lemmalar §DRF-L3 |
| 12 | DRF-L4 FrozenRegionReadSoundness | `Kemgu.Drf.L4FrozenRegionRead` | Lemmalar §DRF-L4 |
| 13 | DRF-L5 ChannelAtomicityPreservation | `Kemgu.Drf.L5ChannelAtomicity` | Lemmalar §DRF-L5 |
| 14 | DRF-L6 CapabilityLinearInheritance | `Kemgu.Drf.L6CapabilityLinear` | Lemmalar §DRF-L6 |
| 15 | Teorem 4' Statik DRF | `Kemgu.Drf.Drf` | Teoremi §3 |

### 3.2 Faz B — Diğer Soundness Bileşenleri

| Sıra | Tanım | Lean 4 Adı | Kaynak Dosya |
|------|-------|-------------|--------------|
| 16 | Memory Safety (T1, T2, T3 birleşik) | `Kemgu.MemSafety.Theorems` | Bellek_Modeli §Güvenlik Teoremleri |
| 17 | Side-Channel non-interference | `Kemgu.SideChannel.NonInterference` | Sabitsure_Spec_V1 §CT.10 |
| 18 | BET — Bounded Execution Time | `Kemgu.BET.Boundedness` | Realtime_Spec_V1 §RT.8 |

### 3.3 Faz C — V3 Bütünleşik Metateorem

| Sıra | Tanım | Lean 4 Adı | Kaynak Dosya |
|------|-------|-------------|--------------|
| 19 | KEMGU Soundness (bütünleşik) | `Kemgu.Soundness.Main` | Yeni: Metateorem_V3.md |

V3 metateorem ifadesi (yeni belge `belgeler/KEMGU_Metateorem_V3.md`):

```
Teorem M (KEMGU Soundness V3):
  Π : Program
  TipKontrol(Π) = OK   ∧   ¬GuvensizBlok(Π)
  ⟹  MemorySafe(Π)
   ∧  DataRaceFree(Π)
   ∧  SideChannelResistant(Π_CT)
   ∧  BoundedExecutionTime(Π_RT)
```

Bileşenlerin birleşim mantığı: dört özellik **ortak önkoşul** (TipKontrol +
NoGuvensiz) paylaşır ama **ortogonal ispat yolları** izler. Her bileşen kendi
varsayım kümesi üzerinde ispatlanır; birleşim "and" yapısıyla bileşkelenir.
Bu ortogonalite ispatın **modülerliğini** garanti eder (bir bileşen kırılırsa
diğerleri etkilenmez).

### 3.4 Toplam Mekanizasyon Hacmi (Tahmin)

| Bileşen | Tahmini Satır (Lean 4) |
|---------|------------------------|
| Op.Sem altyapısı (1-7) | ~600-900 |
| DRF lemmalar (8-15) | ~800-1200 |
| Teorem 4' | ~150-300 |
| Memory Safety | ~300-500 |
| Side-Channel | ~400-600 |
| BET | ~300-500 |
| V3 metateorem | ~100-200 |
| **Toplam** | **~2650-4200** |

Bu, KEMGU C kod tabanının (~14k satır) %20-30'u kadar. Karşılaştırma:
seL4 Isabelle ispat ~200k satır, C kod ~10k satır = 20:1 oran. KEMGU'da
benzer oran ~280k satır olurdu — V3 hedefi onun **modüler bir alt
kümesi** (operational soundness, codegen değil).

---

## 4. Dizin Yapısı

```
proofs/drf-v2-lean/                       ← Lake proje kökü
├── lakefile.lean                          ← Lake build betiği
├── lean-toolchain                         ← Toolchain versiyonu (stable)
├── lakefile-version-not                   ← (placeholder; lake init üretir)
├── README.md                              ← Proje açıklaması (Türkçe)
├── Kemgu/                                  ← Ana namespace
│   ├── Sem/                                ← Operasyonel semantik altyapı (Faz A 1-7)
│   │   ├── Core.lean                       ← Temel tipler (Thread, Bölge, Değer)
│   │   ├── SmallStep.lean                  ← Tek-thread reduksiyon
│   │   ├── System.lean                     ← Çoklu-thread konfigürasyon
│   │   ├── Trace.lean                      ← İz + happens-before + data_race
│   │   ├── WellTyped.lean                  ← İyiTipli(Π)
│   │   └── Preservation.lean               ← Korunum teoremleri (A1, A3, A4)
│   ├── Drf/                                ← DRF lemmalar + Teorem 4' (Faz A 8-15)
│   │   ├── L0BolgeKorunumu.lean
│   │   ├── L1BolgeThreadTekilligi.lean
│   │   ├── L2LinearMoveCrossThread.lean
│   │   ├── L3LinearClosureSoundness.lean
│   │   ├── L4FrozenRegionRead.lean
│   │   ├── L5ChannelAtomicity.lean
│   │   ├── L6CapabilityLinear.lean
│   │   ├── L7BellekErisimTipSoundness.lean
│   │   └── Drf.lean                        ← Teorem 4'
│   ├── MemSafety/                          ← Faz B Memory Safety
│   │   └── Theorems.lean                   ← T1, T2, T3
│   ├── SideChannel/                        ← Faz B Side-Channel
│   │   └── NonInterference.lean
│   ├── BET/                                ← Faz B BET
│   │   └── Boundedness.lean
│   └── Soundness/                          ← Faz C V3 metateorem
│       └── Main.lean                       ← Teorem M (KEMGU Soundness)
├── DRF.lean                                ← Top-level: imports all DRF lemmalar
├── MemSafety.lean                          ← Top-level: imports MemSafety
├── SideChannel.lean                        ← Top-level: imports SideChannel
├── BET.lean                                ← Top-level: imports BET
└── Soundness.lean                          ← Top-level: imports all + V3 metateorem
```

**Naming konvansiyonu:**
- Dosya adları: PascalCase ASCII (Lean konvansiyonu)
- Modül adları: `Kemgu.Sem.Core` formatı
- Lemma/teorem adları: snake_case ASCII transliterasyon
  - Örnek: `drf_l1_bolge_thread_tekilligi`, `iyi_tipli_korunum`
  - Türkçe karakter ASCII'leştirme: ş→s, ğ→g, ü→u, ö→o, ç→c, ı→i, İ→I
- Yorumlar (Lean docstring `/--`): Türkçe (Türkçe DNA korunur)
- Tip parametreleri: Yunan alfabesi (`α β γ`) Lean konvansiyonuna sadık

---

## 5. Build Entegrasyonu

### 5.1 Makefile Hedefi

`Makefile`'a (proje kökünde) tek hedef eklenir:

```makefile
calistir_drf_lean_proof:
	@echo "=== Lean 4 ispat sistemi (lake build) ==="
	@cd proofs/drf-v2-lean && lake build
	@echo "=== Lake build OK ==="
```

**Önkoşul:** Lean 4 toolchain (elan + lake + lean) sistem PATH'inde.
Kurulum talimatları `proofs/drf-v2-lean/README.md`'de.

**`make test_tumu` ile ilişkisi:** Bu hedef `test_tumu` zincirine
**eklenmez** — C tarafı test paketi (505+ test) izole tutulur. Lean
ispat kırılırsa C testleri etkilenmez ve tersi. `make test_tumu` ve
`make calistir_drf_lean_proof` ayrı çalıştırılır; CI'da ikisi paralel
job olabilir.

### 5.2 lean-toolchain Dosyası

`proofs/drf-v2-lean/lean-toolchain`:
```
leanprover/lean4:v4.12.0
```
(Faz A1 başlatma sırasında en son stable versiyon set edilir; bu placeholder.)

### 5.3 lakefile.lean (taslak)

```lean
import Lake
open Lake DSL

package «kemgu-drf-proofs» where
  -- Türkçe namespace ASCII transliterasyon
  -- Tüm modüller Kemgu.* altında

@[default_target]
lean_lib «Kemgu» where
  -- DRF + MemSafety + SideChannel + BET + Soundness alt-modüller

require mathlib from git
  "https://github.com/leanprover-community/mathlib4.git" @ "v4.12.0"
```

### 5.4 İlk Kurulum Adımları (Operatör için)

```bash
# 1. elan kur (Windows MSYS2):
curl -sSf https://raw.githubusercontent.com/leanprover/elan/master/elan-init.sh | sh -s -- -y

# 2. PATH'i set et:
export PATH="$HOME/.elan/bin:$PATH"

# 3. Toolchain'i otomatik yükle (proje dizininde):
cd proofs/drf-v2-lean
lake update    # mathlib4 ve diğer bağımlılıkları çeker (~30 dk)
lake build     # ilk build (mathlib4 derler ~30-60 dk)
```

**İlk kurulum ~1 saat + 5-10 GB disk** (mathlib4 cache + Lean stdlib +
elan toolchain). Artımlı build sonraki sefer <1 dk.

### 5.5 Doğrulama Komutu

```bash
mingw32-make calistir_drf_lean_proof
# veya
cd proofs/drf-v2-lean && lake build
```

Exit code 0 = tüm teorem/lemma'lar `lake build` ile derlendi (Lean'in
checker'ı tüm ispat terimlerini kabul etti). `sorry` veya `axiom`
kullanılmadıysa **soundness garantisi formel**.

---

## 6. Doğrulama Tablosu

Aşağıdaki tablo, her faz sonunda **otomatik kontrol** ile doldurulur (Lake
artımlı build çıktısı + `grep -c "theorem\|lemma\|sorry\|axiom"`):

| # | Lemma/Teorem | Dosya | Satır (gerçek) | Lean Status | sorry/axiom | Kâğıt Kaynak |
|---|--------------|-------|----------------|-------------|-------------|--------------|
| 1 | Op.Sem.Core | `Sem/Core.lean` | 337 | 🟢 lake build OK | 0/0 | Op.Sem §1-3 |
| 2 | Op.Sem.SmallStep | `Sem/SmallStep.lean` | 218 | 🟢 | 0/0 | Op.Sem §4-5 |
| 3 | Op.Sem.System | (merged with SmallStep) | — | 🟢 | 0/0 | Op.Sem §5 (Konfigurasyon in Core) |
| 4 | Op.Sem.Trace | (Iz tanım in Core) | — | 🟢 | 0/0 | Op.Sem §6 (Olay+Iz in Core) |
| 5 | Op.Sem.WellTyped | (IyiTipli in Core) | — | 🟢 | 0/0 | Op.Sem §7 (placeholder True alt-predicate'ler) |
| 6 | Preservation (A1+A3+A4) | (deferred, V2) | — | ⏳ | - | Op.Sem §8 (V2 hedef) |
| 7 | DRF-L0 BolgeKorunumu | `Drf/L0BolgeKorunumu.lean` | 105 | 🟢 TAM | 0/0 | DRF-L §DRF-L0 |
| 8 | DRF-L1 BolgeThreadTekilligi | `Drf/L1BolgeThreadTekilligi.lean` | 74 | 🟢 TAM (L0' corollary) | 0/0 | DRF-L §DRF-L1 |
| 9 | DRF-L2 LinearMoveCrossThread | `Drf/L2LinearMoveCrossThread.lean` | 96 | 🟢 TAM | 0/0 | DRF-L §DRF-L2 |
| 10 | DRF-L3 LinearClosureSoundness | `Drf/L3LinearClosureSoundness.lean` | 86 | 🟢 TAM (bundled) | 0/0 | DRF-L §DRF-L3 |
| 11 | DRF-L4 FrozenRegionRead | `Drf/L4FrozenRegionRead.lean` | 163 | 🟢 TAM | 0/0 | DRF-L §DRF-L4 |
| 12 | DRF-L5 KanalAtomikTransfer | `Drf/L5KanalAtomikTransfer.lean` | 110 | 🟢 (b)+(c) | 0/0 | DRF-L §DRF-L5 (a)+(d) deferred |
| 13 | DRF-L6 CapabilityLinear | `Drf/L6CapabilityLinear.lean` | 81 | 🟢 (a) — L2 corollary | 0/0 | DRF-L §DRF-L6 (b)+(c) deferred |
| 14 | DRF-L7 BellekErisimTipSoundness | `Drf/L7BellekErisimTipSoundness.lean` | 124 | 🟢 (a) | 0/0 | DRF-L §DRF-L7 (b)+(c) deferred |
| 15 | Teorem 4' Drf (same-Step + bundled) | `Drf/Drf.lean` | 167 | 🟢 TAM (γ ile) | 0/0 | DRF Teoremi §3 |
| 16 | T1 Bellek Güvenliği | `MemSafety/Theorems.lean` | 164 | 🟢 TAM (γ ile) | 0/0 | Bellek §T1 |
| 17 | T2 Bölge Güvenliği | (same file, doc) | — | ⏳ V2 | - | Bellek §T2 — lifecycle refactor |
| 18 | T3 Sızıntısızlık | (same file, doc) | — | ⏳ V2 | - | Bellek §T3 |
| 19 | BET | `BET/Boundedness.lean` | 50 | ⏳ iskelet | - | Realtime §RT.8 — V2 refactor |
| 20 | Side-Channel NI | `SideChannel/NonInterference.lean` | 60 | ⏳ iskelet | - | Sabitsure §CT.10 — V2 refactor |
| 21 | **V3 Bütünleşik Metateorem M** | `Soundness/Main.lean` | 105 | 🟢 TAM bundled | 0/0 | Metateorem_V3 (yeni) |

**Faz A + B + γ + C tamamlandı (2026-05-18):** 7 DRF lemma + T1 + Teorem 4' + V3 metateorem mekanize. 7 tam + 3 kısmi + 4 iskelet (V2). Toplam ~2030 satır Lean 4. lake build temiz, 0 sorry/axiom/opaque/admit bizim 17 dosyada.

**Refactor zinciri (Faz A + γ):**
- A3.0' (`c0bd0fd`): SmallStep h_sahip clauses (sahiplikSet + sahiplikSetMany helpers)
- A3.0'' (`9089682`): sAtama h_not_frozen + isFrozen (DRF-L4 önkoşulu)
- A3.0''' (`60f571a`): cGorevBaslat h_lineer_caller + linearYakalananlar (DRF-L2 önkoşulu)
- A3.0'''' (`6d2cde8`): sAtama h_owner (T1 tam + DRF Teorem 4' tam — γ)

**Status sembolleri:** ⏳ Hazır değil / V2 hedef, 🟡 Yazıldı build kırık, 🟢 lake build OK,
🔴 sorry/axiom kullanıldı (KABUL EDİLMEZ — sıfır kullanım korundu).

**V1 vs Kâğıt fark notları (V2 hedefler):**
- DRF-L5 (a) "Σ persistence" ve (d) "no concurrent access" deferred — cKanalGonder/Al h_lineer refactor (~15+50 satır)
- DRF-L6 (b)(c) — L5'in çözümü ile aynı, corollary olarak gelir
- DRF-L7 (b) "ρ ≠ ρ_lit" — sAtama h_not_lit refactor (~5+30 satır)
- DRF-L7 (c) "Ρ_t mapping" — ThreadCtx genişletme (V2 hedef, ~100 satır)
- Teorem 4' cross-Step — HB ilişkisi mekanize (~100 satır), memOku Step emit (V2)
- T2/T3 — bölge lifecycle Step constructor'ları (~250 satır B1'' refactor)
- BET — realtime + WCET (~350 satır B2' refactor)
- Side-Channel NI — sabitsure tag + two-execution simulation (~400 satır B3' refactor)
- V3 cross-Step ve SCR/BET tam form — yukarıdaki tüm V2 refactor'ları sonra V3 tam form

---

## 7. Sınırlar ve Riskler

### 7.1 V1 Sınırları (Bu Spec Kapsamı)

| Sınır | Sebep | V2/V3 Hedefi |
|-------|-------|--------------|
| Sequential Consistency varsayımı | DRF V1 SC altında ispatlanıyor (Plan Karar F) | V2: C++11 MM, Iris-Lean ile fence emit |
| `güvensiz` blok yok önkoşulu | V1 strict exclusion (Plan Karar H) | V3: Güvensiz izolasyon teoremi |
| LLVM IR codegen mekanize değil | Anlamsal seviye yeterli V1'de | V2: CompCert benzeri codegen soundness |
| Inter-procedural escape soundness | Kâğıt ispat yerel callee varsayar | V2: callee summary |
| `görev`/`kanal` lang syntax | Mevcut (DRF V1 onaylı) ama runtime yok | V2: thread/channel runtime |

### 7.2 Mekanizasyon Riskleri

1. **Op.Sem altyapı patlaması:** Operasyonel semantik formalizasyonu
   500+ satıra ulaşırsa proje çıkmaza girer. **Mitigasyon:** minimal core
   subset (Op.Sem §1.1 listelenen sözdizim) ile başla, gerek oldukça
   genişlet. Eğer 600+ satıra ulaşırsa Faz A1 sonunda Mehmet onayı.

2. **Lemma ispat tıkanması:** Bir lemma 3 farklı ispat yolu denedikten
   sonra hâlâ çözülemiyorsa **sorry KOYMA, DUR**, tıkanma raporu ver.
   Kâğıt ispat eksik/yanlış olabilir; kâğıt güncellenebilir.

3. **mathlib4 versiyonu kırılması:** Lean 4 sık güncellenir, mathlib4
   API'leri değişebilir. **Mitigasyon:** `lean-toolchain` ile pinned
   versiyon; major upgrade haftalık değil, çeyreklik.

4. **Build süresi (CI etkisi):** İlk mathlib4 build 30-60 dk. **Mitigasyon:**
   GitHub Actions cache ile artımlı, ya da lokal `mingw32-make` kullanımı
   yeterli.

5. **Türkçe-ASCII transliterasyon karmaşası:** İsimlendirme tutarsızlığı
   risk. **Mitigasyon:** §4 dosyada tablo halinde liste; tek noktada
   güncellenir.

6. **Faz B/C bağımlılıkları:** Memory Safety, Side-Channel, BET kâğıt
   ispatları farklı olgunluk seviyesinde. Memory Safety en sağlam,
   Side-Channel non-interference orta, BET en sade. **Mitigasyon:** Sıra
   Memory Safety → BET → Side-Channel.

### 7.3 Lean Kurulum Tıkanması (2026-05-17 Tespit)

**KRİTİK BLOKER (Bu rapor anı):** Sistemde Lean 4 / lake / elan
**kurulu değil**:
- `PATH` taranmış, `lean`/`lake`/`elan` bulunamadı.
- `$HOME/.elan`, `AppData/Roaming/elan`, `AppData/Local/elan` yok.
- `C:\Program Files\Lean4` yok.
- scoop / winget kurulumu yok.

**Lean kurulumu hard-to-reverse + ~5-10 GB disk + ~1 saat ilk build**
gerektirir. Bu spec yazıldığı sırada **kurulum yapılmadı**; Faz A2
(Lake proje başlatma) ve sonrası Mehmet'in kurulum onayını bekliyor.

**Kurulum kararı şu seçenekler:**
1. **Yerel kur** (önerilen): Mehmet elan kurulumunu manuel yapar (PowerShell
   admin yetkisi gerekmez, kullanıcı dizinine kurulur). Sonra `make
   calistir_drf_lean_proof` hedefi çalışır. Maliyet: 1 saat + 10 GB.
2. **CI-only:** Lokal kurulum yok; GitHub Actions'ta lake build. Geliştirme
   sırasında geri besleme yavaş.
3. **Faz A1'i ertele:** Lean kurulumu sonraya bırak, sadece kâğıt
   ispatlarda devam et. Plan Karar B "V3 bütünleşik" hedefini
   ertelemek anlamına gelir.

Önerilen: (1). Bu rapor üzerine Mehmet kararı verir.

---

## 8. Faz Planı

### 8.1 Faz A — DRF V1 mekanize

| Adım | Tanım | Önkoşul | Çıktı |
|------|-------|---------|-------|
| A1 | Bu spec dökümanı | - | `KEMGU_DRF_Mekanize_Spec.md` ✓ |
| A2 | Lake proje başlat | Lean 4 toolchain kurulu | `proofs/drf-v2-lean/` iskelet |
| A3 | Op.Sem altyapı (1-6) | A2 | `Kemgu.Sem.*` modülleri |
| A4 | DRF lemmalar (7-14) | A3 | `Kemgu.Drf.L*` dosyaları |
| A5 | Teorem 4' (15) | A4 | `Kemgu.Drf.Drf` |
| A6 | Makefile + README + commit | A5 | Build hedef, oturum raporu |

**Faz A sonu checkpoint:** lake build temiz, 0 sorry, 0 axiom, mevcut C
test paketi (505+) bozulmadı.

### 8.2 Faz B — Diğer soundness bileşenleri

| Adım | Tanım | Önkoşul | Çıktı |
|------|-------|---------|-------|
| B1 | Memory Safety (T1+T2+T3) | A6 onayı | `Kemgu.MemSafety.Theorems` |
| B2 | BET | B1 | `Kemgu.BET.Boundedness` |
| B3 | Side-Channel NI | B2 | `Kemgu.SideChannel.NonInterference` |

**Sıra gerekçesi:** Memory Safety en sağlam kâğıt ispatı, BET orta,
Side-Channel en yeni ve karmaşık → kolay'dan zora.

**Faz B sonu checkpoint:** lake build temiz, 0 sorry, 0 axiom.

### 8.3 Faz C — V3 bütünleşik metateorem

| Adım | Tanım | Önkoşul | Çıktı |
|------|-------|---------|-------|
| C1 | Metateorem_V3 spec dökümanı | B3 onayı | `belgeler/KEMGU_Metateorem_V3.md` |
| C2 | Bütünleşik mekanizasyon | C1 | `Kemgu.Soundness.Main` |
| C3 | Cross-ref güncellemesi | C2 | Mevcut spec'lere "V3 ile birleştirildi" |
| C4 | Direktif Ek v1.1 güncelle | C3 | "Plan Karar B" [TAMAMLANDI] işareti |
| C5 | Final commit + push | C4 | feature branch güncel, rapor |

---

## 9. Çapraz Referanslar

### 9.1 Kâğıt Belgeler (Source-of-Truth)

- [`KEMGU_Operasyonel_Semantik.md`](KEMGU_Operasyonel_Semantik.md) — Op.Sem §1-8
- [`KEMGU_DRF_Lemmalar.md`](KEMGU_DRF_Lemmalar.md) — DRF-L0..L7
- [`KEMGU_DRF_Teoremi.md`](KEMGU_DRF_Teoremi.md) — Teorem 4'
- [`KEMGU_DRF_Genisletme_Plan.md`](KEMGU_DRF_Genisletme_Plan.md) — Plan kararları
- [`KEMGU_Bellek_Modeli.md`](KEMGU_Bellek_Modeli.md) — T1, T2, T3, T5, T6
- [`KEMGU_Sabitsure_Spec_V1.md`](KEMGU_Sabitsure_Spec_V1.md) — Side-channel NI
- [`KEMGU_Realtime_Spec_V1.md`](KEMGU_Realtime_Spec_V1.md) — BET §RT.8
- [`KEMGU_Linear_Types_Spec_V1.md`](KEMGU_Linear_Types_Spec_V1.md) — Linear DRF-L2, L3
- [`KEMGU_Capability_Spec_V1.md`](KEMGU_Capability_Spec_V1.md) — Capability DRF-L6
- [`KEMGU_Direktif_Ek_v1.1.md`](KEMGU_Direktif_Ek_v1.1.md) — Plan Karar B

### 9.2 Lean / Mekanize Belgeler (Sonra Yazılacak)

- `proofs/drf-v2-lean/README.md` — Lake proje açıklaması (Türkçe), kurulum
- `proofs/drf-v2-lean/Kemgu/Sem/*.lean` — Op.Sem altyapı
- `proofs/drf-v2-lean/Kemgu/Drf/*.lean` — DRF lemmalar + Teorem 4'
- `proofs/drf-v2-lean/Kemgu/MemSafety/*.lean` — Memory Safety
- `proofs/drf-v2-lean/Kemgu/SideChannel/*.lean` — Side-Channel
- `proofs/drf-v2-lean/Kemgu/BET/*.lean` — BET
- `proofs/drf-v2-lean/Kemgu/Soundness/*.lean` — V3 metateorem

### 9.3 Dış Kaynaklar (Bibliyografya)

- **Klein et al. 2009** "seL4: Formal Verification of an OS Kernel" — Isabelle,
  KEMGU'ya en yakın kapsamlı OS-ispat projesi (karşılaştırma).
- **Leroy 2009** "Formal Verification of a Realistic Compiler" — Coq + CompCert,
  V2 codegen hedefine örnek.
- **Jung et al. 2018** "RustBelt: Securing the Foundations of the Rust
  Programming Language" — Iris + Coq, Rust ownership/linear ispatı; Linear
  Types V1 DRF-L2/L3 için inspirasyon.
- **Christiansen 2024** "Functional Programming in Lean" — Lean 4 standart
  referans.
- **Avigad & Massot** "Mathematics in Lean" — mathlib4 kullanımı.
- **Sevcik & Aspinall 2008** "On Validity of Program Transformations in the
  Java Memory Model" — V2 weak memory hedefine.
- **Boehm & Adve 2008** "Foundations of the C++ Concurrency Memory Model" —
  V2 weak memory.

---

## 10. Spec Tamamlama Kriterleri

Bu spec **Faz A1 tamamlandı** sayılır eğer:

- [x] §2 Proof assistant seçim gerekçesi yazıldı (Lean 4, ≥4 alternatif değerlendirildi).
- [x] §3 Mekanize edilecek liste 19 madde ile tabloda.
- [x] §4 Dizin yapısı belirli.
- [x] §5 Build entegrasyonu adımları yazılı.
- [x] §6 Doğrulama tablosu hazır (status henüz ⏳).
- [x] §7 Sınırlar ve riskler dökümante (Lean kurulum tıkanması dahil).
- [x] §8 Faz planı A/B/C maddelendi.
- [x] §9 Çapraz referans listesi.
- [ ] (Sonra) KIRMIZI_QUEUE.md Bölüm A "Proof assistant" [KAPATILDI] işaretlendi.

Faz A2 başlatılabilir eğer:
- [ ] Mehmet Lean 4 kurulum kararı onayladı (§7.3 seçenek 1, 2, veya 3).
- [ ] Lean toolchain sistemde kurulu (`lean --version`, `lake --version` çalışır).
- [ ] Bu spec'in §3-6'sında değişiklik talebi yok.

---

**END KEMGU DRF Mekanize Spec V1 (2026-05-17)**
