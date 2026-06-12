# DECISIONS_LOG — DRF Mekanizasyon Karar Kaydı

## 2026-06-11 — Çatal 1: YOL B (görev gövdeleri yazma-hedefsiz, V1)

**Sorun:** Ρ-değiştiren kurallar (cGorevBaslat/cKanalGonder/cDondur) global
bölge-ortamını recat'ler; paylaşılan-Ρ altında (a) odaksız thread'lerin
RegionTamam türevleri yeniden kurulmalı (transport), (b) spawn edilen
çocuğun gövdesi, yakalanan bölgeler sahip(t)-kategorili olunca r_atama'nın
yazılabilirlik şartını geçemez (çocuk hiç yazamaz).

**Seçenekler:** A per-thread Ρ (temiz model; F5 progress_konf reworku,
~800-1200 satır) / B çocuk-gövde yazma-hedefsiz V1 daraltması + transport
lemması (F5 korunur; dil-anlamı daralır) / C ∃-Ρctx (Aile2 linkage kırılır).

**KARAR (Mehmet): YOL B.** V1 statement-kararı; per-thread Ρ = V2.
Görünürlük: IyiTipliCekirdek docstring + README kapsam notu.
Durum: premise + transport, kategori-anahtar çözümüyle birlikte girecek
(aşağıya bk.) — tek başına case kapatmıyor.

## 2026-06-11 — Çatal 2: KAPASİTE-1 kanallar

**Sorun:** KanalTransit bileşeni ("dolu kuyruk → ∃ transit bölge") pop
sonrası tanığını kaybediyor (çok-mesajlı kuyrukta ikinci mesajın transit
bölgesi izlenmiyor).

**Seçenekler:** mesaj-başına-transit sayma invariantı (buffer'lı; sayma
argümanı ~+200 satır) / kapasite-1 (dolu kanala gönderim bloklar).

**KARAR (Mehmet): KAPASİTE-1.** Uygulandı: `cKanalGonderTamam h_bos`
guard'ı + `Engelli.gonder_dolu` + `KanalKapasite1` (14.) + `KanalAyrik`
(15.) bileşenleri. Buffer'lı kanal = V2.

## 2026-06-11 — 🔴 DUR-SOR (AÇIK): kategori-anahtar disiplini

**Bulgu (ispat sürecinde):** `sahiplikGet`/`konumGet` anahtarları TAM-Bolge
karşılaştırması yapıyor (kategori dahil). Ρ-recat'leyen kurallar kayıtlı
Bolge DEĞERİNİ değiştirdiğinden: (i) store erişimi kopuyor (recat sonrası
eski konum-anahtarları bulunamaz → comp-10/DegiskenlerBagli korunamaz,
read-after-freeze STUCK), (ii) sahiplik sorguları kopuyor (comp-7
FrozenKategori iff'i cDondur altında kurulamaz). SahiplikTutarli (comp-3)
id-genel forma alınarak yerel olarak çözüldü (tüketicisiz bileşen); kalan
kullanımlar KURAL-DÜZEYİ.

**Önerilen çözüm:** id-anahtarlama — `sahiplikGet`/`konumGet`
karşılaştırmaları `.id` (+ofset) bazlı; "bölge kimliği = id, kategori =
değişken öznitelik" (BolgeAyrik felsefesiyle bire bir). Semantik-model
ifade değişikliği → Mehmet onayı gerekli. Maliyet: F2-mertebesi yeniden-
dokunuş (set_eq/ne koşulları id'ye iner; kapalı 15 case'in sahiplik
argümanları sadeleşerek yeniden geçer; sonra 3 Ρ-case + 3 cong + Yol-B
premise + transport kapanır → sorry 0).

**Durum: ✅ ÇÖZÜLDÜ (2026-06-12, Mehmet onayı — id-anahtarlama passı).**
Uygulandı: `sahiplikGet`/`konumGet` id(+ofset)-anahtarlı; `SigmaTipli`
koşul-2 id-formu; kongruans lemmaları (`sahiplikGet_id_esit` /
`konumGet_id_esit`); `sahiplikSetMany_analiz` master lemması. 15 kapalı
case sadeleşerek yeniden geçti (köprü TAM). Yol-B premise GERÇEK olarak
`r_gorev_baslat`'a girdi; `regionTamam_transport` + `regionTamam_yaz_geri`
TAM ispatlandı; **3 Ρ-case kapandı → adim_korunum 18/21**.

## 2026-06-12 — ⭐ Fırsat-kontrol sonucu: write-free premise GEREKLİ

**Soru (Mehmet):** id-anahtarlama Yol-B yazma-hedefsiz kısıtını gereksiz
kılar mı?

**CEVAP: HAYIR — kısıt kalıyor (ve artık GERÇEK premise).** id-anahtarlama
ERİŞİM sorununu çözer (recat sonrası store/sahiplik lookup'ları kopmaz);
`r_atama`'nın KATEGORİ disiplinini değiştirmez: yakalanan bölgeler spawn'da
`sahip(tYeni)` kategorisine geçer → `kategoriYazilabilir = false` → çocuğun
kendi yakaladığı bölgeye yazması spawn-sonrası ortamda statik tiplenemez.
Kategoriyi gevşetmek (sahip-kategorisini yazılabilir saymak) caller-after-
spawn fault-discharge'ını kırar (comp-8 muafiyeti kaybolur). Çocuğun kendi
bölgesine yazabilmesi = per-thread Ρ (Yol A) = V2. Docstring güncel
(IyiTipliCekirdek: "GERCEK premise" bloğu).

## 2026-06-12 — 🔴 DUR-SOR (AÇIK): cong-penceresi + serbest-tHedef join
## = adim_korunum MEVCUT Step İLE YANLIŞ (counterexample)

**Bulgu (son pass, OdakUyum-tasarımını sağlamlık testinden geçirirken):**
`adim_korunum` mevcut Step-semantiğiyle KANITLANAMAZ — ifade YANLIŞ.
Kök neden iki katmanlı:
1. `cGorevBirlestirTamam.h_hedef` diğer thread'lerin İFADELERİNİ okur
   (`∃ hctx ∈ S.thread, ... hctx.ifade = sabit vSon`) — Step'in tek
   ifade-okuyan global premise'i. `tHedef` ayrıca `g`'ye BAĞLANMAMIŞ
   (serbest değişken — linkage premise'i yok).
2. Cong kuralları (`ifadeyleKonf` penceresi) koşan thread'i pencerede
   alt-ifadesine indirger → pencereli listede `seq (sabit v) b` koşan
   bir thread "bitmiş" (`sabit v`) GÖRÜNÜR.

**Counterexample (tam kurgu):**
- Γ: g : gorev bos; x : scalar. Ρ: g↦bg, x↦bx (yerel). Store: g-konumu
  `gorevVal 0` (varsayılan!), x-konumu `skaler 0`. Sahiplik: bg,bx ↦
  thread 0. Threadler: T = ⟨0, seq (sabit birim) (atama x (sabit
  (skaler 1)))⟩, J = ⟨1, gorevBirlestir g⟩. **KonfTipliFull S: 15/15
  sağlanır** (comp-8: T'nin hedefi x sahibi 0 ✓).
- Adım: `sSeqCong` (odak T) → S1'de T pencereli (`ifade = sabit birim`).
  İç adım: `cGorevBirlestirTamam` (ctx := J, tHedef := 0, h_hedef
  tanığı := PENCERELİ-T, rb := [bx] — h_donen ✓). J biter, **bx sahibi
  thread 1'e geçer**. Cong-restorasyon: T'nin `seq (sabit birim)
  (atama x ...)` ifadesi GERİ gelir.
- S'-comp-8 İHLAL: T'nin HedefVar-x'i yazılabilir-kayıtlı, sahibi artık
  1 ≠ 0. comp-8 `S'.bolge`/`S'.sahiplik` okur — HİÇBİR Ρ' seçimi
  kurtarmaz → `KonfTipliFull Γ Δ Ρ' S'` her Ρ' için YANLIŞ →
  adim_korunum YANLIŞ. (Devamında T'nin x-yazımı J-sahipli bölgeye
  yazar — GERÇEK DRF-ihlali: model bug'ı, ispat tekniği sorunu değil.)

**Önerilen düzeltme (Fix-F — çerçeve yan-koşulu):** üç cong kuralına
(`sSeqCong`/`sAtamaCong`/`sGuvensizCong`) premise ekle:
`h_yan : ts2' = ts2 ∨ ∃ y, ts2' = ts2 ++ [y]`
(ts1-prefix `h_t1'` ile zaten sabit). Bu, iç adımı ODAKLI pozisyona
kilitler: pencere yalnız odaklı thread'in alt-ifade redüksiyonunu sarar;
diğer thread'lerin adımları (gerçek join dahil) doğrudan S-üstünde
atılır — orada `h_hedef` GERÇEK listeyi görür ve koşan-T'yi dışlar.
DAVRANIŞ KAYBI YOK: elenen adımlar yalnız dış-düzeyde temsil edilemeyen
pencere-artefakt'larıdır (semantik-niyet restorasyonu). Maliyet: Step 3
kural + progress_konf cong-tanıklarına yan-koşul tanığı (mekanik — tüm
tanıklar zaten ts2-veya-spawn-append formunda) + step_iz_analiz/
step_fault_gorunum/step_donmus_korunur arity (+1 hipotez, mekanik).
Opsiyonel ikinci katman (ayrı karar): h_hedef'e g-linkage
(g-konumunda `gorevVal tHedef`) — pencereden bağımsız sağlamlaştırma.

**Statü: ONAY BEKLİYOR.** Step = kemgu_soundness_v3'ün denotasyonunun
parçası → görev guardrail'i ("dış kontratı değiştirmek gerekiyorsa
DUR-SOR — build yakalamaz sınıfı") tetiklendi. Fix-F olmadan sorry-0
MATEMATİKSEL OLARAK İMKANSIZ (counterexample). Onay sonrası kalan plan:
Fix-F (statement) → OdakYuk payload (aşağıdaki tasarım; Fix-F sayesinde
payload yalnız odaklı-pozisyon için, sade form) → 3 cong → sorry 0.

## 2026-06-12 — KALAN TEK BLOKER: odak-adım güçlendirilmiş-IH (cong ×3)

**Bulgu:** kategori-anahtar çözüldükten sonra cong case'leri (sSeqCong/
sAtamaCong/sGuvensizCong) farklı ve tek blokere indi: IH yalnız
`KonfTipliFull S1'` verir; odaklanmış S1 yalnız `a` taşıdığından devam-
ifadesi `b`'nin (i) Λmid/Ρmid altında yeniden tiplenmesi (l_seq/r_seq
kompozisyonu), (ii) hedef-sahiplikleri (comp-8/9 büyüyen hedef kümesi)
IH'den gelmez.

**Tasarım (sonraki oturum; final teorem ifadesi DEĞİŞMEZ — iç-lemma
güçlendirmesi):** `adim_korunum` sonucuna odak-yükü ekle
(`∃ Ρ', KonfTipliFull ... ∧ OdakUyum S S' Ρ Ρ'`):
1. Lineer: Λ' ≼ Λ-statik-çıktı monotonluğu (≼: tüketildiler ⊆, aktifler ⊇
   — sVarOku lineer-okumayı runtime'da tüketmediğinden eşitlik değil) +
   lineer-≼ transport lemması (13 kural, aktif/¬tüketildi premise'leri
   monoton).
2. Region: `regionTamam_transport` (MEVCUT — hazır) + OdakUyum'un
   yazılabilir-hedef mutabakatı.
3. comp-8/9: b'nin hedefleri Ρmid'de yazılabilir (r_seq 2. premise) →
   `regionTamam_yaz_geri` (MEVCUT) → a-dokunmamış → sahiplik korunmuş.
Tahmin: ~600-900 satır (21 kuralda odak-yükü + ≼-lemmaları + 3 cong).

**GÜNCELLEME (2026-06-12 son pass):** (1)'in ≼-ailesi İNDİ ve TAM:
`LineerKucuk` + `lineerKucuk_{refl,update_tuketildi,tuket,tuketListe}` +
`lineerTamam_kucuk_transport` (LineerTamam.lean §5, Step-bağımsız).
ANCAK OdakYuk-payload'ın kendisi YENİ DUR-SOR'a bağlandı: tasarım
sağlamlık-testi, adim_korunum'un mevcut Step ile YANLIŞ olduğunu
gösterdi (yukarıdaki cong-penceresi counterexample'ı) — Fix-F onayı
olmadan hiçbir payload cong'u kapatamaz.
