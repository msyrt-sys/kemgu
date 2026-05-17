# KEMGU Bölge Çözümleyici (Region Analyzer)

KEMGU'nun **bölge tabanlı bellek modeli**ni implemente eden çözümleyici.
Compile-time'da her değerin yaşam süresini bir bölgeye bağlar; runtime'da
GC veya manuel free olmadan deterministik bellek yönetimi sağlar.

Bu belge `KEMGU_Bellek_Modeli.md`'nin **operasyonel karşılığıdır** — orada
formal aksiyomlar tanımlanır, burada derleyici kodu açıklanır.

## Modüller

- `src/bolge.h`/`src/bolge.c` — Bölge temsili (kategoriler, eşitlik, LCA, ömür sırası)
- `src/bolge_atama.h`/`src/bolge_atama.c` — AST visitor: her ifadeye bölge atar

## Bölge Kategorileri

| Enum | Sembol | Anlam | Ömür |
|------|--------|-------|------|
| `BOLGE_LIT` | ρ_lit | Basit literal — stack/register, bölge yok | iterasyon altı |
| `BOLGE_YEREL` | ρ_yerel(f) | İşlev f'nin yerel bölgesi | yerel (1) |
| `BOLGE_CAGIRAN` | ρ_cagiran(f) | f'yi çağıran fonksiyonun bölgesi | çağıran (1+) |
| `BOLGE_ITERASYON` | ρ_iterasyon(d) | Döngü d'nin iterasyon bölgesi | iterasyon (1−) |
| `BOLGE_GLOBAL` | ρ_global | Program ömrü | global (max) |
| `BOLGE_SAHIP` | ρ_sahip(t) | Görev t'ye ait bölge (**Katman 2**) | thread |
| `BOLGE_KANAL` | ρ_kanal(k) | Kanal k'nin transfer tamponu (**Katman 2**) | thread |
| `BOLGE_BILINMIYOR` | ρ_? | Henüz çözülmemiş | — |
| `BOLGE_HATA` | ρ_HATA | Çözümleme hatası | — |

**Ömür sırası (kısa ≤ uzun):**
```
ITERASYON  <  YEREL  <  CAGIRAN  <  GLOBAL
```

LCA hesabı `R-KOŞUL` aksiyomu için kullanılır — koşullu dallanmada iki dalın
LCA'sı (daha uzun ömürlü olan) atanır.

## R-* Aksiyomları (Katman 1)

| Aksiyom | AST düğümü | Atanan bölge | Koşul |
|---------|------------|--------------|-------|
| `R-LIT` | DUGUM_TAM/KESIRLI/MANTIKSAL/KARAKTER/BOŞ | `ρ_lit` | her zaman |
| `R-YEREL` | DUGUM_METIN/DİZİ_OLUSTUR/YAPI_OLUSTUR/LAMBDA | `ρ_yerel(f)` | escape yok |
| `R-VER` | DUGUM_VER alt ifadesi | `ρ_cagiran(f)` | `ver_baglaminda=1` |
| `R-İTERASYON` | iken/için gövdesi içi | `ρ_iterasyon(d)` | `dongu_derinligi>0` |
| `R-KOŞUL` | DUGUM_EGER iki dalı | `LCA(b1, b2)` | her zaman |
| `R-YOL` | DUGUM_YOL (`modul::ad`) | `ρ_global` | modul üyesi |

### Context tracking

`BolgeAtama` yapısında üç bayrak/sayaç bulunur:

```c
typedef struct {
    Arena *arena;
    const char *islev_adi;     /* aktif f */
    int dongu_derinligi;        /* nested loop sayisi */
    BolgeBilgisi *aktif_iterasyon;
    int ver_baglaminda;         /* ver içindeyiz */
    /* Katman 2 */
    int thread_id_sayaci;
    int kanal_id_sayaci;
    BolgeBilgisi *aktif_gorev;
} BolgeAtama;
```

`bolge_belirle` recursive olarak AST'yi gezer. `ver` ve `iken/için`
girişinde context bayrakları güncellenir, çıkışta restore edilir.

**Sınırlama (bilinen):** Şu an *context-tracking* — tam DFA escape
analizi yok. Yani:

```kem
işlev test() {
    değişken x = [1, 2, 3];    // R-YEREL atanir
    g(x);                       // ama g x'i sakliyorsa? eşcape!
}
```

Tam DFA için: her ifadenin "post-lifetime reachability" tablosu kurulur,
fixpoint hesaplanır. Bu gelecek iyileştirme (direktif Faz 1 → Faz 2 geçişi).

## Katman 2: Concurrency

`R-GÖREV` ve `R-KANAL` ile thread/channel semantiği eklenir.

### Intrinsic'ler (tip_kontrol global scope'ta predeclared)

| İntrinsic | İmza | Anlam |
|-----------|------|-------|
| `_gorev_baslat(h)` | `(dtam64) → dtam64` | Yeni görev başlatır, handle döner; **R-GÖREV** |
| `_gorev_birlestir(h)` | `(dtam64) → boş` | Görev tamamlanmasını bekler; **R-BİRLEŞTİR** |
| `_kanal_olustur()` | `() → dtam64` | Yeni kanal yaratır, id döner; **R-KANAL** |
| `_kanal_gonder(k, v)` | `(dtam64, tam32) → boş` | Değer kanala transfer; ρ_kanal'a |
| `_kanal_al(k)` | `(dtam64) → tam32` | Kanaldan değer al |

### Aksiyomlar

```
R-GÖREV:    _gorev_baslat(c)   ──▶   ρ_sahip(t_yeni)
R-BİRLEŞTİR: _gorev_birlestir(h) ──▶   ρ_cagiran(f) (handle'a bağlı)
R-KANAL:    _kanal_olustur()   ──▶   ρ_kanal(k_yeni)
            _kanal_gonder(k,v) ──▶   v: ρ_yerel → ρ_kanal (transfer)
            _kanal_al(k)       ──▶   alıcının ρ_sahip'i
```

### Çıkarsama örneği

```kem
işlev main() -> tam32 {
    değişken k: dtam64 = _kanal_olustur();   // k: ρ_kanal(0)
    değişken h: dtam64 = _gorev_baslat(0);   // h: ρ_sahip(0)
    _kanal_gonder(k, 42);                     // 42 ρ_lit'ten ρ_kanal'a
    değişken v: tam32 = _kanal_al(k);         // v: alıcı ρ_yerel
    _gorev_birlestir(h);
    ver v;
}
```

## Veri Yapıları

### BolgeBilgisi

```c
struct BolgeBilgisi {
    BolgeKategorisi kategori;
    union {
        struct { const char *islev_adi; int adi_uzunluk; } yerel;
        struct { const char *islev_adi; int adi_uzunluk; } cagiran;
        struct { int dongu_id; } iterasyon;
        struct { int thread_id; } sahip;
        struct { int kanal_id; } kanal;
    } veri;
};
```

Tüm alanlar arena'dan; düğüm sahipliği AST gibi.

### LCA hesabı

```
bolge_lca(b1, b2):
    if b1 == b2: return b1
    if ömür(b1) >= ömür(b2): return b1
    else: return b2
```

`omur_sirasi` döndürdüğü integer karşılaştırılır:
- ITERASYON = 1, YEREL = 2, CAGIRAN = 3, GLOBAL = 4
- SAHIP/KANAL = 2 (yerel benzeri — thread bağlı)

## Test

Şu an `test/test_bolge.c` (17 test — kategori, eşitlik, ömür, LCA) ve
`test/test_bolge_atama.c` (15 test — R-LIT, R-YEREL, R-VER, R-KOŞUL,
R-GÖREV, R-KANAL).

Toplam: **32 bölge testi** ASan altında temiz.

## DRF Bağlantısı

`KEMGU_Bellek_Modeli.md`'deki **Data Race Freedom (DRF) teoremi** ispatı
şu invariant'lara dayanır:

1. Her bölge **tek bir thread'e ait** (sahiplik unique)
2. Threadler arası veri sadece `ρ_kanal` üzerinden transfer
3. `ρ_kanal` send-receive bir kez (FIFO, linear)
4. `ρ_sahip` görev sonunda yok edilir

Çözümleyici (1) ve (2)'yi compile-time'da kanıtlar. (3) ve (4) runtime
implementasyonun (scheduler + kanal kütüphanesi) sorumluluğundadır.

## Hala Eksik

- **Tam DFA escape analizi** — context-tracking yeterli olmayabilir
- **Bölge polimorfizmi** — `işlev f<ρ>(x: &ρ T) -> &ρ T` gibi (henüz
  syntax yok; B grubu spec gerekir)
- **Bölge hatası raporlama** — şu an analyzer hata raporlamıyor
- **Borrow checker** — `&değişken T` ile çakışan ödünç alma kontrolü
