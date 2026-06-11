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
