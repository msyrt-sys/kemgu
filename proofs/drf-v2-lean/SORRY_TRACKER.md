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

---

## Aktif sorry listesi

(Adim 1.2'den itibaren dolacak. Her giris asagidaki sablonu izler:)

```
- [ ] proofs/drf-v2-lean/<dosya>:<satir> — <lemma_adi>
      Case: <fault constructor adi>
      Sebep: Pattern matching exhaustivity; fault case'i Discharge olmadan kapatamiyoruz
      Discharge: Adim 7 — <typing_excludes_*_fault lemma adi>
      Eklendi: YYYY-MM-DD (commit hash)
```

Henuz aktif sorry yok (Adim 1.1 build-clean).

---

## Asamali plan ve sorry beklentisi

| Adim | Hafta | Beklenen sorry hareketi | Toplam |
|------|-------|--------------------------|--------|
| 1.1 — FaultSebep + Konfigurasyon.fault | 1 | 0 | 0 |
| 1.2 — sAtama Tamam/Hata dual + L0/L4 case extension | 2 | +~4 | ~4 |
| 1.3 — Diger 6 Step constructor Tamam/Hata dual + L0-L7 case extension | 3 | +~12 | ~16 |
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
- [x] **C0 (bu commit):** Adim 1.1 — FaultSebep + Konfigurasyon.fault, build temiz, sorry: 0
- [ ] **C1:** Adim 1.3 sonu — Step constructor dual tamamlandi
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
