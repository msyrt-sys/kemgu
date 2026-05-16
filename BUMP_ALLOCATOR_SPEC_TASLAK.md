# KEMGU Bump Allocator Spec — TASLAK V1

**Tarih:** 2026-05-15
**Faz:** Bare-Metal Hedef Genişletme — Kalem 6
**Durum:** 🔴 KIRMIZI — Direktif Ek v1.1 §A "yeni unsafe primitif" ve
"breaking change" kategorilerine girer. Implementation **bu commit'te YOK**;
sadece spec taslağı. Mehmet onayı bekler.

---

## Amaç

Bare-metal hedefte `malloc`/`realloc`/`free`'nin freestanding karşılığı.
KDL Arena ABI'sini koruyup (KdlArena, kdl_bolge_ayir, kdl_bolge_serbest)
backing layer'ı libc'siz, statik buffer'a yönlendirir. Heap-allocated
KEMGU stdlib modüllerinin (metin, dizi) bare-metal'de çalışmasını sağlar.

---

## Felsefe

- **Region tabanlı:** Tekil bump pointer, tek-arena yaşam döngüsü.
  KEMGU'nun "bölge serbest bırakma" semantiğine doğal uyum.
- **Sıfır overhead:** Allocation = pointer increment. Free = no-op
  (gerçek serbestleme arena reset / serbest bırakma'da toplu).
- **Statik buffer:** Bare-metal'de `malloc` yok; compile-time sabit
  `BUMP_HEAP_BOYUTU` ile linker script `.bss` bölgesinde ayrılır.
- **Belirleyici davranış:** Realtime Spec V1 ile uyum — allocation
  süresi sabit, fragmentation yok, GC pause yok.

---

## ABI

### Yeni Tipler

```c
/* Bump allocator durumu — statik buffer üzerinde takip eder. */
typedef struct {
    char *buf;             /* arena'nın başlangıç pointer'ı (statik) */
    size_t boyut;          /* toplam kapasite (byte) */
    size_t kullanildi;     /* şu ana kadar ayrılmış (byte) */
    size_t hizalama;       /* default 16 byte (ABI uyumu) */
    size_t tasma_sayisi;   /* OOM sayacı (diag) */
} KdlBumpArena;
```

### Mevcut API ile Uyum (Korunan)

KdlArena interface bare-metal'de korunur:

```c
KdlArena *kdl_bolge_olustur(void);          /* statik buffer'a baglar */
void *kdl_bolge_ayir(KdlArena *a, int32_t boyut);
void kdl_bolge_serbest(KdlArena *a);        /* reset — buffer'i sifirlar */
int32_t kdl_bolge_toplam_byte(KdlArena *a);
```

İçeride bare-metal'de `KdlArena` aslında `KdlBumpArena`. Mevcut kullanıcı
kodu değişmez. Host'ta linked-chunk allocator, bare-metal'de tek statik
buffer.

### Yeni Built-in Çağrılar (Opsiyonel — V2)

KEMGU programcısı doğrudan bump arena yönetmek isterse:

```kemgu
bolge_olustur() -> *KdlArena                  // mevcut, aynı semantik
bolge_ayir<T>(b: &KdlArena, n: tam32) -> *T   // mevcut
bolge_serbest(b: *KdlArena) -> ()             // mevcut, bare-metal'de reset
bolge_kullanilan(b: &KdlArena) -> tam64       // YENİ — diag
bolge_kapasite(b: &KdlArena) -> tam64         // YENİ — diag
```

---

## Açık Sorular ve Önerilen Default'lar

### S1 — Statik Buffer Boyutu

**Sorun:** `BUMP_HEAP_BOYUTU` ne olmalı?

**Önerilen default:** **64 KB** (`65536 byte`).
- Linker script `.bss` rezervi (bkz. `linker/bare-metal-aarch64.ld`'ye
  ekleme).
- Compile-time override: `make CC_DEFINES="-DKEMGU_BARE_METAL -DKEMGU_BUMP_HEAP_BOYUTU=131072"`
- Kernel-mode küçük: küçük embedded RTOS için 16 KB yeterli.
- DGX Spark / büyük ARM: 4 MB+ olabilir.

**Alternatif:** Sabit yerine linker symbol (`__heap_basi`, `__heap_sonu`)
ile dinamik. Bu daha esnek ama linker script + runtime senkronizasyon
gerektirir.

**Mehmet Karar:** Default 64 KB + compile-time override ✓ veya linker
symbol ❌

### S2 — Hizalama

**Sorun:** Bump pointer hizalama varsayılanı kaç?

**Önerilen default:** **16 byte** (ARM64/x86-64 maximum primitive align +
SIMD vektör hizalama uyumlu).
- malloc varsayılanı 8 veya 16 (platform).
- KEMGU SIMD `vektör<T, 4-64>` 16-64 byte hizalama gerek; bump arena
  sözleşmesi tüm allocate'leri en az 16'ya hizalar; daha büyük
  hizalama gerekirse explicit param.

**Yeni API (V2):**
```c
void *kdl_bolge_ayir_hizali(KdlArena *a, int32_t boyut, int32_t hizalama);
```

**Mehmet Karar:** Default 16 ✓

### S3 — Taşma Davranışı

**Sorun:** Bump pointer'ı `boyut`'a yetişirse ne olur?

**İki seçenek:**

#### Seçenek A — `NULL` döner, kullanıcı handle eder

```c
void *p = kdl_bolge_ayir(a, 1024);
if (!p) {
    // OOM — kullanıcı kararı: panik, retry, alternatif strategy
}
```

- **Avantaj:** Determinist; kullanıcı kontrolü maksimum
- **Dezavantaj:** Tüm allocator çağrılarında null check zorunlu
- **KEMGU felsefe:** ASLA exception ✓ (null check sonuç<T,IOHata> ile sarılabilir)

#### Seçenek B — Panik (abort/halt)

```c
void *p = kdl_bolge_ayir(a, 1024);
// p NULL olamaz — taşma'da kdl_panik() çağrılır (halt veya reset)
```

- **Avantaj:** Sadelik; null check zorunluluğu yok
- **Dezavantaj:** Belirsizliği saklar; tüm OOM kritik hale gelir
- **Bare-metal:** Tipik kernel davranışı (panic = halt + diagnose)

**Önerilen default:** **Seçenek A (NULL)** — KEMGU felsefesi ile uyumlu.
Kullanıcı `sonuç<*T, IOHata>` sarmalayabilir.

**Mehmet Karar:** A ✓ veya B ❌

### S4 — Reset Semantiği

**Sorun:** `kdl_bolge_serbest(a)` bare-metal'de ne yapmalı?

**Önerilen:** **Arena reset** — `kullanildi = 0`, buffer aynı kalır,
sonraki `kdl_bolge_ayir` baştan başlar.

- Host: linked-chunk free
- Bare-metal: tek statik buffer reset
- KEMGU arena ABI'ye uyumlu: "bölge ömrü bitti → tüm tahsisat tek
  seferde serbest" semantiği.

**Alternatif yok** — bu konsensüs.

### S5 — ABI Uyumluluğu

**Sorun:** `KdlArena` struct layout host vs bare-metal'de aynı mı?

**Önerilen:** **Aynı interface, farklı internal**.
- Host: `KdlArenaChunk *bas; *aktif; size_t toplam_tahsis;`
- Bare-metal: `KdlBumpArena` field'ları (`buf, boyut, kullanildi, ...`)

İki layout farklı ama `kdl_bolge_*` API çağrıları aynı imzayı korur.
Programcı `sizeof(KdlArena)` veya internal field'lara erişmemeli — bu
zaten KEMGU encapsulation prensibi.

**Mehmet Karar:** Aynı interface, farklı internal ✓

### S6 — Multi-arena Desteği

**Sorun:** Bir programda birden çok bump arena olabilir mi?

**Önerilen:** **V1: Tek-arena**. `kdl_bolge_olustur()` her zaman statik
buffer'ın aynı pointer'ını döner; ikinci çağrı reset etmez (önceden
allocate olanlara dokunmaz).

- Bare-metal'de multi-arena için multi-buffer linker rezervi gerek;
  bu V2.
- KEMGU "bölge sahipliği" modeli için tek-arena yeterli — concurrency
  Faz 5+ thread runtime gelince ele alınır.

**V2:** `kdl_bolge_olustur_buf(buf, boyut)` — kullanıcı buffer verir
(stack-allocated veya başka statik bölge).

**Mehmet Karar:** V1 tek-arena ✓

---

## Implementation Skeleton (Faz 5+)

```c
/* Linker script .bss bolgesinden statik buffer */
extern char __heap_basi[];
extern char __heap_sonu[];

/* Compile-time override desteği */
#ifndef KEMGU_BUMP_HEAP_BOYUTU
#define KEMGU_BUMP_HEAP_BOYUTU (64 * 1024)
#endif

static char kdl_bump_buf[KEMGU_BUMP_HEAP_BOYUTU]
    __attribute__((aligned(16)));
static KdlBumpArena kdl_bump_arena = {
    .buf = kdl_bump_buf,
    .boyut = KEMGU_BUMP_HEAP_BOYUTU,
    .kullanildi = 0,
    .hizalama = 16,
    .tasma_sayisi = 0,
};

KdlArena *kdl_bolge_olustur(void) {
    /* V1: tek-arena — her zaman aynı global arena */
    return (KdlArena *)&kdl_bump_arena;
}

void *kdl_bolge_ayir(KdlArena *a, int32_t boyut) {
    KdlBumpArena *ba = (KdlBumpArena *)a;
    if (boyut <= 0) return NULL;
    /* Hizalama */
    size_t mask = ba->hizalama - 1;
    size_t hizalanmis = ((size_t)boyut + mask) & ~mask;
    /* Taşma kontrolü (Seçenek A: NULL) */
    if (ba->kullanildi + hizalanmis > ba->boyut) {
        ba->tasma_sayisi++;
        return NULL;
    }
    void *p = ba->buf + ba->kullanildi;
    ba->kullanildi += hizalanmis;
    return p;
}

void kdl_bolge_serbest(KdlArena *a) {
    KdlBumpArena *ba = (KdlBumpArena *)a;
    ba->kullanildi = 0;
    /* tasma_sayisi diag için korunur */
}

int32_t kdl_bolge_toplam_byte(KdlArena *a) {
    KdlBumpArena *ba = (KdlBumpArena *)a;
    return (int32_t)ba->kullanildi;
}
```

(Bu kod **YAZILMADI** — bu spec'in implementation Faz 5+'tır.)

---

## Test Stratejisi (Faz 5+)

- T1: Tek allocation + reset round-trip
- T2: Hizalama doğrulama (16'nın katı)
- T3: Taşma → NULL dönüşü
- T4: Reset sonrası taşma sayacı korunur
- T5: Multi-allocation + reset toplu serbest
- T6: KdlDizi backing değişimi — mevcut test_dizi_perf bare-metal'de
- T7: kdl_metin_birlestir backing değişimi — test_runtime_link bare-metal'de

---

## Sonraki Adımlar (Onay Sonrası)

1. Linker script güncelle: `__heap_basi`/`__heap_sonu` sembolleri + heap
   bölgesi reserve
2. `runtime/kdl_runtime_bare.c` (yeni) — bump allocator implementation;
   `#ifdef KEMGU_BARE_METAL` ile aktif
3. Makefile: bare-metal pipeline (kemgu --hedef=aarch64-... | clang
   -target | bump_alloc.o + linker_script | ELF)
4. Test: `make calistir_bare_metal_test` — kernel.kem + bump_alloc bağla
5. KIRMIZI_QUEUE: bu spec onaylandığında resolve

---

## Risk Tablosu

| Risk | Olasılık | Etki | Azaltma |
|------|----------|------|---------|
| 64 KB heap kernel'da yetersiz | Orta | Allocator OOM hata zinciri | Compile-time override + diag |
| Multi-thread arena race | Yüksek (Faz 5'te) | UB / corruption | Atomic bump pointer veya per-thread arena (V2) |
| Static buffer .bss bloated | Düşük | ELF büyük | Linker script `.bss` ayrı segment, zero-init optimize |
| Mevcut KdlArena field-bypass | Düşük | Internal layout change kırar | Encapsulation kontrolü — programcı erişmemeli |

---

## KIRMIZI_QUEUE Madde Önerisi

Bu spec onaylanırsa, KIRMIZI_QUEUE'ya:

```
## [2026-05-XX] — Bump Allocator Spec V1 ONAYLI

Karar 1: 64 KB default + compile-time override (KEMGU_BUMP_HEAP_BOYUTU)
Karar 2: 16 byte default hizalama
Karar 3: Seçenek A (NULL dönüşü taşmada)
Karar 4: Arena reset (kullanildi = 0)
Karar 5: Aynı KdlArena interface, farklı internal
Karar 6: V1 tek-arena, V2 multi-arena

Implementation: Faz 5+ runtime/kdl_runtime_bare.c
```

---

**END BUMP_ALLOCATOR_SPEC_TASLAK.md**
