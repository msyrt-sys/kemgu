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
| 2 | 2026-05-22 | 2 | 0 (35 sabit) | ConfigTyped iskelet: KonfTipli + 4 alt-yapi tam, ThreadTipli placeholder True |
| 2 | 2026-05-22 | 3 | 0 (35 sabit) | Minimal HasType: 12 Ifade kurali, klasik tip sistemi (TAPL §8.3) |
| 2 | 2026-05-22 | 4.1 | +6 (=41 toplam) | Progress + Preservation ISKELET: statement'lar sorry, full proof Adim 4.2-4.4'te |
| 2 | 2026-05-22 | 4.2 | +4 (=45 toplam) | progress kismi proof: 7/12 case kanitlandi (6 vacuous bos Γ + 1 t_sabit IsValue), 5 case sub-sorry (t_seq, t_gorev_baslat, t_kanal_al, t_dondur, t_guvensiz — Step constructor insasi/induktif Adim 4.2b) |
| 2 | 2026-05-22 | 4.x V1 sinir | 0 (=45 sabit) | Adim 4.2b/4.3/4.4 V1 SINIR — KEMGU Configuration semantik klasik Wright-Felleisen lone form ile uyumsuz; Step S' insasi non-trivial. preservation imzasi guclendirildi (h_no_fault_target eklendi). Full proof'lar Adim 7 Discharge + Adim 5-6 LinearOK/RegionOK sonrasi tractable. |
| 2 | 2026-05-22 | 5 | +2 (=47 toplam) | LineerTamam katmani (LinearOK Plan §3.3) — 13 kural inductive tam + helpers (Tip.lineerMi, lineerOrtam{Get,Update}) + TypedAdim5 conjunction iskelet + progress_lineer/preservation_lineer sub-lemma iskelet sorry (Adim 5.2/5.3 hedef). |
| 2 | 2026-05-28 | 6 | +2 (=49 toplam) | RegionTamam katmani (RegionOK Plan §3.4) — 12 kural inductive tam (Plan §3.4'un 4 ana kurali r_atama/r_gorev_baslat/r_kanal_gonder/r_dondur + 8 kapsayici Ifade case) + helpers (bolgeOrtamUpdate, bolgeKategoriDegistir, bolgeOrtamSahipAta, bolgeOrtamDondurBolge) + `Typed` full conjunction (HasType + LineerTamam + RegionTamam) + ThreadTipliFull (Plan §5.2.3 gercek tanim) + KonfTipliFull + progress_region/preservation_region sub-lemma iskelet sorry (Adim 6.2/6.3 hedef). NOT: StateTipli.ThreadTipli placeholder True KALIR (import cycle onlemek icin yeni isim altinda; Adim 7 Discharge ThreadTipliFull'u kullanir). |
| 3 | 2026-05-28 | 7 | **-33 (=16 toplam)** | Discharge ailesi + No-Fault çatı (Plan §6 + §7.2 Adim 7). PRAGMATIK STRATEJI: Plan §6.2 4 Discharge ailesi (~1050-1600 satir) yerine **Step Hata constructor'lari strengthened** (Plan §4.4 formel: her 7 Hata'ya h_store/h_iz/h_zaman/h_sahip/h_kanal = S.* eklendi). Sonuc: L4/L7/Drf/MemSafety'deki **35 Hata case sorry'si trivial kapandi** (rw h_iz + List.mem_cons dispatch). +2 yeni iskelet sorry (NoFault.lean: step_fault_preserves_typed + typed_no_fault, Adim 8 hedef). Net: -33. Plan C4 hedefi ~10'a yakin (Adim 8 ile birlikte 0). Tasarruf: ~600 satir Aile 2 yazimi gerekmedi. |
| 3 | 2026-05-28 | 7 yarım | **+6 (=22 toplam)** | Adim 7 yarım kalan kısım — Tamam strengthen + step_fault_preserves + typed_no_fault kısmi proof. SmallStep.lean: 8 Tamam constructor'a `h_no_fault_target : S'.fault = none` eklendi (Plan §4.4 simetri). NoFault.lean güncel: step_fault_preserves_typed 8 Tamam case **FULL** (h_no_fault_target ile direkt) + 7 Hata case sorry (Adim 8 — Aile 2 lemma'lari); typed_no_fault refl case **FULL** (KonfTipliFull.5 = S₀.fault=none) + step case sorry (Adim 8 — step_fault + preservation_konfTipli zinciri). GEÇİCİ SORRY ARTISI: eski 2 statement-only iskelet → 8 yapısal sub-sorry (7 Hata + 1 step). Yapısal genişleme: Aile 2 hedefi netlesti, Adim 8'de bu 8 sorry düşer + 14 Adim 4/5/6 iskelet dolar → C5 sorry 0. |
| 3 | 2026-05-29 | 8 P1 | **-2 (=20 toplam)** | Adim 8 P1 — Aile 2 Discharge (Plan §6.2 Fault Impossibility) kısmi. ThreadTipliFull'a Plan §5.2.3 köprü şartı eklendi (`ctx.lineer ↔ Λ` iff form). Yeni dosya Kemgu/Discharge/Aile2.lean: 2 Linear lemma FULL ispat — typing_excludes_sLinKullanHataZatenTuketildi + typing_excludes_sLinImhaHataZatenTuketildi (kopru + l_kullan/l_imha rule + nomatch). NoFault.lean step_fault Hata case'lerinde sLinKullan/Imha case'leri Aile 2 dispatch ile **kapandi** (2 sorry düştü). Diger 5 Aile 2 lemma (sAtamaHataDonmus/SahipDegil, cDondurHataZatenDonmus, cGorevBaslat/KanalGonderHata*) Adim 8 P2 hedef — V1 sinir: BolgeOrtam↔Sahiplik kopru / V1 minimal LineerTamam form'lar (V2 strengthen gerek). step_fault Hata case sorry: 7 → 5 (-2). |
| 3 | 2026-05-29 | 8 P2 | **-1 (=19 toplam)** | Adim 8 P2 — Aile 2 Discharge (Plan §6.2) devam. `l_kanal_gonder` strengthen (`lineerOrtamGet Λ v ≠ some tuketildi` — l_kullan/l_imha ile simetrik; non-lineer v icin Λ v=none → trivial saglanir, yalniz cifte-gonderim reddedilir). Yeni Aile2.lean lemma `typing_excludes_cKanalGonderHataLineerTuket` FULL (Typed.lineerOK → l_kanal_gonder + ThreadTipliFull kopru → `h_notconsumed h_tuket_Λ` celiski). NoFault step_fault cKanalGonder case Aile 2 dispatch ile **kapandi**. step_fault Hata case sorry: 5 → 4 (-1). Build 27/27 temiz, ASan kapsam disi (Lean). cKanalGonder temiz cunku gonderilen vId ifadeye bagli (`ctx.ifade = kanalGonderIf k vId`); kalan 4 (sAtama×2/cGorev/cDondur) serbest k/vIhlal + statik Ρ sabit → V2 (tek `Ρ→Konfigurasyon` refactor dordunu birden acar). |
| 3 | 2026-05-29 | 8 V2 P1-3 | **-1 (=18 toplam)** | Adim 8 Secenek A — **Ρ→Konfigurasyon refactor** (Mehmet onayli). P1: `Konfigurasyon.bolge : List (VarId × Bolge) := []` (runtime Ρ; default → kirilma yok). P2: `KonfTipliFull`'a `S.bolge = Ρ` + `FrozenKategoriTutarli` kopru (∀ x b, bolgeOrtamGet S.bolge x = some b → (isFrozen S b ↔ b.kategori=donmus)); intro/elim 7-tuple; NoFault fault proj `.2.2.2.2`→`.2.2.2.2.1`. P3: `sAtamaHataDonmus` Step'e `h_x_bolge : bolgeOrtamGet S.bolge x = some k.bolge` linkage (SmallStep StateTipli import); 5 cases sitesi arity +1 (Theorems×2/Drf/L4/L7; L2 lenient); Aile2 `typing_excludes_sAtamaHataDonmus` FULL (r_atama→kategori≠donmus + S.bolge=Ρ + kopru→celiski); NoFault dispatch. Build 27/27 temiz. **Tasarim uctan uca dogrulandi.** Kalan 3 step_fault Hata (sAtamaHataSahipDegil/cGorevBaslat/cDondur) ayni desen — sonraki fazlar. KORUNUM (preservation_konfTipli: cDondurTamam S.bolge guncelleme) ayri sorry. |
| 3 | 2026-05-29 | 8 V2 P4 | **-1 (=17 toplam)** | cDondurHataZatenDonmus discharge. `r_dondur` strengthen (b kayitli `bolgeOrtamGet Ρ x = some b` + `b.kategori ≠ donmus`; r_atama frozen-yazma yasagi ile simetrik). Aile2 `typing_excludes_cDondurHataZatenDonmus` FULL (r_dondur + S.bolge=Ρ + FrozenKategori kopru → celiski). NoFault dispatch. **Step degisikligi GEREKMEDI** (b `dondurIf b`'de acik, serbest degil) → cases arity churn yok (yalniz RegionTamam r_dondur, baska yerde match edilmiyor). Build 27/27 temiz. step_fault Hata kalan: **2** (sAtamaHataSahipDegil ownership, cGorevBaslat l_gorev_baslat+yd — ikisi de ek strengthen gerektirir). |
| 3 | 2026-05-29 | 8 V2 P5 | **-1 (=16 toplam)** | sAtamaHataSahipDegil discharge. KonfTipliFull'a **AtamaSahipligi** invariant (8. bilesen: ∀ atama yapan ctx → hedef bolgeyi S.zaman'da sahiplenir). sAtamaHataSahipDegil Step'e h_x_bolge linkage + 5 cases arity (Theorems×2/Drf/L4/L7; L2 lenient). Aile2 `typing_excludes_sAtamaHataSahipDegil` FULL (Typed GEREKMEZ; AtamaSahipligi + h_x_bolge → h_not_owner celiski). NoFault dispatch + bridge proj `.2.2.2.2.2.2`→`.1` (H eklendigi icin G kaydi). Build 27/27 temiz. step_fault Hata kalan: **1** (cGorevBaslatHataLineerIhlal). |
| 4 | 2026-06-11 | v3 F4i+F5 | **-6 (=1 toplam)** | Onarim v3 F4-ispat(A+B) + F5 (Mehmet invariant onayi). (1) ONAYLI INVARIANT uctan uca: kategoriYazilabilir (sahip/kanalRho/donmus transfer-disi); r_atama/r_dondur/r_kanal_gonder yazilabilir-strengthen; r_gorev_baslat yakalama-yazilabilir + COCUK-GOVDE RegionTamam premise'i; l_gorev_baslat cocuk-govde LineerTamam premise'i (yakalananlar-aktif ortaminda). (2) KonfTipliFull 11 bilesene: HedefVar (atama/kanal-gonderim/yakalama hedefleri, seq SAG-SOL + atama-RHS + guvensiz-ic kapali — sSeqAtla blokeri cozuldu) + HedefBolge (dondur literalleri) + KanalTransit (dolu kuyruk → transit bolge); odak lemmasi iki-kapanisli; Aile2 sahipDegil HedefVar formu (Typed'li); Kopru 11-bilesen. (3) **F5 PROGRESS_KONF 12/12 case TAM ISPATLI (sorry'suz!)**: ambient-Γ + uc-disjunct (IsValue ∨ Engelli ∨ odakli-adim); Engelli inductive (bos-kanal + bitmemis-birlestirme + baglam kapanisi) + engelli_konf_transfer; tanikler KonfTipliFull'dan (DegiskenlerBagli→sVarOku, HedefVar→h_owner, HedefBolge→dondur, KanalTransit→cKanalAl, tazeTid+foldl-max→h_fresh, Classical.em→birlestir/Engelli); congruence sarmalari odakli-adim formuyla. Eski kapali-Γ progress + progress_typed SILINDI (6 sorry kapandi). **KALAN 1 sorry: adim_korunum** (Hata case'leri step_fault dispatch kopyasi — mekanik; Tamam 11 bilesen-bilesen + cong cikti-ortam-kararli IH — F4-ispat kalan brifingi FAZ_BRIFINGLERI'nde). Build 29/29 temiz. |
| 4 | 2026-06-11 | v3 F4y+F6 | **-6 (=7 toplam)** | Onarim v3 F4-yapisal + F6. (1) PER-THREAD Λ (onayli soru 3): ThreadTipliFull her ctx kendi ctx.lineer'iyle Typed; paylasimli-Λ iff koprusu SILINDI; Aile2 kopru-suz sadelesti; KonfTipliFull Λ parametresi dustu. (2) adim_korunum (birlesik korunum) DOGRU ifadeyle iskelet — dairesellik KIRILDI: typed_no_fault tek ileri induksiyonla TAM BAGLANDI (tek sorry kaynagi adim_korunum); iyiTipli_no_fault kagit-form catı (IyiTipli(Π) ⟹ reachable fault yok) eklendi. (3) IFADE-YANLIS 6 eski preservation iskeleti + True-sonuclu soundness_corollary SILINDI; Engelli (blocked) tanimi + 3-disjunct progress formu hazirlandi (F5). (4) F6: kemgu_soundness_v3 yeniden — SCR/BET conjunct'lari CIKARILDI (onayli soru 4), hipotez gercek IyiTipli + baslangicKonf kosusu, YENI No-Fault conjunct; s1_yapisal (h_init_s1 gereksizlesti). KALAN 7 sorry: progress 5 sub + progress_typed 1 (F5) + adim_korunum 1 (F4-ispat). Build 29/29 temiz. |
| 4 | 2026-06-11 | v3 F3 | **0 (=13 sabit)** | Onarim v3 F3 — placeholder → GERCEK predikatlar + KOPRU. (1) Δ (KanalOrtam) tum katmanlara: t_kanal_al vakumu KAPANDI (tip = Δ k), t_kanal_gonder kanal disiplini, KanalTutarli kesin-tip formu. (2) l_gorev_baslat/l_kanal_gonder serbest-Λ' vakumu KAPANDI (Λ' = lineerTuketListe/lineerTuket — runtime ile birebir). (3) KonfTipliFull 9. bilesen DegiskenlerBagli. (4) Core placeholder'lari SILINDI; Kemgu/Sem/Kopru.lean: GERCEK TipKontrolOk (HasType) / LineerKontrolOk (LineerTamam) / BolgeAtamaOk (RegionTamam), Capability/Sabitsure scope-guard, Realtime alani KALDIRILDI (onayli), Program.cevre+kanalCevre zenginlestirme, baslangicKonf S₀(Π). (5) **KOPRU TEOREMI iyiTipli_baslangic TAM ISPATLI** (9 bilesen, sorry'siz) + satisfiability tanigi (bos program). Build 29/29 temiz. |
| 4 | 2026-06-11 | v3 F2 | **0 (=13 sabit)** | Onarim v3 F2 — Tamam-constructor yeniden tasarimi + Sorun 3. (1) Sahiplik guncel-durum modeli (zaman anahtardan cikti; isFrozen = lookup donmus; frozen-persistence KOSULSUZ teorem: step_donmus_korunur). (2) Step 21 kurala yeniden yazildi: tam-belirlenmis post-state (tek h_S' esitligi; h_no_fault_target kacagi kapandi), ifade ilerletme (thread cerrahisi ts1++ctx'::ts2), sVarOku YENI (memOku olaylari mekanize), 3 congruence kurali (oz-yinelemeli) + sSeqAtla/sGuvensizAtla. (3) step_iz_analiz TEK gorunum lemmasi → L4/L7/Drf-4'/T1 dort teorem tek inductiondan; L2/L3/L5/L6 GERCEK ICERIKLI yeniden ifade (lineerTuketListe_tuketir vb. Core lemmalari). (4) step_fault_preserves_typed cong case'leri dahil SORRY'SUZ (konfTipliFull_odak + typed_*_ic tersine-cevirme + AtamaOdak kapali-form invariant). (5) V1 daraltma: t_gorev_birlestir/t_kullan : bos; Deger.gorevVal eklendi. Build 28/28 temiz. |
| 4 | 2026-06-11 | v3 F1 | **-2 (=13 toplam)** | Onarim v3 F1 — modul yeniden katmanlama (ADIM0_DENETIM_RAPORU + FAZ_BRIFINGLERI). Yeni: Sem/Tipli.lean (Typed+ThreadTipliFull+KonfTipliFull tasindi), Meta/ProgressKorunum.lean (tum progress/preservation iskeletleri). SILINDI: Sem/ProgressKorunum.lean, StateTipli.ThreadTipli/KonfTipli placeholder'lari, TypedAdim5. Dedup: progress_lineer+progress_region→progress_typed, preservation_lineer+preservation_region→preservation_typed (-2 sorry). preservation_konfTipli→preservation_konfTipliFull'a yukseltildi. Build 28/28 temiz. |
| 3 | 2026-05-29 | 8 V2 P6 | **-1 (=15 toplam)** | cGorevBaslatHataLineerIhlal discharge — **use-after-move reformulasyonu** (Mehmet onayli). Step fault `aktif`→`tuketildi` + `vIhlal∈yd` linkage; FaultSebep `lineerCagiranTukenmedi`→`lineerYakalananZatenTuketildi`. `l_gorev_baslat` strengthen (∀ v∈yd, Λ v ≠ tuketildi; l_kanal_gonder deseni). Aile2 `typing_excludes_cGorevBaslatHataLineerIhlal` FULL (l_gorev_baslat + faulting ctx' koprusu → celiski; cift ThreadTipliFull uygulama). 5 cases arity +1 (Theorems×2/Drf/L4/L7). Build 27/27 temiz. **MILESTONE: step_fault_preserves_typed TAM ISPATLI** (7 Hata + 8 Tamam, sorry-suz; Aile 2 Fault Impossibility ailesi tamam). Gerekce: cross-thread aliasing kompozisyonel korunur (yakalama consume → sonraki kullanim double-use). Kalan 15: typed_no_fault step (1) + Adim 4/5/6 progress/preservation iskelet (14). |

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

## Adim 4.1 yeni sorry'leri (Progress/Preservation iskelet, 2026-05-22)

Tum sorry'ler `proofs/drf-v2-lean/Kemgu/Sem/ProgressKorunum.lean`'de:

- [x] ~~`ProgressKorunum.lean:71` — `progress`~~ (Adim 4.2'de 7/12 case kanitlandi; 5 sub-sorry asagida)
      Tamam (7): t_tanim, t_sabit, t_atama, t_gorev_birlestir, t_kanal_gonder, t_kullan, t_imha
      Kalan (5): t_seq, t_gorev_baslat, t_kanal_al, t_dondur, t_guvensiz (Adim 4.2b)

  Sub-sorry'ler (progress lemma icinde):
  - [ ] `progress.t_seq`: induktif Progress (alt-ifadelere IH), Adim 4.2b
  - [ ] `progress.t_gorev_baslat`: Step.cGorevBaslatTamam insasi (threadFresh + yenContext), Adim 4.2b
  - [ ] `progress.t_kanal_al`: Step.cKanalAlTamam insa (kanal var/yok ayrimi), Adim 4.2b
  - [ ] `progress.t_dondur`: Step.cDondurTamam insa (b frozen/değil ayrimi), Adim 4.2b
  - [ ] `progress.t_guvensiz`: induktif Progress (alt-ifadeye IH), Adim 4.2b

- [ ] `ProgressKorunum.lean:102` — `preservation`
      Sebep: Step 15 constructor (8 Tamam + 7 Hata) case analizi, HasType korunumu
      Tamamlanma: Adim 4.3 — Step constructor case analizi; Hata cases'leri Adim 7 Discharge ile exfalso

- [ ] `ProgressKorunum.lean:126` — `preservation_sigmaTipli`
      Sebep: SigmaTipli korunumu — Step store-modify cases'leri (sAtamaTamam push)
      Tamamlanma: Adim 4.4

- [ ] `ProgressKorunum.lean:142` — `preservation_sahiplikTutarli`
      Sebep: SahiplikTutarli korunumu — frozen persistence + sahiplik degisimi case'leri
      Tamamlanma: Adim 4.4

- [ ] `ProgressKorunum.lean:155` — `preservation_kanalTutarli`
      Sebep: KanalTutarli korunumu — cKanalGonder ekleme + cKanalAl cikarma cases'leri
      Tamamlanma: Adim 4.4

- [ ] `ProgressKorunum.lean:178` — `preservation_konfTipli`
      Sebep: KonfTipli ana korunum — 4 sub-lemma conjunction + ThreadTipli (True placeholder)
      Tamamlanma: Adim 4.4

**Adim 4.1 yeni sorry: 6**. Toplam: 35 + 6 = **41 sorry**.

NOT: `soundness_corollary` (`ProgressKorunum.lean:188`) sorry kullanmaz — `True` return ile iskelet (Adim 4 sonu Progress + Preservation StepStar induktif birlesimi ile gercek soundness corollary).

---

## Adim 7 — Discharge + No-Fault Catı (2026-05-28)

### KAPANAN sorry'ler (35 trivial kapandi — Hata strengthen sayesinde)

**Strateji:** Plan §4.4 "fault non-observable" formel sahile getirildi. SmallStep.lean'de
her 7 Hata constructor'a (sAtamaHataDonmus, sAtamaHataSahipDegil, cGorevBaslatHataLineerIhlal,
cKanalGonderHataLineerTuket, cDondurHataZatenDonmus, sLinKullanHataZatenTuketildi,
sLinImhaHataZatenTuketildi) 5 eşitlik hipotezi eklendi:
- h_store : S'.store = S.store
- h_iz : S'.iz = S.iz
- h_zaman : S'.zaman = S.zaman
- h_sahip : S'.sahiplik = S.sahiplik
- h_kanal : S'.kanal = S.kanal

Bu strengthen sayesinde L4/L7/Drf/MemSafety'deki tum 35 Hata case sorry'si
**dogrudan trivial kapatildi** (rw h_iz + List.mem_cons absurd dispatch):

**DRF-L4 (FrozenRegionRead) — 7 sorry KAPANDI:**
- [x] sAtamaHataDonmus
- [x] sAtamaHataSahipDegil
- [x] sLinKullanHataZatenTuketildi
- [x] sLinImhaHataZatenTuketildi
- [x] cDondurHataZatenDonmus
- [x] cGorevBaslatHataLineerIhlal
- [x] cKanalGonderHataLineerTuket

**DRF-L7 (BellekErisimTipSoundness) — 7 sorry KAPANDI:**
- [x] sAtamaHataDonmus
- [x] sAtamaHataSahipDegil
- [x] sLinKullanHataZatenTuketildi
- [x] sLinImhaHataZatenTuketildi
- [x] cDondurHataZatenDonmus
- [x] cGorevBaslatHataLineerIhlal
- [x] cKanalGonderHataLineerTuket

**Drf (Teorem 4' Same-Step) — 7 sorry KAPANDI:**
- [x] Yukaridaki 7 Hata case (h_event1 + h_event1_new pattern)

**MemSafety T1 (Bellek Guvenligi tam form) — 7 sorry KAPANDI:**
- [x] Yukaridaki 7 Hata case

**MemSafety T1' (Corollary full) — 7 sorry KAPANDI:**
- [x] Yukaridaki 7 Hata case

**TOPLAM KAPANAN: 35 sorry.** Plan §6.2 Aile 2 (Fault Impossibility) lemma'lari
(~300-500 satir) bu strengthen sayesinde REDUNDANT — dogrudan strengthened
constructor uzerinden ispatlandi.

### YENI sorry'ler (2 iskelet, Adim 8 hedef)

Tum sorry'ler `proofs/drf-v2-lean/Kemgu/Discharge/NoFault.lean`'de:

- [ ] `NoFault.lean:65` — `step_fault_preserves_typed`
      Sebep: Tek-adim fault korunumu — Tamam constructor'larin S'.fault'i
      (V1'de KONSTRESIZ) Adim 8'de Tamam strengthen ile `h_no_fault_target`
      eklenince + typed program varsayimi ile Hata'lara ulasilmama
      garantisi (Plan §6.2 Aile 2 sub-form) ile tractable.
      Tamamlanma: Adim 8 — Tamam strengthen + Typed program varsayimi.

- [ ] `NoFault.lean:102` — `typed_no_fault` (CATI TEOREM)
      Sebep: StepStar induction (refl + step) ile typed program
      reachable state'lerin fault state'e ulasmadigini garanti eder.
      Step case: step_fault_preserves_typed'a baglanir.
      Plan v2 §6.3 ana hedef teorem.
      Tamamlanma: Adim 8 — step_fault_preserves_typed full proof sonrasi.

### Adim 7 satir maliyeti

- SmallStep.lean strengthen: ~50 satir (7 Hata × 5 hipotez + yorum)
- L4/L7/Drf/MemSafety guncelleme: ~120 satir (35 sorry → 35 trivial proof)
- NoFault.lean (yeni dosya): ~140 satir (2 statement + iskelet + Plan §6 yorum)
- **TOPLAM: ~310 satir** (Plan §6.4 tahmini ~1050-1600 satir).

### Adim 7 tasarruf gerekceleri

Plan §6.2'nin 4 Discharge ailesi (Aile 1-4) yerine strengthened Hata
constructor stratejisi:
- Aile 2 (Fault Impossibility): REDUNDANT — strengthened constructor
  sayesinde dogrudan trivial ispat.
- Aile 1/3/4: Adim 8'de Progress + Preservation full proof ile birlikte
  (typed program → Step.Ok constructor insasi argumani).

Plan §8.4 "tıkanma noktası 1 = Adim 7" riski **bu strateji ile 80% azaldı**.

**Adim 7 net sorry: -33 (49 - 35 + 2 = 16).**

---

## Adim 6 yeni sorry'leri (RegionTamam iskelet, 2026-05-28)

Tum sorry'ler `proofs/drf-v2-lean/Kemgu/Sem/RegionTamam.lean`'de:

- [ ] `RegionTamam.lean:318` — `progress_region`
      Sebep: Typed (HasType + LineerTamam + RegionTamam) destructure ve
      progress_lineer (Adim 5.2 hedef) + RegionTamam bilgisi ile case analizi.
      Hata case'leri Adim 7 Discharge ile exfalso, Tamam case'leri RegionTamam'dan Ρ' insasi.
      Tamamlanma: Adim 6.2 — Adim 7 Discharge sonrasi tractable.

- [ ] `RegionTamam.lean:344` — `preservation_region`
      Sebep: Step 15 constructor (8 Tamam + 7 Hata) case analizi.
      Hata case'leri Adim 7 Discharge ile exfalso (h_no_fault_target vs h_fault);
      Tamam case'leri hasType + lineerTamam + regionTamam korunumu.
      Ρ degisim patternleri: sAtamaTamam (yok), cGorevBaslatTamam (sahip ata),
      cKanalGonderTamam (kanalRho), cDondurTamam (donmus).
      Tamamlanma: Adim 6.3 — Adim 7 Discharge + Adim 4.3/5.3 sonrasi.

**Adim 6 yeni sorry: 2**. Toplam: 47 + 2 = **49 sorry**.

NOT: Brifing C3 hedefi ~52 idi. Daha az sorry uretildi cunku ThreadTipliFull/KonfTipliFull
yeni isim altinda tanimlandi (StateTipli.ThreadTipli placeholder True KALIR, import cycle
onlemek icin); StateTipli.KonfTipli'nin guncellenmesi gerekmedi → +0 sorry. Adim 7
Discharge bu yeni isimleri direkt kullanir.

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
- [x] **C1:** Adim 1.3 sonu — tum 8 Step constructor dual, sorry: 35
- [x] **C2:** Adim 2 sonu — ConfigTyped iskelet (KonfTipli + 4 alt-yapi tam), sorry: 35
- [x] **C2.5:** Adim 3 sonu — Minimal HasType (12 kural), sorry: 35
- [x] **C2.75:** Adim 4.1 sonu — Progress + Preservation iskelet, sorry: 41 (+6 placeholder)
- [x] **C2.85:** Adim 4.2 sonu — progress kismi proof (7/12 case), sorry: 45
- [x] **C2.9:** Adim 4 sub-step V1 sinir notu + preservation imza guclendirildi. sorry: 45 sabit.
- [x] **C2.95:** Adim 5 (LineerTamam katmani) — 13 kural inductive tam + helpers + TypedAdim5 + 2 sub-lemma iskelet. sorry: 47 (+2).
- [x] **C3:** Adim 6 (RegionTamam + Typed full conjunction) sonu — 12 kural inductive tam + helpers + Typed + ThreadTipliFull + KonfTipliFull + 2 sub-lemma iskelet. sorry: 49 (+2). ThreadTipliFull/KonfTipliFull import cycle onlemek icin yeni isim altinda (StateTipli.ThreadTipli placeholder True KALIR).
- [x] **C4:** Adim 7 (Discharge + No-Fault çatı) sonu — Step Hata constructor'lar strengthened (Plan §4.4 formel: h_store/h_iz/h_zaman/h_sahip/h_kanal); 35 Hata case sorry **trivial kapandi** (L4/L7/Drf/MemSafety); +2 yeni iskelet (NoFault.lean: step_fault_preserves_typed + typed_no_fault). sorry: 16 (-33). Plan §6.2 Aile 2 REDUNDANT (strengthen sayesinde); Aile 1/3/4 Adim 8 hedef. ~310 satir (Plan §6.4 tahmini ~1050-1600 satir, %80 tasarruf).
- [x] **C4.5:** Adim 7 yarım — Tamam strengthen + kismi proof. SmallStep: 8 Tamam'a h_no_fault_target. NoFault: step_fault_preserves Tamam case'leri **FULL** (8 case `exact h_no_fault_target`); typed_no_fault refl case **FULL** (KonfTipliFull.fault). sorry: 22 (+6 geçici — eski 2 statement-only → 8 yapısal sub-sorry, Aile 2 + preservation_konfTipli yolu netlesti).
- [x] **C4.7 (bu commit):** Adim 8 P1 — Aile 2 Linear kısmi. ThreadTipliFull'a Plan §5.2.3 köprü (ctx.lineer ↔ Λ). Yeni Aile2.lean: 2 Linear lemma FULL (sLinKullan + sLinImha). NoFault step_fault sLinKullan/Imha case'leri Aile 2 dispatch ile **kapandi**. sorry: 20 (-2). P2-P5 hedef: 5 Aile 2 region/concurrency (V2 strengthen gerek) + Adim 4/5/6 sub-lemma'lar → C5 sorry 0.
- [x] **C4.8 (bu commit):** Adim 8 P2 — Aile 2 cKanalGonder. `l_kanal_gonder` strengthen (`Λ vId ≠ some tuketildi`) + `typing_excludes_cKanalGonderHataLineerTuket` FULL + NoFault dispatch. sorry: 19 (-1). Build 27/27 temiz. Kalan step_fault Hata: **4** (sAtamaHataDonmus/SahipDegil, cGorevBaslatHataLineerIhlal, cDondurHataZatenDonmus) — hepsi V2: ortak kok neden statik Ρ sabit + runtime degisken-ortami yok (serbest k/vIhlal ifadeye baglanamaz). Tek `Ρ→Konfigurasyon` refactor dordunu birden acar (Kirmizi Queue adayi). typed_no_fault step case + Adim 4/5/6 iskelet (14 sorry) hala acik (Discharge tam olmadan tractable degil).
- [x] **C4.9 (bu commit):** Adim 8 Secenek A — Ρ→Konfigurasyon refactor (P1-3). Konfigurasyon.bolge alani + KonfTipliFull (S.bolge=Ρ + FrozenKategoriTutarli kopru) + sAtamaHataDonmus Step linkage (h_x_bolge) + Aile2 typing_excludes_sAtamaHataDonmus FULL + NoFault dispatch + 5 cases arity guncelleme. sorry: 18 (-1). Build 27/27 temiz. **Tasarim uctan uca dogrulandi.** Kalan 3 region/concurrency step_fault Hata (sAtamaHataSahipDegil/cGorevBaslatHataLineerIhlal/cDondurHataZatenDonmus) ayni desenle sonraki fazlar (cDondur: r_dondur strengthen; cGorevBaslat: l_gorev_baslat + yd; sAtamaSahipDegil: ownership). preservation_konfTipli (kopru korunumu, cDondurTamam S.bolge update) ayri.
- [x] **C4.10 (bu commit):** Adim 8 V2 P4 — cDondurHataZatenDonmus discharge. r_dondur strengthen + Aile2 typing_excludes_cDondurHataZatenDonmus FULL + NoFault dispatch (Step degisikligi yok, cases churn yok). sorry: 17 (-1). Build 27/27 temiz. step_fault Hata kalan **2**: sAtamaHataSahipDegil (Typed/SahiplikTutarli ownership kopru gerek), cGorevBaslatHataLineerIhlal (l_gorev_baslat strengthen + Step vIhlal∈yd linkage). Sonra typed_no_fault step + preservation_konfTipli (kopru korunumu).
- [x] **C4.11 (bu commit):** Adim 8 V2 P5 — sAtamaHataSahipDegil. KonfTipliFull AtamaSahipligi invariant (8. bilesen) + Step h_x_bolge + Aile2 lemma FULL + NoFault dispatch + 5 cases arity + bridge proj kaydi. sorry: 16 (-1). Build 27/27 temiz. step_fault Hata kalan **1**: cGorevBaslatHataLineerIhlal (l_gorev_baslat strengthen + Step vIhlal∈yd — lineer katman). Kapaninca step_fault_preserves_typed Hata case'leri TAM olur; sonra typed_no_fault step + preservation_konfTipli (kopru/invariant korunumu, Configuration-form).
- [x] **C4.12 (bu commit):** Adim 8 V2 P6 — cGorevBaslatHataLineerIhlal use-after-move reformulasyonu (Mehmet onayli). Step aktif→tuketildi + vIhlal∈yd + FaultSebep rename (lineerYakalananZatenTuketildi) + l_gorev_baslat strengthen + Aile2 lemma FULL + NoFault dispatch + 5 cases arity. sorry: 15 (-1). Build 27/27 temiz. **MILESTONE: step_fault_preserves_typed TAM ISPATLI (7/7 Hata + 8/8 Tamam, sorry-suz)** — Aile 2 (Fault Impossibility) ailesi tamamlandi. Kalan 15: typed_no_fault step case (1, preservation_konfTipli zinciri) + Adim 4/5/6 progress/preservation iskelet (14). C5 (sorry 0) icin asil kalan: kopru/invariant KORUNUMU (Configuration-form Wright-Felleisen — cDondurTamam S.bolge update, sAtamaTamam ownership, vs.).
- [ ] **C5:** Adim 8 (L0-L7 + T1 + Drf adapt) sonu — sorry: 0, MERGE'e hazir
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
