# KEMGU Linear Types Spec V1

**Durum:** ONAYLI (Direktif Ek v1.1 Bölüm B'de listelenir).
**Spec içi alt-adımlar otomatik onaylı.**

---

## B.0 — Motivasyon ve Üç Stratejik Hedef Bağlantısı

KEMGU `tekkez<T>` tipi, **lineer tipler** (Wadler 1990, Idris/ATS/Granule
geleneği) için Türkçe-doğal bir adlandırmadır: "tek kez kullanılabilir değer".

Üç stratejik hedef hizmeti:

- **HEDEF 1 (Kırılamaz Güvenlik):** OneTimePad anahtarı (OTP) tekrar kullanılırsa
  şifre çözülebilir. `tekkez<OTP_Anahtar>` ile aynı anahtarın iki kez kullanımı
  **derleme zamanında** engellenir. Capability handle, lock, secret-zeroize
  benzer şekilde modellenir.
- **HEDEF 2 (Maksimum Performans):** `tekkez<T>` zero-overhead — sadece tip
  düzeyinde varlık; çalışma zamanında `T` ile aynı bellek temsili. Refcount,
  GC, atomic counter yok.
- **HEDEF 3 (Evrensel OS):** Sistem kaynakları (`Dosya`, `Soket`, `Kilit`,
  `Kanal`) için kaçırılmaz `dosya_kapat`, `kilit_serbest_bırak` disiplini.

ASLA listesinden ödün yok: null yok, exception yok, GC yok, implicit conv yok.

---

## B.1 — Tip Tanımı

```
tekkez<T> : tip      (T : tip)
```

`tekkez` sadece bir tip kurucusu, runtime temsili = `T` (zero-overhead). Lineerlik
tip sisteminde takip edilir. Bir bağlamadaki tüm bağlamalar arenada izlenir.

`T` kendisi de `tekkez<...>` olabilir (`tekkez<tekkez<U>>`) — bu durumda iç ve
dış lineerlik ayrı ayrı takip edilir (iç değer dış kabuktan extract edilince
yeni linear bağlama olur).

---

## B.2 — Sözdizim

İki yeni anahtar kelime:

- `tekkez` — tip kurucusu (`tekkez<T>`)
- `imha` — yıkıcı tüketici (`imha(t)`)

Mevcut `kullan` anahtar kelimesi **context-sensitive** olarak iki anlama gelir:
- **Üst düzey deyim:** `kullan modul::Tip;` — import (mevcut, değişmedi)
- **İfade içi:** `kullan(e)` — linear consume (yeni)

Parser ayırımı: deyim başlangıcında `kullan` + tanımlayıcı/`::` zinciri görürse
import; aksi takdirde (özellikle `kullan(...)` formu) ifade.

### Sözdizimsel Örnekler

```kemgu
// Üretim
değişken k: tekkez<tam32> = tekkez_olustur(42);

// Çıkarım (extract) → değer döner, k tüketilir
değişken n: tam32 = kullan(k);

// İmha → değer atılır, k tüketilir
değişken k2: tekkez<tam32> = tekkez_olustur(7);
imha(k2);

// Linear closure (otomatik)
değişken c: tekkez<işlev() -> tam32> = || { kullan(k) };  // k yakalanır
değişken sonuç: tam32 = c();   // closure çağrısı tüketir
```

---

## B.3 — Tip Kuralları

Notasyon: `Γ ⊢ e : τ` — `Γ` bağlamında `e` ifadesinin tipi `τ`.
`Γ ⊨ e : τ ⇒ Γ'` — `e` tüketildikten sonra bağlam `Γ'`.

### L-LINEAR-FORM
```
T : tip
──────────────────
tekkez<T> : tip
```

### L-PROD (Producer Intrinsic)
```
Γ ⊢ e : T
─────────────────────────────────────
Γ ⊨ tekkez_olustur(e) : tekkez<T> ⇒ Γ
```
Producer **lineer borç** üretir; girdi `e` lineer değilse Γ değişmez.

### L-CONS (Extract — `kullan`)
```
Γ ⊢ e : tekkez<T>     e bir bağlama (tanımlayıcı veya path)
──────────────────────────────────────────────────────────
Γ ⊨ kullan(e) : T  ⇒  Γ \ {e}
```

### L-DISPOSE (`imha`)
```
Γ ⊢ e : tekkez<T>
─────────────────────────────────
Γ ⊨ imha(e) : boş  ⇒  Γ \ {e}
```

### L-NO-COPY (Kopyalama Yasağı)
Bir `tekkez<T>` bağlaması atama, çağrı argümanı, yapı alanı veya dizi elemanı
olarak görünüyorsa **owner transfer** (move) yapılır. Aynı bağlama bir kez
transfer edildikten sonra tekrar erişilirse compile error (L002).

### L-NO-ALIAS (Referans Yasağı)
```
e : tekkez<T>
─────────────────────────────────
&e, &değişken e   →  L004 hatası
```

### L-LINEARITY (Tüketim Zorunluluğu)
Her `tekkez<T>` bağlama, geçerli scope **kapanmadan önce tam bir kez**
tüketilmelidir. Tüketim şekilleri:
- `kullan(t)` veya `imha(t)`
- Çağrı argümanına geçirme (ownership devri)
- `ver t` (ownership devri çağırana)
- `tekkez_olustur`'a wrap etme
- Bir `tekkez` field'ı olan yapıyı kurmada alan değeri olarak verme

Tüketilmemiş = compile error (L001 LINEAR_NOT_CONSUMED).

### L-COND (Koşullu Tüketim)
İki dallı koşulda her dal aynı bağlamayı tüketmeli (ya ikisi tüketir ya
ikisi de tüketmez). Aksi: L005 LINEAR_COND_INCONSISTENT.

```
eğer p { kullan(t); } değilse { imha(t); }   // OK — iki dal tüketir
eğer p { kullan(t); }                         // HATA — değilse'de tüketim yok
```

---

## B.4 — Producer Intrinsic

`tekkez_olustur<T>(değer: T) -> tekkez<T>` — built-in derleyici intrinsic'i.
Sembol tablosuna global olarak işaretlenir; gövdesi yok, derleyici doğrudan
tip imzasını verir.

İleride: özel producer'lar (`dosya_aç → tekkez<Dosya>`, `otp_üret → tekkez<OTP_Anahtar>`)
stdlib içinde tanımlanır; bunlar da imzalarında `tekkez<...>` döndürür.

---

## B.5 — Consumer'lar

`kullan(e)` — extract; iç değeri çıkarır.
`imha(e)` — dispose; iç değeri **kullanmadan** atar (çift tüketim koruması).

Consumer'lar `tekkez<T>` üzerinde **arity 1** çağrı sözdizimi kullanır.

---

## B.6 — Stdlib (Tasarım — implementasyon ileride)

İlk sürümde **tasarımlar** (uygulama gelecek faza):

- `Dosya`: `dosya_aç(yol: metin) -> sonuç<tekkez<Dosya>, HataKodu>`,
   `dosya_kapat(d: tekkez<Dosya>) -> boş` (`imha` semantik).
- `OTP_Anahtar`: `otp_üret() -> tekkez<OTP_Anahtar>`,
   `otp_şifrele(anahtar: tekkez<OTP_Anahtar>, mesaj: Dizi<dtam8>) -> Dizi<dtam8>`
   (anahtar tüketilir).
- `Kilit`: `kilit_oluştur() -> tekkez<Kilit>`,
   `kilit_serbest_bırak(k: tekkez<Kilit>) -> boş`.

---

## B.7 — Region / Linear Entegrasyonu

### LR-1 — Sahip-Bölge
`tekkez<T>` değerinin bölgesi her zaman bir **sahip-bölge** (R-YEREL veya
R-VER ile çağırana terfi). Hiçbir zaman R-İTERASYON içinde **tüketilmeden**
yaşamaya bırakılamaz.

### LR-2 — Bölge Yapısına Gömme Yasağı
Bir `tekkez<T>` değeri, kendisi `tekkez<...>` olmayan bir yapıya gömülemez.
İhlal: LR002 LINEAR_REGION_EMBED.

```
yapı Sıradan { x: tekkez<tam32>; }   // HATA LR002 (Sıradan tekkez değil)
yapı Sahip { x: tekkez<tam32>; }
değişken s: tekkez<Sahip> = tekkez_olustur(Sahip { x: tekkez_olustur(1) });   // OK
```

(V1'de yapılar lineer alan içeremez. V2'de "lineer yapı" kavramı eklenebilir;
şimdilik dış sarmalayıcı `tekkez` zorunlu.)

> **V2 GÜNCELLEMESİ (D-313 — uygulandı):** `yapı tekkez K { ... }` eklendi.
> Yapının KENDİSİ lineerdir (tam bir kez tüketilir; L001/L002 + L-COND/L-LOOP
> otomatik uygulanır) ve **LR002'den muaftır** — yalnız lineer yapı lineer alan
> taşıyabilir, çünkü sahiplik zinciri kopmaz. Sıradan `yapı` için yasak sürer.
> ```
> yapı tekkez Kilit { id: tam32; kaynak: tekkez<tam32>; }   // OK (V2)
> yapı Sıradan { x: tekkez<tam32>; }                         // HATA LR002
> ```
> Tüketim: `imha(k)` veya taşıma (çağrı argümanı / `ver`). `kullan` KABUL ETMEZ
> (lineer yapının sarmalanmış değeri yoktur → L007).
> **Kısmi taşıma YASAK:** lineer yapının LİNEER alanı dışarı okunamaz (aynı kaynak
> iki kez imha edilirdi); lineer-OLMAYAN alan okunabilir. Alan-bazlı taşıma V2.1.
> **Sınır:** C derleyicide; self-host henüz kabul etmiyor (gürültülü reddeder).

### LR-3 — Bölge Kapanışında Sızıntısızlık
Bir bölge serbest bırakılmadan önce o bölgedeki tüm `tekkez<T>` bağlamaları
**tüketilmiş** olmalıdır. Tüketilmemiş bağlama = LR001
LINEAR_LEAK_ON_REGION_CLOSE.

### LR-4 — Move ile Bölge Terfi
`ver t` çağrılırsa `t` çağıranın bölgesine devredilir; mevcut scope'taki
bağlama silinir. Tip kontrol bunu otomatik yapar.

---

## B.8 — Closure-Itself-Linear

### LC-1
Bir closure'ın tipi `tekkez<işlev(τ₁,...,τₙ) -> τ_dönüş>` olabilir. Bu, closure'ın
**tek kez çağrılabileceğini** garanti eder.

### LC-2 (Otomatik Promosyon)
Eğer bir closure lineer bir değer yakalıyorsa (yakalama listesinde `tekkez<U>`
varsa), closure kendisi otomatik `tekkez<işlev(...)>` olarak işaretlenir.
Aksi: LC001 LINEAR_CLOSURE_NOT_LINEAR.

```
değişken k: tekkez<tam32> = tekkez_olustur(5);
değişken c = || { kullan(k) };   // c : tekkez<işlev() -> tam32>  (otomatik)
değişken sonuç = c();             // OK — c tüketildi
değişken sonuç2 = c();            // HATA L002 — c tekrar çağrıldı
```

### LC-3 (Çağrı = Tüketim)
Bir `tekkez<işlev(...)>` değerine çağrı uygulamak otomatik `kullan` semantiğine
sahiptir; ek `kullan(c)` yazmaya gerek yok.

### B.8 Test Minimum Sayısı: **50**

Test dağılımı (`test/test_linear.c`):
- L1–L10: Tip ifadesi + tekkez_olustur (producer)
- L11–L18: kullan extract + tek-tüketim
- L19–L24: imha dispose + tek-tüketim
- L25–L30: Single-use enforcement (L001/L002)
- L31–L36: No-copy enforcement (L003)
- L37–L42: No-alias enforcement (L004)
- L43–L46: Region/linear etkileşimi (LR001, LR002)
- L47–L50: Closure-itself-linear (LC001, çağrı semantiği)

50+ test eşiği checkpoint tetikleyicidir.

---

## Hata Kodları Özeti

| Kod | Anlam |
|-----|-------|
| L001 | LINEAR_NOT_CONSUMED — tekkez bağlama scope sonunda tüketilmemiş |
| L002 | LINEAR_DOUBLE_USE — tekkez bağlama iki kez tüketildi (move sonrası erişim) |
| L003 | LINEAR_COPY_ATTEMPT — tekkez kopyalanmaya çalışıldı |
| L004 | LINEAR_REFERENCE_ATTEMPT — tekkez'e referans alındı |
| L005 | LINEAR_COND_INCONSISTENT — koşullu dallar tutarsız tüketim |
| LR001 | LINEAR_LEAK_ON_REGION_CLOSE — bölge kapanırken tüketilmemiş |
| LR002 | LINEAR_REGION_EMBED — tekkez bölge yapısına gömüldü |
| LC001 | LINEAR_CLOSURE_NOT_LINEAR — linear yakalayan closure linear işaretlenmemiş |

---

## Uygulama Sırası

1. Lexer: `tekkez`, `imha` keyword.
2. AST: `DUGUM_TIP_TEKKEZ`, `DUGUM_KULLAN_IFADE`, `DUGUM_IMHA_IFADE`.
3. Parser: `parse_tip` → `tekkez<T>`; `parse_birincil` → `kullan(e)`, `imha(e)`.
4. Tip sistemi: `TIP_TEKKEZ` kategori; `TipBilgisi.lineer` flag.
5. Tip kontrol: producer/consumer + lineerlik takibi (Lineer Bağlama Kayıtları).
6. Region entegrasyonu: bolge_atama'ya tekkez sızıntı kontrolü.
7. Closure-itself-linear: lambda yakalama analizi → otomatik `tekkez` promosyonu.
8. `test/test_linear.c`: 50+ test, ASan temiz.

(LLVM IR çıkışı bu spec'in dışında — runtime overhead yok, mevcut backend
`tekkez<T>`'yi `T` olarak emit eder.)

---

## DRF Teoremi ile İlişki (2026-05-14)

Linear V1'in `L-NO-COPY` + `L-NO-ALIAS` kuralları, **Genişletilmiş DRF
Teoremi** (Teorem 4', `KEMGU_DRF_Teoremi.md`) ispatının temel taşıyıcısıdır.
İlgili lemmalar:

- **DRF-L2 (Linear Move = Cross-Thread No-Alias)** — `tekkez<T>` thread
  spawn'da move semantiği; çağıran thread `v`'ye erişimini kaybeder.
- **DRF-L3 (Linear Closure Soundness)** — LC-2 ile otomatik
  `tekkez<işlev>` olan closure'lar yalnız spawn edilen thread'de çalışır.
- **DRF-L5 (Channel Atomicity)** — `kullan(t)` semantiğinin kanal
  transferinde atomik move ile birleşimi.

Detay: [`KEMGU_DRF_Lemmalar.md`](KEMGU_DRF_Lemmalar.md), Plan referansı
[`KEMGU_DRF_Genisletme_Plan.md`](KEMGU_DRF_Genisletme_Plan.md) Karar C.
