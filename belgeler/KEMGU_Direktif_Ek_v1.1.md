## KEMGU Geliştirme Direktifi — Ek v1.1 (Operasyonel Kalibrasyon)

**Tarih:** 2026-05-12 (onaylı, v1.0 operasyonel kuralları yerine geçer).
**Tasarım belgeleri** (v1.0 Bölüm B – Linear Types Spec V1 dahil) **aynen geçerlidir**.
**Ana direktifin Bölüm 4 (ASLA listesi)** aynen geçerlidir.

Bu ek, agent-tarafı operasyonel kuralları sadeleştirir. Ana hedef: konuşma trafiğini
azaltmak, agent'ı 100x hızlı çalıştırmak, kararları toplu hale getirmek.

---

### A) Üç Karar Katmanı

| Renk | Bildirim | Kapsam | Kural |
|------|----------|--------|-------|
| 🟢 Yeşil | Yok (checkpoint'te toplu) | Test, codegen, refactor, doc, bug fix, perf, mevcut özelliklerin spec-içi genişletilmesi | Direkt yap |
| 🟡 Sarı | Post-facto (yapıldıktan sonra raporda) | Sözdizimine küçük ek, yeni stdlib modülü, internal API, yeni hedef, yeni flag, yeni built-in, FFI, mevcut tasarım çerçevesi içinde yeni intrinsic | Yap + raporda `[SARI YAPILDI]` etiketle |
| 🔴 Kırmızı | Karar talebi `KIRMIZI_QUEUE.md`'ye | Tip sistemine yeni katman, formal teorem etkisi, breaking change, yeni anahtar kelime, yeni unsafe primitif, ABI değişikliği, concurrency modeli değişikliği | Queue'ya ekle, başka işe geç |

**Sadece tüm yapılabilir iş Kırmızı'ya bağımlıysa dur.**

---

### B) Spec-İçi Ön-Onay

Bir Kırmızı spec onaylanınca → o spec'in tüm Yeşil + Sarı alt-adımları
**otomatik onaylı**. Ayrı bildirim atmadan ilerlenir.

**Onaylı specler:**
- **Linear Types Spec V1** (`tekkez<T>` + `kullan` + `imha` + stdlib + intrinsic'ler)
  → Detay: `belgeler/KEMGU_Linear_Types_Spec_V1.md`
- (gelecek)

---

### C) Checkpoint (Rapor Noktası)

Tetikleyiciler:
1. 4–6 saatlik blok
2. Bir Kırmızı spec'in tüm Yeşil/Sarı alt-adımları bitti
3. 50+ yeni test eklendi
4. Önemli milestone
5. Tıkanma (build kırık, queue dışı sorun)

Format:
```
## Checkpoint #N — <tarih>
### Yapıldı (Yeşil tek satırlık + [SARI YAPILDI] altları)
### Test/metrik (toplam X/X, Δ, ASan)
### Kırmızı queue (bekleyen)
### Sonraki checkpoint hedefi
```

---

### D) YASAK İfadeler

- "Devam edebilir miyim?"
- "Sıradakini yapayım mı?"
- "Bu doğru mu?"
- İnsan-saati tahmini ("20 saatlik iş" — agent 100x hızlı, anlamsız)

**Sessizlik = onay.** Mehmet checkpoint görüp cevap vermezse, sonraki hedefe geç.

---

### E) Acil Durum (anında Mehmet)

1. Testlerin %5+'i kırık + geri alma kolay değil
2. DRF / teorem sessizce bozuldu
3. Bir Sarı işin Kırmızı olduğu fark edildi
4. Mehmet'in açık talimatıyla çelişki

---

### F) Tahmin

İnsan-saati yasak. Yerine:
- "1 checkpoint" = 4–6 saat ≈ orta spec'in Yeşil alt-adımları
- "Spec B.8 minimumuna kadar" / "tüm Yeşil alt-adımlar bitene kadar"

---

### G) ASLA Listesi (Ana Direktif Bölüm 4 — değişmez)

- Null pointer (yerine `seçimlik<T>`)
- Exception (yerine `sonuç<T,H>`)
- Garbage collector (region tabanlı)
- Implicit type conversion
- `güvensiz` normalleştirme
- C macro tarzı tekstüel substitution
- Cryptic hata mesajı
- Performance regression
- Akademik saflık için ergonomi feda

---

### H) Uygulama Akışı

1. Görev başlamadan **hangi katman** olduğunu belirle.
2. Spec içiyse → Yeşil/Sarı ayrımı bile yapma, direkt yap.
3. Spec dışı Sarı → yap, raporda `[SARI YAPILDI]` yaz.
4. Spec dışı Kırmızı → `KIRMIZI_QUEUE.md`'ye ekle, başka işe geç.
5. Acil durum hariç checkpoint'e kadar mesaj atma.
6. Çerçeve içindeysen YAP — sorma.
