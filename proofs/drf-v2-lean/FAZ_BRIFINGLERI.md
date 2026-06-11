# DRF Onarım v3 — Faz Brifingleri (ADIM 0 onayı sonrası)

> **DURUM (2026-06-11 oturum sonu):**
> ✅ **F1** (654a8ab) ✅ **F2** (ca07545) ✅ **F3** (e708ee1) ✅ **F4-yapısal + F6** (124052d).
> Build 29/29 temiz; sorry **15 → 7** (hepsi iki iskelette: `adim_korunum` + progress ailesi).
>
> **KALAN İŞ — F4-ispat (adim_korunum doldurma, ~600-900 satır):**
> 1. Hata (7): Aile 2 exfalso (step_fault_preserves_typed deseni kopyalanır — mekanik).
> 2. Tamam (11): bileşen-bileşen; redex-sonucu tiplemeleri ∃-form sayesinde
>    SigmaTipli/KanalTutarli'dan gelir. ÖNCE iki strengthen gerekir:
>    `l_gorev_baslat` + `r_gorev_baslat`'a çocuk-gövde premise'i (spawn edilen
>    thread'in ThreadTipliFull üyeliği için).
> 3. Congruence (3): ∃-form IH YETMEZ — güçlendirilmiş "çıktı-ortam-kararlı"
>    yardımcı lemma gerekir: *iç adım, odaktaki ifadenin statik çıktı
>    ortamlarını (Λout, Ρout) korur* (örn. kullanIf x: statik çıktı
>    `update Λ x tuketildi` = runtime sonrası ortam — örnekler uyumlu).
> 4. 🔴 **DUR-SOR (F4-ispat brifinginde tasarlanacak):** `AtamaSahipligi`
>    bileşeninin sSeqAtla altında korunumu eski invarianttan GELMEZ
>    (yeni odağa giren atamalar): tipleme↔sahiplik ilişkilendiren ek
>    invariant gerekli (aday: "thread, Ρ-erişilebilir donmuş-olmayan tüm
>    bölgelerinin güncel sahibidir" — spawn transferi ile bölümlenmiş form).
>    Bu tasarım kararı Mehmet'e sorulmalı.
>
> **KALAN İŞ — F5 (progress, ~500-800 satır):** `progress_konf` ambient-Γ formu
> (KonfTipliFull tanıkları: AtamaSahipligi→h_owner, DegiskenlerBagli→konum/değer,
> kanalIlk dolu/boş→cKanalAlTamam/Engelli); `threadFresh` tanığı (1 + max tid
> lemması); kanal-transit invariant'ı (kuyruk dolu → kanalSahip bölge var —
> KonfTipliFull'a 10. bileşen adayı). Eski kapalı-Γ `progress`/`progress_typed`
> iskeletleri progress_konf gelince silinir.
>
> **F6 kalanı:** typed_no_fault conjunct'ı zaten teoremde (adim_korunum'a bağlı);
> F4-ispat kapanınca kemgu_soundness_v3 sorry'siz zincire oturur. README +
> kapanış güncellemesi C5'te.

**Onay:** 2026-06-11 (Mehmet) — ADIM 0 raporundaki 5 açık soru önerilen yönde onaylandı:
(1) Sorun 3 kalıcı-form sahiplik, F2'ye dahil; (2) kapsam-dışı üçlü scope-guard;
(3) ortam-evrimi imzası F3, korunumu F4; (4) `kemgu_soundness_v3`'ten SCR/BET çıkar;
(5) sıra F1 → F2 → F3 → F4 → F5 → F6 (tek yürütücü olduğundan F3, F2 sonrası — paralel değil).

**Genel DoD (her faz):** `lake build` temiz (yalnız bilinen sorry uyarıları), SORRY_TRACKER
güncel, Türkçe commit. Politika: ASCII identifier, Türkçe yorum, mathlib bağımsız.

---

## F1 — Modül yeniden katmanlama (davranış değişikliği yok)

**Hedef:** Judgment tanımları ile meta-teoremleri ayır; placeholder çiftlenmesini kaldır;
`typed_no_fault`'un ihtiyaç duyduğu Full-korunum lemmasının yaşayabileceği modülü yarat.

**Yeni DAG:**
```
Core → StateTipli → HasType → LineerTamam(judgment) → RegionTamam(judgment)
     → Tipli(YENİ: Typed + ThreadTipliFull + KonfTipliFull)
     → Meta/ProgressKorunum(YENİ: IsValue + tüm progress/preservation iskeletleri)
     → Discharge/{Aile2, NoFault}
```

**İşler:**
1. `Kemgu/Sem/Tipli.lean` (yeni): `Typed`, `ThreadTipliFull`, `KonfTipliFull`,
   intro/elim, `bolgeOrtamBos` — RegionTamam'dan taşınır.
2. `Kemgu/Meta/ProgressKorunum.lean` (yeni): `IsValue` + `progress` (5 sub-sorry) +
   `preservation` + 3 alt-korunum + `preservation_konfTipli` (KonfTipliFull'a
   YÜKSELTİLMİŞ ifade, sorry kalır) + `soundness_corollary`; LineerTamam'dan
   `progress_lineer`/`preservation_lineer`, RegionTamam'dan `progress_region`/
   `preservation_region` buraya taşınır (TypedAdim5 SİLİNİR — Typed kullanılır).
3. `Kemgu/Sem/StateTipli.lean`: §6 `ThreadTipli`/§7 `KonfTipli`/§8 intro-elim SİLİNİR.
4. `Kemgu/Sem/ProgressKorunum.lean` SİLİNİR (içerik Meta'ya).
5. Import/open güncellemeleri: LineerTamam, RegionTamam, Aile2, NoFault, Kemgu.lean.

**Sorry beklentisi:** 15 sabit (taşıma). **Tahmin:** ~250-350 satır churn.

---

## F2 — Tamam-constructor yeniden tasarımı + Sorun 3 (sahiplik modeli)

**Hedef:** Step'in post-state'ini TAM belirle (preservation'ı ifade-doğru yap),
ifade ilerletme semantiği ekle, zaman-anahtarlı sahiplik kusurunu gider,
`h_no_fault_target` kaçağını kapat.

**Tasarım kararları (bu brifingde sabitlenen):**

1. **Sahiplik modeli (Sorun 3):** `Sahiplik := List (Bolge × Sahip)` — Zaman anahtardan
   ÇIKAR (current-state; newest-wins prepend; `lineerOrtam`/`bolgeOrtam` idiyomu).
   `isFrozen S b := sahiplikGet S.sahiplik b = some Sahip.donmus`.
   Frozen-persistence artık tanımdan + yeni transfer-guard'larından gelir:
   sahiplik yazan her Tamam kuralı (`cGorevBaslat`/`cGorevBirlestir`/`cKanalGonder`/
   `cKanalAl`) hedef bölgeler için `¬ isFrozen` guard'ı alır (donmuş bölge transfer
   edilemez — R-PAYLAS ile tutarlı). S1 invariant z'siz forma iner.
2. **Tam-belirlenmiş post-state:** her constructor tek eşitlikle kapanır:
   `h_S' : S' = { S with store := …, iz := …, zaman := …, thread := …, sahiplik := …,
   bolge := …, fault := none }`. `h_no_fault_target` ayrı hipotez olmaktan çıkar
   (eşitliğin parçası — Sorun 2(a) kapanır). Hata constructor'ları:
   `h_S' : S' = { S with fault := some sebep }` (fault-non-observable tek satır).
3. **Thread cerrahisi + ifade ilerletme:** `S.thread = ts₁ ++ ctx :: ts₂` ve
   `S'.thread = ts₁ ++ ctx' :: ts₂` (+ spawn'da `++ [yeniCtx]`).
   Redex sonuçları: `atama x (sabit v) → sabit birim`; `gorevBaslat → sabit
   (gorevVal tYeni)` (YENİ `Deger.gorevVal`); `kanalGonderIf → sabit birim`;
   `kanalAlIf → sabit v` (alınan değer; kuyruktan çıkarma kanal güncellemesiyle);
   `dondurIf → sabit birim`; `kullanIf/imhaIf → sabit birim`;
   `gorevBirlestir → sabit birim`; `tanim x → sabit v` (YENİ `sVarOku` kuralı:
   `bolgeOrtamGet S.bolge x = some b`, `konumGet S.store ⟨b,0⟩ = some v`, `memOku`
   olayı — okuma olayları nihayet mekanize, L4(b) kapsamı açılır).
   **Offset-0 konvansiyonu (V1):** her değişkenin konumu `⟨bolge(x), 0⟩`;
   `sAtamaTamam` da `k := ⟨b,0⟩` + `h_x_bolge` linkage'ı Tamam'a taşır
   (AtamaSahipligi korunumu için gerekli).
4. **Congruence (3 kural):** `sSeqCong` (seq-sol), `sAtamaCong` (atama-RHS),
   `sGuvensizCong` — `Step` öz-yinelemeli premise ile (`ifadeyiDegistir` helper:
   odaklı thread'in ifadesini değiştirir). + `sSeqAtla : seq (sabit v) b → b` ve
   `sGuvensizAtla : guvensiz (sabit v) → sabit v`. Trace-lemma ispatları
   `cases` → `induction`'a döner (cong case'leri IH ile kapanır).
5. **V1 daraltmaları (HasType):** `t_gorev_birlestir : … → bos` (join sync-only;
   değer V2), `t_kullan : … → bos` (consume effect-only; değer çıkarımı V2) —
   redex sonuç değerleri `birim` ile tip-uyumu için zorunlu.
   `DegerTipli`'ye `dt_gorev : gorevVal t : gorev τ` eklenir.
6. **Lineer hipotez normalizasyonu:** üyelik (`(x,lin) ∈ ctx.lineer`) yerine
   lookup formu (`lineerOrtamGet ctx.lineer x = some lin`) — gölgeleme (shadowing)
   belirsizliği kalkar; Aile 2 köprüsüz çalışabilir (F4'te per-thread Λ).

**Etki alanı:** Core.lean (§7 Sahiplik, §4 Deger, §10 yardımcılar, `konumGet` yeni),
SmallStep.lean (TÜM constructor'lar), L0/L4/L7/Drf/MemSafety/Main (ispat yeniden
yazımı — induction formu), HasType (2 kural daraltma + dt_gorev), Aile2/NoFault
(arity + lookup-form), Meta iskeletleri (ifade güncelleme).

**Sorry beklentisi:** 15 civarı sabit (iskeletler taşınır; trace lemmaları tam kalır).
**Tahmin:** ~700-1000 satır.

---

## F3 — Placeholder → gerçek predikat + köprü

**Hedef:** `TipKontrolOk` ailesini Adım 3-6 katmanına bağla; Program↔Konfigurasyon
köprüsünü kur; vakum kuralları kapat.

**Tasarım kararları:**
1. **Program zenginleştirme:** `Program`'a `cevre : TipOrtam` alanı (üst-düzey
   değişken bildirimleri — mevcut model değişken bildirimi içermiyordu).
   `lambdaBaslangic` (tekkez-tipli değişkenler → aktif), `rhoBaslangic` (değişken
   başına taze bölge), `varsayilanDeger : Tip → Deger`, `baslangicKonf Pi`.
2. **Gerçek predikatlar:**
   `TipKontrolOk Pi := ∀ p ∈ Pi.islevler, ∃ τ, HasType Pi.cevre Δ p.snd τ`
   (LineerKontrolOk/BolgeAtamaOk benzer, kendi judgment'larıyla).
3. **Kanal tipi ortamı Δ:** `HasType` imzası `HasType Γ Δ e τ` olur;
   `t_kanal_al : HasType Γ Δ (kanalAlIf k) (Δ k)` — vakum kapanır.
   `KanalTutarli` Δ'ya göre kesin tip ister (∃τ değil).
4. **Λ' çıkışları:** `l_gorev_baslat`'a `Λ' = lineerTuketListe Γ Λ yd`,
   `l_kanal_gonder`'e lineer-ise-tüket tanımı — serbest Λ' kalmaz.
5. **`DegiskenlerBagli`** (KonfTipliFull yeni bileşeni): Γ'daki her değişken için
   bölge + konum + tip-uyumlu değer mevcut (sVarOku progress'i + preservation için).
6. **Kapsam-dışı üçlü:** `CapabilityKontrolOk := programYetkiIcermez Pi`
   (sözdizimsel: `yetkiTok` literali + Γ'da `Tip.yetki` yok);
   `SabitsureKontrolOk := cevreSabitsureIcermez Pi`; `RealtimeKontrolOk` alanı
   IyiTipli'den KALDIRILIR (V1 Ifade'de realtime yapısı yok — guard bile vakum olur;
   dürüst seçim kaldırmak).
7. **Köprü + tanık:** `iyiTipli_baslangic : IyiTipli Pi → KonfTipliFull … (baslangicKonf Pi)`
   + somut `example` satisfiability tanığı.

**Etki:** Core (§11), HasType (imza), LineerTamam (2 kural), StateTipli (KanalTutarli),
Tipli (KonfTipliFull bileşenleri), Aile2/NoFault (arity churn), yeni Kemgu/Sem/Kopru.lean.
**Tahmin:** ~450-650 satır.

---

## F4 — Birleşik korunum (dairesellik kırma)

**Hedef:** `adim_korunum` tek lemması ile typed_no_fault↔preservation döngüsünü
tek yönlü indüksiyona indir; KonfTipliFull bileşen korunumlarını İSPATLA.

**Tasarım:**
1. **Per-thread Λ:** `ThreadTipliFull Γ Δ Ρ threads := ∀ ctx ∈ threads,
   ∃ τ Λ' Ρ', Typed Γ Δ ctx.lineer Ρ ctx.ifade τ Λ' Ρ'` — paylaşımlı-Λ köprüsü
   (iff form) SİLİNİR; Aile 2 lemmaları köprüsüz sadeleşir.
2. `adim_korunum : KonfTipliFull Γ Δ Ρ S → Step S S' → ∃ Ρ', KonfTipliFull Γ Δ Ρ' S'`
   — fault-yokluğu bileşen olarak sonuçta; Hata case'leri Aile 2 ile exfalso;
   Tamam case'leri bileşen-bileşen (functional S' sayesinde subst + yapısal lemmalar).
   Redex-tipleme korunumu: her Tamam için `HasType/LineerTamam/RegionTamam`
   sonuç-ifade lemmaları (sabit-değer tiplemesi + ortam güncelleme uyumu).
   Congruence case'leri: IH + bağlam yeniden-kurma (seq/atama/guvensiz tipleme
   ters-çevirme lemmaları).
3. `typed_no_fault` = `adim_korunum`'un StepStar köşesi (sorry kapanır).
4. Eski iskeletler (`preservation`, `preservation_lineer`, `preservation_region`,
   `preservation_sigmaTipli/sahiplikTutarli/kanalTutarli/konfTipli`):
   `adim_korunum`'un izdüşümleri olarak yeniden türetilir ya da SİLİNİR
   (ifade-yanlış olanlar mutlaka silinir/değiştirilir).

**Sorry beklentisi:** 15 → ~7 (preservation tarafı kapanır; progress kalır).
**Tahmin:** ~600-900 satır. **Risk:** congruence ters-çevirme lemmaları.

---

## F5 — Progress (Aile 1/3/4)

**Hedef:** Tipli konfigürasyon ya değer ya ENGELLİ ya adım atar.

**Tasarım:**
1. Form: `progress_konf : KonfTipliFull Γ Δ Ρ S → ctx ∈ S.thread →
   IsValue ctx.ifade ∨ Engelli S ctx.ifade ∨ ∃ S', Step S S'`.
   `Engelli S e := ∃ k, e kanalAlIf k odaklı ∧ kuyruk boş` (kanal-al bloklanması;
   klasik concurrency-progress üçlü disjunct).
2. Kapalı-Γ formu TERK edilir — ambient Γ (ThreadTipliFull'dan).
   Guard tanıkları KonfTipliFull'dan: `AtamaSahipligi → h_owner`,
   `FrozenKategoriTutarli → h_not_frozen`, `DegiskenlerBagli → konum/değer`,
   `l_kullan/l_imha → h_aktif`, transfer-guard'ları → bölge-frozen-değil.
   Congruence/seq case'leri: Typed ters-çevirme + IH.
3. Eski `progress`/`progress_lineer`/`progress_region` iskeletleri progress_konf'un
   köşeleri olarak kapanır ya da silinir.

**Sorry beklentisi:** ~7 → 0. **Tahmin:** ~500-800 satır.
**Risk:** spawn (threadFresh tanığı — taze tid inşası: `1 + max tid` lemması).

---

## F6 — Üst teorem yeniden bağlama

**Hedef:** TOPLAS-savunulabilir dürüst üst-teorem ifadeleri.

**İşler:**
1. `kemgu_soundness_v3` yeniden ifade: hipotez gerçek `IyiTipli Pi` + köprü;
   sonuç `DrfHolds ∧ MemSafe_perStep ∧ NoFault(typed_no_fault) ∧ Progress`;
   SCR/BET conjunct'ları ÇIKAR (onaylı) — V2 hedefi yorumda.
2. `kemgu_drf_v1_bundled` köprü üzerinden; `IyiTipli`-süs hipotezleri ya gerçek
   kullanım kazanır ya imzadan düşer (L0/L1: düşer — yapısal oldukları dürüstçe kalır).
3. L2/L3 değerlendirme: hipotez-paketleme formları korunur ama yorum başlıkları
   "yapısal özet" olarak netleştirilir; L3 (a) iddiası için F3 sonrası ne
   ispatlanabilir notu.
4. SORRY_TRACKER kapanış + README güncelleme.

**Tahmin:** ~200-300 satır. **Çıkış kriteri:** sorry 0, build temiz → C5/C6, merge adayı.
