# KEMGU Genişletilmiş DRF Teoremi V1 (Statik)

**Tarih:** 2026-05-14
**Durum:** TASLAK FORMALİZASYON — kâğıt üzerinde
**Sürüm:** V1 (Statik DRF) — Plan Karar B "V1 dar, V2 geniş, V3 metateorem"
**Önkoşullar:**
- [`KEMGU_Operasyonel_Semantik.md`](KEMGU_Operasyonel_Semantik.md)
- [`KEMGU_DRF_Lemmalar.md`](KEMGU_DRF_Lemmalar.md)
- [`KEMGU_DRF_Genisletme_Plan.md`](KEMGU_DRF_Genisletme_Plan.md) (kararlar onaylı)

Bu belge, KEMGU'nun **Genişletilmiş Data Race Freedom (DRF) Teoremi'ni**
formel olarak ifade eder ve ispatlar. Mevcut `KEMGU_Bellek_Modeli.md` Teorem 4
(satır 260-272) **silinmez**; bu belge ek olarak durur ve Teorem 4' olarak
referanslanır.

---

## 1. Tarihçe ve Kapsam

### 1.1 Eski Teorem 4 (Aynen Korunur)

Mevcut form (`KEMGU_Bellek_Modeli.md`):

> **Teorem 4 — Data Race Freedom:** Güvenli alt kümede data race imkansız.

Bu informel ifade KEMGU'nun ilk fazından beri var. Linear V1 + Capability V1 +
Sabitsüre V1 + Realtime V1 + SIMD V1 katmanları eklendikten sonra:
- "Güvenli alt küme" — tanım belirsiz
- "Eşzamanlı erişim" — küçük-adım modeli yok
- "Data race" — happens-before bağıntısı yok

### 1.2 Yeni Teorem 4' (Bu Belgede)

`İyiTipli(Π)` (Op.Sem. §7) önkoşulu altında **statik garanti**: derleme
geçen program, mümkün olan tüm yürütme izlerinde data race içermez.

### 1.3 V1 vs V2 vs V3 Kapsam

(Plan Karar B onayı uyarınca:)

- **V1 — STATİK DRF (BU BELGE):**
  `İyiTipli(Π) ⟹ ∀ τ ∈ Tr(Π) : ¬ data_race(τ)`
  Önkoşul: tip+linear+capability+sabitsüre+bölge+güvensiz-yok hepsi.
- **V2 — OPERASYONEL DRF (SAKLI):** Sequential consistency yerine C++11
  weak memory model + fence emit politikası ile genişletme.
- **V3 — BÜTÜNLEŞİK GÜVENLİK METATEOREMİ (SAKLI):** DRF + Memory Safety +
  Side-Channel Freedom + BET birleşik.

---

## 2. Ana Teorem İfadesi

```
═══════════════════════════════════════════════════════════════════════
  Teorem 4' (Genişletilmiş Statik DRF)
═══════════════════════════════════════════════════════════════════════

  Π : Program            (KEMGU AST)
  İyiTipli(Π)             (Op.Sem. §7 — 7 koşulun hepsi geçer)

  ⟹

  ∀ τ ∈ Tr(Π) :  ¬ data_race(τ)

  Burada:
  - Tr(Π) = Π'nin (Op.Sem. §5) sistem reduksiyonu altında tüm izlerin kümesi
  - data_race(τ) Op.Sem. §6.5 tanımı (happens-before ile sıralanmamış
    iki bellek operasyonu, en az biri yazma)

═══════════════════════════════════════════════════════════════════════
```

### 2.1 İyiTipli(Π) Önkoşulunun Detayı

`KEMGU_Operasyonel_Semantik.md` §7'den:

1. `tip_kontrol_program(Π) = OK` (T001..T031 yok)
2. `lineer_kontrol_program(Π) = OK` (L001..LR002 yok)
3. `capability_kontrol_program(Π) = OK` (CP001..CP005 yok)
4. `sabitsüre_kontrol_program(Π) = OK` (CT001..CT008 yok — DRF için ortogonal)
5. `bölge_atama_program(Π) = OK` (escape DFA + R-* aksiyom geçti)
6. `realtime_kontrol_program(Π) = OK` (RT001..RT007 yok — DRF için ortogonal)
7. Π hiçbir `güvensiz` blok içermez (Plan Karar H "izolasyon")

(4) ve (6) sabitsüre/realtime DRF için **doğrudan gerek değil** ama
`İyiTipli` kavramının bütünlüğü için listelenir.

### 2.2 Sabitsüre / Realtime DRF ile İlişki

(Plan §2.4 D-E uyarınca:)

- `sabitsüre<T>` — Side-channel timing freedom; DRF ortogonal.
- `gerçekzamanlı` — Bounded execution time; DRF ortogonal.

İki nitelik **DRF teoreminin doğruluğunu etkilemez**, ama "iyi-tipli" tanımına
dahil edildiği için pragmatik olarak korunur (gelecek bütünleşik metateorem
V3 için).

---

## 3. Ana İspat (Yapısal İndüksiyon Sistem Reduksiyonu Üzerine)

### 3.1 İspat Stratejisi

(Plan §5.4 onaylı seçim: **MODÜLER**.)

Yapısal indüksiyon — Π'nin **olası reduksiyon adımlarının** kümesi üzerine.
Her olası adım için ya:
- Reduksiyon DRF-yardımcı lemmalar (DRF-L1..L7) ile data race üretmez,
- Veya tip kontrol fazı bu reduksiyona izin vermez (`İyiTipli(Π)` çelişir).

### 3.2 İspat (Açık Form)

**Temel:** Π'nin başlangıç konfigürasyonu `S₀ = ⟨{T₀}, σ₀, Σ₀, K⃗₀⟩`
tek thread (`t₀`) içerir. Tek thread'de paralel bellek operasyonu **mümkün
değil** ⟹ data race konfigürasyonu `data_race(_)` boş.

İspat trivial: `t₁ ≠ t₂ ∧ t₁, t₂ ∈ Threads(τ)` kuantifikasyonu boş (sadece
bir thread). ∎_temel

**İndüktif Adım:** `S ⟹ S'`. Reduksiyon kuralı ne ise (Op.Sem. §4-5):

#### Durum A — Tek-thread reduksiyonu (S-VAR, S-ATAMA, ...)

Olay (ör. `mem_oku(t, ρ, ofs, v)`) tek bir thread `t`'nin işidir.

- DRF-L7: bellek operasyonu tipli ve bölgeli (`ρ ≠ ρ_lit, ρ ∈ Ρ`).
- DRF-L1: ρ'nun sahibi tekil (∉ ρ_donmuş ise tek thread t).
- Aksiyom A4 (Op.Sem. §8): `Σ(ρ, z) ∈ {t, DONMUŞ}` (yani t bu ρ'nun
  sahibi veya ρ donmuş).

Eğer ρ donmuş değilse: ρ yalnız t'ye ait. Başka thread bu ρ'ya erişemez
çünkü her erişim aynı A4 ile filtrelidir. ∴ data race konfigürasyonu yok.

Eğer ρ donmuş ise (DRF-L4):
- Yazma yasak (yazma reduksiyonu gerçekleşmez)
- Çoklu okuma izinli ama "all reads" data race değil (data_race tanımı:
  "en az biri yazma")

∴ Durum A'da data race yok. ∎_A

#### Durum B — Thread Spawn (C-GÖREV-BAŞLAT)

Olay: `thread_başlat(t_yeni)` + sahiplik transferleri.

DRF-L2 (Linear Move): Yakalananlar t₁'den t_yeni'ye move; t₁ erişimini
kaybeder (Λ₁ \ YD_lin).

DRF-L3 (Linear Closure): t_yeni'nin closure gövdesi yalnız `ρ_sahip(t_yeni)`
+ `ρ_global` + `ρ_lit`'ten okur/yazar.

DRF-L1: Yeni Σ'da her transferred bölgenin sahibi t_yeni (tek).

Senkronize-eder (Op.Sem. §6.3): `thread_başlat(t_yeni) ≺_sw e` for e ∈
t_yeni'nin tüm sonraki olayları. ∴ ≺_hb t₁ ve t_yeni arasında thread_başlat
noktasında zincirler.

**Anahtar:** Sonraki thread olayları arasında **mevcut yakalanmış bölgelere
erişim** thread_başlat ile senkronize. Eski erişimler (t₁ tarafından, görev
başlatma öncesi) ≺_hb yeni erişimleri (t_yeni tarafından). Happens-before
ile sıralı ⟹ data race değil.

∴ Durum B'de data race yok. ∎_B

#### Durum C — Kanal Gönder/Al (C-KANAL-GÖNDER, C-KANAL-AL)

DRF-L5: kanal transferi atomik (S3). Bölge sahipliği t_a → ρ_kanal → t_b
sıralı.

Senkronize-eder: `kanal_gönder(t_a, k, v) ≺_sw kanal_al(t_b, k, v)`
(Op.Sem. §6.3).

DRF-L2: `v` linear ise t_a'da Λ_{t_a}(v) = TÜKETİLDİ (gönderim sonrası).
Tip kontrol L002 t_a'nın sonraki v erişimini reddeder.

t_a'nın v'ye **kanal gönderim öncesi** erişimi + t_b'nin v'ye **kanal alım
sonrası** erişimi:
- t_a erişim @ z_a < z_gönder
- t_b erişim @ z_b > z_al
- ≺_hb zinciri: t_a@z_a ≺_pl gönder ≺_sw al ≺_pl t_b@z_b
- ∴ t_a@z_a ≺_hb t_b@z_b

Happens-before sıralı ⟹ data race değil. ∎_C

#### Durum D — Donmuş Bölge (C-DONDUR)

DRF-L4: donmuş ρ üzerinde yazma reduksiyonu gerçekleşmez (tip kontrol
reddeder). Çoklu okuma data race değil ("en az biri yazma" koşulu
karşılanmaz).

Senkronize-eder: `dondur(t_a, ρ) ≺_sw mem_oku(t_b ≠ t_a, ρ, _, _)`
(Op.Sem. §6.3) ⟹ ≺_hb zinciri.

∴ Durum D'de data race yok. ∎_D

#### Durum E — Birleştir (C-BİRLEŞTİR)

t_hedef bitti (son olayı vardı). `birleştir(t_a, t_hedef)` çağrısı.

Senkronize-eder: `t_hedef'in son olayı ≺_sw birleştir(t_a, t_hedef)`
(Op.Sem. §6.3).

t_hedef'in ρ_sahip bölgeleri serbest bırakılır (Σ güncelleme). Dönüş değeri
ρ_çağıran'a terfi (R-BİRLEŞTİR). Bu noktadan sonra ρ_çağıran'da v'ye
erişim yalnız t_a'da.

∴ Durum E'de data race yok. ∎_E

#### Durum F — Capability Transferleri (delege, geri_al)

`delege(y, izin)`: yeni y2 oluşturur (linear), y orijinal kalır (CP-DELEGE
kuralı). y2 başka thread'e geçirilirse DRF-L6 ile DRF-L2 argümanı miras
alır.

`geri_al(y)`: y'yi tüketir (Λ \ {y}). Sonraki erişim L002.

∴ Durum F'de data race yok. ∎_F

#### Durum G — Linear Closure Çağrısı (LC-3)

`c : tekkez<işlev>` çağrıldığında otomatik tüketilir (LC-3). c'nin gövdesi
yalnız t_yeni'de çalışır (DRF-L3).

∴ Durum G'de data race yok. ∎_G

---

### 3.3 İndüksiyon Sonucu

Sistem reduksiyonu `⟹` (Op.Sem. §5.3) için olası tüm kurallar Durumlar
A-G'ye dahildir:

- Tek-thread S-* kuralları → Durum A
- C-GÖREV-BAŞLAT → Durum B
- C-KANAL-GÖNDER, C-KANAL-AL → Durum C
- C-DONDUR → Durum D
- C-BİRLEŞTİR → Durum E
- C-DELEGE, C-GERI_AL → Durum F (capability)
- LC-3 reduksiyonları → Durum G

Tüm kurallar incelendi. Her durumda data race konfigürasyonu **oluşmaz**.

∴ İndüksiyon tamamlandı. ∀ τ ∈ Tr(Π) : ¬ data_race(τ). ∎_ana

---

## 4. Sonuçlar ve Yorum

### 4.1 Statik Garanti

KEMGU derleyicisi `tip_kontrol_program(Π) = OK` döndürürse (`make calistir_*`
'da `--check` modu) ve Π `güvensiz` blok içermiyorsa, **runtime'da hiçbir
şekilde data race oluşmaz**. Derleme zamanı statik kontroller bu mesaj
güçlülüktedir.

Karşılaştırma:
- **C/C++** data race UB; programcı sorumluluğu.
- **Java** "happens-before" runtime; data race tanımlanabilir ama olabilir.
- **Rust** ownership + Send/Sync; statik ama lifetime annotation gerek.
- **Go** runtime detector; statik garanti yok.
- **KEMGU** statik garanti, sıfır annotation (escape DFA otomatik).

### 4.2 Linear Types'ın Matematik Gücü (Plan Karar C)

Plan Karar C: "Linear types DRF'in temel taşıyıcısı."

İspat bu kararı destekler:
- DRF-L2, DRF-L3, DRF-L5, DRF-L6 hepsi `L-NO-COPY` + `L-NO-ALIAS` üzerine
  kuruldu.
- Linear'sız (Linear V1 önce) DRF ispatı **runtime invaryantı** S1'e
  bağımlıydı (eski Teorem 4 ispatı).
- Linear V1 ile artık **compile-time** kanıt — `İyiTipli(Π)` doğru ise
  S1 her z için **otomatik** doğru (kanıtlanan, kabul edilen değil).

### 4.3 Donmuş Bölge S1 İstisnası

DRF-L4 R-PAYLAŞ ile birlikte: donmuş bölge "tek sahip" yerine "çoklu okuyucu,
sıfır yazıcı". S1'in tek istisnası; ama veri yarışı tanımı "en az biri yazma"
koşuluyla zaten ortak okumayı dışlar.

### 4.4 Capability Ayrı Teorem (Plan Karar G)

DRF-L6 capability'nin linear yanını kapsar. Capability'nin asıl katkısı
**confused-deputy** + **ambient authority** sınıfı; bu DRF dışında bir
teoreme bırakılır:

```
Teorem 7 (Authority Soundness) — TASLAK
  İyiTipli(Π) ⟹ Π'de confused-deputy attack izi olmaz
```

(Capability Spec V1'in CP.13 "Direktif Hedef 1 Uyum Tablosu" buraya
karşılık gelir; ayrı belge gelecekte yazılır.)

---

## 5. V1 Sınırları (V2'ye Bırakılanlar)

(Plan Bölüm 7 onaylı kararlar uyarınca:)

| Sınır | Plan Maddesi | V2 Hedefi |
|-------|--------------|-----------|
| Sequential Consistency varsayımı | 7.F | C++11 MM + fence emit |
| Yerel callee escape analizi | (Plan §6 D.1) | Inter-procedural |
| `güvensiz` blok dışlama | 7.H | Güvensiz izolasyon teoremi |
| Kâğıt ispat | 7.A | Mekanize (Coq/Lean/F*) |
| Statik DRF (V1) | 7.B | Operasyonel DRF V2 |
| Lang syntax (`görev`/`kanal`) eksik | 7.D | Parser implementasyonu (paralel görev) |

### 5.1 Lang Syntax İmplementasyonu (2026-05-14 Tamamlandı)

İlk taslakta bu ispat **varsayım**la yazılmıştı (lang syntax yok). Şu an:

- **Lexer:** `görev`, `kanal` keyword (toplam 35).
- **AST:** `DUGUM_TIP_GOREV`, `DUGUM_TIP_KANAL` tip kurucuları.
- **Parser:** `parse_tip` → `görev<T>`, `kanal<T>`.
- **Tip sistemi:** `TIP_GOREV`, `TIP_KANAL` kategorileri; `tip_olustur_gorev`,
  `tip_olustur_kanal`, nominal eşitlik, yazdırma.
- **Tip kontrol:** 5 built-in çağrı özel-case:
  - `görev_başlat(c: işlev() -> T) -> görev<T>` (DRF001)
  - `görev_birleştir(g: görev<T>) -> T` (DRF002, linear tüketim)
  - `kanal_gönder(k, v) -> boş` (DRF003, v tüketilir)
  - `kanal_al(k: kanal<T>) -> T` (DRF004, k yeniden kullanılır)
  - `dondur(v: &değişken T) -> &T` (DRF005)
- **Linear miras:** `görev<T>` linear (`tip_lineer_mi` 1 döner);
  `kanal<T>` non-linear V1 (transfer tamponu, yeniden kullanılır).
- **LR-2 güçlendirildi:** yapı içine herhangi linear tip yasak (eskiden
  sadece TIP_TEKKEZ idi → şimdi `tip_lineer_mi` genelleştirildi).
- **Test:** `test/test_drf.c` 36/36 ASan temiz.

### 5.2 V1 Sınırları (Lang syntax mevcut, ama bazı eksiklikler)

- **Lambda body block-form:** `|| { ver e; }` lambda gövdesi blok ise
  tip_belirle T001 verir (block içindeki son `ver` tipinin çıkarsanması
  yok). V1'de **lambda body ifade-form** zorunlu: `|| e`. Block-form V2.
- **Closure linear capture body-tüketim:** Linear yakalanan değerin
  closure body'sinde tüketimi block-form'a bağlı; V1'de outer-scope
  tüketim kullanılır (test D34, D35).
- **Runtime semantik:** Concurrency runtime (thread/channel
  implementation, LLVM codegen) henüz yok. `görev_başlat` ve diğerleri
  şu an yalnız tip kontrol seviyesinde; LLVM IR'de extern fonksiyon
  olarak link-time'a bırakılır. V2: thread runtime + fence emit.
- **kanal_aç:** kanal endpoint üretici built-in V1'de yok (kanal'lar
  parametre olarak alınır). V2.

---

## 6. Test Stratejisi (Plan Faz C — 30+ test)

(Plan Karar J onaylı — 30+ test eşiği.)

### 6.1 Negatif Test'ler (Derleme Reddi — ~20)

DRF ile ilgili statik kontroller derlenmemeli:

| # | Senaryo | Beklenen Hata |
|---|---------|---------------|
| D1 | Linear değer iki thread'e yakala | L002 LINEAR_DOUBLE_USE |
| D2 | tekkez referansı al (&t) | L004 LINEAR_REFERENCE |
| D3 | Capability iki thread'e geçir | CP005 LINEAR_VIOLATION |
| D4 | Donmuş bölgeye yaz | (yeni) DRF001 FROZEN_WRITE |
| D5 | Yapı içinde tekkez (LR002) | LR002 LINEAR_REGION_EMBED |
| D6 | Mutable referans aliasing | R-REF-ÇAKIŞMA (mevcut) |
| D7 | Kanal gönderim sonrası v erişim | L002 |
| D8 | Closure with linear capture, çağrı sonrası tekrar | L002 (LC-3) |
| D9 | Multi-mutable reference | R-REF-DEĞ aliasing |
| D10-D20 | + Capability edge case'ler, frozen edge case'ler |

### 6.2 Pozitif Test'ler (Derleme Geçer + Runtime Safe — ~10)

| # | Senaryo | Beklenen |
|---|---------|----------|
| P1 | `görev_başlat(c)` + capture transfer | --check OK |
| P2 | `kanal_gönder/al` round-trip | --check OK |
| P3 | `dondur(v)` + çoklu okuyucu | --check OK |
| P4 | Linear closure spawn (LC-2) | --check OK |
| P5 | Capability delege + alt-yetki | --check OK |
| P6 | Producer/consumer pattern | --check OK |
| P7 | Sequential consistency yeterli kod | --check OK |
| P8-P10 | Daha karmaşık compositional pattern'lar |

### 6.3 Test Dosyası Konum Önerisi

`test/test_drf.c` (yeni dosya) — Plan Faz C uygulamasında yazılır. Bu
PLAN'ın kapsamı dışında; lang syntax tamamlandıktan sonra Faz C başlar.

---

## 7. Referanslar

- KEMGU Bellek Modeli (eski Teorem 4) — [`KEMGU_Bellek_Modeli.md`](KEMGU_Bellek_Modeli.md)
- Operasyonel Semantik — [`KEMGU_Operasyonel_Semantik.md`](KEMGU_Operasyonel_Semantik.md)
- DRF Lemmaları — [`KEMGU_DRF_Lemmalar.md`](KEMGU_DRF_Lemmalar.md)
- Linear Types V1 — [`KEMGU_Linear_Types_Spec_V1.md`](KEMGU_Linear_Types_Spec_V1.md)
- Capability V1 — [`KEMGU_Capability_Spec_V1.md`](KEMGU_Capability_Spec_V1.md)
- DRF Plan — [`KEMGU_DRF_Genisletme_Plan.md`](KEMGU_DRF_Genisletme_Plan.md)
- **Reynolds 2002** — Separation Logic
- **O'Hearn 2007** — Concurrent Separation Logic
- **Boehm & Adve 2008** — C++ MM (V2 hedefi)

---

**END DRF Teoremi V1 (Statik)**
