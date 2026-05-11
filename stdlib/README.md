# KEMGU Standart Kütüphane

**Saf KEMGU.** Hiç bir runtime/FFI bağımlılığı yok. Her şey monomorphization
ile somut tipe derlenir.

## Felsefe

Diğer dillerin tuzaklarından kaçınma:

| Tuzak | Kaçınılan Pattern | KEMGU yaklaşımı |
|-------|--------------------|------------------|
| Java `equals`/`hashCode` | Method-zorunluluğu + heap object | Fonksiyonel `esit_mi<T>(a, b)` |
| C `int` taşması | Sessiz bozulma | Tipe göre kontrollü (`tam8`/`tam64`) |
| Python sayısal muğlaklık | Runtime check | Compile-time tip |
| Go yıllar boyunca generic yok | API patlaması | Gün 1'den generic |
| C++ STL karmaşası | Çoklu inheritance, iter chains | Düz fonksiyonlar |
| Rust `'a` lifetime yükü | Programcı annotation | Sıfır annotation |

## Mevcut Modüller

### `stdlib/temel/`

- **`matematik.kem`** — `mutlak`, `en_kucuk`, `en_buyuk`, `kare`, `kup`, `sinirla`, `isaret`
- **`karsilastir.kem`** — `esit_mi`, `farkli_mi`, `karsilastir`, `en_kucuk_uc`, `en_buyuk_uc`
- **`sayisal.kem`** — `ortalama`, `us`, `obe` (GCD), `ekok` (LCM)

## Kullanım

```kemgu
// İleride 'kullan' direktifi ile import edilebilir.
// Şu an: tek dosyada birleştirilerek derleniyor.

işlev main() -> tam32 {
    değişken x: tam32 = mutlak(0 - 42);   // -> 42
    değişken y: tam32 = en_buyuk(10, 32);  // -> 32
    ver x;
}
```

## Tip Kontrolünden Geçer

Tüm modüller `kemgu --check` ile doğrulanmıştır.
Hiçbir runtime gerektirmez — saf type-checked KEMGU.

## Roadmap

| Modül | Durum | Bağımlılık |
|-------|-------|------------|
| temel/matematik | ✓ | Yok |
| temel/karsilastir | ✓ | Yok |
| temel/sayisal | ✓ | Yok |
| koleksiyon/Dizi | ⏳ | Allocator runtime |
| koleksiyon/Tablo | ⏳ | Allocator runtime |
| metin/Metin | ⏳ | Allocator runtime |
| io/yazdir | ⏳ | Syscall layer |
| iş/Görev | ⏳ | Thread runtime |

Runtime gereksinimleri olmadan yazılabilecek modüller önce. Diğerleri
KEMGU runtime (allocator + syscall) eklendikten sonra gelecek.
