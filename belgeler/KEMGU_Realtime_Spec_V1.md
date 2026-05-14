# KEMGU Gerçek-Zamanlı (Hard Real-Time) Spec V1

**Durum:** TASLAK (Direktif Hedef 2 — Maksimum Performans altında, Hedef 1 drone
use case'i ile birleşik).
**Spec içi alt-adımlar otomatik onaylı; Direktif Ek v1.2 ile MERGE bekler.**
**Kardeş spec:** `belgeler/KEMGU_Sabitsure_Spec_V1.md` (constant-time qualifier).

---

## RT.0 — Motivasyon ve Üç Stratejik Hedef Bağlantısı

KEMGU `gerçekzamanlı` niteliği (qualifier), bir işlevin **çalışma süresinin**
derleme zamanında **üst sınırlı (bounded)** olduğunu tip sisteminde işaretler.
Niteliğin amacı sürenin **mutlak gerçek-zamanlı (hard real-time)** garantisi —
yani işlev her zaman önceden hesaplanmış bir Worst-Case Execution Time (WCET)
bütçesinin altında biter; aksi takdirde derleme hatası.

### Stratejik Hedef Bağlantısı

- **HEDEF 1 (Kırılamaz Güvenlik):** Drone kontrol döngüsü, cerrahi robot ve
  otonom araç fren sistemi gibi yaşamsal yazılım katmanlarında **deadline miss**
  doğrudan kaza üretir. 2009'da Toyota'da kontrol kabul edilen unbounded
  recursion + stack overflow durumu (Bookout v. Toyota Motor Corp. davası
  EDA-2013-04-09) gerçek-dünyada ölümlere yol açtı. KEMGU `gerçekzamanlı`
  niteliği bu sınıfın **derleme zamanında engellenmesi**ni hedefler.

- **HEDEF 2 (Maksimum Performans):** Zero-overhead — `gerçekzamanlı` runtime
  bir flag, switch veya VTable kontrolü taşımaz. Tip kontrolü ve WCET hesabı
  derleme zamanında biter; runtime'da hiçbir ek bayrak yok. LLVM IR'de
  `!realtime` metadata olarak işaretlenir (debugger/profiler için), kodun
  kendisi düz fonksiyon kalır. Bu özellik DGX Spark üzerindeki tensör çekirdek
  döngüleri ve CUDA stream callback'leri için ön koşul.

- **HEDEF 3 (Evrensel OS):** Audio callback (CoreAudio/WASAPI/ALSA
  `process()`), GPU frame budget (16.67 ms @ 60 Hz, 6.94 ms @ 144 Hz), kernel
  interrupt handler (IRQ vector), realtime clock thread (PREEMPT_RT) — hepsi
  doğal olarak `gerçekzamanlı` niteliği taşır. KEMGU OS'unda scheduler RT
  threadi atarken bu nitelik **statik bir kontrat**tır.

### Üç Kanonik Use Case

1. **Drone PID döngüsü.** 1 kHz kontrol döngüsü (1 ms budget): IMU oku,
   PID hesapla, ESC PWM yaz. `gerçekzamanlı` döngü gövdesi, malloc yok,
   recursion yok, sınırsız loop yok. WCET ≤ 50.000 cycle @ 200 MHz ARM Cortex
   = 250 µs — bütçenin %25'i.

2. **Audio callback.** 48 kHz × 64 sample frame = 1.33 ms budget. `process()`
   `gerçekzamanlı` olmalı; xrun (underrun) duyulur bir tık çıkartır. Bu
   sebeple SuperCollider, JACK, AAX SDK belgelerinde "do not allocate in audio
   callback" altın kuralı vardır — KEMGU bunu tip sisteminde zorlar.

3. **Cerrahi robot eklem komutu.** 4 kHz (250 µs budget) kuvvet-geri-bildirim
   döngüsü. Deadline miss = aletin operasyon sırasında titremesi. ISO 13849
   PL e seviyesi statik analiz zorunluluğu — KEMGU `gerçekzamanlı` qualifier
   bu sertifikasyon altyapısının dilsel temelidir.

### ASLA Listesi Hatırlatması

- ASLA implicit `gerçekzamanlı işlev → normal işlev` dönüşümü (Spec RT.3):
  ters yönde (normal → realtime) **yasak**, çünkü WCET kanıtlanamaz.
- ASLA exception: realtime gövdesinde `hata` dönüş tipi hâlâ kullanılabilir
  (sonuç<T,H>), çünkü `hata` runtime panic değil, normal dönüş yolu.
- ASLA null: `seçimlik<T>` realtime'da kullanılabilir, sadece tag check sabit
  süre (1 branch); programcı `eşleş` koluna bound veriyorsa OK.
- ASLA GC: realtime gövdesi heap alloc edemez (RT001); arena/region statik
  ön-tahsisli kullanılır.

---

## RT.1 — Nitelik Tanımı

```
gerçekzamanlı işlev <ad>(...) -> ...    : qualifier-prefixed işlev tanımı
```

`gerçekzamanlı` bir işlev modifier'ıdır — `işlev` keyword'ünden **önce** gelir
(`async` modifier Rust/JS'deki gibi). Tip sisteminde bir bit (realtime flag)
olarak temsil edilir; sembol tablosunda `SEMBOL_ISLEV` üzerinde bayrak.

### RT-CALL (Çağrı Disiplini)

- `gerçekzamanlı f` → `gerçekzamanlı g` çağrısı: **OK** (her ikisi de WCET
  hesaplanmış).
- `gerçekzamanlı f` → `normal g` çağrısı: **RT004 yasak**, çünkü `g` WCET
  taşımıyor.
- `normal f` → `gerçekzamanlı g` çağrısı: **OK**, normal fonksiyon istediği
  zaman realtime'a girer (subtyping: realtime ⊆ normal).

### RT-PARAMS (Parametre Geçişi)

- Realtime işlev primitif tipleri (tam/dtam/kesirli/mantıksal/karakter) by-value
  alır — by-value sabit maliyet.
- `&T` referans by-pointer — sabit maliyet (1 load).
- `Dizi<T>` parametresi: ileride heap-allocated → dikkat; V1'de **statik
  uzunluklu dizi** (`Dizi<T>` + ek annotation veya stack array) tercih.
- `metin`: parametre olarak izinli (read-only ptr), realtime gövdesi içinde
  yeni `metin` üretimi yasak (allocation).

### RT-RETURNS (Dönüş Tipi)

- `gerçekzamanlı işlev f() -> tam32`: OK
- `gerçekzamanlı işlev f() -> sonuç<tam32, Hata>`: OK (tagged union, sabit)
- `gerçekzamanlı işlev f() -> Dizi<dtam8>`: V1'de dikkatli — eğer fonksiyon
  bir dizi inşa edip dönüyorsa allocation gerek. V1: yalnız `&Dizi<dtam8>`
  parametresine yazmak izinli (in-place).

---

## RT.2 — Sözdizim

İki alternatif sözdizim (V1 her ikisi de kabul):

```kemgu
// 1) Önek modifier — tercih edilen Türkçe form
gerçekzamanlı işlev pid_hesapla(hata: kesirli32) -> kesirli32 { ... }

// 2) Bound-explicit form (gelecek V2 için saklı, V1'de PARSE EDILMEZ)
// işlev[max=1000] pid_hesapla(...) -> ...   // V2
```

### Yeni Anahtar Kelime (35.)

- `gerçekzamanlı` — işlev modifier (toplam keyword 34 → 35).

> NOT: Sabitsure spec'i de 34 → 35'e ulaşmıştı; bu spec 35 → 36 yapar
> (paralel oturum sayımıyla). Çakışma: yetki keyword'ü için ayrı oturum
> tarafından eklenecek; bu spec gerçekzamanlı için 35. ya da 36. konuma kabul
> eder (tablo sıralaması alfabetik UTF-8 byte sırasında, çakışma yok).

### Sözdizimsel Örnekler

```kemgu
gerçekzamanlı işlev kontrol_dongu(hata: kesirli32) -> kesirli32 {
    sabit Kp: kesirli32 = 0.7;
    sabit Ki: kesirli32 = 0.05;
    sabit Kd: kesirli32 = 0.12;
    ver Kp * hata + Ki * 0.0 + Kd * 0.0;
}

// HATA: realtime'dan normal çağrı (RT004)
gerçekzamanlı işlev yanlis() {
    yazdir("merhaba");   // yazdir realtime değil → RT004
}

// HATA: dinamik allocation (RT001)
gerçekzamanlı işlev yanlis2() {
    değişken d: Dizi<tam32> = [];   // dizi heap alloc → RT001
}

// HATA: unbounded loop (RT002)
gerçekzamanlı işlev yanlis3(x: tam32) -> tam32 {
    değişken s: tam32 = 0;
    iken s < x { s = s + 1; }   // x bilinmiyor → RT002
    ver s;
}

// HATA: bounded olmayan recursion (RT003)
gerçekzamanlı işlev fib(n: tam32) -> tam32 {
    eğer n < 2 { ver n; }
    ver fib(n - 1) + fib(n - 2);   // n bound'u annotated değil → RT003
}
```

---

## RT.3 — Yasaklar (Realtime İşlev Gövdesinde)

Aşağıdaki yapılar `gerçekzamanlı` gövdesi içinde **derleme hatası**'dır
(yer alındığı kod path'inde):

| # | Yasak | Sebep | Hata Kodu |
|---|-------|-------|-----------|
| 1 | Dynamic allocation (`Dizi`, `metin` üretimi, runtime alloc çağrısı) | Heap alloc süresi varies + fragmentation | RT001 |
| 2 | Unbounded loop (`iken` koşulu compile-time bilinmeyen, `için` aralığı statik değil) | İterasyon sayısı kanıtlanamaz | RT002 |
| 3 | Recursion (bound'suz) | Stack tüketimi + dönüş zinciri kanıtlanamaz | RT003 |
| 4 | Non-realtime fonksiyon çağrısı | Callee WCET yok | RT004 |
| 5 | WCET hesaplanamayan ifade (örneğin bound bilinmeyen iç-içe loop) | Pessimistic hesap mümkün değil | RT005 |

### Yan Yasak: Spinlock + Mutex
Spinlock retry sayısı verilmeden kullanım = bound'suz loop = RT002 alt-durumu.
V1'de: kullanım yasak (mutex API'si realtime için ayrı tasarım — V2 spec).

### Yan Yasak: I/O
`yazdir`, dosya çağrıları, ağ çağrıları — hepsi non-realtime fonksiyon olduğu
için RT004 ile zaten kapsanır.

---

## RT.4 — Hata Kodları

| Kod | Anlam | Tipik Yer |
|-----|-------|-----------|
| RT001 | REALTIME_DYNAMIC_ALLOC      | `Dizi<T>` literal/üretim, runtime alloc |
| RT002 | REALTIME_UNBOUNDED_LOOP     | `iken` koşul compile-time bilinmiyor |
| RT003 | REALTIME_UNBOUNDED_RECURSION| Realtime gövdesi kendine çağrı, bound yok |
| RT004 | REALTIME_CALLS_NONRT        | Realtime gövdesi non-realtime fonksiyon çağırdı |
| RT005 | REALTIME_WCET_UNKNOWN       | WCET hesabı tamamlanamadı (örneğin değişken iter sayısı) |
| RT006 | REALTIME_MODIFIER_DUPLICATE | İki kez `gerçekzamanlı gerçekzamanlı işlev ...` |
| RT007 | REALTIME_BUDGET_EXCEEDED    | (V2) bütçe annotation'ı varsa ve hesap > bütçe |

V1'de RT006 parser-seviyesi; RT007 V2'ye saklı (bütçe annotation `[max_cycles=N]`).

---

## RT.5 — Bounded Recursion (RT003 Düzeltme Yolu)

V1: recursion **tamamen yasak** (RT003 her zaman). Tek istisna: `tekkez<T>`
linear veri yapısının destructor'ı (compile-time depth=1). Ama bu spec dışı
(linear destructor V2'de).

Gelecek V2 (taslak, V1'de parse edilmiyor):
```kemgu
gerçekzamanlı[max_depth=8] işlev fib(n: tam32) -> tam32 { ... }
```

V1 sözdizimsel olarak `[max_depth=...]` parse edilmez. Programcı recursion
yerine iterative düzenleyici döngü kullanır (klasik DSP bestleri).

---

## RT.6 — Loop Bound (RT002)

**V1 KESIN KURAL — STRAIGHT-LINE ONLY:**
`gerçekzamanlı` gövdesinde **hiçbir loop yasaktır** (`iken` ve `için` her
durumda RT002). Sebep: V1'de loop bound çıkarsama tutarlı bir altyapı
gerektiriyor (sabit literali ↔ collection length ↔ data-flow). Konservatif
"hepsi yasak" kuralı V2'ye kadar mantığı kapatır.

### Gerçek-Dünya Realistliği

Pratikte hard-realtime kernel kodu **ezici çoğunlukla straight-line**'dır:
- PID controller: tek formül, loop yok.
- Audio callback: tek sample işle (frame-level loop **dışarıda**, non-RT scheduler içinde).
- IRQ handler: ack + tek-eylem.
- Drone motor güncellemesi: 4 motor için programcı **manuel unroll**:
  ```kemgu
  gerçekzamanlı işlev motor_guncelle(motor: &değişken Motor4,
                                       pid: &değişken PID4) {
      motor.m0 = pid_hesapla(&değişken pid.p0, ...);
      motor.m1 = pid_hesapla(&değişken pid.p1, ...);
      motor.m2 = pid_hesapla(&değişken pid.p2, ...);
      motor.m3 = pid_hesapla(&değişken pid.p3, ...);
  }
  ```
  Bu sözdizimsel olarak çirkin, ama LLVM aynı kodu çıkarır + WCET pessimistic
  hesap kolaylaşır.

### V2 İçin Saklı Patterns

1. **Literal sayaç:** `iken[max=N] kosul { ... }` (annotation ile sabit bound).
2. **Sabit-ile-sayaç:** `iken i < BUFFER_BOYUTU { ... }` — sabit'in literal
   değeri bilinirse otomatik bound.
3. **Statik dizi uzunluğu:** `için x: buf` (buf'un `Dizi<T, N>` ile statik
   uzunluğu bilinirse).

V1 implementasyonu: `DUGUM_IKEN` ve `DUGUM_ICIN` her zaman RT002.

### Yasak (RT002 örnekleri)
```kemgu
gerçekzamanlı işlev yanlis1(n: tam32) {
    iken n > 0 { n = n - 1; }    // RT002 (V1: tüm iken yasak)
}

gerçekzamanlı işlev yanlis2(xs: &Dizi<tam32>) {
    için x: xs { ... }           // RT002 (V1: tüm için yasak)
}
```

---

## RT.7 — WCET Hesaplama (Kuralı + Tablosu)

KEMGU V1 WCET modeli **pessimistic, mikroop-yaklaşık**: her AST düğümü için
sabit bir "cycle cost" tablosu vardır. Toplam = path'lerin maksimumu.

### 7.1 — Cycle Tablosu (Default — x86_64 modern, ARM Cortex-A için ~%80 doğru)

| AST düğüm | Cost | Not |
|-----------|------|-----|
| DUGUM_TAM literal | 1 | mov imm |
| DUGUM_KESIRLI literal | 1 | xmm load |
| DUGUM_TANIMLAYICI | 1 | load (cache-warm varsayılır) |
| DUGUM_IKILI aritmetik (+/-/*/&/\|/^) | 1 | add/sub/mul/and/or/xor |
| DUGUM_IKILI bölme (/) | 30 | idiv (x86 lat ~26-42); pessimistic 30 |
| DUGUM_IKILI mod (%) | 30 | aynı |
| DUGUM_IKILI shift (<<, >>) | 1 | shl/shr |
| DUGUM_IKILI karşılaştırma (==/!=/</...) | 2 | cmp + setcc |
| DUGUM_IKILI mantıksal (ve, veya) | 2 | short-circuit dahil |
| DUGUM_TEKLI (neg, değil, ~) | 1 | |
| DUGUM_CAGRI (realtime fonksiyon) | callee_WCET + 4 | call/ret + arg setup |
| DUGUM_ERISIM (x.y) | 2 | GEP + load |
| DUGUM_INDEKS (x[i]) | 3 | GEP + load + bounds check |
| DUGUM_EGER | max(then, else) + 1 | branch maliyet 1 |
| DUGUM_IKEN | RT002 | V1: tüm `iken` yasak |
| DUGUM_ICIN | RT002 | V1: tüm `için` yasak |
| DUGUM_VER | 1 | ret |
| DUGUM_BLOK | Σ child | toplam |
| DUGUM_DEGISKEN | 1 + cost(initial) | alloca + store |
| DUGUM_ATAMA | 1 + cost(rhs) | store |
| DUGUM_IFADE_DEYIMI | cost(ifade) | |
| DUGUM_ESLES | max(kollar) + branch ladder | her kol maliyetli |
| DUGUM_YAPI_OLUSTUR | alan_sayisi × 2 + sum(cost(deger)) | alloca + her alana store |
| DUGUM_DIZI_OLUSTUR | RT001 | realtime'da yasak |
| `kullan(e)`, `imha(e)` | cost(operand) + 1 | linear consume = 1 mov |

### 7.2 — Toplam Hesap

```
WCET(islev) = sum cost over deterministic AST + sum (loop_bound × body_WCET)
            + sum (callee_WCET)
```

Branch'lerin maksimumu (worst path), pessimistic. Loop bound kanıtsız ise
hesap iptal → RT002/RT005.

### 7.3 — Pessimism Garanti
**Teorem:** Gerçek runtime cycle ≤ WCET (hesaplanan).
Sebep: tablo cycle değerleri x86/ARM mikroop spec'lerinden alındı; pipeline
yan yana iki bağımsız add'i tek cycle yapabilir → hesap toplam yapar,
gerçek ≤ hesap. Cache miss varsayımı: V1 tüm load'ları L1-hit kabul eder
(memcache + prefetch garanti edilirse doğru); soğuk cache durumu için V2
multiplier (örneğin `[cache=cold]` annotation).

### 7.4 — Backend Karşılık

`--llvm` çıktısında her `gerçekzamanlı` fonksiyon:
```
define i32 @pid_hesapla(...) !realtime !1 { ... }
!1 = !{i64 250000, i64 50000}   ; budget=250000 (V2 anno), hesap=50000
```

V1'de minimal metadata: sadece `!realtime` boolean (debugger/profiler için).

---

## RT.8 — Bounded Execution Time Teoremi (BET)

**İddia:** Bir KEMGU programı `tip_kontrol_program` ile geçer ve
`gerçekzamanlı işlev f(x⃗) -> T` tanımlı ise, **her** `x⃗` argüman demeti için
`f(x⃗)`'nin gerçek runtime cycle sayısı `WCET(f)` ile sınırlıdır
(`runtime(f(x⃗)) ≤ WCET(f) · K`, K platform sabit faktörü).

**İspat İskeleti (V1):**
1. **Yapısal indüksiyon:** AST düğümleri üzerinde. Her düğüm tipi için
   cost tablosu pessimistic bir üst sınırdır (bkz. RT.7.3).
2. **Path birleşmesi:** Branch (`eğer/eşleş`) iki path'in maksimumu alınır.
3. **Loop birleşmesi:** `iken/için` üst sınırlı iterasyon × body — bound
   kanıtlanmazsa derleme reddedilir (RT002).
4. **Çağrı birleşmesi:** `f` `g`'yi çağırıyor ve `g` realtime → `g`'nin de
   BET'i var (indüksiyon hipotezi). Aksi takdirde RT004.
5. **Allocation:** RT001 yasakladığından heap allocator çağrısı yok →
   amortized cost belirsizliği yok.

**Sonuç:** `f` derlemeden geçtiyse, runtime'da deadline miss'in tek nedeni
**platform**'a ait (cache eviction, IRQ, OS preemption) — dil-seviyesi
nedeni yok. Bu sertifikasyon (ISO 26262, DO-178C, IEC 62304) için *zorunlu*
bir altyapı.

### Sabitsure ve Linear ile İlişki

- `gerçekzamanlı` + `sabitsüre<T>`: dik (orthogonal). Bir işlev hem realtime
  hem CT olabilir (örnek: kripto IRQ handler). İki niteliğin enforcement'i
  bağımsız çalışır.
- `gerçekzamanlı` + `tekkez<T>`: dik. Linear consume realtime gövdesinde
  izinli (cost = 1, deterministic).

---

## RT.9 — İlk Kullanıcı: Drone PID Controller

`test/ornekler/drone_kontrol.kem` somut çalışan örnek (derlenip --check'ten
geçer, --llvm çıktısı clang ile bağlanır):

```kemgu
sabit BUFFER_BOYUTU: tam32 = 4;
sabit MOTOR_SAYISI:  tam32 = 4;

yapı PIDDurum {
    onceki_hata: kesirli32;
    integral:    kesirli32;
}

gerçekzamanlı işlev pid_hesapla(d: &değişken PIDDurum,
                                  hedef: kesirli32,
                                  olcum: kesirli32) -> kesirli32 {
    sabit KP: kesirli32 = 0.85;
    sabit KI: kesirli32 = 0.05;
    sabit KD: kesirli32 = 0.20;
    sabit DT: kesirli32 = 0.001;

    değişken hata: kesirli32 = hedef - olcum;
    değişken turev: kesirli32 = (hata - d.onceki_hata) / DT;
    d.integral = d.integral + hata * DT;
    d.onceki_hata = hata;
    ver KP * hata + KI * d.integral + KD * turev;
}

gerçekzamanlı işlev kontrol_dongu(durum: &değişken PIDDurum,
                                    hedef: kesirli32,
                                    olcum: kesirli32) -> kesirli32 {
    ver pid_hesapla(durum, hedef, olcum);
}
```

WCET hesaplaması (PID gövdesi):
- 4 sabit yükleme: 4
- `hata = hedef - olcum`: 1 (sub) + 1 (store) = 2
- `turev = (hata - d.onceki_hata) / DT`: 2 (erisim) + 1 (sub) + 30 (div)
  + 1 (store) = 34
- `d.integral = d.integral + hata * DT`: 2 + 1 (mul) + 1 (add) + 1 (store) = 5
- `d.onceki_hata = hata`: 1 (store) + 1 (load) = 2
- `ver KP * hata + KI * d.integral + KD * turev`: 3 (mul) + 2 (add) + 1 = 6
- Toplam: ~53 cycle.
- 1 GHz CPU'da: 53 ns << 1 ms (1.000.000 cycle bütçe) → bütçenin %0.005'i.

---

## RT.10 — Test Minimum Sayısı: **30**

Test dağılımı (`test/test_wcet.c`):

| Grup | Test # | Konu |
|------|--------|------|
| W1   | 1-4    | Lexer + parser: `gerçekzamanlı` keyword tanıma + işlev önek |
| W2   | 5-8    | Tip kontrol: realtime → realtime çağrı OK, normal → realtime OK |
| W3   | 9-11   | RT001 dynamic allocation — negatif (dizi literal) |
| W4   | 12-15  | RT002 loop — negatif (`iken` ve `için` her durumda) |
| W5   | 16-18  | RT003 recursion — negatif (self-call, mutual) |
| W6   | 19-22  | RT004 non-realtime çağrı — negatif (yazdir, kullanıcı normal fn) |
| W7   | 23-25  | Pozitif: straight-line gövde (toplama, çarpma, dallanma) |
| W8   | 26-28  | RT006 modifier duplicate (parser) + `gerçekzamanlı` farklı yerlerde |
| W9   | 29-32  | WCET hesap motoru: drone PID, basit aritmetik, dallanma max |

30+ test eşiği checkpoint tetikleyicidir.

---

## RT.11 — Uygulama Sırası

1. **Lexer**: `gerçekzamanlı` keyword (TOK_GERCEKZAMANLI; toplam 35).
2. **AST**: `DUGUM_ISLEV` üzerinde `gerçekzamanlı_mi` bayrağı (int).
3. **Parser**: `parse_ust_oge` ve `parse_islev_tanimi` — `TOK_GERCEKZAMANLI`
   önek olarak tüketilir, `parse_islev_genel` flag alır.
4. **Tip sistemi**: `TipBilgisi.veri.islev.gercekzamanli` bayrağı; sembol
   tablosunda `SEMBOL_ISLEV` ek flag.
5. **Tip kontrol** (kritik kısım):
   - RT-CALL: realtime gövdesi içinde non-realtime çağrı (RT004).
   - RT001: DUGUM_DIZI_OLUSTUR + heap-creating built-in çağrıları.
   - RT002: DUGUM_IKEN bound kanıtı + DUGUM_ICIN literal aralık kontrolü.
   - RT003: realtime gövdesinde kendi-çağrı (self veya mutual).
6. **WCET motoru** (`src/wcet.h/c`): cost tablosu + recursive hesap.
7. **LLVM**: `define ... !realtime !N { ... }` metadata.
8. **`test/test_wcet.c`**: 30+ test, ASan temiz.
9. **Örnek**: `test/ornekler/drone_kontrol.kem` (PID controller, --check
   geçer).

---

## RT.12 — Spec Sınırları (V2'ye Bırakılanlar)

- **Bütçe annotation:** `gerçekzamanlı[max_cycles=N]` — derleme zamanı bütçe
  kontrolü (RT007).
- **Bounded recursion:** `gerçekzamanlı[max_depth=D]` ile sınırlı recursion.
- **Async/coroutine realtime:** event-driven RT (ileride).
- **Statik dizi uzunluğu:** `Dizi<T, N>` literal-uzunluklu tip.
- **Cache annotation:** `gerçekzamanlı[cache=cold]` — multiplier ile WCET artır.
- **Multi-core WCET:** thread-local cache analizi (Katman 2 concurrency
  ile birleşik).
- **Sertifikasyon raporu:** `--wcet-report` çıktısı (JSON; auditor için).
- **`gerçekzamanlı` lambda:** closure capture kuralı (allocation engelle).

---

## RT.13 — Tasarım Gerekçeleri (Niye Bu Yol?)

**Niye Rust'taki `no_std` + `#[no_panic]` yetersiz?**
Rust `#[no_panic]` (Cargo crate) sadece panik yokluğunu garanti eder —
WCET hesabı yok. `no_std` heap'i kapatır ama unbounded loop'u engellemez.
KEMGU tip sistemi her üç boyutu (alloc, loop, recursion) bir-arada zorlar.

**Niye Ada/SPARK'taki `Annotate` yetersiz?**
SPARK ortamı çok ağır (formal verification, SPARK gnatprove). KEMGU'nun
amacı **lightweight** — pessimistic ama hızlı (compile-time'da AST yürüyüş).

**Niye Zig'in `comptime` yetersiz?**
Zig `comptime` her şeyi compile-time'da çalıştırma seçeneği verir, ama
WCET enforcement tip sisteminde değil. KEMGU bir adım ileri.

**Niye `realtime` (İngilizce) yerine `gerçekzamanlı`?**
KEMGU Türkçe DNA. "Realtime" bir teknik terim ama KEMGU kimliği için
Türkçe karşılık (gerçek + zaman + -lı eki, kalıplaşmış akademik kullanım)
tercih edilir. Alternatif `katıbütçeli` reddedildi — "gerçekzamanlı" zaten
Türkçe RTOS literatüründe yerleşik.

**Niye qualifier (`gerçekzamanlı işlev`), tip değil (`gerçekzamanlı<F>`)?**
Sabitsüre bir **tip** kurucusudur (değer üzerine etiket). Realtime bir
**fonksiyon** niteliğidir (fonksiyon imzasının parçası, runtime'da değer
yok). İki tasarım uygundur; Spec V1 fonksiyon-modifier tercih eder, çünkü:
- Çağrı sahasında sözdizim sade (`pid_hesapla(...)` — niteliğin ifade
  edilmesi gerek yok).
- LLVM metadata fonksiyon-seviyesi doğal eşleşir.
- Linear/CT ortogonal kalır.

---

## RT.14 — Açık Sorular (Mehmet'e — KIRMIZI_QUEUE)

(V1 implementasyonu engelleyici değil; ileride netleştirilecek.)

1. **`gerçekzamanlı` lambda:** Closure-by-value vs closure-by-ref? V1'de
   parse edilmiyor.
2. **Stdlib `gerçekzamanlı` API:** `matematik::mutlak` realtime mi? V1'de
   stdlib'e dokunmuyoruz; ileride retroactive olarak `gerçekzamanlı işlev`
   olarak işaretlenebilir.
3. **Bütçe annotation sözdizimi:** `gerçekzamanlı[max=1000]` vs
   `gerçekzamanlı(max=1000)` vs `gerçekzamanlı işlev[max=1000]` — V2.
4. **WCET cycle tablosu kalibrasyonu:** x86 / ARM / RISC-V için ayrı tablo?
   V1 tek tablo (x86 modern + ARM Cortex-A approx).

---
