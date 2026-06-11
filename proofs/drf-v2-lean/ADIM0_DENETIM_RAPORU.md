# ADIM 0 — DRF Lean Mekanizasyonu Denetim Raporu + Onarım Mimarisi

**Tarih:** 2026-06-11
**Branch:** `feature/drf-onarim-v3` (base: `feature/drf-onarim-v2-WIP` @ 99058fd)
**Kapsam:** YALNIZ denetim + mimari öneri + fazlama. Kanıt yazımı YOK.
**Durum özeti:** 15 aktif `sorry` (SORRY_TRACKER C4.12 ile uyumlu), build son commit'te temiz (27/27).

---

## BÖLÜM 1 — NE BULUNDU (DENETİM)

### SORUN 1: `TipKontrolOk = True` placeholder'ları

#### 1.1 Envanter (file:satır)

| Konum | Placeholder | Etki |
|---|---|---|
| `Kemgu/Sem/Core.lean:347` | `TipKontrolOk (_Pi) : Prop := True` | IyiTipli.tipOk vakum |
| `Kemgu/Sem/Core.lean:350` | `LineerKontrolOk := True` | IyiTipli.lineerOk vakum |
| `Kemgu/Sem/Core.lean:353` | `CapabilityKontrolOk := True` | IyiTipli.capabilityOk vakum |
| `Kemgu/Sem/Core.lean:356` | `SabitsureKontrolOk := True` | IyiTipli.sabitsureOk vakum |
| `Kemgu/Sem/Core.lean:359` | `BolgeAtamaOk := True` | IyiTipli.bolgeOk vakum |
| `Kemgu/Sem/Core.lean:362` | `RealtimeKontrolOk := True` | IyiTipli.realtimeOk vakum |
| `Kemgu/Sem/Core.lean:369-376` | `IyiTipli` — 7 alanın 6'sı `True` | tek gerçek alan: `noGuvensiz` (Core.lean:341) |
| `Kemgu/Sem/StateTipli.lean:184-186` | `ThreadTipli := True` | `KonfTipli` (StateTipli.lean:208-214) 2. bileşeni vakum |
| `Kemgu/Soundness/Main.lean:50` | `SideChannelResistant_v2_placeholder := True` | **sonuç-tarafı** placeholder (daha kötü: teorem `True` conjunct İSPATLIYOR) |
| `Kemgu/Soundness/Main.lean:55` | `BET_v2_placeholder := True` | sonuç-tarafı placeholder |

#### 1.2 Hangi lemma/teoremin geçerliliğini sahte kılıyor

`IyiTipli`'yi hipotez alan teoremler (hiçbiri alanlarını KULLANMIYOR — hipotez süs):

- `Kemgu/Soundness/Main.lean:84` — `kemgu_soundness_v3`. **En ağır vaka:** kağıt iddiası "TipKontrol(Π)=OK ⟹ MemSafe ∧ DRF ∧ SCR ∧ BET". Mekanize halde: hipotez vakum, 4 conjunct'tan 2'si (`SCR`, `BET`) `trivial` ile kapanan `True`. Dış değerlendirmenin 3-4/10 puanının ana nedeni bu teoremin görünüşü ile içeriği arasındaki fark.
- `Kemgu/Drf/Drf.lean:173` — `kemgu_drf_v1_bundled`: `h_iyi` taşınıyor, kullanılmıyor; içerik `drf_l0`'a delege.
- `Kemgu/Drf/L0BolgeKorunumu.lean:74, 93` — `_h_iyi` açıkça kullanılmıyor (dosya bunu dürüstçe belirtiyor, L0:71).
- `Kemgu/Drf/L1BolgeThreadTekilligi.lean:38, 59` — aynı desen.
- `Kemgu/Drf/L3LinearClosureSoundness.lean:35-38` — kağıt iddiasının (a) parçası ("body only accesses certain regions") placeholder yüzünden **ispatlanamıyor**; dosya bunu açıkça itiraf ediyor. L3'ün kalan içeriği hipotez-yeniden-paketleme (sonuç, hipotezlerin alt-konjunktı — içerik ≈ 0; aynısı L2 `drf_l2_linear_move_consumed` için geçerli).

**Önemli nüans:** Adım 3-6'da inşa edilen yeni katman (`HasType`, `LineerTamam`, `RegionTamam`, `Typed`, `ThreadTipliFull`, `KonfTipliFull`) placeholder DEĞİL — gerçek inductive judgment'lar. Sorun: (i) eski `IyiTipli`-tabanlı teoremler bu katmana hiç bağlanmadı, (ii) `Program` ile `Konfigurasyon` dünyaları arasında köprü teoremi YOK (hiçbir teorem `IyiTipli Pi → KonfTipliFull ... (baslangicKonf Pi)` demiyor; `baslangicKonf` diye bir tanım bile yok), (iii) `KonfTipliFull`'un örneklenebilirliği (satisfiability) hiçbir yerde gösterilmiyor — vakum-hipotez riski.

#### 1.3 Yeni katmanın kendi vakum noktaları (gerçek predikata geçişte kapatılmalı)

| Konum | Kural | Sorun |
|---|---|---|
| `Kemgu/Sem/HasType.lean:95-96` | `t_kanal_al` | **her** τ için türetilebilir — kanal tipi ortamı yok; preservation'ı temelden bozar |
| `Kemgu/Sem/LineerTamam.lean:127-129` | `l_gorev_baslat` | çıkış ortamı `Λ'` SERBEST değişken — kural çıkışı hiç kısıtlamıyor (yorumdaki `Λ' = Λ \ ...` kodda yok); `kod` gövdesi ve yakalananlar tip-kontrol edilmiyor |
| `Kemgu/Sem/LineerTamam.lean:144-146` | `l_kanal_gonder` | `Λ'` SERBEST — aynı sorun |
| `Kemgu/Sem/LineerTamam.lean:109-111` | `l_atama` | hedef x'in lineer durumu kontrolsüz (dosyada kabul edilmiş V1 sınırı) |
| `Kemgu/Sem/StateTipli.lean:74-79` | `dt_dizi`, `dt_closure` | eleman/arg-ret tipi serbest → `∃τ, DegerTipli v τ` HER değer için türetilebilir → `SigmaTipli`/`KanalTutarli`'nın tip-bileşeni fiilen vakum (yalnız "bölge kayıtlı" bileşeni gerçek) |

Desen tespiti: Aile 2 lemmaları için **tam gereken hipotez** kurallara eklendi (`l_kanal_gonder`/`l_gorev_baslat` strengthen), kuralın geri kalanı serbest bırakıldı. Lemma yerel olarak doğru ama yük, hiç kurulmamış invariant'lara taşındı (bkz. Sorun 2).

---

### SORUN 2: Dairesel Step önkoşulları

#### 2.1 Semantik-seviye: sonuç önkoşula gömülü

**(a) `h_no_fault_target` — 8 Tamam constructor'ında:**
`Kemgu/Sem/SmallStep.lean:72, 168, 226, 246, 290, 311, 348, 385`.
"Tamam adımı fault üretmez" iddiası semantiğin AKSIYOMU yapılmış. Sonuç: `step_fault_preserves_typed`'in (NoFault.lean:61) 8 Tamam case'i `exact h_no_fault_target` (NoFault.lean:69-84) — **içeriksiz**. Teoremin tüm yükü "tipli config Hata adımı alamaz"a kayıyor; o da...

**(b) ...fault-guard'ların aynadaki görüntüsü olan invariant'lara dayanıyor:**
- `KonfTipliFull` 8. bileşen `AtamaSahipligi` (`Kemgu/Sem/RegionTamam.lean:298-301`) ≡ `sAtamaHataSahipDegil.h_not_owner`'ın (SmallStep.lean:119-120) negasyonu. Discharge lemması `typing_excludes_sAtamaHataSahipDegil` (Aile2.lean:210-223) tek satır modus ponens; **`Typed` hipotezine bile ihtiyaç duymuyor** (Aile2.lean:204'te itiraf: "Typed GEREKMEZ"). İnvariant = sonucun kendisi.
- `KonfTipliFull` 7. bileşen `FrozenKategoriTutarli` (RegionTamam.lean:291-293): `isFrozen` (runtime) ↔ `kategori = donmus` (statik) köprüsü — `typing_excludes_sAtamaHataDonmus`/`cDondurHataZatenDonmus` bunu tüketiyor.
- Bu iki bileşenin **Step altında korunumu** (asıl içerik) hiçbir yerde ispatlanmadı; RegionTamam.lean:288-290'daki yorum bunu açıkça erteliyor.

**(c) Hipotez-zinciri döngüsü (kalan sorry'lerin kilidi):**
```
typed_no_fault (NoFault.lean:180, step case sorry @195)
  └─ IH için KonfTipliFull S1 gerek
      └─ preservation_konfTipliFull gerek — MEVCUT DEĞİL
          (en yakını preservation_konfTipli, ProgressKorunum.lean:230 — ama:
           (i) placeholder KonfTipli üstüne yazılı, Full üstüne değil;
           (ii) hipotezi h_no_fault_target : S'.fault = none)
              └─ S'.fault = none nereden? → step_fault_preserves_typed
                  └─ hipotezi KonfTipliFull S → iz boyunca korunum gerek → BAŞA DÖN
```
Bu döngü ilkesel olarak kırılabilir (standart birleşik-indüksiyon, §2.3) — yani Lean-tanım-seviyesinde kısır döngü değil, **ispat-yükümlülüğü tasarımında** döngü. Ancak kırma denemesi bir sonraki bulguya çarpıyor:

**(d) Üç preservation iskeleti İFADE EDİLDİĞİ HALİYLE YANLIŞ (sadece ispatsız değil):**
- `Kemgu/Sem/ProgressKorunum.lean:144-163` (`preservation`)
- `Kemgu/Sem/LineerTamam.lean:233-248` (`preservation_lineer`)
- `Kemgu/Sem/RegionTamam.lean:389-406` (`preservation_region`)

Karşı-örnek inşası: Tamam constructor'ları `S'.thread`'i ve `S'.bolge`'yi HİÇ kısıtlamıyor (örn. `sAtamaTamam`, SmallStep.lean:56-73: store/iz/zaman/sahiplik/kanal/fault eşitlikleri var, thread/bolge yok). `S.thread = [ctx_tipli, ctx_atama]` al (`ctx_tipli.ifade = sabit v` boş Γ'da tiplenir; `ctx_atama` guard'ları sağlasın), `S'.thread := []` seç → `Step S S'` geçerli, ama sonuçtaki `∃ ctx' ∈ S'.thread, ...` yanlış. Gelecekteki `preservation_konfTipliFull` de aynı nedenle (artı serbest `S'.bolge`, 6. bileşen `S.bolge = Ρ`'yu bozar) ifade edildiği haliyle yanlış olur.
İlgili itiraf: ProgressKorunum.lean:288-290 — "Step ifadeleri DEĞİŞTİRMİYOR mu, yoksa ilerletiyor mu? V1 model belirtilmemiş."
→ **Bu, Tamam-constructor tasarım pass'inin kapsamı (kapalı karar). Burada yalnız İŞARETLİYORUM, tasarlamıyorum.** İşaretli yerler: SmallStep.lean 8 Tamam constructor (56-73, 144-169, 212-227, 233-247, 277-291, 300-312, 337-349, 374-386); 3 yanlış-ifadeli preservation (yukarıda); `progress.t_kanal_al` (ProgressKorunum.lean:108-109 — boş kanalda Step inşa edilemez, dosyada kabul: 278-279).

**(e) Modül katmanlama döngüsü (placeholder çiftlenmesinin kök nedeni):**
İmport DAG: `StateTipli → HasType → ProgressKorunum → LineerTamam → RegionTamam → {Aile2, NoFault}`.
- `Typed`/`ThreadTipliFull`/`KonfTipliFull` RegionTamam'da tanımlı; `StateTipli` bunları göremez → `ThreadTipli = True` "mecburen" kaldı (StateTipli.lean:8-11, RegionTamam.lean:23-31'de itiraf).
- `preservation_konfTipli` ProgressKorunum'da; RegionTamam ProgressKorunum'u **import ettiği için** Full-versiyon korunum lemması orada İFADE BİLE EDİLEMİYOR. `typed_no_fault`'un ihtiyaç duyduğu lemmanın yaşayacağı modül yok.
- Kök neden: judgment tanımları ile meta-teoremler aynı dosyalarda karışık (LineerTamam.lean içinde `progress_lineer`, RegionTamam.lean içinde `progress_region`).

#### 2.2 SORUN 3 (denetimde yeni bulundu — DUR-SOR bayrağı): zaman-damgalı sahiplik modeli

`sahiplikGet` (Core.lean:159-161) `(Bolge × Zaman)` çiftinde **tam-anahtar** eşleşme yapar. Sahiplik yazımları adımın çalıştığı `S.zaman`'da anahtarlanır (örn. `cGorevBaslatTamam.h_sahip`, SmallStep.lean:154-155), adım `zaman`'ı +1 ilerletir. Sonuç: **z anında verilen sahiplik z+1 anında görünmez.**

Somut etki zinciri:
1. `sAtamaTamam.h_owner` (SmallStep.lean:62-63) `(k.bolge, S.zaman)` tam-anahtarında sahiplik ister → başlangıç konfigürasyonunun sonlu sahiplik listesi ancak sonlu zaman damgasını kapsar → yeterince adım sonra **hiçbir thread hiçbir bölgeye yazamaz**. Progress (Aile 1) tipli programlar için yapısal olarak ispatlanamaz hale gelir.
2. `AtamaSahipligi` (KonfTipliFull 8. bileşen) `S.zaman`'a bağlı: zaman ilerleyince sorgu anahtarı kayar, korunum imkansızlaşır.
3. `isFrozen` bu sorunu `∃ z₀ ≤ S.zaman` kalıcı-formuyla ÇÖZMÜŞ (Core.lean:303-316) — thread-sahipliği sorguları aynı kalıcı forma geçirilmemiş. Asimetri tasarım hatası göstergesi.

Bu, görev tanımındaki iki sorunun dışında üçüncü yapısal sorun. **Çözüm yönü** (karar Mehmet'te): sahiplik sorgusunu "z' ≤ z olan en yeni kayıt" semantiğine geçirmek (isFrozen deseni) — Core.lean'de `sahiplikGetSon` benzeri tanım + tüm guard/invariant'ların bu forma migrasyonu. Step kurallarına dokunduğu için Tamam-constructor pass'i ile birlikte ele alınması doğal.

#### 2.3 Kırma stratejisi önerisi (özet; detay Bölüm 2)

Döngü **iyi-temelli tek yönlü indüksiyonla** kırılır: ayrı `typed_no_fault` + `preservation_konfTipliFull` çifti yerine tek birleşik adım lemması:
```lean
adim_korunum : KonfTipliFull Γ Λ Ρ S → Step S S' →
               ∃ Γ' Λ' Ρ', KonfTipliFull Γ' Λ' Ρ' S'
```
- Fault-yokluğu `KonfTipliFull`'un 5. bileşeni olarak sonuçta ZATEN var → `typed_no_fault` bunun StepStar köşesine düşer (trivial indüksiyon, döngü yok).
- Hata case'leri mevcut Aile 2 lemmalarıyla kapanır (korunur).
- Ortamlar `∃`-formda evrilmeli (mevcut sabit-Γ/Λ/Ρ formu lineer tüketim ve bölge geçişiyle çelişir; "tek paylaşımlı Λ" V1 sınırı RegionTamam.lean:242'de zaten V2 hedefi olarak kabul edilmiş).
- Tamam case'lerinin İSPATLANABİLİR olması Tamam-constructor pass'ine + Sorun 3 kararına bağlı (post-state thread/bolge belirlenmeden ThreadTipliFull/6. bileşen korunamaz).

İkincil katmanlama: `KonfTipliFull`'u iki gruba ayır — (i) tip-bağımsız makine invariant'ları (S1, frozen-persistence; `drf_l0` deseniyle Step yapısından tek başına ispatlanabilir), (ii) tipleme invariant'ları (ifade-ilerletme semantiği gerektirir). Önce (i), üstüne (ii) — iyi-temelli sıralama.

---

### Geri kalanın sağlamlığı

**Gerçek ve tipleme-bağımsız (placeholder/döngü düşse de ayakta):**
- `sahiplikSet_eq/ne` (Core.lean:180-188), `sahiplikGet_funkc` (L0:47).
- `drf_l0_bolge_korunumu(+starStep)` (L0:73, 92) — lookup-determinizmi; gerçek ama zayıf (model S1'i yapısal kılıyor; dosya bunu dürüstçe belirtiyor, L0:24-33).
- `drf_l4_a_step` (L4:38) — "frozen bölgeye tek adımda yazılmaz"; `h_not_frozen` guard'ından gerçek içerik. `isFrozen_persistent_simple` (L4:153) yalnız KOŞULLU (ek hipotezlerle).
- `t1_bellek_guvenligi_tam` + corollary (MemSafety:37, 111) — "her yeni yazma, yazanın sahip olduğu donmamış bölgeye"; `h_owner`/`h_not_frozen` guard'larından. Guard'lar semantiğin önkoşulu olduğu için yarı-tanımsal ("safety-by-construction") — meşru, ama asıl yükümlülük (tipli program guard'lı adımı HEP atabilir = progress) açık.
- `kemgu_drf_v1_no_concurrent_writes` (Drf.lean:86) — gerçek ama çok zayıf: "tek adım ≤1 olay ekler" iz-muhasebesi. Cross-step HB yok (kabul edilmiş V1 sınırı).
- `drf_l7_a_step` (L7:39) — iz-store senkronu; aynı muhasebe deseni.
- Aile 2 lemmaları (Aile2.lean) — yerel olarak sağlam ispatlar; ama güçleri hipotezlerine (KonfTipliFull 7/8, köprü) taşınmış durumda.
- HappensBefore/DrfCrossStep tanımları + same-step köşesi; BET/SideChannel açık iskeletler (sorry'siz, dürüst).

**İki/üç soruna bağımlı (sorun çözülmeden değersiz veya yanlış):**
- 15 sorry'nin tamamı: 3'ü ifade-yanlış (preservation ailesi), 1'i kısmen-yanlış (`progress.t_kanal_al`), kalanları döngü + Tamam-pass + Sorun 3'e kilitli.
- `kemgu_soundness_v3` ve `kemgu_drf_v1_bundled`'ın İDDİA biçimleri (vakum hipotez + True conjunct).
- `progress`'in kapalı-Γ formu (ProgressKorunum.lean:71): boş ortamda değişkenli hiçbir ifade tiplenemez → 6 case "vacuous" diye kapandı ama bu, teoremin yalnız değişkensiz programları kapsadığı anlamına geliyor. Konfigürasyon-seviyesi (ThreadTipliFull-ambient-Γ) forma geçmeli.

---

## BÖLÜM 2 — ONARIM MİMARİSİ

### 2.1 TipKontrolOk: placeholder → gerçek predikat

**Önerilen imzalar** (Adım 3-6 katmanına bağlama):
```lean
-- Program-ortam inşası (YENİ):
def gammaProgram   (Pi : Program) : TipOrtam    -- üst-düzey tanımlardan
def lambdaBaslangic (Pi : Program) : LineerOrtam
def rhoBaslangic   (Pi : Program) : BolgeOrtam
def baslangicKonf  (Pi : Program) : Konfigurasyon  -- S₀(Π)

-- Gerçek predikatlar:
def TipKontrolOk (Pi : Program) : Prop :=
  ∀ p ∈ Pi.islevler, ∃ τ, HasType (gammaProgram Pi) p.snd τ
def LineerKontrolOk (Pi : Program) : Prop :=
  ∀ p ∈ Pi.islevler, ∃ Λ', LineerTamam (gammaProgram Pi) (lambdaBaslangic Pi) p.snd Λ'
def BolgeAtamaOk (Pi : Program) : Prop :=
  ∀ p ∈ Pi.islevler, ∃ Ρ', RegionTamam (gammaProgram Pi) (rhoBaslangic Pi) p.snd Ρ'

-- Kapsam-dışı üçlü (KARAR: Mehmet) — öneri: sözdizimsel scope-guard:
def CapabilityKontrolOk (Pi : Program) : Prop := programYetkiIcermez Pi = true
-- (SabitsureKontrolOk / RealtimeKontrolOk benzer; alternatif: IyiTipli'den çıkar)

-- KÖPRÜ TEOREMİ (mimarinin kilit taşı — şu an hiç yok):
theorem iyiTipli_baslangic (Pi : Program) (h : IyiTipli Pi) :
    KonfTipliFull (gammaProgram Pi) (lambdaBaslangic Pi) (rhoBaslangic Pi)
                  (baslangicKonf Pi)

-- Satisfiability tanığı (vakum-hipotez sigortası):
example : KonfTipliFull [] [] [] ornekKonf := ...
```

**Yeniden ispat gerekenler:** `IyiTipli` alan 7 teorem (Drf.lean:173; L0:74,93; L1:38,59; Main:84) alanları kullanmadığından derlenmeye devam eder — asıl iş onları köprü üzerinden GERÇEK içerikle yeniden bağlamak (Faz 6). `Aile2`/`NoFault` etkilenmez (KonfTipliFull kullanıyorlar). Vakum kural onarımları (`t_kanal_al` kanal-tipi ortamı Δ; `l_gorev_baslat`/`l_kanal_gonder` çıkış Λ' tanımı; `dt_dizi`/`dt_closure` v2) HasType/LineerTamam imza değişikliği → Aile2 + NoFault'ta cases arity güncellemesi (Adım 8 deneyiminden: sınırlı, mekanik churn).

### 2.2 Dairesellik: kırma + yeniden yapılandırma

1. **Modül yeniden katmanlaması** (önkoşul): judgment'lar ile meta-teoremleri ayır.
   Yeni DAG: `Core → StateTipli → HasType → LineerTamam(yalnız judgment) → RegionTamam(yalnız judgment) → Tipli.lean(YENİ: Typed + ThreadTipliFull + KonfTipliFull) → Meta/ProgressKorunum.lean(tüm progress/preservation, tüm katmanları görür) → Discharge/{Aile2, NoFault}`.
   `StateTipli.ThreadTipli/KonfTipli` placeholder'ları SİLİNİR (tek tüketicisi `preservation_konfTipli` — taşınıp Full'e yükseltilir). Tek KonfTipli tanımı kalır; import-döngüsü baskısı biter.
2. **Birleşik adım lemması** (`adim_korunum`, §2.3'teki imza) — typed_no_fault + preservation ayrımındaki hipotez döngüsünü tek yönlü indüksiyona indirger. Eski `preservation_konfTipli`, `h_no_fault_target` hipotezli üç preservation iskeleti ve `typed_no_fault`'un step-sorry'si bunun köşeleri olarak yeniden türetilir.
3. **KonfTipliFull iki-katman ayrımı:** makine-invariant'ları (tip-bağımsız; önce) + tipleme-invariant'ları (sonra) — iyi-temelli sıralama; 7/8. bileşenlerin korunum ispabı bu çerçevede yazılır.
4. **Bağımlılık işareti (tasarım değil):** 2-3'ün Tamam case'leri, Tamam-constructor pass'i (post-state thread/bolge/ifade-ilerletme) + Sorun 3 kararı (sahiplik zaman modeli) olmadan İSPATLANAMAZ. İşaretli konum listesi §2.1(d).

---

## BÖLÜM 3 — FAZLAMA (her faz ayrı PR olabilir)

| Faz | İçerik | Tahmin | Bağımlılık |
|---|---|---|---|
| **F1 — Modül yeniden katmanlama** | `Tipli.lean` + `Meta/` ayrımı; StateTipli placeholder'larının silinmesi; import güncellemeleri. Davranış değişikliği yok, taşıma+silme. | ~250-350 satır churn | — |
| **F2 — Tamam-constructor pass'i** | AYRI ODAKLI PASS (kapalı karar — bu rapor kapsamı dışı). Sorun 3 (sahiplik zaman modeli) kararının da burada ele alınması önerilir. | (ayrı brifing) | F1 önerilir |
| **F3 — Placeholder → gerçek predikat** | §2.1'in tamamı: ortam inşaları, gerçek TipKontrolOk ailesi, scope-guard'lar, köprü teoremi, satisfiability tanığı, vakum kural onarımları (t_kanal_al/l_*/dt_*) + Aile2 arity churn. | ~400-600 satır | F1 (F2'ye paralel yürüyebilir — statik taraf) |
| **F4 — Dairesellik kırma: birleşik korunum** | `adim_korunum` + KonfTipliFull 7/8 + ThreadTipliFull korunumları; eski preservation iskeletlerinin Full-forma migrasyonu; `typed_no_fault` kapanışı. | ~600-900 satır | F1+F2+F3 (+Sorun 3 kararı) |
| **F5 — Progress (Aile 1/3/4)** | Guarded-step inşası; blocked-disjunct'lı progress formu (`t_kanal_al`); konfigürasyon-seviyesi progress (kapalı-Γ yerine ambient-Γ); kalan 7 progress sorry. | ~500-800 satır | F2 (+F3) |
| **F6 — Üst teorem yeniden bağlama** | `kemgu_soundness_v3` + bundled DRF'in köprü üzerinden gerçek-IyiTipli formu; SCR/BET conjunct'larının ifadeden çıkarılması/scope-guard'a bağlanması; L2/L3 içerik değerlendirmesi. | ~200-300 satır | F3+F4+F5 |

Toplam (F2 hariç): ~2.000-2.950 satır; F2 ile birlikte görev tahminindeki ~3.5-4k bandı tutarlı.

---

## BÖLÜM 4 — AÇIK SORULAR (Mehmet kararı)

1. **Sorun 3 (sahiplik zaman modeli) — DUR-SOR:** tam-anahtar `(b, z)` sorgusu kalıcı-forma (`z' ≤ z` en-yeni-kayıt, isFrozen deseni) geçirilmeli mi? Step kurallarına dokunduğu için F2 ile birlikte mi ele alınsın? (Önerim: evet, F2 brifingine dahil edilsin.)
2. **Kapsam-dışı üçlü** (Capability/Sabitsure/Realtime): `IyiTipli`'den kaldır mı, sözdizimsel scope-guard mı? (Önerim: scope-guard — kağıt iddiası "Π bu yapıları içermiyorsa" şeklinde dürüstleşir.)
3. **Ortam evrimi imzası:** korunum `∃ Γ' Λ' Ρ'` formuna geçecek; per-thread Λ_ctx (V1 "tek paylaşımlı Λ" sınırının kalkması) imza değişikliği F3'te mi F4'te mi? (Önerim: imza F3, korunum F4.)
4. **`kemgu_soundness_v3` ifade küçültmesi:** SCR/BET conjunct'larını teoremden çıkarmak kağıt-iddiayı daraltır ama dürüstleştirir. Onay?
5. **Tamam-constructor pass zamanlaması:** F1 → F2 → (F3∥) → F4 → F5 → F6 sırası onaylanıyor mu? (F3, F2 beklemeden başlayabilir.)

---

## BÖLÜM 5 — ÖNERİLEN SIRA

```
F1 (katmanlama, risksiz churn)
 ├─→ F2 (Tamam-constructor + Sorun 3 — ayrı odaklı pass, ayrı brifing)
 └─→ F3 (gerçek predikatlar + köprü; F2'ye paralel)
        ↓
       F4 (birleşik korunum — F2+F3 birleşince)
        ↓
       F5 (progress)
        ↓
       F6 (üst teoremler — TOPLAS-savunulabilir form)
```

**Bu pass'te yapılmayanlar (DoD gereği):** toplu kanıt yazımı yok, lemma yeniden-ispatı yok, Tamam-constructor tasarımı yok (yalnız konum işaretleri).
