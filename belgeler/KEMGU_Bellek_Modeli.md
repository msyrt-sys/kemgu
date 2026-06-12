# KEMGU Bellek Modeli Formalizasyonu

## λ-KEMGU Core Calculus

Bu belge KEMGU'nun bölge tabanlı bellek modelinin formal tanımını içerir.
Üç katmanlı formalizasyon: Bölge yaşam döngüsü → Concurrency → Güvensiz blok.

---

## Altı Kritik Tasarım Kararı

| # | Konu | Karar | Seçenek |
|---|------|-------|---------|
| 1 | Referans semantiği | Hibrit: otomatik referans + & her yerde + * güvensiz blokta | C |
| 2 | Büyük veri yapıları | Homojen + compiler bölme optimizasyonu | C |
| 3 | Concurrency | Bölge sahipliği modeli (her bölge tek thread'e ait) | B |
| 4 | Closure | Tam closure (yakalama + bölge kuralları) | C |
| 5 | Koşullu dallanma | LCA otomatik üst sınır | A |
| 6 | Güvensiz blok | & her yerde, * güvensiz blokta | A |

---

## Katman 1 — Bölge Yaşam Döngüsü ve Otomatik Atama

### Tipler

**Basit (σ):** tam8-64, dtam8-64, kesirli32-64, mantıksal, karakter, boş
**Bileşik (τ):** yapı{l:τ}, Dizi<τ>, metin, seçimlik<τ>, sonuç<τ,η>, &τ, *τ

### Bölge Tipleri (ρ)

```
ρ_yerel(f)       — fonksiyon f'nin yerel bölgesi
ρ_çağıran(f)     — f'yi çağıran fonksiyonun bölgesi
ρ_iterasyon(d)   — döngü d'nin iterasyon bölgesi
ρ_global         — global bölge (program ömrü)
ρ_sahip(t)       — thread t'ye ait bölge
ρ_kanal(k)       — kanal k'nın transfer tamponu
```

### Bölge Ömrü Aksiyomları

```
ρ_iterasyon(d) ≤ ρ_yerel(f)    eğer d, f içindeyse
ρ_yerel(f) ≤ ρ_çağıran(f)
ρ_yerel(f) ≤ ρ_global
```

### Bölge Atama Kuralları

**R-LIT:** Basit literal → bölge yok (stack)
**R-YEREL:** Escape etmeyen bileşik değer → ρ_yerel(f)
**R-VER:** `ver` ile döndürülen değer → ρ_çağıran(f)
**R-GÖMME:** Veri yapısına gömülen değer → hedefin bölgesi
**R-İTERASYON:** Döngüden escape etmeyen değer → ρ_iterasyon(d)
**R-KOŞUL:** Koşullu dallanma → iki dalın bölgelerinin LCA'sı
**R-BÖLME:** Kısa ömürlü koleksiyon elemanı → alt bölge (optimizasyon)

### Closure Bölge Etkileşimi

**YD(c)** = closure c'nin yakaladığı değişkenler kümesi (Yakalama Düzeni)

**R-YAKALAMA (Ana Kural):**
```
Γ ⊢ c : closure    YD(c) = {v₁, v₂, ..., vₙ}
∀ vᵢ ∈ YD(c) : bölge(vᵢ) = ρᵢ
bölge(c) = ρ_c

────────────────────────────────────────────
∀ vᵢ ∈ YD(c) : ρ_c ≤ ρᵢ   (bölge terfi zorunluluğu)
```

Closure'ın bölgesi, yakaladığı her değişkenin bölgesinden daha kısa ömürlü veya eşit olmalı.
Eğer closure daha uzun yaşıyorsa, yakalanan değişkenler closure'ın bölgesine terfi ettirilir.

**R-YAKALAMA-YEREL** (escape yok):
```
c escape etmiyor ∧ ∀ vᵢ ∈ YD(c) : ρᵢ = ρ_yerel(f)
─────────────────────────────────────────────────────
bölge(c) = ρ_yerel(f)    // terfi gerekmez
```

**R-YAKALAMA-ESCAPE** (closure döndürülüyor):
```
c escape ediyor (ver ile)    bölge(c) = ρ_çağıran(f)
∀ vᵢ ∈ YD(c) : ρᵢ = ρ_yerel(f)
──────────────────────────────────────────────────────
∀ vᵢ ∈ YD(c) : terfi(vᵢ, ρ_çağıran(f))
```

**R-YAKALAMA-THREAD** (closure başka thread'e geçiyor):
```
c thread t'ye transfer ediliyor    bölge(c) = ρ_sahip(t)
∀ vᵢ ∈ YD(c) : ρᵢ = ρ_yerel(f)
──────────────────────────────────────────────────────────
∀ vᵢ ∈ YD(c) : sahiplik_transfer(vᵢ, ρ_sahip(t))
∧ ρ_yerel(f) artık vᵢ'ye erişemez   (move semantics)
```

### Terfi Mekanizması

```
terfi(v, ρ_hedef) =
  eğer basit(tip(v)):
    v' = kopya(v, ρ_hedef)          // değer kopyası
    closure v' kullanır
  değilse:
    ömür_uzat(bölge(v), ρ_hedef)    // bölge serbest bırakma ertelenir
    closure orijinal v'yi kullanır

sahiplik_transfer(v, ρ_sahip(t)) =
    bölge(v) → ρ_sahip(t)           // sahiplik geçer
    ρ_yerel(f) erişim(v) = ⊥        // kaynak erişim kaybeder (move)
```

---

## Katman 2 — Bölge Sahipliği ve Concurrency

### Sahiplik Aksiyomları

```
sahip : ρ × T → {doğru, yanlış}

S1 (Tekil Sahiplik):
  ∀ ρ, ∀ zaman noktası t_z :
    |{t ∈ Threads : sahip(ρ, t, t_z) = doğru}| = 1

S2 (Başlangıç Sahipliği):
  bölge ρ, thread t içinde yaratıldı ⟹ sahip(ρ, t, t_yaratım) = doğru

S3 (Atomik Transfer):
  sahip(ρ, t₁, t_z) = doğru ∧ transfer(ρ, t₁, t₂, t_z)
  ⟹ sahip(ρ, t₂, t_z+1) = doğru ∧ sahip(ρ, t₁, t_z+1) = yanlış
```

### R-GÖREV — Thread Oluşturma

```
Γ ⊢ c : closure    YD(c) = {v₁, ..., vₙ}
t_yeni = yeni_thread()    ρ_yeni = ρ_sahip(t_yeni)

─────────────────────────────────────────────────────────
∀ vᵢ ∈ YD(c) : sahiplik_transfer(vᵢ, ρ_yeni)    [R-YAKALAMA-THREAD]
Γ' ⊢ görev_başlat(c) : görev<T>
  burada Γ' = Γ \ {v₁, ..., vₙ}                  [move: kaynak erişim kaybeder]
```

### R-BİRLEŞTİR — Thread Birleştirme

```
Γ ⊢ g : görev<T>    t_hedef = g.thread
g.birleştir() çağrıldı, t_hedef sonlandı, dönüş değeri r : T

──────────────────────────────────────────────────────────────
bölge(r) terfi → ρ_çağıran(f)
∀ ρ ∈ sahip_bölgeler(t_hedef) \ bölge(r) : serbest(ρ)
t_hedef sonlandırılır
```

### R-KANAL — Mesaj Geçişi

**Gönderim:**
```
Γ ⊢ gönderen : gönderen<T>    Γ ⊢ v : T

───────────────────────────────────────────────────────
sahiplik_transfer(v, ρ_kanal(k))
Γ' = Γ \ {v}
```

**Alım:**
```
Γ ⊢ alan : alan<T>    v kanal k'dan alındı

────────────────────────────────────────────────────
sahiplik_transfer(v, ρ_sahip(t_alan))
```

### R-PAYLAŞ — Salt-Okunur Bölge Dondurma

```
dondur(v) çağrıldı    bölge(v) = ρ
──────────────────────────────────────────────────────
değiştirilebilir(ρ) = yanlış
birden fazla thread ρ'yu okuyabilir   [S1 istisnası]
hiçbir thread ρ'ya yazamaz
```

**Çözme:** Tüm okuyucu thread'ler bittiğinde → değiştirilebilir(ρ) = doğru

---

## Katman 3 — Güvensiz Blok ve Bölge Bölme

### Güvensiz Blokta İzin Verilenler

```
G1: Ham pointer oluşturma        *T
G2: Ham pointer dereference       *ptr
G3: Pointer aritmetiği            ptr + n
G4: Tip dönüştürme (unchecked)    dönüştür<*tam8>(adres)
G5: Bölge sahipliğini atlama      başka thread'in bölgesine ham erişim
```

### Sınır Kuralları

**SINIR-1:** Güvensiz bloktan çıkan değer güvenli tip kurallarına tabi
**SINIR-2:** `&τ` döndürülebilir ama bölge ömür kontrolü uygulanır
**SINIR-3:** `*τ` (ham pointer) güvensiz bloktan ÇIKAMAZ → derleme hatası

### Referans Kuralları

**R-REF (Güvenli Referans):**
```
Γ ⊢ &v : &T
zorunluluk: ρ_ref ≤ ρ_v    [referans hedefinden uzun yaşayamaz]
```

**R-REF-DEĞ (Değiştirilebilir Referans):**
```
Γ ⊢ &değişken v : &değişken T
zorunluluk: ρ_ref ≤ ρ_v
zorunluluk: aynı anda en fazla 1 adet &değişken v    [aliasing yasağı]
```

**R-REF-ÇAKIŞMA:**
```
∃ &değişken v  ⟹  ∄ &v
∄ &değişken v  ⟹  &v sayısı sınırsız
```

### Escape Analizi ve Bölge Bölme

```
ESC-0 (Kaçmaz):     scope'tan çıkmaz → ρ_yerel veya ρ_iterasyon
ESC-1 (Çağırana):    ver ile döner → ρ_çağıran
ESC-2 (Belirsiz):    gömülme, yakalama, koşullu dal → bağlam-duyarlı analiz
```

**R-BÖLME (Döngü Alt-Bölge):**
```
Γ ⊢ döngü d { e_gövde }
P_kaçmaz = {p ∈ tahsisler(e_gövde) : escape(p, d) = ESC-0}
P_kaçar = tahsisler(e_gövde) \ P_kaçmaz

∀ p ∈ P_kaçmaz : bölge(p) = ρ_iterasyon(d)
∀ p ∈ P_kaçar  : bölge(p) = escape_hedef(p)
iterasyon_sonu(d) → serbest(ρ_iterasyon(d))
iterasyon_başı(d) → yeni(ρ_iterasyon(d))
```

---

## Güvenlik Teoremleri

**Teorem 1 — Bellek Güvenliği:** Serbest bırakılmış bölgeye erişim yok
  *(V1 bundled mekanize 2026-05-18: `proofs/drf-v2-lean/Kemgu/MemSafety/Theorems.lean`*
  *— `t1_bellek_guvenligi_tam`; sAtama h_owner garantisi ile UAF prevention.*
  *V2 T2/T3 + lifecycle: bkz. `KEMGU_Metateorem_V3.md`.)*
**Teorem 2 — Bölge Güvenliği:** Her bölge tam 1 kez yaratılır, 1 kez serbest bırakılır
**Teorem 3 — Sızıntısızlık:** Erişilemeyen bölge sonlu sürede serbest bırakılır
**Teorem 4 — Data Race Freedom:** Güvenli alt kümede data race imkansız
**Teorem 5 — Güvensiz Sınır Bütünlüğü:** Ham pointer güvensiz bloktan çıkamaz
**Teorem 6 — Bölge Bölme Doğruluğu:** Optimizasyon gözlemlenebilir davranışı değiştirmez

**V3 Bütünleşik Metateorem M** (2026-05-18, V1 bundled mekanize):
T1 + Teorem 4' + SCR (placeholder) + BET (placeholder) → birleşik
`kemgu_soundness_v3`. Bkz. [`KEMGU_Metateorem_V3.md`](KEMGU_Metateorem_V3.md).

### Teorem 4 İspat Taslağı (Tarihsel — V1 öncesi informel)

> **NOT (2026-05-15):** Aşağıdaki 4-bullet taslak V1 formel ispatının
> (`KEMGU_DRF_Teoremi.md` — Teorem 4', İyiTipli(Π) üzerinden SC altında)
> **öncesidir.** Geçerli ispat orada; bu blok yalnız tarihsel amaçla
> korunur. Aşağıdaki bulletler bağımsız olarak yanıltıcıdır (S1
> runtime invaryantı olarak kabul ediliyor, ispatlanmıyor; closure
> yakalamada lineerlik ile bölge taşıyıcılarının ayrımı yok).

Data race: iki thread aynı belleğe eşzamanlı erişir, en az biri yazma.

- **Değiştirilebilir bölge:** S1 → tek sahip → iki thread erişemez ∎
- **Donmuş bölge:** yazma yasak → data race tanımı karşılanmaz ∎
- **Kanal transferi:** S3 → atomik transfer → asla iki sahip yok ∎
- **Closure yakalama:** R-YAKALAMA-THREAD → move → kaynak erişim kaybeder ∎

### Teorem 4' — Genişletilmiş DRF (2026-05-14)

Linear Types V1 + Capability V1 + Sabitsüre V1 katmanlarının eklenmesiyle
Teorem 4'ün **formel genişletilmiş versiyonu** ayrı dökümanlarda yazıldı:

- [`KEMGU_DRF_Genisletme_Plan.md`](KEMGU_DRF_Genisletme_Plan.md) — plan + kararlar
- [`KEMGU_Operasyonel_Semantik.md`](KEMGU_Operasyonel_Semantik.md) — küçük-adım semantik, izler, happens-before, data race formel tanımı
- [`KEMGU_DRF_Lemmalar.md`](KEMGU_DRF_Lemmalar.md) — DRF-L1..L7 ara lemmaları
- [`KEMGU_DRF_Teoremi.md`](KEMGU_DRF_Teoremi.md) — Teorem 4' tam ispatı (V1 statik)

Yukarıdaki Teorem 4 ispat taslağı **korunur** (geriye uyumluluk + okur için
hızlı özet). Teorem 4' onun **formel genişletilmiş varyantıdır**;
`İyiTipli(Π) ⟹ ∀ τ ∈ Tr(Π) : ¬ data_race(τ)` ifadesi ile yapısal indüksiyon
ispatı içerir. Lang syntax (`görev`/`kanal`) eklendikten sonra anlamlı hale
gelir (şu an `Tr(Π)` her zaman tek-thread izleri içerdiğinden trivially
korunur).

---

## MLKit ve Rust Karşılaştırması

| Özellik | MLKit | Rust | KEMGU |
|---------|-------|------|-------|
| Bölge ataması | Programcı + compiler | N/A (ownership) | Tamamen compiler |
| Büyük bölge sorunu | Var (bilinen) | N/A | Bölge bölme ile çözülür |
| Programcı annotation | Az | Lifetime annotation zorunlu | Sıfır |
| Concurrency güvenliği | Yok | Borrow checker | Bölge sahipliği |
| Closure yönetimi | Basit | Fn/FnMut/FnOnce üçlemesi | Bölge kurallarıyla |
| Debug bilgisi | Yok | Yok (compile error) | Bölge haritası |
