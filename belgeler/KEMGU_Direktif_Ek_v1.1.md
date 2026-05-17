# KEMGU Direktifi Eki — v1.1 KALIBRASYON

**Onay tarihi:** 2026-05-12
**Statü:** Aktif (v1.0'ın yerine geçer)
**Bağımlılık:** Ana direktif Bölüm 4 (ASLA listesi) + v1.0 Bölüm B (Linear Types Spec V1) aynen geçerli.

## Özet (v1.0'dan değişiklik)

v1.0'daki "30 dk Sarı bekleme" hatası düzeltilir. Agent insan-zamanlı senkronizasyon noktasından kurtulur. Mehmet darboğaz olmaktan çıkar.

## A — Katman Anlamları (yeni)

### A.1 — 🟢 Yeşil — Raporsuz Devam
Bildirim YOK. Checkpoint'te toplu özet.

### A.2 — 🟡 Sarı — Post-Facto Bildirim
Yap + raporda bildir (aynı anda). 30 dk bekleme **iptal**. Format:
```
[SARI YAPILDI] <başlık>
DEĞİŞEN: <dosya/modül>
GERİ ALMA: <yöntem>
GEREKÇE: <tek cümle>
```

### A.3 — 🔴 Kırmızı — Queue, Durma
Karar talebi `KIRMIZI_QUEUE.md`'ye yazılır, agent diğer Yeşil/Sarı işlere geçer.

## B — Spec-İçi Ön-Onay
Kırmızı spec onaylanınca → o spec'in tüm Yeşil + Sarı alt-adımları ön-onaylı.
**Linear Types Spec V1** içindeki tüm intrinsic/built-in/stdlib alt-adımları otomatik onaylı.

## C — Checkpoint
Tetikleyiciler: 4-6 saatlik blok / spec tamamlandı / 50+ test eklendi / milestone / tıkanma.

Format:
```
## Checkpoint #N — <tarih>
### Yapıldı
### Test/metrik
### Kırmızı queue
### Sonraki checkpoint hedefi
```

## D — KIRMIZI_QUEUE.md Formatı
```
## [K-N] <başlık>
TARİH: <ISO>
BAĞLAM:
SORU: (≤5 satır)
SEÇENEKLER: (a/b/c)
ETKİ EĞER ŞİMDİ CEVAPLANMAZSA:
ALTERNATIF YOL:
```

## E — Acil Durum (anında Mehmet)
1. Testlerin %5+'i kırık + geri alma kolay değil
2. DRF/teorem sessizce bozuldu
3. Sarı'nın Kırmızı olduğu fark edildi
4. Mehmet'in açık talimatıyla çelişki

## F — Rapor Minimizasyonu
- Yeşil: checkpoint'te toplu
- Sarı: rapor alt-bölümü
- Kırmızı: rapor + queue dosyası
- **YASAK**: "Devam edebilir miyim?" / "Sıradakini yapayım mı?" / "Bu doğru mu?"
- Sessizlik = onay

## G — Tahmin
İnsan-saati yasak. "1 checkpoint" / "Spec B.8 tamamlanana kadar" / "tüm Yeşil alt-adımlar".

## H — Geçerlilik
Onayından itibaren. v1.2 → uzun süre sonrası ince ayar.
