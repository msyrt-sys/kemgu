# KEMGU Statik Dizi Uzunluğu Spec V1 — `Dizi<T, N>`

**Durum:** UYGULANDI (2026-07-28). Tasarım kararları Mehmet tarafından onaylandı;
DZ.8 adımlarının tamamı indi. Uygulama sırasında ortaya çıkan iki sapma DZ.11'de.
**Kapsam:** Aşama (a) — bildirim-yeri arity kontrolü (DZ001-DZ005) **+ Aşama (b)**
— büyütücü-parametre etki analizi (DZ006). İkisi de indi.
**⚠ Buna rağmen `Dizi<T, N>` TAM invaryant DEĞİLDİR** — kapanış ve yapı-alanı
yolları ölçülerek açık bulundu; WCET hâlâ N'e dayanamaz. Ayrıntı DZ.5.

---

## DZ.0 — Motivasyon

### Bu spec'i doğuran somut olay

D-339'da `stdlib/kripto/karma.kem` içindeki SHA-256 mesaj çizelgesi dizisinin
**62 elemanlı** olduğu ortaya çıktı — algoritma **64** ister. `W[62]`/`W[63]`
sınır dışıydı, yani `sha256_blok_sikistir` **hiç çalışmamıştı**. Kusur şu anda
üretilen kodda değil, **görülebilirlikteydi**:

- Runtime sınır kontrolü hatayı yakaladı (temiz PANİK) — **bellek güvenliği
  ihlal edilmedi**.
- Ama `--check` yakalayamadı: dizi literalinin eleman sayısı, o dizinin
  indekslendiği aralıkla karşılaştırılmıyordu.
- Ve hata yıllarca görünmedi çünkü kripto kapısı yalnız `--check` yapıyordu.

`Dizi<T, N>` bu hata sınıfını **derleme zamanına** çeker.

### Stratejik hedef bağlantısı

- **HEDEF 1 (Kırılamaz Güvenlik):** N, güvenliği *artırmaz* — runtime sınır
  kontrolü zaten OOB'yi temiz panikle durduruyor, sessiz bozulma yok. N'in
  kazandırdığı **daha erken teşhis**: PANİK yerine `--check` hatası. Bu ayrımı
  abartmamak önemlidir; aşağıda DZ.5'te V1'in *neyi garanti etmediği* açıkça
  yazılıdır.
- **HEDEF 2 (Maksimum Performans):** `belgeler/KEMGU_Realtime_Spec_V1.md`
  (§RT.12 ve "V2 İçin Saklı Patterns" #3) `için x: buf` döngüsüne statik bound
  vermek için `Dizi<T, N>`'i **zaten V2 maddesi olarak rezerve etmişti**. Bu
  spec o maddenin tip-sistemi ayağıdır. **⚠ WCET, V1'in N'ine GÜVENEMEZ** —
  bkz. DZ.5.
- **HEDEF 3 (Evrensel OS):** Temsil değişmediği için (DZ.2) x86_64/ARM64 ve
  bare-metal yollarında hiçbir etki yok.

### ASLA listesi

- **ASLA temsil değişikliği.** `Dizi<T, N>` heap `KdlDizi*` kalır. Stack
  `[N × T]` temsiline geçilmez — gerekçe DZ.10.
- **ASLA runtime sınır kontrolünün kaldırılması.** N, `i < N` ispatı vermez;
  kontrol yerinde kalır. Kontrolü elemek bağımlı/refinement tipler ister (V2+).
- **ASLA örtük dönüşüm.** `Dizi<T, 32>` ile `Dizi<T, 64>` **farklı** N'lerdir;
  birbirine geçmez (DZ005).
- **ASLA doğrulanmamış N iddiası.** Uzunluğu bilinmeyen bir değer, N-bilinen bir
  anotasyona sessizce akmaz (DZ004). Loud > silent.
- **ASLA sabit ifade** (V1'de). `Dizi<T, W_SAYI>` V1'de YOK — `vektör<T, N>` ile
  aynı kısıt, aynı gerekçe (derleme-zamanı sabit değerlendirici mevcut değil).

---

## DZ.1 — Tip tanımı ve sözdizimi

```
Dizi<T>        : tip      (uzunluk BİLİNMİYOR — bugünkü davranış, değişmedi)
Dizi<T, N>     : tip      (uzunluk N olarak BİLİNİYOR; N > 0 tamsayı literali)
```

`N` **çıplak tamsayı literali** olmalıdır. Bu, `vektör<T, N>`'in bugünkü kısıtıyla
birebir aynıdır (`src/ifade.c` `parse_tip` VEKTOR dalı, `TOK_TAMSAYI` bekler) ve
aynı gerekçeye dayanır: projede **derleme-zamanı sabit değerlendirici yoktur**.

Belirsizlik yok: bir tip asla tamsayı literaliyle başlayamaz, dolayısıyla
`Dizi<` sonrası `tip` ve ardından gelen isteğe bağlı `, TAMSAYI` tek anlamlıdır.

```kemgu
değişken W: Dizi<sabitsüre<dtam32>, 64> = [ /* tam 64 eleman */ ];
işlev sikistir(blok: Dizi<sabitsüre<dtam32>, 16>) -> tam32 { ... }
```

**Sabit ifade neden V1'de yok — ve neden `vektör` ile birlikte gelmeli.**
Okunabilirlik açısından `Dizi<T, SHA256_W_SAYI>` açıkça üstündür. Ama bu, sıfırdan
bir sabit-değerlendirici altyapısı demektir. Bu altyapı geldiğinde `vektör<T, N>`
de **aynı adımda** yükseltilmelidir; ikisini ayrı ayrı sabit-değerlendiriciye
bağlamak iki farklı "sabit nedir" tanımı üretir.

---

## DZ.2 — Temsil: DEĞİŞMEZ

Bu spec'in en önemli maddesi.

| | |
|---|---|
| Çalışma-zamanı temsili | `KdlDizi*` — `{ ptr veri, i32 boyut, i32 kapasite, i32 eleman_byte }` |
| LLVM IR tipi | `ptr` (bugünkü ile birebir) |
| Sınır kontrolü | **Kalır** (`kdl_dizi.inc`, `i < 0 \|\| i >= boyut` → `kdl_panik`) |
| Kod üretimi | **Hiç değişmez** |

N **yalnız tip düzeyinde** yaşar; `--llvm` çıktısına hiç geçmez. Sonuçları:

- Üretilen IR byte-identik kalır → **codegen FIXPOINT risksiz**.
- Runtime, bare-metal, ARM64 yolları etkilenmez.
- `codegen_diff` / `codegen_bootstrap` korpusu değişmez.
- Mevcut **1316 adet** `Dizi<` kullanımı (stdlib 204, selfhost 258, test 746,
  src 108) olduğu gibi çalışır — N isteğe bağlıdır.

---

## DZ.3 — Akış kuralları

N tek yöne akar: **dışa doğru silinir, içe doğru iddia edilemez.**

| Kaynak | Hedef | Sonuç |
|---|---|---|
| `Dizi<T, 64>` | `Dizi<T>` | ✅ İzinli — N silinir |
| `Dizi<T>` | `Dizi<T, 64>` | ❌ **DZ004** — doğrulanamayan iddia |
| `Dizi<T, 32>` | `Dizi<T, 64>` | ❌ **DZ005** — N uyuşmazlığı |
| `Dizi<T, 64>` | `Dizi<T, 64>` | ✅ İzinli |

Bu kural atama, işlev argümanı, `ver` (dönüş) ve yapı alanı değeri için **aynı**
şekilde uygulanır.

**Gerekçe (silinme neden serbest):** kademeli benimseme. 1316 mevcut kullanımın
hiçbiri bozulmadan, N istenen yere tek tek eklenebilir.

**Gerekçe (daralma neden yasak):** N doğrulanabildiği yerde doğrulanır;
doğrulanamayan yerde iddia edilmesine izin verilmez. Aksi hâlde N, WCET'in ve
okuyucunun güvendiği ama hiçbir şeyin denetlemediği bir yorum satırına dönüşür.
Ergonomik bedeli kabul edildi: `Dizi<T>` döndüren bir işlevin sonucunu
`Dizi<T, 64>`'e almak için o işlevin **dönüş tipi de** N ile yazılmalıdır.

---

## DZ.4 — Kontroller

N'in **bilindiği** her bağlamda:

### DZ001 — Dizi literali eleman sayısı ≠ N
```kemgu
değişken W: Dizi<tam32, 64> = [ /* 62 eleman */ ];
// DZ001: dizi literali 62 eleman iceriyor, tip 64 istiyor
```
**Bu, D-339'daki SHA-256 hatasını doğrudan yakalayan kuraldır.**

### DZ002 — Sabit indeks N sınırı dışında
```kemgu
değişken W: Dizi<tam32, 64> = [ ... ];
değişken x: tam32 = W[64];   // DZ002: sabit indeks 64, sinir 0..63
```
Yalnız indeks **tamsayı literali** olduğunda uygulanır. Değişken indeks
(`W[i]`) V1'de statik olarak denetlenmez — runtime kontrolü devrededir.

### DZ003 — N-bilinen bağlamda büyütme
```kemgu
değişken W: Dizi<tam32, 64> = [ ... ];
dizi_ekle(W, 5);                 // DZ003
dizi_kapasite_ayarla(W, 128);    // DZ003
```
`Dizi<T, N>` "tam olarak N" demektir; büyütmek bu sözü bozar. N yazılmamış
`Dizi<T>` eskisi gibi büyür — **hiçbir mevcut kod kırılmaz**, çünkü N yeni
sözdizimidir ve bugün hiçbir kaynakta yoktur.

### DZ004 / DZ005 — Akış ihlalleri
DZ.3 tablosundaki iki hata durumu.

### P370 / P371 — Parser
- **P370:** `Dizi<T, N>` ikinci argümanı derleme-zamanı tamsayı literali olmalı
  (`vektör`'ün P354'ünün eşi).
- **P371:** `N` pozitif olmalı (`Dizi<T, 0>` ve negatif reddedilir).

---

## DZ.5 — ⚠ V1'İN GARANTİ ETMEDİĞİ: bilinen delik

**Bu bölüm silinmemeli, küçültülmemeli.**

DZ.3 (silinme serbest) ile DZ003 (N-bilinen bağlamda büyütme yasak) birlikte bir
boşluk bırakır. Diziler heap'te ve **referansla** geçtiği için, N'in silindiği bir
çağrının içinde yapılan büyütme çağırana **görünür**:

```kemgu
işlev buyut(xs: Dizi<tam32>) -> tam32 { dizi_ekle(xs, 99); ver 0; }

değişken W: Dizi<tam32, 2> = [1, 2];
buyut(W);        // DZ.3: N silinir → izinli
                 // callee'nin parametresinde N yok → DZ003 tetiklenmez
// W'nin tipi hâlâ Dizi<tam32, 2>; gerçekte 3 elemanlı. N YALAN SÖYLÜYOR.
```

**Ampirik doğrulama (2026-07-28):** yukarıdaki şeklin N'siz hâli derlenip
çalıştırıldı; `dizi_boyut(W)` çağrıdan sonra **3** döndü — yani callee'nin
büyütmesi çağırana gerçekten görünüyor. Delik teorik değil.

### Aşama (b) — ✅ UYGULANDI (DZ006)

**Büyütücü-parametre etki analizi:** gövdesinde bir parametreye `dizi_ekle` /
`dizi_kapasite_ayarla` uygulayan işlevin o parametresi *büyütücü* işaretlenir;
çağrı grafiğinde fixpoint ile yayılır (doğrudan + transitif); N-bilinen argüman
büyütücü parametreye geçemez → **DZ006**.

- **Alias analizi GEREKMEZ** — yalnız parametre adının doğrudan geçirilmesi
  izlenir.
- Yukarıdaki delik örneği artık `DZ006` verir (ölçüldü).

**Analiz yönü — over-approximate.** Bir büyütücüyü *kaçırmak* unsound'dur
(delik açık kalır); *fazladan* işaretlemek yalnızca geçerli kodu reddeder
(loud). Bu yüzden gövde gezicisi **tüm konteyner düğüm tiplerini açıkça
listeler**; `default:` dalı yalnız çocuksuz yapraklar içindir. Yeni bir
konteyner düğüm tipi eklenirse geziciye de eklenmelidir.

**Üç çağrı biçimi de kapsanır** (üçü de ayrı ayrı ölçüldü):
`f(W)` · `m::f(W)` (modül-nitelikli) · `k.f(W)` (metot). Son ikisi ilk
uygulamada **sessizce atlanıyordu** — bkz. DZ.11 (3).

### Bilinen yanlış pozitif: parametre gölgeleme

```kemgu
işlev golge(xs: Dizi<tam32>) -> tam32 {
    değişken xs: Dizi<tam32> = [9];   // param'i gölgeler
    dizi_ekle(xs, 1);                  // YEREL'i büyütür, param'ı DEĞİL
    ver 0;
}
```
Analiz ad-tabanlı olduğu için `xs` parametresi büyütücü işaretlenir → çağıran
`Dizi<T,N>` geçirirse DZ006 alır. **Yanlış pozitif, ama loud** ve güvenli yönde;
çözüm yerel değişkeni yeniden adlandırmaktır. Kapsam takibi V2.

### ⚠ DZ006'dan SONRA HÂLÂ AÇIK OLAN İKİ YOL (ölçüldü, 2026-07-28)

DZ006 **adlandırılmış işlev parametresi** yolunu kapatır — DZ.5'te ölçülen yol
buydu. Ama aynı sınıftan iki yol daha var ve **ikisi de hâlâ açık**:

**(i) Kapanış/lambda** — büyütücü bir lambda'ya geçirmek DZ006 vermez:
```kemgu
değişken W: Dizi<tam32, 2> = [1, 2];
değişken f: işlev(Dizi<tam32>) -> tam32 = |q: Dizi<tam32>| { dizi_ekle(q, 9); ver 0; };
f(W);      // ölçüldü: `--check` OK — DZ006 YOK
```
Sebep: çağrılan şey adlandırılmış bir işlev değil, bir kapanış *değeri*;
büyütücü tablosu AST işlev düğümleriyle anahtarlanır. Kapatmak değer-akışı
analizi ister.

**(ii) Yapı alanı** — N, yapı kurulumunda silinir, sonra alan üzerinden büyütülür:
```kemgu
yapı Kutu { ic: Dizi<tam32>; }
işlev buyut(k: Kutu) -> tam32 { dizi_ekle(k.ic, 9); ver 0; }
değişken k: Kutu = Kutu { ic: W };   // DZ.3: alan N'siz → silinme, izinli
buyut(k);                             // ölçüldü: `--check` OK — DZ006 YOK
```
Sebep: büyütülen şey bir parametre *adı* değil, `k.ic` alan erişimi.

### Sonuç — abartmadan

> **DZ006'dan sonra bile `Dizi<T, N>` TAM bir invaryant DEĞİLDİR.**
> Kapanan: işlev-parametresi yolu (DZ.5'in ölçtüğü yol). Açık kalan: kapanış
> ve yapı alanı yolları (yukarıda ölçüldü).

- ✅ D-339'un SHA-256 hata sınıfını yakalar (DZ001) ve artık büyütücü işlevlere
  kaçmasını da engeller (DZ006).
- ✅ Bellek güvenliğini hiç etkilemez — runtime kontrolü her hâlükârda yerinde.
- ❌ **WCET / `gerçekzamanlı` bound'u HÂLÂ N'e DAYANDIRILAMAZ.** Realtime Spec
  §RT.12'nin `Dizi<T, N>` maddesi, yukarıdaki iki yol da kapanana kadar
  **açılmamalıdır.** (b) beklenen faydayı tam vermedi; bu dürüstçe kaydedilir.

---

## DZ.6 — EBNF değişikliği

`belgeler/KEMGU_Grammar_EBNF.md:208`:

```ebnf
(* ÖNCE *)
dizi_tipi       = "Dizi" "<" tip ">" ;

(* SONRA *)
dizi_tipi       = "Dizi" "<" tip [ "," tamsayi_literal ] ">" ;
```

---

## DZ.7 — Hata kodları

| Kod | Aşama | Anlam |
|---|---|---|
| `P370` | Parser | `Dizi<T, N>` ikinci argümanı tamsayı literali olmalı |
| `P371` | Parser | `N` pozitif olmalı |
| `DZ001` | Tip | Dizi literali eleman sayısı ≠ N |
| `DZ002` | Tip | Sabit indeks N sınırı dışında |
| `DZ003` | Tip | N-bilinen bağlamda büyütme (`dizi_ekle` / `dizi_kapasite_ayarla`) |
| `DZ004` | Tip | Uzunluğu bilinmeyen değer N-bilinen hedefe veriliyor |
| `DZ005` | Tip | N uyuşmazlığı (`Dizi<T,32>` → `Dizi<T,64>`) |
| `DZ006` | Tip | N-bilinen argüman büyütücü parametreye veriliyor (Aşama b) |

`DZ` öneki serbest olarak doğrulandı (mevcut önekler: AS, BL, CP, CT, DRF, E, G,
L, LR, M, MM, P, RT, T, V). `P370/P371` de `P363`'ten sonra boştadır.

---

## DZ.8 — Uygulama planı

Sıra önemlidir: **C oracle'dır**, self-host onu aynalar.

1. **AST + parser (C).** `DUGUM_TIP_DIZI`'ye `uzunluk` alanı (0 = bilinmiyor).
   `parse_tip` Dizi dalına isteğe bağlı `, TAMSAYI` — `vektör` dalının (P352-P355)
   kopyası. P370/P371.
2. **Tip temsili.** `TipBilgisi.dizi`'ye `uzunluk` (0 = bilinmiyor).
   `tip_esit` DZ.3 tablosunu uygular. `tip_yazdir` → `Dizi<tam32, 64>`.
3. **Tip kontrolü.** DZ001 (literal arity), DZ002 (sabit indeks), DZ003
   (büyütme built-in'leri), DZ004/DZ005 (akış).
4. **Codegen: DEĞİŞİKLİK YOK** — bilinçli. Bu adımda `src/llvm.c` **hiç
   dokunulmamalıdır**; dokunulduysa DZ.2 ihlal edilmiş demektir.
5. **Self-host paritesi.** `selfhost/codegen.kem` parser + checker dallarına
   aynı kurallar; `--ast` / `--checkdump` çıktı paritesi korunmalı.
6. **EBNF + CLAUDE.md** güncellemesi.
7. **stdlib benimsemesi (dar).** Önce yalnız `stdlib/kripto/karma.kem`'in
   SHA-256 `W`/`K`/`H0` dizileri N ile yazılır — D-339 kusurunun *tekrar
   edemeyeceğini* gösteren canlı kanıt. Geniş benimseme ayrı adım.

### Kapılar

`calistir_tip_test`, `calistir_tip_kontrol_test`, `calistir_parser_test`,
`calistir_llvm_test`, `calistir_kripto_vektor`, `calistir_stdlib_check`,
`calistir_codegen_diff`, `calistir_checker_diff`, `calistir_codegen_bootstrap`
(FIXPOINT), `calistir_self_driver`.

**Özel beklenti:** `codegen_diff` ve `codegen_bootstrap` çıktısı **byte-identik**
kalmalıdır. Değişirse DZ.2 ihlal edilmiştir → adım geri alınır.

### Sabotaj doğrulaması (zorunlu)

`stdlib/kripto/karma.kem`'deki `W` dizisinden bir eleman silindiğinde **DZ001**
tetiklenmelidir — yani D-339'daki kusur artık `--check` aşamasında yakalanır.
Sabotajın kendisi `diff` ile teyit edilir.

---

## DZ.9 — V2'ye bırakılanlar

- **Aşama (c) — N kaçışının kalan iki yolu** (DZ.5'te ölçüldü, öncelikli):
  büyütücü **kapanışa** geçirilen dizi ve **yapı alanında** taşınan dizi.
  Bunlar kapanmadan `Dizi<T, N>` tam invaryant olmaz.
- **Parametre gölgeleme yanlış pozitifi:** ad-tabanlı eşleme yerine kapsam
  takibi (DZ.5).
- **Sabit ifade N:** `Dizi<T, W_SAYI>` — sabit değerlendirici gerekir;
  `vektör<T, N>` ile **aynı adımda**.
- **WCET tüketimi:** `için x: buf` statik bound (Realtime §RT.12) — **Aşama
  (c)'ye** bağlı. (b) tek başına yetmedi.
- **Self-host DZ002-DZ006:** bugün self-host'ta yalnız DZ001 var.
- **Bağımlı/refinement tipler:** `i < N` ispatı ile runtime sınır kontrolünün
  elenmesi. Ayrı faz; tip sistemi baştan tasarlanır.
- **N-generic işlev:** `işlev f<N>(xs: Dizi<T, N>)` — const-generic parametre.

---

## DZ.11 — Uygulamada ortaya çıkan iki sapma

Spec yazılırken öngörülmemiş, uygulama sırasında **ölçümle** bulundu.

### (1) N, literalden ÇIKARSANMAZ — yalnız açık annotasyondan bilinir

İlk uygulama dizi literalinin gerçek eleman sayısını tipe taşıyordu. Bu,
annotasyonsuz mevcut kodu kırardı:

```kemgu
değişken xs = [1, 2, 3];   // cikarsama N=3 verirdi
dizi_ekle(xs, 4);          // → DZ003! Oysa bu kod bugun GECERLI.
```

Spec'in "hiçbir mevcut kod kırılmaz" sözü, N'in **yalnız açık annotasyondan**
gelmesine bağlıdır. Uygulama buna göre düzeltildi: `DUGUM_DIZI_OLUSTUR`
beklenen-tip yolundan **annotasyonun N'ini** döndürür, literalinkini değil.
Sonuç: `Dizi<T>` yazan (yani 1316 mevcut kullanımın tamamı) hiçbir DZ kuralına
takılmaz.

### (2) Self-host'ta SESSİZ parite kaybı vardı — parser dalı

`Dizi<tam32, 4>` self-host'ta hata vermiyordu; **`TIP_KULLANICI`** üretiyordu
(C: `TIP_DIZI`). Sebep: self-host `Dizi`'yi `parse_generic_args` (tip listesi)
sonrası `dizi_boyut(args) == 1` koşuluyla tanıyor; iki argümanda koşul düşüp
genel kullanıcı-tipi dalına kayıyordu. Hata modu **sessiz**ti — `--check` "OK"
diyordu.

Onarım: C'deki gibi `Dizi` dalı `parse_generic_args`'tan **önce** ele alınır.
N, düğümün `a_deg` alanında metin olarak taşınır. Düz döküm paritesi için C
`ast_duz_yaz`'a da `DUGUM_TIP_DIZI` durumu eklendi; **her ikisi de N'i yalnız
`> 0` iken basar** → N kullanmayan korpusun dökümü byte-identik kalır.

**Checker mantığı İKİ yerde:** `selfhost/codegen.kem` (driver) *ve*
`selfhost/checker.kem` (Aşama 2 referans checker'ı; `checker_diff` harness'ı
bunu kullanır). DZ001 ikisine de eklendi — biri atlanırsa kapı sessizce yeşil
kalırdı (ölçüldü: yalnız `codegen.kem` düzenlendiğinde `checker_diff` kırmızı).

### Self-host kapsamı (V1 sınırı)

Self-host checker'da **yalnız DZ001** var ve yalnız doğrudan
`değişken x: Dizi<T, N> = [ ... ];` şeklinde. Gerekçe: self-host'ta beklenen-tip
yayılımı yok, dolayısıyla DZ002-DZ005 için gereken bağlam mevcut değil. Bu,
D-339'un kusurunun aldığı şekli tam olarak kapsar. **C, oracle'dır**;
DZ002-DZ005 C'ye özgüdür ve `check_korpus` bunları sınamaz.

---

## DZ.10 — 🔴 Kırmızı çizgi: heap-uniform invaryantı

`CLAUDE.md`'nin **KRİTİK** invaryantı:

> Self-host derleyici heap-uniform: diziler **her zaman** heap'te (`KdlDizi*`),
> stack `[N × T]` yolu YOK. Bu yüzden self-host codegen'de inline stack-OOB
> kontrolü hiç gerekmez.

`Dizi<T, N>` "artık uzunluk belli, stack'e alalım" fikrini **davet edecektir.**
Bu yapılırsa:

1. Self-host'a stack dizi yolu girer;
2. CLAUDE.md kuralı gereği inline stack-OOB kontrolü **aynı commit'te** zorunlu
   olur;
3. Aksi hâlde kontrolsüz stack dizisi = **bellek-güvenliği regresyonu**.

**Bu spec'in duruşu: N temsili hiç değiştirmez (DZ.2).** Stack temsili istenirse
ayrı bir spec, ayrı bir karar ve kendi OOB-kontrol yükümlülüğüyle gelmelidir.
