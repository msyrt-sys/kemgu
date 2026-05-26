# SORRY_TRACKER — Plan v2 Onarim Gecici Borc Takibi

**Branch:** `feature/drf-onarim-v2-WIP` (yalnizca bu branch'te `sorry` kabul)
**Politika:** Her `sorry` icin `-- TODO: Adim N'de discharge_X ile dolacak` yorumu zorunlu.
**Hedef:** Adim 7 (Discharge Lemma ailesi + No-Fault catı teoremi) tamamlandiginda **0 sorry**.
**Asla:** `main` veya `feature/drf-mekanize-ve-v3-metateorem` branchine `sorry` merge edilmez.

Kaynak plan: [`belgeler/KEMGU_Mekanize_Onarim_Plan.md`](../../belgeler/KEMGU_Mekanize_Onarim_Plan.md) (commit `da4d10f`).
Karar A onayi: 2026-05-18 oturumu (Mehmet).

---

## Haftalik durum

| Hafta | Tarih | Adim | sorry sayisi | Notlar |
|-------|-------|------|--------------|--------|
| 1 | 2026-05-18 | 1.1 | 0 | FaultSebep enum + Konfigurasyon.fault eklendi; constructor refactor henuz yok |
| 1 | 2026-05-18 | 1.2 | 10 | sAtama Tamam/Hata dual; 5 yer (L4 + L7 + Drf + MemSafety T1 + MemSafety T1') gecici sorry; L2 wrapper trivial |
| 1 | 2026-05-22 | 1.3 | +25 (=35 toplam) | C1 checkpoint: kalan 6 Step constructor dual (cGorevBaslat/cGorevBirlestir/cKanalGonder/cKanalAl/cDondur/sLinKullan/sLinImha); 5 yer x 5 yeni Hata case = 25 sorry; L2 trivial bypass |

---

## Aktif sorry listesi

Sablon (her giris):
```
- [ ] <dosya>:<satir> — <lemma_adi>
      Case: <fault constructor adi>
      Sebep: Pattern matching exhaustivity; fault case'i Discharge olmadan kapatamiyoruz
      Discharge: Adim 7 — <typing_excludes_*_fault lemma adi>
      Eklendi: YYYY-MM-DD (commit hash)
```

**Adim 1.2 sonrasi: 10 aktif sorry** (kategori: sAtamaHata* exhaustivity)

### DRF-L4 (FrozenRegionRead)

- [ ] `proofs/drf-v2-lean/Kemgu/Drf/L4FrozenRegionRead.lean` — `drf_l4_a_step`
      Case: `sAtamaHataDonmus`
      Sebep: Pattern matching exhaustivity; sAtama dual'a refactor edildi, fault constructor case'i Discharge olmadan kapatamiyoruz
      Discharge: Adim 7 — `typing_excludes_sAtamaHataDonmus`
      Eklendi: 2026-05-18 (Adim 1.2 commit)

- [ ] `proofs/drf-v2-lean/Kemgu/Drf/L4FrozenRegionRead.lean` — `drf_l4_a_step`
      Case: `sAtamaHataSahipDegil`
      Discharge: Adim 7 — `typing_excludes_sAtamaHataSahipDegil`
      Eklendi: 2026-05-18 (Adim 1.2 commit)

### DRF-L7 (BellekErisimTipSoundness)

- [ ] `proofs/drf-v2-lean/Kemgu/Drf/L7BellekErisimTipSoundness.lean` — `drf_l7_a_step`
      Case: `sAtamaHataDonmus`
      Discharge: Adim 7 — `typing_excludes_sAtamaHataDonmus`
      Eklendi: 2026-05-18 (Adim 1.2 commit)

- [ ] `proofs/drf-v2-lean/Kemgu/Drf/L7BellekErisimTipSoundness.lean` — `drf_l7_a_step`
      Case: `sAtamaHataSahipDegil`
      Discharge: Adim 7 — `typing_excludes_sAtamaHataSahipDegil`
      Eklendi: 2026-05-18 (Adim 1.2 commit)

### Drf (Teorem 4' Same-Step)

- [ ] `proofs/drf-v2-lean/Kemgu/Drf/Drf.lean` — `kemgu_drf_v1_no_concurrent_writes`
      Case: `sAtamaHataDonmus`
      Discharge: Adim 7 — `typing_excludes_sAtamaHataDonmus`
      Eklendi: 2026-05-18 (Adim 1.2 commit)

- [ ] `proofs/drf-v2-lean/Kemgu/Drf/Drf.lean` — `kemgu_drf_v1_no_concurrent_writes`
      Case: `sAtamaHataSahipDegil`
      Discharge: Adim 7 — `typing_excludes_sAtamaHataSahipDegil`
      Eklendi: 2026-05-18 (Adim 1.2 commit)

### MemSafety T1 (Bellek Guvenligi tam form)

- [ ] `proofs/drf-v2-lean/Kemgu/MemSafety/Theorems.lean` — `t1_bellek_guvenligi_tam`
      Case: `sAtamaHataDonmus`
      Discharge: Adim 7 — `typing_excludes_sAtamaHataDonmus`
      Eklendi: 2026-05-18 (Adim 1.2 commit)

- [ ] `proofs/drf-v2-lean/Kemgu/MemSafety/Theorems.lean` — `t1_bellek_guvenligi_tam`
      Case: `sAtamaHataSahipDegil`
      Discharge: Adim 7 — `typing_excludes_sAtamaHataSahipDegil`
      Eklendi: 2026-05-18 (Adim 1.2 commit)

### MemSafety T1' (Corollary full)

- [ ] `proofs/drf-v2-lean/Kemgu/MemSafety/Theorems.lean` — `t1_bellek_guvenligi_corollary_full`
      Case: `sAtamaHataDonmus`
      Discharge: Adim 7 — `typing_excludes_sAtamaHataDonmus`
      Eklendi: 2026-05-18 (Adim 1.2 commit)

- [ ] `proofs/drf-v2-lean/Kemgu/MemSafety/Theorems.lean` — `t1_bellek_guvenligi_corollary_full`
      Case: `sAtamaHataSahipDegil`
      Discharge: Adim 7 — `typing_excludes_sAtamaHataSahipDegil`
      Eklendi: 2026-05-18 (Adim 1.2 commit)

### Trivial bypass (sorry GEREKMIYOR)

L2 wrapper'i (`drf_l2_step_uygulama_ornegi`) sonuc tipi `True`; her case `trivial` ile geciliyor. Yeni `sAtamaHataDonmus`/`sAtamaHataSahipDegil` ve Adim 1.3'teki 5 yeni Hata case (cGorevBaslatHataLineerIhlal/cKanalGonderHataLineerTuket/cDondurHataZatenDonmus/sLinKullanHataZatenTuketildi/sLinImhaHataZatenTuketildi) de `trivial` ile kapatildi.

---

## Adim 1.3 yeni sorry'leri (C1 checkpoint, 2026-05-22)

### DRF-L4 (FrozenRegionRead)

- [ ] `proofs/drf-v2-lean/Kemgu/Drf/L4FrozenRegionRead.lean` — `drf_l4_a_step`
      Case: `sLinKullanHataZatenTuketildi`
      Discharge: Adim 7 — `typing_excludes_sLinKullanHataZatenTuketildi`
      Eklendi: 2026-05-22 (Adim 1.3 commit)

- [ ] `proofs/drf-v2-lean/Kemgu/Drf/L4FrozenRegionRead.lean` — `drf_l4_a_step`
      Case: `sLinImhaHataZatenTuketildi`
      Discharge: Adim 7 — `typing_excludes_sLinImhaHataZatenTuketildi`
      Eklendi: 2026-05-22 (Adim 1.3 commit)

- [ ] `proofs/drf-v2-lean/Kemgu/Drf/L4FrozenRegionRead.lean` — `drf_l4_a_step`
      Case: `cDondurHataZatenDonmus`
      Discharge: Adim 7 — `typing_excludes_cDondurHataZatenDonmus`
      Eklendi: 2026-05-22 (Adim 1.3 commit)

- [ ] `proofs/drf-v2-lean/Kemgu/Drf/L4FrozenRegionRead.lean` — `drf_l4_a_step`
      Case: `cGorevBaslatHataLineerIhlal`
      Discharge: Adim 7 — `typing_excludes_cGorevBaslatHataLineerIhlal`
      Eklendi: 2026-05-22 (Adim 1.3 commit)

- [ ] `proofs/drf-v2-lean/Kemgu/Drf/L4FrozenRegionRead.lean` — `drf_l4_a_step`
      Case: `cKanalGonderHataLineerTuket`
      Discharge: Adim 7 — `typing_excludes_cKanalGonderHataLineerTuket`
      Eklendi: 2026-05-22 (Adim 1.3 commit)

### DRF-L7 (BellekErisimTipSoundness)

- [ ] `proofs/drf-v2-lean/Kemgu/Drf/L7BellekErisimTipSoundness.lean` — `drf_l7_a_step`
      Case: `sLinKullanHataZatenTuketildi` / `sLinImhaHataZatenTuketildi` /
            `cDondurHataZatenDonmus` / `cGorevBaslatHataLineerIhlal` /
            `cKanalGonderHataLineerTuket` (5 case, hepsi ayni discharge ailesi)
      Discharge: Adim 7 — sirayla `typing_excludes_*`
      Eklendi: 2026-05-22 (Adim 1.3 commit)
      (5 entry; her biri ayri TODO yorumlu sorry dosyada)

### Drf (Teorem 4' Same-Step)

- [ ] `proofs/drf-v2-lean/Kemgu/Drf/Drf.lean` — `kemgu_drf_v1_no_concurrent_writes`
      Case: 5 yeni Hata constructor (yukaridakilerle ayni)
      Discharge: Adim 7 — sirayla `typing_excludes_*`
      Eklendi: 2026-05-22 (Adim 1.3 commit)
      (5 entry tek lemma'da)

### MemSafety T1 (Bellek Guvenligi tam form)

- [ ] `proofs/drf-v2-lean/Kemgu/MemSafety/Theorems.lean` — `t1_bellek_guvenligi_tam`
      Case: 5 yeni Hata constructor (yukaridakilerle ayni)
      Discharge: Adim 7 — sirayla `typing_excludes_*`
      Eklendi: 2026-05-22 (Adim 1.3 commit)
      (5 entry tek lemma'da)

### MemSafety T1' (Corollary full)

- [ ] `proofs/drf-v2-lean/Kemgu/MemSafety/Theorems.lean` — `t1_bellek_guvenligi_corollary_full`
      Case: 5 yeni Hata constructor (yukaridakilerle ayni)
      Discharge: Adim 7 — sirayla `typing_excludes_*`
      Eklendi: 2026-05-22 (Adim 1.3 commit)
      (5 entry tek lemma'da)

**Adim 1.3 yeni sorry: 25** (5 yer × 5 case). Tek satir notasyon kullanildi (her yerde 5 ayri sorry'nin ayri entry'sini yazmak yerine; sayim build dogrulamasinda grep ile yapilir).

**Toplam C1 sonu (Adim 1.2 + 1.3): 10 + 25 = 35 sorry.**

---

## Asamali plan ve sorry beklentisi

| Adim | Hafta | Beklenen sorry hareketi | Toplam |
|------|-------|--------------------------|--------|
| 1.1 — FaultSebep + Konfigurasyon.fault | 1 | 0 | 0 |
| 1.2 — sAtama Tamam/Hata dual (5 dosya × 2 case, L2 trivial) | 1-2 | +10 (revize, eski tahmin +4 hatalı) | 10 |
| 1.3 — Kalan 6 Step constructor Tamam/Hata dual + L2 trivial bypass | 2-3 | +25 (gerçek: 5 yeni Hata × 5 cases yerinde, L2 trivial) | **35 (C1)** |
| 2 — ConfigTyped iskeleti (5 alt-yapi) | 4-6 | sabit | ~16 |
| 3 — Minimal HasType (klasik) | 7-8 | sabit | ~16 |
| 4 — Progress + Preservation (HasType) | 8-10 | sabit | ~16 |
| 5 — LinearOK katmani + Progress/Preservation update | 10-12 | sabit | ~16 |
| 6 — RegionOK katmani + Progress/Preservation update | 12-14 | sabit | ~16 |
| 7 — Discharge ailesi (Aile 1-4) + No-Fault catı | 14-18 | DUSER (her discharge lemma birkac sorry'yi exfalso ile siler) | DUSEN |
| 8 — L0-L7 + T1 + Drf adapt | 19 | 0 | **0 (hedef)** |

---

## Kontrol noktalari (CHECKPOINT)

Her checkpoint'te:
1. `lake build` temiz (uyari kabul, hata yasak)
2. Mevcut sorry sayisi bu dokumana isleneicek
3. Yeni eklenen her sorry icin TODO yorumu var (grep `-- TODO: Adim` ile dogrula)
4. Mehmet review

CHECKPOINT listesi (Plan §7.5):
- [x] **C0:** Adim 1.1 — FaultSebep + Konfigurasyon.fault, build temiz, sorry: 0
- [x] **C1 (bu commit):** Adim 1.3 sonu — tum 8 Step constructor dual (sAtama/cGorevBaslat/cGorevBirlestir/cKanalGonder/cKanalAl/cDondur/sLinKullan/sLinImha), sorry: 35 geçici
- [ ] **C2:** Adim 2 sonu — ConfigTyped 5 alt-yapi
- [ ] **C3:** Adim 4 sonu — HasType Progress/Preservation (klasik)
- [ ] **C4:** Adim 6 sonu — HasType + LinearOK + RegionOK
- [ ] **C5:** Adim 7 sonu — Discharge lemma ailesi + No-Fault catı
- [ ] **C6:** Adim 8 sonu — L0-L7 + T1 + Drf adapt — sorry: 0, MERGE'e hazir

---

## Merge politikasi

Bu branch (`feature/drf-onarim-v2-WIP`) **sorry icerebilir**.
`feature/drf-mekanize-ve-v3-metateorem` branchine merge **yalnizca C6'da, 0 sorry ile**.
Force-push **asla**.
