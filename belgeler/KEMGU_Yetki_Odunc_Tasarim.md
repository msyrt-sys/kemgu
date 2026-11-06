# `yetki<R>` ödünç alma — tasarım notu (LOOP D2, yol haritası H1d)

**Durum:** KARAR (2026-11-06) — Opus önerisi uygulandı; Mehmet itiraz ederse geri alınır.
**Uygulama:** LOOP D2b–e (checker, codegen, self-host parite, korpus).

## 1. Bugün (ölçüldü, iki derleyicide aynı)
| Şekil | Sonuç |
|---|---|
| Yerleşik ödünç: `mmio_yaz32(y, 0, 1); geri_al(y);` | OK — yerleşikler zaten ödünç alır (D-365) |
| Değerle ver, içeride tüket: `f(y: yetki<MMIO>) { geri_al(y); }` | OK — ama çağıran yetkiyi KAYBEDER |
| Değerle ver, sonra kullan: `f(y); geri_al(y);` | **CP005** (taşındıktan sonra erişim) |
| İade deseni: `f(y: yetki<MMIO>) -> yetki<MMIO> { ver y; }` | OK — bugünkü tek dolanma yolu |
| Referans parametre: `f(y: &yetki<MMIO>)` + `f(&y)` | **CP005 ×2** — yasak |

Sonuç: her sürücü işlevi yetkiyi geri döndürmek zorunda. `sonuç<T, H>` dönen
bir işlevde yetki hem başarı hem hata yolunda iade edilmeli → imzalar kirlenir,
kopyala-yapıştır hatası doğar (bir yolda iade unutulursa L001/CP005).

## 2. Karar: ÇAĞRI SÜRELİ ödünç
`&yetki<R>` tipinde parametre = **ödünç**. Kurallar:

1. **Ödünç verme:** çağıran `f(&y)` yazar; `y` canlı olmalı (tüketilmiş → CP005).
   Çağrıdan sonra `y` çağıranda **aynen canlıdır**.
2. **Ödünç alan ne yapabilir:** yetkiyi yerleşiklere (`mmio_*`, `bölge_al`,
   `soket_*` …) ve `&yetki<R>` alan başka işlevlere geçirebilir.
3. **Ödünç alan ne yapamaz** (yeni tanı **CP006 — ödünç yetki kaçamaz**):
   - `geri_al(y)` (tüketim) ve değerle `yetki<R>` parametresine geçirme,
   - `ver y` (dönüş), yapı alanına / diziye koyma, küresele atama,
   - kapanışa yakalama, `görev_başlat`a verme, kanala gönderme.
4. **Ömür yazımı YOK.** Ödünç, çağrı bitince biter; bunu ifade etmek için
   sözdizimi gerekmez çünkü kaçış zaten yasak (kural 3). Dilin "Rust'ın
   lifetime yükü yok" ilkesiyle tutarlı.
5. **Kaynak türü korunur:** `&yetki<MMIO>` yerine `yetki<Soket>` verilemez
   (D-733 MM002/MM003 aynen geçerli).
6. **Değerle parametre DEĞİŞMEZ** (geriye uyum): `yetki<R>` almak hâlâ
   sahiplik devridir; iade deseni çalışmaya devam eder.

Neden bu: kural 3'ün her maddesi mevcut makinede bir karşılığa sahip —
L004 (lineere referans), G005 (kaçan kapanış), D-505 (görev yakalaması),
LR002 (kapsayıcıya lineer koyma). Yeni bir analiz icat edilmez; mevcut kaçış
yüklemleri ödünç bağlamaya uygulanır.

## 3. Reddedilen seçenekler
- **Ömür parametreli ödünç** (`&'a yetki`): ifade gücü fazla, sözdizimi yükü
  dilin ilkesine aykırı; sürücülerde gerçek bir ihtiyaç ölçülmedi.
- **Yetkiyi kopyalanabilir yapmak:** iptal (`geri_al`) sonrası kullanım
  güvencesini (D-503) yok eder.
- **Yalnız iade desenini belgelemek:** ölçülen ergonomi sorunu kalır.

## 4. Kabul ölçütleri (D2b–e)
- Korpus fikstürü: olumlu (ödünç ver → çağrıdan sonra `geri_al` OK; iç içe
  ödünç), olumsuz (kural 3'ün her maddesi için ayrı satır → CP006; tüketilmiş
  yetkiyi ödünç verme → CP005).
- C ve self-host (`selfhost/codegen.kem`) kod+satır+sütun birebir
  (`checker_diff`); codegen: `&yetki<R>` = `%kdl_yetki`'ye işaretçi, davranış
  `codegen_diff` ile.
- Sabotaj: CP006'nın her dalını ayrı kapat → kapı kırmızı.
- Sürücü göçü (ayrı iş): `drivers/virtio` iade desenleri `&yetki<MMIO>`'ya
  çevrilir, davranış testleri aynı kalır.
