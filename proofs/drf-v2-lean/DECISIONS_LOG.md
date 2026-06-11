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

**Durum:** ONAY BEKLİYOR. Kalan 6 sorry'nin tek blokeri.
