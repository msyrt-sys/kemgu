# KEMGU Direktifi Eki — Yetki Katmanları + Linear Types Meta-Spec

**Versiyon:** v1.0
**Onay tarihi:** 2026-05-12 (Mehmet sözlü onayı)
**Statü:** Aktif — bu döküman ana direktifin Bölüm 6'sını ince taneli hale getirir.

> Bu döküman ana direktife (`kemgu_development_directive.md`) eklenir,
> onun yerine geçmez. Ana direktifin Bölüm 6 (Karar Sorulması Gereken
> Durumlar) ve Bölüm 4 (ASLA listesi) hâlâ geçerlidir.

---

## A. YETKİ KATMANLARI

### A.1 — Yeşil Katman (Tam Otomatik)

**Yetki:** Sormadan yap. Toplu raporda bildir.

**Kapsam:**
- Onaylanmış özelliklerin implementasyonu
- Test yazma (unit, integration, snapshot, property-based, fuzz)
- Codegen iyileştirme (mevcut semantiği koruyan)
- Refactor (mevcut public API'yi koruyan)
- Hata mesajı iyileştirme
- Performans optimizasyonu (semantik değişikliği olmayan)
- Dokümantasyon
- Build sistemi, CI, tooling
- LSP server iyileştirmeleri
- Mevcut özelliklerin **mevcut tasarım çerçevesi içinde** genişletilmesi
- Bug fix
- Test coverage genişletme, lcov, bench JSON
- Generic monomorphization codegen
- Mevcut pattern matching scope hatalarını düzeltme

**Yasak:** Bu kategori içinde olduğunu sandığı bir iş sırasında
sözdizimi/tip sistemi/teorem etkisi fark ederse durur, Sarı veya
Kırmızı'ya yükseltir.

### A.2 — Sarı Katman (5 Satırlık Özet, Asenkron Onay)

**Yetki:** Kısa özet bildir, **30 dakika** itiraz olmazsa başla.

**Kapsam:**
- Mevcut sözdizimine küçük ek
- Yeni stdlib modülü (mevcut tip sistemini kullanan)
- Internal API ekleme
- Yeni hedef mimari desteği
- Yeni derleyici flag'i
- Yeni diagnostic kategorisi
- Yeni built-in fonksiyon (mevcut tipler üzerinde)
- Üçüncü-parti C kütüphanesi için FFI sarmalayıcı

**Bildirim formatı (TAM 5 satır):**
```
[SARI] <başlık>
GEREKÇE: <neden gerekli, tek cümle>
ETKİ: <hangi modüller/dosyalar değişecek>
RİSK: <geri alma maliyeti tahmini>
BAŞLANGIÇ: <UTC zaman> + 30dk = otomatik onay
```

### A.3 — Kırmızı Katman (Spec Şart)

**Yetki:** Yok. Spec olmadan implementasyon kesinlikle yok.

**Kapsam:**
- Tip sistemine yeni katman
- DRF veya başka formal teoremi etkileyen değişiklik
- Yeni teorem ifadesi/ispatı
- Public API breaking change
- Yeni anahtar kelime
- Yeni `unsafe` primitif
- Region semantiğinde değişiklik
- ABI değişikliği
- Concurrency modelinde değişiklik
- Capability/permission modelinin temel ilkesi

### A.4 — Keşif Dalı Protokolü

Kırmızı ve karmaşık Sarı işler için: `kesif/<konu>` Git dalı.

### A.5 — Güven Kademesi (Trust Ratchet)

Başarılı kararlar üst üste verildikçe agent bir alt katmana iner.

**Şu anki başlangıç pozisyonu:**
- Tüm tip sistemi → Kırmızı
- Tüm sözdizimi → Kırmızı (boyut + attribute istisnası post-hoc onaylı)
- Stdlib modülleri → Sarı
- Codegen / LLVM IR → Yeşil
- Test / tooling / refactor → Yeşil
- Hata mesajı / dokümantasyon → Yeşil

### A.6 — Toplu Spec Oturumu

Haftada bir, Mehmet'in seçeceği 1 saatlik blokta.

### A.7 — Tereddüt Edildiğinde

Bir üst katmanı seç.

---

## B. LINEAR TYPES META-SPEC v1

### B.1 — Affine Semantik

`tekkez<T>` tipi **affine**: en fazla bir kez tüketilir, asla sessizce
düşmez. İki tüketim: `kullan(x, ...)` veya `imha(x)`.

### B.2 — Sözdizimi

```kemgu
işlev üret_anahtar() -> tekkez<[u8; 32]> { ... }

let anahtar = üret_anahtar();
let şifreli = kullan(anahtar, |k| şifrele(k, mesaj));

let anahtar2 = üret_anahtar();
imha(anahtar2);
```

Yeni anahtar kelime: `tekkez` (32.).
Yeni built-in'ler: `kullan`, `imha`.

### B.3 — Region Etkileşimi

Linearity ⊥ region. Ama linear değer LIT region'da olamaz.

### B.4 — DRF Uzantısı

Tek sahip ⇒ aliasing yok ⇒ race yok. DRF teoremi linear ekleme sonrası
basit uzantı ile yeniden ispatlanır (Kırmızı, ayrı oturum).

### B.5 — Closure Linearity

Linear değer yakalayan closure kendisi linear olur.

### B.6 — İlk Kullanıcı

```kemgu
modül kripto.otp

tip OneTimeKey<sabit N: usize> = tekkez<[u8; N]>
    + sıfırla_imha + sabitsüre
```

### B.7 — İmplementasyon Sırası

1. `tekkez` keyword (Yeşil, ~1 gün)
2. Tip sisteminde `tekkez<T>` (Yeşil, ~2 gün)
3. Linear use-tracking DFA (Yeşil, ~3-5 gün)
4. `kullan` + `imha` (Yeşil, ~2 gün)
5. Closure linearity inference (Yeşil, ~3 gün)
6. `sıfırla_imha` trait + codegen (Yeşil, ~2 gün)
7. Hata mesajları + IDE (Yeşil, ~2 gün)
8. Test suite (≥50 yeni test, Yeşil, ~3 gün)
9. `OneTimeKey<N>` stdlib (Sarı, ~2 gün)
10. DRF uzantı ispatı (Kırmızı, ayrı oturum)

**Feature flag:** `--experimental-linear` — flag kapalıyken `tekkez`
parser hatası.

### B.8 — Test Stratejisi

Minimum kabul: 50 yeni test (≥411 + 50 = 461).

---

## C. GEÇERLILIK

Bu ek, ana direktife ek olarak geçerlidir. Çelişki olursa ana direktif
kazanır.
