# KEMGU DRF (Data Race Freedom) Teoremi Genişletme Planı

**Tarih:** 2026-05-14
**Durum:** TASLAK PLAN — kod / ispat değişikliği YOK
**Karar bekleyenler:** Bölüm 7 (Mehmet'in cevaplaması gereken sorular)
**Branch hedefi:** `feature/drf-genisletme-plan` (bu döküman bu branch'te hazırlanır;
ispat dosyalarına dokunulmaz)

---

## 1. Yönetici Özeti

KEMGU'nun Data Race Freedom (DRF) teoremi (`Teorem 4`, `KEMGU_Bellek_Modeli.md`
satır 260-272) **kâğıt üzerinde**, **4 satırlık informel** bir ispat taslağıdır.
Sistem o tarihten bu yana üç yeni katman kazandı:

1. **Linear Types Spec V1** (`tekkez<T>` + `kullan` + `imha`) — *onaylı,
   uygulanmış*
2. **Capability Spec V1** (`yetki<R>` + `delege` + `geri_al`) — *taslak,
   uygulama 2026-05-14 itibarıyla devam ediyor*
3. **Sabitsüre Spec V1** (`sabitsüre<T>` + `ifşa`) — *taslak, uygulanmış*

Ayrıca **Realtime** (`gerçekzamanlı`) ve **SIMD** (`vektör<T,N>`) niteliği
geldi — bunlar DRF'in **dik (orthogonal)** kapsamındadır.

**Hedef:** DRF teoremini bu üç yeni katmanla **birleşik şekilde** yeniden ifade
etmek; informel taslağı **yapısal indüksiyon iskeletine** dönüştürmek; mekanize
ispat (Coq / Isabelle / Lean) için altyapıyı hazırlamak.

Bu döküman **plan**dır; kod / ispat dosyası üretmez. Karar gerektiren her nokta
Bölüm 7'de toplanmıştır.

---

## 2. Mevcut Durum Analizi

### 2.1 Mevcut DRF Teoremi (Aynen)

```
Teorem 4 — Data Race Freedom: Güvenli alt kümede data race imkansız.

Data race: iki thread aynı belleğe eşzamanlı erişir, en az biri yazma.

İspat taslağı:
- Değiştirilebilir bölge: S1 → tek sahip → iki thread erişemez ∎
- Donmuş bölge: yazma yasak → data race tanımı karşılanmaz ∎
- Kanal transferi: S3 → atomik transfer → asla iki sahip yok ∎
- Closure yakalama: R-YAKALAMA-THREAD → move → kaynak erişim kaybeder ∎
```

(`belgeler/KEMGU_Bellek_Modeli.md`, satır 260-272.)

### 2.2 Formaliz Dili — YOK

- **Proof assistant kullanılmamış.** Coq / Isabelle/HOL / Lean / F* dosyası
  yok (`Glob *.v *.thy *.lean` → boş).
- Tüm ispat KEMGU belgelerinde **doğal Türkçe + matematik** (informel) olarak.
- Karşılaştırma: BET teoremi (`Realtime_Spec_V1.md`, RT.8) **5 adımlık** bir
  yapısal indüksiyon iskeleti taşır — DRF'ten **bir adım** daha formel.

### 2.3 Mevcut Lemma / Aksiyom Setleri (Bellek Modeli, Katman 1+2)

| Ad | Yer | Anlamı |
|----|-----|--------|
| ρ-omur sıralama | Katman 1 | `ρ_iterasyon ≤ ρ_yerel ≤ ρ_çağıran ≤ ρ_global` |
| R-LIT, R-YEREL, R-VER, R-İTERASYON, R-KOŞUL, R-GÖMME, R-BÖLME | Katman 1 | Bölge atama aksiyomları |
| R-YAKALAMA, R-YAKALAMA-YEREL, R-YAKALAMA-ESCAPE, R-YAKALAMA-THREAD | Katman 1 | Closure yakalama bölge terfi |
| S1 (Tekil Sahiplik) | Katman 2 | `\|{t : sahip(ρ,t,t_z)}\| = 1` |
| S2 (Başlangıç Sahipliği) | Katman 2 | Oluşturan thread sahiptir |
| S3 (Atomik Transfer) | Katman 2 | Transfer tek t_z adımında olur |
| R-GÖREV | Katman 2 | `görev_başlat(c)` → yakalananların sahibi değişir |
| R-BİRLEŞTİR | Katman 2 | `g.birleştir()` → dönüş değeri ρ_çağıran'a terfi |
| R-KANAL (gönderim+alım) | Katman 2 | Sahiplik kanal üzerinden taşınır |
| R-PAYLAŞ | Katman 2 | `dondur(v)` → çoklu okuyucu, sıfır yazıcı |

### 2.4 Tip Sistemi Katmanları — Mevcut Lemma Etkisi

#### A) Region Sistemi (Katman 1 + 2) — DRF'in birinci dayanağı

- **Katman 1** (`bolge.c` + `bolge_atama.c` + `escape.c`): TAMAMLANDI.
  R-LIT/R-YEREL/R-VER/R-İTERASYON/R-KOŞUL aktif; escape DFA çalışıyor.
- **Katman 2** (`bolge.c` ek API'leri, `bolge_olustur_sahip`,
  `bolge_olustur_kanal`, `bolge_sahiplik_transfer`, `bolge_kanal_gonder`,
  `bolge_donmus_mu`): **SCAFFOLDING SADECE.** Lang syntax (`görev`/`kanal`
  keyword'leri) HENÜZ PARSE EDİLMİYOR.
  - **Sonuç:** S1/S2/S3 aksiyomları **derleme zamanında zorlanmıyor** —
    sadece runtime API.

#### B) Linear Types (`tekkez<T>`) — DRF'in ikinci dayanağı (potansiyel)

- TAMAMLANDI (54/54 test). L001..LR002, LC-2 closure-itself-linear, LR-3
  bölge kapanışında sızıntısızlık aktif.
- **DRF ile bağ:** `L-NO-ALIAS` (referans yasağı) + `L-NO-COPY` (kopyalama
  yasağı) bir `tekkez<T>` değerinin **iki thread'de eşzamanlı erişilmesini**
  derleme zamanında engeller. Mevcut DRF ispatında **bu lemmalar
  kullanılmıyor** — sadece S1'in runtime invaryantı geçiyor.

#### C) Capability (`yetki<R>`) — Linear'a paralel; DRF'i pekiştirir

- Spec taslak; CP.1.1 (Linear Integration) `yetki<R>`'yi linear olarak işaretler.
- **DRF ile bağ:** Aynı `yetki<R>` token'ı iki thread'de tutulamaz çünkü
  L-NO-COPY + L-NO-ALIAS uygulanır. *Confused deputy* sınıfı dolaylı olarak
  thread güvenliğini etkiler, ama DRF teoreminin **kapsamı dışı** sayılabilir
  (capability erişim kontrolü; data race olmayan yetki ihlali da olabilir).

#### D) Sabitsüre (`sabitsüre<T>`) — DRF'le DİK (orthogonal)

- TAMAMLANDI (30+ test). CT001..CT008 aktif.
- **DRF ile bağ:** YOK (doğrudan). Side-channel timing attack farklı bir
  güvenlik sınıfı. Concurrent ortamda timing kanalları daha sömürülebilir
  olur (cross-thread timing inference) — ama bu DRF dışında "Side-Channel
  Freedom" teoremi (ileride) için saklı.

#### E) Realtime (`gerçekzamanlı`) — DRF'le DİK

- TAMAMLANDI (32/32 test). RT001..RT007 aktif.
- **DRF ile bağ:** RT001 (alloc yasak) realtime gövdesinde paylaşılan heap
  state azaltır; RT004 (non-RT çağrı yasak) realtime izolasyonu güçlendirir.
  Ama DRF teoreminin kapsamı dışıdır — BET (Bounded Execution Time) ayrı
  teorem.

#### F) SIMD (`vektör<T,N>`) — DRF'le DİK

- TAMAMLANDI. Vektör operasyonları lane-paralel ama **tek thread içinde**;
  DRF'i etkilemez.

### 2.5 İspat Dosyalarının Yolları

| Dosya | İçerik |
|-------|--------|
| `belgeler/KEMGU_Bellek_Modeli.md` | Teorem 4 (DRF) + ispat taslağı, S1/S2/S3, R-GÖREV/R-BİRLEŞTİR/R-KANAL/R-PAYLAŞ aksiyomları |
| `belgeler/KEMGU_Linear_Types_Spec_V1.md` | L-NO-COPY, L-NO-ALIAS, LR-1..LR-4, LC-2 |
| `belgeler/KEMGU_Capability_Spec_V1.md` | CP-NO-COPY, CP-NO-ALIAS (Linear ile aynı), CP.1.1 Linear Integration |
| `belgeler/KEMGU_Realtime_Spec_V1.md` | BET Teoremi (RT.8) — referans; yapısal indüksiyon iskeletinin örneği |
| `src/bolge.h` + `src/bolge.c` | Katman 2 API scaffolding |
| `src/escape.c` | DFA escape analizi |
| `src/tip_kontrol.c` | Linear hooks, region binding |

---

## 3. Genişletilmiş DRF Teoremi — Öneri

### 3.1 Yeni Teorem İfadesi (Önerilen — Mehmet onayına açık)

```
Teorem 4' (Genişletilmiş DRF):

Önkoşul:
  - Π bir KEMGU programı
  - tip_kontrol_program(Π) = OK (statik tüm katmanlar geçer)
  - Π `güvensiz` blok içermez   [Teorem 5'in yardımı ile genişletilebilir]
  - Π'nin herhangi bir thread spawn'i yalnız `görev_başlat(c)` ile;
    her kanal transferi yalnız R-KANAL gönderim/alım ile gerçekleşir
    (Katman 2 sözdizimi)

Sonuç:
  - Π'nin tüm çalışma zamanı izleri (execution traces) Tr(Π) kümesinde,
    hiçbir izde "data race" konfigürasyonu yoktur:

    data_race(τ) ⟺ ∃ t1 ≠ t2 ∈ Threads(τ), ∃ ℓ ∈ Mem(τ), ∃ z ∈ Zaman :
                   erişim(t1, ℓ, z) ∧ erişim(t2, ℓ, z) ∧
                   (yazma(t1, ℓ, z) ∨ yazma(t2, ℓ, z))

    Teorem: ∀ τ ∈ Tr(Π) : ¬ data_race(τ)
```

**Mevcut Teorem 4'ten farklar:**

1. **"Güvenli alt küme"** belirsiz ifadesi yerine `tip_kontrol_program(Π) = OK
   ∧ güvensiz blok yok` formel önkoşulu.
2. **"İz" (trace) bazlı tanım** — operasyonel semantik notasyonuna geçiş.
   Önce "izi" tanımlamak gerek (Bölüm 5.1).
3. **"Konfigürasyon" yerine "iz"** — Java Memory Model / C++11 MM tarzı.
4. **Concurrency lang syntax'a bağlılık** — `görev`/`kanal` keyword'lerinin
   olması zorunlu (mevcut sadece API).

### 3.2 Alternatif Daha Zayıf İfade (V1 — Konservatif)

```
Teorem 4-V1 (Statik DRF):

Önkoşul:
  - Π tip_kontrol_program ile geçer
  - Π'de `görev_başlat` ve `kanal_gönder` çağrıları yalnız compile-time
    bilinen closure'larla yapılır (interprocedural escape yok)
  - Π `güvensiz` blok içermez

Sonuç:
  - Π'nin **derleme aşaması** (`tip_kontrol`, `escape`, `linear_kontrol`,
    `bolge_atama`) herhangi bir potansiyel data race konfigürasyonu için
    reddeder.
```

Bu **derleme-zamanı zorlama** ifadesi; ispat **runtime izi** yerine **statik
analizin tamlığı** üzerinedir. Tip kuralları tablosunun (R-* + S* + L-* + CP-*)
**örtüştüğünün** kanıtlanması.

### 3.3 Hangi Versiyon? (Karar Noktası — Bkz. Bölüm 7.A)

- 3.1 ("İz bazlı" tam DRF): TOPLAS makale çıktısı için **gerekli**; daha çok
  iş.
- 3.2 (Statik tip-sound DRF): V1 implementasyonu kanıtlanır; pratik. Sonra
  3.1'e genişletilir.

**Öneri:** İki aşama — V1 önce 3.2'yi yazar; V2 3.1'e (operasyonel semantik
+ izler) geçer. BET teoremi paterni (`RT.8`) bunu izler.

---

## 4. Yeni Lemmalar ve Mevcut Olanın Güncellenmesi

### 4.1 Mevcut Aksiyomlardan Lemma'ya Yükseltilmesi Gerekenler

| Mevcut | Yeni Rol | Neden? |
|--------|----------|--------|
| S1 (Tekil Sahiplik) | **Lemma S1 — kanıtlanır** | Mevcut aksiyom — kanıtlanmadan kabul edilmiş. Yeni: R-GÖREV/R-KANAL ile birlikte yapısal indüksiyon ile kanıtlanır. |
| S3 (Atomik Transfer) | **Aksiyom kalır** | Runtime invaryantı (kanal protokolü atomik); dil-içi kanıtlanamaz. |
| R-YAKALAMA-THREAD | **Lemma DRF-CC** | Linear types ile birlikte yeniden kanıtlanır (LC-2 ile bağlanır). |

### 4.2 Eklenecek Yeni Lemma'lar

```
Lemma DRF-L1 (Region-Thread Tekilliği):
  ∀ ρ ∈ Bolgeler(Π), ∀ z : |{t : sahip(ρ, t, z)}| ≤ 1
  Bağlı: S1 + S2 + S3, R-GÖREV, R-KANAL.
  Hariç: ρ donmuş (R-PAYLAŞ — özel durum).

Lemma DRF-L2 (Linear Move = Cross-Thread No-Alias):
  Eğer Γ ⊢ v : tekkez<T> ve `görev_başlat(c)` yakalama listesinde v varsa,
  o zaman v'nin orijinal scope'unda v erişilebilir DEĞİL (Γ' \ {v}).
  Bağlı: L-NO-COPY, L-NO-ALIAS, LC-2.

Lemma DRF-L3 (Linear Closure Soundness):
  Eğer c : tekkez<işlev(...) -> τ> ve `görev_başlat(c)` çağrıldı,
  o zaman c'nin gövdesi yalnız ρ_sahip(t_yeni) bölgesindeki değerlere
  erişir.
  Bağlı: LC-2, R-YAKALAMA-THREAD.

Lemma DRF-L4 (Frozen Region Read-Soundness):
  Eğer dondur(v) çağrıldıysa ve bölge(v) = ρ_donmuş, o zaman:
    ∀ t ∈ Threads, ∀ z : ¬ yazma(t, ρ_donmuş, z)
    ∧ ∀ t : okuma(t, ρ_donmuş, z) izinli
  Bağlı: R-PAYLAŞ.

Lemma DRF-L5 (Channel Atomicity Preservation):
  R-KANAL gönderim ve alım arasında, transfer edilen değer
  ne gönderene ne alıcıya tek başına aittir (kanal_id'ye aittir).
  Bağlı: S3 + R-KANAL.

Lemma DRF-L6 (Capability Linear Inheritance):
  yetki<R> linear olarak takip edildiğinden (CP.1.1), yetki<R>
  değerleri thread'lerarası DRF-L2 ile aynı garantiyi taşır.
  Bağlı: CP-NO-COPY, CP-NO-ALIAS = L-NO-COPY, L-NO-ALIAS.

Lemma DRF-L7 (Type-Soundness for Memory Access):
  Eğer tip_kontrol_program(Π) = OK ve `güvensiz` blok yok, o zaman
  her bellek erişim ifadesinin (atama, okuma, dereference) tip'i
  bilinir ve bölge ataması yapılmıştır.
  Bağlı: Bellek Modeli Katman 1 tüm R-* + Linear LR-1..LR-4.
```

### 4.3 Mevcut Aksiyom'un Korunması (Değişmeyenler)

- S2 (Başlangıç Sahipliği): aksiyom kalır
- S3 (Atomik Transfer): aksiyom kalır
- Bölge ömür sıralaması: aksiyom kalır
- R-LIT/R-YEREL/R-VER/R-İTERASYON: Katman 1 aksiyomları kalır

---

## 5. İspat İskeleti (Modüler Yapısal İndüksiyon)

### 5.1 Ön Hazırlık (Önerilen Sıra)

**Adım 0 — Operasyonel Semantik Tanımı (yeni dosya):**
- `belgeler/KEMGU_Operasyonel_Semantik.md` oluştur (taslak)
- Konfigürasyon `⟨Π, σ, τ⟩` (program, store, thread context map)
- Küçük adım reduksiyon `⟶`
- İz `Tr(Π) = {τ : ⟨Π, σ₀, τ₀⟩ ⟶* ⟨_, _, τ⟩}`
- `happens-before`, `data_race` formel tanımları

**Adım 1 — Önkoşulları Toplama:**
- "İyi-tipli program" tanımı (Well-Typed):
  - Tip kontrol geçti
  - Linear kontrol geçti (L001..LR002)
  - Bölge ataması yapıldı (escape DFA + R-* aksiyomları)
  - `güvensiz` blok yok (`güvensiz` izole edilir → ayrı teorem)

### 5.2 Lemmaların İspatlanma Sırası (Bağımlılık Grafı)

```
                        Lemma DRF-L7 (Type-Soundness)
                        /                              \
              Lemma DRF-L1                          Lemma DRF-L2
            (Region-Thread Tekilliği)          (Linear Move No-Alias)
                  |                                       |
                  v                                       v
            Lemma DRF-L4                          Lemma DRF-L3
         (Frozen Region Safety)               (Linear Closure Soundness)
                  |                                       |
                  +--------------+   +--------------------+
                                 |   |
                                 v   v
                         Lemma DRF-L5
                       (Channel Atomicity)
                                 |
                                 v
                         Lemma DRF-L6
                       (Capability Inheritance)
                                 |
                                 v
                          ═══════════════
                          Teorem 4' (DRF)
                          ═══════════════
```

### 5.3 Ana Teorem İspatı (V1 — Statik, Önerilen 3.2)

```
İspat (Teorem 4-V1, yapısal indüksiyon Π'nin üst-düzey tanımı üzerine):

Temel durum: Π = {modul1, ..., modulN}, tüm tanımlar üst-düzey.
  - Her tanım için tip kontrol geçti (varsayım).
  - DRF-L7: her bellek erişimi tipli ve bölgeli.

İndüksiyon adımı: Yeni bir thread spawn / kanal işlemi eklendiğinde

  Case A — Sıralı kod (tek thread):
    DRF-L1 + L7 ile bellek erişimleri tek thread içinde; data race yok (∅).

  Case B — `görev_başlat(c)`:
    DRF-L3: c'nin yakalama listesindeki yer değişkenleri yeni ρ_sahip(t_yeni)'ye
    transfer (R-YAKALAMA-THREAD).
    DRF-L2: tekkez yakalananlar orijinal scope'tan silinir.
    DRF-L1: yeni thread'in ρ_sahip(t_yeni) hiçbir başka thread tarafından
    erişilemez (S1 ile).
    ∴ Cross-thread data race yok.

  Case C — `kanal_gönder(k, v)`:
    DRF-L5: v ρ_kanal(k)'ya atomik transfer (S3 ile).
    Gönderim öncesi v gönderen thread'in; sonrası kanalın; alındıktan sonra
    alan thread'in.
    Her t_z anında v'nin sahibi ≤ 1.
    ∴ Cross-thread data race yok.

  Case D — `dondur(v)`:
    DRF-L4: yazma yasak → data race tanımı karşılanmaz (yazma yok).
    Çoklu okuma izinli, ama "all reads, no writes" konfigürasyonu data race
    değil.
    ∴ DRF korunur.

  Case E — Capability transferi (`delege(y, izin)`):
    DRF-L6: yetki<R> linear → DRF-L2 ile aynı argüman.
    ∴ Yetki tokenları thread'ler arası transfer'de DRF korunur.

  Case F — Linear closure çağrısı (tekkez<işlev(...)>):
    LC-3 (çağrı = tüketim) + DRF-L3 → closure gövdesi yalnız kendi
    ρ_sahip'inde çalışır.
    ∴ DRF korunur.

Tüm durumlar tüketildi. ∎
```

### 5.4 İspat Stratejisi — Tek Bütünleşik mi, Modüler mi?

**Öneri: MODÜLER.**

Sebepler:
1. **Yeniden kullanılabilirlik:** DRF-L1..L7 ayrı lemmalar; her biri bağımsız
   olarak Linear/Region/Capability spec'lerine atıfla kanıtlanır.
2. **Hata izolasyonu:** Bir lemma bozulursa hangi katmanın bozulduğu belli.
3. **Spec gelişimi:** Capability V2 / Linear V2 / Concurrency V2 geldiğinde
   sadece ilgili lemmalar güncellenir; teoremin gövdesi değişmez.
4. **Mekanik ispat'a uyum:** Coq / Lean'da Lemma'lar ayrı dosya; teorem
   bunları toplayan ana dosya.

Alternatif (tek bütünleşik) avantajı: kısa görünür. Dezavantajı: bakım zor,
spec gelişimine direnç.

---

## 6. Tahmini İş Büyüklüğü (Direktif Ek v1.1 F bölümü — checkpoint bazlı)

> İnsan-saati yasak. "1 checkpoint = 4-6 saat = orta spec'in Yeşil alt-adımları"
> (Direktif Ek v1.1 F).

### Faz A — Kâğıt Üzerinde Formalizasyon (3-4 checkpoint)

1. **Checkpoint A1:** Operasyonel semantik dosyası (Adım 0).
   - `KEMGU_Operasyonel_Semantik.md` taslak (küçük-adım, store, izler).
   - Tahmini boyut: ~300-500 satır markdown.
   - 🟢 Yeşil — spec içi.

2. **Checkpoint A2:** DRF-L1..L7 lemma'larının kâğıt ispatları.
   - Her lemma için: ifade, varsayımlar, ispat metni.
   - `belgeler/KEMGU_DRF_Lemmalar.md` (yeni dosya).
   - Tahmini boyut: ~600-900 satır markdown.
   - 🟢 Yeşil.

3. **Checkpoint A3:** Genişletilmiş Teorem 4' ispatı.
   - `KEMGU_Bellek_Modeli.md` Teorem 4 bölümünün yerine geçer.
   - Veya yeni dosya: `KEMGU_DRF_Teoremi.md`.
   - Tahmini boyut: ~200-300 satır.
   - 🔴 Kırmızı (formal teorem etkisi — Bölüm A direktif kategori
     "formal teorem etkisi" → KIRMIZI_QUEUE'ya tasarım onayı gerek).

4. **Checkpoint A4:** Spec'lerin cross-reference güncellemesi.
   - `Linear_Types_Spec_V1.md`, `Capability_Spec_V1.md`, `Bellek_Modeli.md`
     içinde DRF-L* atıfları eklenir.
   - 🟡 Sarı (spec-içi cross-ref).

### Faz B — Mekanize İspat (10-15 checkpoint — V2'ye saklı)

5. **Checkpoint B1:** Proof assistant seçimi (Bölüm 7.A — Mehmet karar verir).
6. **Checkpoint B2-Bn:** Lemma'ların mekanizasyonu (Coq/Lean syntax).
7. **Final:** Teorem 4' mekanize edilir.

Bu faz **V1'in dışı**. Faz A bittikten sonra ayrı sprint.

### Faz C — Test Doğrulama (2-3 checkpoint)

8. **Checkpoint C1:** DRF negative test'ler — derleme reddedilmesi gereken
   senaryolar (en az 20):
   - İki thread'in aynı bölgeye yazması (görev syntax sonrası — Bölüm 7.D).
   - Linear değerin iki thread'e geçirilmesi → L002.
   - Capability'nin alias edilmesi → CP005.
   - Donmuş bölgeye yazma → yeni hata kodu (Bölüm 7.E).

9. **Checkpoint C2:** DRF positive test'ler — derleme geçmeli, runtime
   safe (en az 10):
   - `görev_başlat(c)` + capture transferi.
   - `kanal_gönder/al` round-trip.
   - `dondur(v)` + çoklu okuyucu.

10. **Checkpoint C3:** ThreadSanitizer ile runtime doğrulama (eğer Clang64
    TSan destekliyorsa Win11'de — KIRMIZI_QUEUE 2026-05-13 maddesinde
    Dr. Memory'nin çöktüğü kayıtlı, TSan ayrı sorun olabilir).

### Toplam V1 Hedefi

- **Faz A (kâğıt formalizasyon):** ~3-4 checkpoint.
- **Faz C (test):** ~2-3 checkpoint.
- **Faz B (mekanize):** V2 — V1 dışı.
- **Toplam V1:** 5-7 checkpoint ≈ 25-40 saat ajan-zamanı (ajan 100x kuralı).

### Önkoşul Bağımlılıklar

DRF teoremi tam ispatlanmadan önce:

- **D.1 — KRİTİK:** Concurrency lang syntax (`görev` / `kanal` keyword'leri)
  parse edilmeli. Şu an sadece `bolge.h` API var; dilde syntax yok.
  Bu **dış görev** — DRF planı kapsamı dışı. CLAUDE.md "Sıradaki büyük
  seçenekler" → "Concurrency lang syntax".
- **D.2:** Capability spec'in implementasyonu (CP001..CP005 enforcement)
  devam ediyor; tamamlanması DRF-L6 için gerek.
- **D.3:** Operasyonel semantik formalizasyonu (Checkpoint A1) DRF-L1..L7
  hepsinin önkoşulu.

---

## 7. Mehmet'in Karar Vermesi Gereken Noktalar 🔴

> Bu bölüm KRİTİK. Direktif Ek v1.1 Bölüm A uyarınca **formal teorem etkisi**
> 🔴 Kırmızı kategoride. Aşağıdaki her madde `KIRMIZI_QUEUE.md`'ye taşınacak
> (bkz. Bölüm 8).

### 7.A 🔴 Proof assistant seçimi (V2 mekanizasyonu için)

**Sorun:** Mevcut tüm KEMGU ispatları kâğıt üzerinde. TOPLAS makale planı
(`belgeler/KEMGU_Makale_Tasari.md` referansı — Glob'ta görünmedi, mevcut
değil olabilir; CLAUDE.md'de adı geçiyor) için mekanize ispat artı-değer.

**Seçenekler:**
1. **Coq (gallina):** Akademik standart, MathComp ekosistemi geniş. Ama
   syntax İngilizce + ASCII; Türkçe kimliğe ters.
2. **Isabelle/HOL:** seL4 ekibi kullandı; daha "kâğıt benzeri" syntax. Yine
   İngilizce.
3. **Lean 4:** Modern, hızlı, mathlib aktif. İngilizce + Unicode.
4. **F\* (effects):** Mikro-kernel doğrulamaları (Project Everest). DRF için
   doğal effects sistemi. İngilizce.
5. **Kâğıt + paper makale (V1):** Mekanik ispat gerekmez; sadece formel
   kâğıt yeterli. KEMGU Türkçe DNA korunur.

**Etki:**
- Seçim, **Faz B** boyutunu belirler (3 ay vs 6 ay vs hiç).
- KEMGU Türkçe kimliği (memory: `feedback_turkce_kimlik`) ile çelişebilir
  (proof assistant syntax İngilizce).

**Karar:** *Bekleniyor.* Önerim: **V1'de kâğıt yeterli;** Faz B'yi V2'ye
ertele.

---

### 7.B 🔴 DRF teoreminin kapsamı

**Sorun:** "Güvenli alt küme" tabiri muğlak. Üç olası kapsam:

1. **Dar (Statik DRF — V1):** `tip_kontrol_program = OK ∧ güvensiz yok` →
   derleyici reddeder veya DRF korunur. **Önerilen (Bölüm 3.2).**
2. **Geniş (Operasyonel DRF — V2):** Tüm runtime izleri için. Operasyonel
   semantik gerek (Faz A.1). **Önerilen sonraki adım (Bölüm 3.1).**
3. **Bütünleşik Güvenlik Metateoremi:** DRF + Memory Safety + Side-Channel
   Freedom + Bounded Execution Time hepsi tek "Kemgu Güvenlik Bütünlüğü"
   altında. Çok geniş; her bir teorem ayrı kanıtlandığında bileşke
   yazılabilir.

**Karar:** *Bekleniyor.* Önerim: **V1 dar, V2 geniş, V3 metateorem.**

---

### 7.C 🔴 Linear types'ın DRF'e statik mı yoksa semantik mi katkısı?

**Sorun:** Mevcut Teorem 4'te S1 **runtime invaryantı**dır (sahip(ρ,t,t_z)
zaman noktası). Linear types ise **compile-time** zorlanır (L001..L004).

İki seçenek:
1. **Lineer zorlama → S1'in compile-time önkoşulu** (yeni):
   "Tip kontrol geçtiyse, S1 her z için runtime'da otomatik geçerli."
   Bu güçlü bir ifade; ama Linear'ın DRF teoreminin **temel taşıyıcısı**
   olduğu anlamına gelir.
2. **Lineer zorlama → DRF için "güçlendirici"** (zayıf):
   S1 hala runtime invaryantı; Linear sadece programcı hatalarını compile
   zamanında yakalar. DRF ispatı S1'i runtime aksiyomu olarak kullanır.

**Karar:** *Bekleniyor.* Önerim: **(1) — Linear'ın matematik gücünü tam
göstermek için.** Lakin (1) seçilirse, `bolge.h` Katman 2 API'leri lang
syntax ile zorunlu kılınmalı (Bölüm 7.D ile bağlı).

---

### 7.D 🔴 Concurrency lang syntax öncelik

**Sorun:** DRF teoremini ispat etmek için `görev` ve `kanal` keyword'leri
dilde olmalı. Şu an sadece `bolge.h` runtime API var.

**Seçenekler:**
1. **Önce lang syntax (DRF teoremi onu bekler):**
   - `görev_başlat(c) -> görev<T>` keyword
   - `kanal_aç<T>() -> (gönderen<T>, alan<T>)` keyword
   - `gönder(g, v)`, `al(a) -> T` built-in çağrı
   - Bu işin kendisi büyük (10+ checkpoint).
2. **Önce kâğıt ispat (lang syntax variyantsız):**
   - DRF teoremi Katman 2 aksiyomları üzerine yazılır, varsayım: "lang
     syntax ileride buna sadık eklenecek."
   - Risk: lang syntax geldiğinde teorem ufak değişiklik isteyebilir.
3. **Paralel:** Plan dökümanı yaz (bu döküman), lang syntax başka oturumda
   yap, DRF teoremi son adımda.

**Karar:** *Bekleniyor.* Önerim: **(3) — paralel.** Bu plan Faz A1+A2'yi
hemen başlatır; lang syntax bağımsız ilerler; Faz A3 ikisi de bitince yazılır.

---

### 7.E 🔴 Frozen region tipinin formal modeli

**Sorun:** R-PAYLAŞ ` dondur(v)` ile çağrılır. Bu **runtime API mi** yoksa
**compile-time tip qualifier mi**?

**Seçenekler:**
1. **Runtime API:** `dondur: &değişken T -> &T` (mutable → immutable cast)
   - Donmuş referansın iki thread'de paylaşılması runtime invaryantı
   - DRF-L4 sadece runtime invaryantı
2. **Compile-time tip qualifier:** `donmuş<T>` yeni tip operatörü
   - `donmuş<T>` ile `T` arasındaki dönüşüm sadece `dondur(v)` ile
   - DRF-L4 statik garantilenir
3. **Hibrit:** `dondur` builtin call + tip sistemi `donmuş<T>` çıkarsar
   (Java `final` benzeri — tip değişmez, ama compiler track eder)

**Etki:** Yeni keyword (`donmuş`) → 🔴 Kırmızı. Yeni tip operatörü → 🔴.

**Karar:** *Bekleniyor.* Önerim: **(3) hibrit** — `dondur` zaten Katman 2'de
var; ek tip qualifier olarak track edilir. Yeni keyword YOK; sembol tablosu
flag.

---

### 7.F 🔴 Bellek modeli (Sequential Consistency vs Weak)

**Sorun:** DRF tanımı genelde **sequential consistency** (SC) varsayımı altında
yazılır. Modern donanım (ARM64, RISC-V) **weak memory model** (relaxed,
acquire-release).

**Karşılaştırma:**
- C++11 Memory Model: `std::memory_order_*` annotation
- Java Memory Model: happens-before, volatile
- ARM64: relaxed default, dmb sy fence
- DGX Spark (Hedef 3): ARM64 → weak

**Seçenekler:**
1. **V1 SC varsayımı:** "DRF kuralları SC altında geçerli; runtime fence
   eklemek runtime sorumluluğu." Basit ama eksik.
2. **C++11 MM ile entegrasyon:** Her `görev_başlat`, `kanal_gönder`,
   `dondur` çağrısı bir **acquire-release** fence emit eder.
3. **Daha güçlü model (release acquire by default):** `görev_başlat`
   acquire/release pair otomatik.

**Karar:** *Bekleniyor.* Önerim: **V1 = SC varsayımı + LLVM IR'de**
`atomic acq_rel` **fence emit** (görev/kanal/dondur sınırlarında). V2 weak
memory analizi.

---

### 7.G 🔴 Capability + DRF ilişkisi: ayrı teorem mi, bileşke mi?

**Sorun:** `yetki<R>` linear olduğu için (CP.1.1) DRF'in lineer dalı (DRF-L6)
capability'i kapsayabilir. Ama Capability ayrıca **erişim kontrolü** (CP001
yetki yok, CP003 izin yetersiz) sağlar — DRF dışında.

**Seçenekler:**
1. **Tek DRF teoremi (DRF-L6 ile kapsanır):** Capability'nin linear yanı
   DRF içinde; erişim kontrolü ayrı (yetki teoremi).
2. **Ayrı teoremler:**
   - Teorem 4' (DRF) — Linear/Region
   - Teorem 7 (Authority Soundness) — Capability özelinde
   - İki teorem birleştirilmez; ortak lemma DRF-L6 paylaşır.

**Karar:** *Bekleniyor.* Önerim: **(2) — ayrı.** Capability'nin asıl katkısı
**confused-deputy** ve **ambient authority** sorunları; DRF dışında.

---

### 7.H 🔴 `güvensiz` blok + DRF: opt-out modeli

**Sorun:** Mevcut Teorem 4 "Güvenli alt küme" diyor. Teorem 5 (Güvensiz
Sınır Bütünlüğü) `*` pointer'ın güvensiz bloktan çıkamayacağını söylüyor.
DRF teoremi `güvensiz` blok içindeki davranışı **dışlamalı** mı yoksa
**sınırlandırmalı** mı?

**Seçenekler:**
1. **Tamamen dışlama:** "Π güvensiz blok içermez" önkoşul; içeriyorsa DRF
   garanti edilmez.
2. **Güvensiz izolasyon:** Güvensiz blok içinde DRF korunur garanti yok;
   ama güvensiz bloğun dışında DRF korunur (Teorem 5 ile).
3. **Programcı izin verir:** Güvensiz blok başında `[etiket: "no-DRF"]`
   gibi annotation ile DRF opt-out.

**Karar:** *Bekleniyor.* Önerim: **(2)** — Teorem 5'in mantığını izleyerek.

---

### 7.I 🔴 Plan onayı + dosya yapısı

**Sorun:** Plan dökümanı (`KEMGU_DRF_Genisletme_Plan.md` — bu dosya)
yazıldı. Sonraki belgeler:

1. `KEMGU_Operasyonel_Semantik.md` — yeni dosya
2. `KEMGU_DRF_Lemmalar.md` — yeni dosya
3. `KEMGU_DRF_Teoremi.md` — yeni dosya (veya `Bellek_Modeli.md` yenile)
4. Spec'lerin cross-ref'i

**Soru:** Bu yapıyı onaylıyor musun yoksa farklı bir organizasyon (örneğin
tek dosya `KEMGU_DRF_V2.md` tüm bunları içerecek) tercih ediyor musun?

**Karar:** *Bekleniyor.* Önerim: **çok dosya** — modüler, küçük commit'ler,
git history düzgün.

---

### 7.J 🟡 Test sayısı eşiği

**Sorun:** Konsolidasyon spec'lerinin test minimum eşiği var (Linear 50,
Capability 35, Sabitsüre 30, Realtime 30, SIMD 25). DRF için ne?

**Öneri:** **30+ test** (Linear/CT/RT'a yakın). Bunun ~20'si negative
(derleme reddi), ~10'u positive (derleme geçer + runtime sound).

**Karar:** *Bekleniyor.* Bu Sarı seviye (Direktif A); sessizlik onay.

---

## 8. Sonraki Adımlar (Plan Onayından Sonra)

> Bu döküman PLAN'dır. Kod yok, ispat yok. Mehmet'in onayı + Bölüm 7
> kararları ile aşağıdaki sıra başlar:

1. KIRMIZI_QUEUE.md'ye Bölüm 7 maddeleri eklendi (✅ — bkz. aynı commit).
2. Mehmet 7.A-7.I cevaplarını verir.
3. Cevaba göre Faz A1 (`Operasyonel_Semantik.md`) başlar.
4. Paralel: Concurrency lang syntax (Bölüm 7.D'ye göre).
5. Faz A2 (Lemmalar) → Faz A3 (Teorem) → Faz C (test).
6. (V2) Faz B mekanizasyon.

---

## 9. Risk Tablosu

| Risk | Olasılık | Etki | Azaltma |
|------|----------|------|---------|
| Concurrency lang syntax gecikir | Yüksek | DRF ispatı engellenir | Faz A1+A2 paralel; teorem A3 sonraya kalır |
| Bellek modeli (SC vs weak) sorunu | Orta | LLVM fence emit gerekir | V1 SC + fence emit; V2 weak |
| Proof assistant seçimi takılır | Orta | Faz B başlamaz | V1'de kâğıt yeterli; Faz B opsiyonel |
| Frozen region tipinin tasarımı bozar | Düşük | Yeni keyword Kırmızı | Hibrit önerisi (7.E.3) |
| Capability spec implementasyonu gecikir | Orta | DRF-L6 yarım | Lemma stub, capability tamamlanınca güncelle |
| Test sayısı 30'a ulaşamaz | Düşük | Eşik tetiklenmez | Önce negative test + Faz C2 positive |
| Operasyonel semantik dosyası büyük | Düşük | Faz A1 uzun | Sadece DRF için minimal subset yaz |

---

## 10. Referanslar

- KEMGU Bellek Modeli — `belgeler/KEMGU_Bellek_Modeli.md` (Teorem 4 burada)
- Linear Types Spec V1 — `belgeler/KEMGU_Linear_Types_Spec_V1.md`
- Capability Spec V1 — `belgeler/KEMGU_Capability_Spec_V1.md`
- Sabitsüre Spec V1 — `belgeler/KEMGU_Sabitsure_Spec_V1.md`
- Realtime Spec V1 — `belgeler/KEMGU_Realtime_Spec_V1.md` (BET teoremi paterni)
- Direktif Ek v1.1 — `belgeler/KEMGU_Direktif_Ek_v1.1.md`
- Bölge implementasyon — `src/bolge.{h,c}`, `src/escape.{h,c}`,
  `src/bolge_atama.{h,c}`
- Karşılaştırmalı literatür (KEMGU spec'lerinin atıfları):
  - **Reynolds 2002** (separation logic — DRF için klasik temel)
  - **O'Hearn 2007** (CSL — concurrent separation logic)
  - **Klein et al. 2009** (seL4 — Isabelle/HOL mekanize OS doğrulama)
  - **Lattner & Adve 2004** (LLVM IR; KEMGU backend referansı)
  - **Boehm & Adve 2008** (C++11 memory model — weak memory)
  - **Sevcik & Aspinall 2008** (Java MM happens-before)

---

## 11. Çıktı Beklenti Formatı (Bu Plan İçin)

Memory'deki `feedback_cikti_formati` uyarınca her görev sonunda:

- **Ne yapıldı:** Plan dökümanı (bu dosya) + KIRMIZI_QUEUE eklemesi.
- **Ne yapılmadı (bilinçli):** Kod yok, ispat dosyası yok, mevcut Teorem 4
  değişmedi, lang syntax eklenmedi, proof assistant kurulmadı.
- **Test:** Yok (plan dökümanı için test gerekmez).
- **Sonraki adım:** Mehmet Bölüm 7 karar listesini cevaplar → Faz A1 başlar.

---

**END PLAN — DRF Genişletme V1**
