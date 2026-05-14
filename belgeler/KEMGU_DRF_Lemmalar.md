# KEMGU DRF Lemmaları (DRF-L1..L7) V1

**Tarih:** 2026-05-14
**Durum:** TASLAK FORMALİZASYON — kâğıt üzerinde
**Önkoşul:** [`KEMGU_Operasyonel_Semantik.md`](KEMGU_Operasyonel_Semantik.md)
**Plan referansı:** [`KEMGU_DRF_Genisletme_Plan.md`](KEMGU_DRF_Genisletme_Plan.md), Bölüm 5

Bu belge, KEMGU'nun **Genişletilmiş DRF Teoremi** (`Teorem 4'`) ispatında
kullanılan 7 ara-lemmayı tanımlar. Lemmaların **bağımlılık grafı**
(`KEMGU_DRF_Genisletme_Plan.md` Bölüm 5.2'de) ve ispat sırası:

```
        DRF-L7  →  DRF-L1
                     ↓
                  DRF-L2  →  DRF-L3
                                ↓
                  DRF-L4   →   DRF-L5  →  DRF-L6
                       ↘  ↓  ↗
                       Teorem 4'
```

---

## Notasyon Hatırlatması

`KEMGU_Operasyonel_Semantik.md` Bölüm 2-7'den:

| Sembol | Anlam |
|--------|-------|
| Π | KEMGU programı |
| τ | Tip |
| Γ | Tip ortamı |
| Λ | Lineerlik durumu (Var → {AKTİF, TÜKETİLDİ}) |
| Ρ | Bölge ortamı (AST → BolgeBilgisi) |
| Σ | Sahiplik haritası (ρ × Zaman → Thread ∪ {⊥, DONMUŞ}) |
| σ | Store (Konum → Değer) |
| K⃗ | Kanal durumları |
| ⟹ | Sistem küçük-adım reduksiyonu |
| Tr(Π) | Π'nin tüm reduksiyon izleri kümesi |
| ≺_hb | Happens-before bağıntısı |
| İyiTipli(Π) | 7 koşulun hepsi geçer (Op.Sem. §7) |

---

## Lemma DRF-L7 — Tip-Soundness for Memory Access

**(Bağımlılık grafının kökü; diğer lemmalar buna dayanır.)**

### İfade

```
İyiTipli(Π) ∧ Π ⟹* S = ⟨T⃗, σ, Σ, K⃗⟩ ∧
mem_op(t, ρ, ofs, v) ∈ S.gözlemler

⟹

(a) ∃ τ : σ(ρ, ofs) = v ile v : τ uyumlu
(b) ρ ≠ ρ_lit (literal'lar stack'te, mem_op yapılmaz)
(c) Ρ haritası bu erişimi etiketler: ∃ AST düğümü e : Ρ(e) = ρ
```

### Varsayımlar (Açık)

- A1 (Tip Korunumu) — Op.Sem. §8
- Bellek modeli SC (Plan Karar F)
- `güvensiz` blok yok (Plan Karar H — izolasyon modu)

### İspat (Yapısal İndüksiyon Π'nin Reduksiyonu Üzerine)

**Temel:** Π'nin başlangıç konfigürasyonu `⟨e₀, σ₀, ∅, ∅⟩` — mem_op yok.
Önerme boş olarak doğru.

**İndüktif:** Tek-adım `S ⟹ S'`. Sadece mem_op üreten kuralları
incele:

- **S-VAR (okuma):** `σ(Ρ(x), 0) = v` zaten σ'da. `x : τ` tip kontrol
  geçti varsayımı (A1). Ρ(x) zaten `bölge_atama` sırasında atanmış.
  ∴ (a), (b), (c) sağlanır.

- **S-ATAMA (yazma):** `σ'' = σ'[Ρ(x), 0 ↦ v]`. RHS'in tipi τ_e;
  tip kontrol `τ_e ≤ τ_x` (atama soundness) garanti etti.
  Ρ(x) atandı; ρ_lit değil (literal'lara atama yasak — T022 lvalue).
  ∴ (a), (b), (c) sağlanır.

- **S-LIN-KULLAN, S-LIN-İMHA:** mem_op değil (sadece Λ değişir); önerme
  trivial.

- **C-KANAL-GÖNDER/AL:** v σ'da kalır; sadece Σ değişir (sahiplik
  transfer). mem_op okuma/yazma yok bu adımda.

- **C-DONDUR:** sadece Σ değişir; mem_op yok.

Tüm kurallar tüketildi. ∎

---

## Lemma DRF-L1 — Region-Thread Tekilliği

### İfade

```
İyiTipli(Π) ∧ Π ⟹* S = ⟨T⃗, σ, Σ, K⃗⟩

⟹  ∀ ρ ∉ {ρ_donmuş, ρ_lit, ρ_global, ρ_kanal(_)}, ∀ z :
    |{t : Σ(ρ, t, z) = t ∧ t ∈ Threads(τ)}| ≤ 1
```

### Yorum

Donmuş bölge çoklu okuyucu için istisna; `ρ_lit` bellek'te değil; `ρ_global`
tüm thread'lere ait (program ömrü, çoklu okuyucu); kanal bölgeleri ayrı
S3 ile yönetilir.

### Varsayımlar

- A2 (Bölge Korunumu) — Op.Sem. §8
- S1, S2, S3 (Bellek Modeli Katman 2 aksiyomları)
- R-GÖREV, R-KANAL, R-PAYLAŞ kuralları (Op.Sem. §5.4)

### İspat (Yapısal İndüksiyon Π'nin Reduksiyonu Üzerine)

**Temel:** Π başlangıçta tek thread `t₀`. `Σ₀(ρ_yerel(main), 0) = t₀`,
diğer ρ'lar `⊥` veya t₀. Tekillik trivial.

**İndüktif:** S ⟹ S'. Σ'yi değiştiren kurallar:

- **C-GÖREV-BAŞLAT:** `Σ' = Σ[bölge(v_i) ↦ t_yeni]` for v_i ∈ YD⃗ (yakalama
  listesi). Önceki sahibi: t₁ (çağıran). Yeni sahibi: t_yeni.
  - **Linear yakalananlar:** L-NO-COPY (Linear V1 L-NO-COPY) zorlamış;
    sadece move. Λ₁'den silinir (Λ₁' = Λ₁ \ YD⃗_lin). t₁ artık erişemez.
  - **Non-linear yakalananlar:** kopyalanır; ama orijinal hâlâ t₁'in
    bölgesinde — değiştirilebilir mi? R-YAKALAMA-THREAD (Bellek Modeli)
    her yakalanan v_i için move semantiği zorlar (Op.Sem. §5.4 kuralı).
  - **Yeni Σ':** her ρ ∈ transferred için tek sahip t_yeni; tekillik
    korunur. ∎

- **C-KANAL-GÖNDER:** `Σ' = Σ[bölge(v) ↦ ρ_kanal(g_id)]`. ρ_kanal kümesi
  donmuş değil ama özel kategoride. Tekrar: v'nin önceki sahibi t silindi.
  ∴ tekillik korunur (ρ ↦ ρ_kanal eşlemesi tek).

- **C-KANAL-AL:** `Σ' = Σ[bölge(v) ↦ ρ_sahip(t_alan)]`. ρ_kanal serbest
  bırakılır; v t_alan'a transfer. Tekillik korunur.

- **C-DONDUR:** `Σ' = Σ[bölge(v) ↦ DONMUŞ]`. Bu bölge artık `ρ_donmuş`
  kategorisinde — lemmanın hariç tutmasıyla denir.

- **C-BİRLEŞTİR:** t_hedef'in ρ_sahip bölgeleri serbest bırakılır; dönüş
  değeri ρ_çağıran'a terfi. Tekillik korunur.

- **Diğer küçük-adım kuralları (S-*):** Σ'yi değiştirmez (sadece σ).

Tüm Σ-değiştiren kurallar tüketildi. ∎

---

## Lemma DRF-L2 — Linear Move = Cross-Thread No-Alias

### İfade

```
İyiTipli(Π) ∧
Γ ⊢ v : tekkez<τ>     veya     Γ ⊢ v : yetki<R>
∧ Π'nin bir izinde C-GÖREV-BAŞLAT(c) reduksiyonu uygulanır ve
  v ∈ YD(c) (yakalama listesinde)

⟹

∀ z > z_görev_başlat :
  v t₁ (çağıran thread)'in scope'unda erişilebilir DEĞİL (Λ₁(v) = TÜKETİLDİ)
```

### Yorum

Lineer yakalanan değer **çağıran thread**'in bağlamından silinir; yalnız
yeni thread'de yaşar. Bu DRF için kritik: t₁ ve t_yeni aynı `v`'ye
**eşzamanlı erişemez**.

### Varsayımlar

- Linear V1: L-NO-COPY (kopyalama yasak), L-NO-ALIAS (referans yasak)
- LC-2 (Closure-Itself-Linear: lineer yakalayan closure otomatik tekkez)
- Capability V1: CP-NO-COPY = L-NO-COPY (CP.1.1 Linear Integration)
- R-YAKALAMA-THREAD (Bellek Modeli)

### İspat

C-GÖREV-BAŞLAT kuralının (Op.Sem. §5.4) son satırı:
```
Λ₁' = Λ₁ \ {YD⃗ ∩ Lineer}
```

`v ∈ YD(c)` ve `Γ ⊢ v : tekkez<τ>` (Lineer kategori) ⟹ `v ∈ YD⃗ ∩ Lineer`.
∴ `Λ₁'(v) = TÜKETİLDİ`.

`yetki<R>` durumu: CP.1.1 ile aynı linear track ⟹ aynı Λ kümesinde.

İndüktif olarak: sonraki adımlarda Λ₁'(v) ↦ AKTİF dönüşümü hiçbir kuralda
yok (Lineer V1: tüketim irreversible). ∴ ∀ z > z_görev_başlat : Λ₁(v) =
TÜKETİLDİ.

Tip kontrol L002 (LINEAR_DOUBLE_USE) zaten tüketildikten sonra erişimi
reddeder. Op.Sem. §4.2 S-LIN-DOUBLE-USE: bu reduksiyon HATA — yani
çalıştırılabilir değil. ∴ t₁ `v`'ye dokunamaz. ∎

---

## Lemma DRF-L3 — Linear Closure Soundness

### İfade

```
İyiTipli(Π) ∧
Γ ⊢ c : tekkez<işlev(τ⃗_arg) -> τ_dönüş>  (LC-2 ile otomatik veya manuel)
∧ C-GÖREV-BAŞLAT(c) uygulandı (t_yeni oluştu)

⟹

c'nin gövdesi (closure kodu) yalnız aşağıdaki bölgelerden okur/yazar:
  - ρ_sahip(t_yeni)
  - YD⃗ üzerinden transfer edilen bölgeler (artık ρ_sahip(t_yeni) içinde)
  - ρ_lit, ρ_global (her thread için ortak okuma)
```

### Varsayımlar

- LC-2 (Linear V1) — lineer yakalayan closure otomatik `tekkez<işlev>`
- LC-3 (Linear V1) — `tekkez<işlev>` çağrısı = `kullan` (tek-kullanım)
- R-YAKALAMA-THREAD — yakalananlar t_yeni'ye move
- DRF-L7 — bellek operasyonu tip-kontrollü

### İspat

İki nokta:

**(a) Yakalama dışı erişim — yok:**

Tip kontrol fazında `c`'nin gövdesindeki tüm tanımlayıcılar:
1. `c`'nin parametreleri (`τ⃗_arg`) — yerel, t_yeni içinde tahsis.
2. `c`'nin yakalama listesindeki değişkenler — `YD⃗`.
3. Global tanımlar (`ρ_global`).
4. Built-in çağrılar (ρ_lit literalleri).

`c`'nin gövdesinde başka bir thread'in bölgesine erişim **mümkün değildir**
çünkü:
- Yakalama listesinde yer almayan dış değişken **scope dışı** (`sembol_bul`
  None dönecekti) — tip kontrol reddeder.
- Pointer/referans aliasing yasak (R-REF-DEĞ → en fazla 1 mutable; Linear
  L-NO-ALIAS lineer için).

**(b) Yakalananlar t_yeni'ye geçti:**

DRF-L2 (yukarıda) ile linear yakalananlar t₁'den silinir, t_yeni'ye aktarılır.
Non-linear yakalananlar R-YAKALAMA-THREAD ile bölge sahipliği değişir
(Bellek Modeli §Closure). Sahiplik haritası `Σ` güncellenir.

DRF-L1 ile her transferred bölgenin tek sahibi t_yeni.

∴ c'nin gövdesinin bellek operasyonları `ρ_sahip(t_yeni) ∪ ρ_lit ∪ ρ_global`
kümesindedir. ∎

---

## Lemma DRF-L4 — Frozen Region Read-Soundness

### İfade

```
İyiTipli(Π) ∧
Π ⟹* S = ⟨T⃗, σ, Σ, K⃗⟩ ∧
∃ z, ρ : Σ(ρ, z) = DONMUŞ      (dondur(v) çağrıldı, bölge(v) = ρ)

⟹

(a) ∀ t ∈ Threads, ∀ z' ≥ z : ¬ mem_yaz(t, ρ, _, _) ∈ S.olaylar
(b) ∀ t₁, t₂ ∈ Threads, ∀ z' ≥ z :
    mem_oku(t₁, ρ, _, _) ∧ mem_oku(t₂, ρ, _, _) izinli
    ∧ data_race(τ) konfigürasyonu oluşturmaz   (sadece okumalar)
```

### Yorum

Donmuş bölgenin S1 istisnası: çoklu okuyucu (S1'in §Bellek Modeli'nde
"S1 istisnası" açıkça not edilmiş). Yazma yasak.

### Varsayımlar

- R-PAYLAŞ (Bellek Modeli §Katman 2)
- C-DONDUR (Op.Sem. §5.4) — Σ[ρ ↦ DONMUŞ]
- Plan Karar E hibrit: `dondur` builtin call + sembol tablosu flag

### İspat

**(a) Yazma yasak:** C-DONDUR sonrası Σ(ρ, z) = DONMUŞ. Tip kontrol fazı:
- `dondur(v)` çağrısı v'nin tipini `&değişken T` → `&T` dönüştürür
  (immutable referans).
- Plan Karar E hibrit: sembol tablosunda `donmus_mu` flag set;
  `tip_kontrol_atama` lvalue check'inde `donmus_mu` ise T022 lvalue hatası
  (yeni hata kodu — Bölüm 7.E'de belirtilen). Bu hata derlemeyi reddeder.

∴ İyiTipli(Π) varsayımı altında yazma reduksiyonu **gerçekleşmez**. ∎

**(b) Çoklu okuma DRF korur:** Data race tanımı (Op.Sem. §6.5):
```
∃ e₁, e₂ : (e₁ veya e₂ bir mem_yaz)
```
**Her iki olay da okuma** ⟹ data race konfigürasyonu oluşmaz. ∎

---

## Lemma DRF-L5 — Channel Atomicity Preservation

### İfade

```
İyiTipli(Π) ∧
∃ z_gönder, z_al, t_a, t_b, v, k :
  C-KANAL-GÖNDER(t_a, k, v) @ z_gönder
∧ C-KANAL-AL(t_b, k, v)   @ z_al
∧ z_gönder < z_al

⟹

(a) ∀ z ∈ [z_gönder, z_al) :  Σ(bölge(v), z) = ρ_kanal(k)
    (v kimseye ait değil — kanal'ın transfer tampon'unda)
(b) z = z_gönder noktasında: bölge(v) atomik olarak t_a → ρ_kanal(k)
(c) z = z_al noktasında: bölge(v) atomik olarak ρ_kanal(k) → t_b
(d) t_a ve t_b'nin `v`'ye eşzamanlı erişimi yok
```

### Varsayımlar

- S3 (Atomik Transfer) — Bellek Modeli §Katman 2
- C-KANAL-GÖNDER, C-KANAL-AL kuralları (Op.Sem. §5.4)
- DRF-L2 — gönderim sonrası t_a'nın v erişimi silinir

### İspat

**(a)** C-KANAL-GÖNDER kuralı: `Σ' = Σ[bölge(v) ↦ ρ_kanal(k)]`. Sonraki
adımlar Σ'yi değiştirmez (kanal'ın `K⃗(k).gönder_kuyruğu` v içerirken)
çünkü `v`'ye ait Σ entry'i yalnız C-KANAL-AL ile değişir. ∴ ∀ z ∈
[z_gönder, z_al) : Σ(bölge(v), z) = ρ_kanal(k). ∎

**(b) Atomiklik:** C-KANAL-GÖNDER tek bir küçük-adım. Aksiyom S3 atomikliği
garanti eder (runtime atomik primitif — mutex / lock-free queue ile
implemente). ∎

**(c)** C-KANAL-AL benzer şekilde tek bir küçük-adım. ∎

**(d) DRF garanti:** DRF-L2 ile gönderim sonrası t_a'nın Λ_a'sında `v`
tüketildi (Linear V1: gönderim = transfer = move). t_b'nin Λ_b'sinde
yalnız C-KANAL-AL sonrası `v` AKTİF. Tip kontrol L002 t_a'nın
`v`'ye sonraki erişimini reddeder.

∴ t_a ve t_b'nin `v`'ye **eşzamanlı erişimi** yok. ∎

---

## Lemma DRF-L6 — Capability Linear Inheritance

### İfade

```
İyiTipli(Π) ∧
Γ ⊢ y : yetki<R>     (Capability V1)

⟹

y'nin bütün bellek erişimleri DRF-L2 ve DRF-L3'ün argümanlarını miras alır:
  (a) C-GÖREV-BAŞLAT'ta y yakalandıysa, y t₁'den silinir (DRF-L2)
  (b) C-KANAL-GÖNDER'de y transfer edildiyse, y kanal'da (DRF-L5)
  (c) y aynı anda iki thread'de erişilemez
```

### Yorum

Capability spec CP.1.1 "Linear Integration": `yetki<R>` linear olarak takip
edilir. CP-NO-COPY = L-NO-COPY. CP-NO-ALIAS = L-NO-ALIAS.

### Varsayımlar

- Capability V1 CP.1.1 — Linear Integration
- CP-NO-COPY, CP-NO-ALIAS — Linear semantik aynı
- DRF-L2, DRF-L5 — yukarıda ispatlandı

### İspat

`y : yetki<R>` ⟹ Linear track (CP.1.1). Λ'nın bir parçası.

(a) C-GÖREV-BAŞLAT: y ∈ YD(c) ise, DRF-L2 ile Λ₁(y) = TÜKETİLDİ. ∎

(b) C-KANAL-GÖNDER: y kanal'a transfer ⟹ DRF-L5'in (d) maddesi. ∎

(c) t₁ ve t₂ eşzamanlı erişim için Λ_{t₁}(y) = Λ_{t₂}(y) = AKTİF olması
gerekir. Ama tek-üretici (`yetki_olustur`) → tek-AKTİF kümesi. Linear
transfer → bir thread'den diğerine geçer, eşzamanlı iki AKTİF yok. ∎

(*Not:* `delege(y, izin)` yeni bir token üretir (y2); y orijinal aktif
kalır (CP-DELEGE — Linear tüketmez). y2 ayrı bir linear entity. y2'nin
de erişimi aynı kurallar altında.)

---

## Tüm Lemmaların Özet Tablosu

| Lemma | İfade Özeti | Anahtar Varsayım | İspat Tekniği |
|-------|--------------|------------------|---------------|
| DRF-L7 | Bellek erişimi tipli ve bölgeli | A1 + İyiTipli | Yapısal indüksiyon kurallar |
| DRF-L1 | Region-thread tekil sahip | A2 + S1/S2/S3 | İndüksiyon Σ-değiştiren kurallar |
| DRF-L2 | Linear move'da kaynak silinir | Linear L-NO-COPY/ALIAS + LC-2 | C-GÖREV-BAŞLAT kuralı |
| DRF-L3 | Linear closure yalnız t_yeni'de | LC-2/LC-3 + R-YAKALAMA-THREAD | Tip kontrol + DRF-L7 |
| DRF-L4 | Donmuş bölgede yazma yok | R-PAYLAŞ + Plan E hibrit | Tip kontrol reddeder |
| DRF-L5 | Kanal transfer atomik | S3 + DRF-L2 | C-KANAL kuralları + Linear |
| DRF-L6 | Capability linear miras alır | CP.1.1 + DRF-L2 | Aynı linear track |

---

## Bağımlılık Sırası (İspat Çalışma Planı)

İspatlar aşağıdaki sırada yazılmalı (yukarıdaki belge bu sırayı izledi):

1. DRF-L7 (kök — diğer hepsi buna dayanır)
2. DRF-L1 (region sahip tekilliği)
3. DRF-L2 (linear move semantiği)
4. DRF-L3 (linear closure DRF-L2'ye dayanır)
5. DRF-L4 (frozen region — bağımsız)
6. DRF-L5 (channel — DRF-L2'ye dayanır)
7. DRF-L6 (capability — DRF-L2 ve DRF-L5'e dayanır)
8. Ana Teorem 4' (`KEMGU_DRF_Teoremi.md`)

---

## V1 Sınırları

- **Inter-procedural escape soundness:** DRF-L3 yerel callee analizi varsayar;
  callee başka closure'lar yaratırsa V2'de genişletilir.
- **Weak memory model:** DRF-L5 SC altında ispatlandı (Plan Karar F). V2
  C++11 MM ile fence emit + acquire/release lemma'ları gerekir.
- **Pattern matching consume:** `eşleş` desen-binding'de linear tüketim
  v2'de daha detaylı (KIRMIZI_QUEUE F maddesi).
- **Mekanize ispat:** V2 — Coq/Lean syntax'a taşıma (Plan Karar A).

---

## Referanslar

- Operasyonel Semantik — [`KEMGU_Operasyonel_Semantik.md`](KEMGU_Operasyonel_Semantik.md)
- Bellek Modeli — [`KEMGU_Bellek_Modeli.md`](KEMGU_Bellek_Modeli.md)
- Linear Types V1 — [`KEMGU_Linear_Types_Spec_V1.md`](KEMGU_Linear_Types_Spec_V1.md)
- Capability V1 — [`KEMGU_Capability_Spec_V1.md`](KEMGU_Capability_Spec_V1.md)
- DRF Plan — [`KEMGU_DRF_Genisletme_Plan.md`](KEMGU_DRF_Genisletme_Plan.md)

---

**END DRF Lemmaları V1**
