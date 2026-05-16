# NOTES — Track B: UART / Konsol Driver

Tarih: 2026-05-16
Branch: claude/goofy-banach-95360a (worktree: goofy-banach-95360a)
Görev: KEMGU — UART/Console Driver (Otonom, kapsam-temelli)
Direktif: Ek v1.1 §A/§D/§E/§G

---

## ADIM 0 — Keşif Bulguları

### Beklenen "önceki bare-metal oturum çıktıları"

Görev metni şunların mevcut olduğunu varsayıyordu:
- `BARE_METAL_DESTEK.md` (stdlib haritası) → **YOK**
- `NOTES_BARE_METAL.md` (karar defteri) → **YOK**
- `linker/bare-metal-aarch64.ld` → **YOK** (linker/ klasörü hiç yok)
- `src/llvm.c` bare-metal guard pattern → **YOK** (KEMGU_BARE_METAL hiçbir
  yerde tanımlı değil)
- `--hedef` CLI flag → **YOK**
- `_baslat` entry point → **YOK**

`feedback_kemgu_gorev_dogrulama` memory kuralı ("KEMGU görev talimatları
gerçeklikle uyumsuz olabilir — git+grep ile doğrula") tetikledi:

```bash
$ grep -r 'KEMGU_BARE_METAL\|--hedef\|aarch64-unknown-none\|_baslat' src/ runtime/
# Hiçbir hit yok (kdl_runtime'da pure host kod)
```

### Mevcut olan bare-metal altyapı

- **`calistir_arm64_test` Makefile target** (Makefile:450-461): KEMGU LLVM IR
  çıktısını `clang -target aarch64-unknown-none -x ir … -c` ile ARM64 ELF
  object'e çevirir. Link YOK, sadece `file` + `llvm-objdump -h` doğrulaması.
- **`test/ornekler/kernel.kem`**: ARM64 aritmetik demo (bit shift + masking).
  UART içermez; libc-bağımsız aritmetik gösteriyor.
- **`test/ornekler/09_arm64.kem`**: Aynı, cross-compile dokümantasyon
  yorumlarıyla.
- **`belgeler/MIMARI.md` §ARM64 cross**: `clang -target aarch64-unknown-none`
  yolu belgelenmiş.
- **Toolchain mevcut**: clang 22.1.4, llvm-objdump, mingw32-make
  (`PATH=/c/msys64/clang64/bin:/c/msys64/ucrt64/bin:$PATH` ile).

### Karar: Görev gerçeklik-uyumlu adaptasyon

Çekirdek kapsam (Kalem 1-4) tamamen **bağımsız yeni dosyalar** üzerine
kurulu. UART driver heap-free + raw MMIO ile bump allocator gerekmeden
yazılır. Önceki oturumun çıktıları olmadığı için:

1. `KEMGU_BARE_METAL` compile-time guard'ı **bu görevde tanımlıyorum**
   (yeni runtime dosyalarında `#ifdef KEMGU_BARE_METAL`). LLVM emit binding
   (src/llvm.c'de hedef tabanlı sembol seçimi) **kapsam dışı** olarak
   bırakılıyor — driver C kodu önce, IR binding sonraki görev.
2. `linker/bare-metal-aarch64.ld` + minimal `_start` (asm) → Kalem 3'te
   üretilecek (hello world link için gerekli).
3. `BARE_METAL_DESTEK.md` → Kalem son raporda oluşturulacak.

### Test stratejisi (KEMGU host = Windows x86_64)

Bare-metal kod host'ta çalıştırılamaz (libc yok, donanım yok). Test
yaklaşımı:

1. **Host mock testi** (`KEMGU_UART_MOCK`): MMIO erişimi global buffer'a
   yönlendiren `static inline` adapter. Driver mantığı (formatlama, putc
   loop, FR flag bekleme) host'ta çalışır, mock buffer içeriği doğrulanır.
2. **Cross-compile + sembol testi** (`KEMGU_BARE_METAL` + `clang -target …`):
   Object dosyası üret, `llvm-nm` ile semboller var mı, `llvm-objdump -r`
   ile yasak relocations (libc sembolleri) var mı kontrol.
3. **Hello world link**: linker script + _start ile gerçek ELF üret,
   `llvm-objdump -d` ile section + giriş noktası doğrula.

QEMU smoke testi opsiyonel — toolchain hızlı kuruluyorsa eklenecek;
gerekmiyorsa ELF analiz yeterli (görev metni izin veriyor).

### Self-classification (§A)

Tüm Kalem 1-4 işleri:
- **Yeni internal API + yeni hedef** → 🟡 SARI
- Spec-içi (Direktif Hedef 3 — Evrensel OS bare-metal ilk adımı)
- KIRMIZI_QUEUE'ya girmez (yeni keyword/teorem/ABI değişimi yok)

Bu görev kapsamında: 🟢/🟡 ayrımı yok, direkt yap (Ek v1.1 §H).

---

## Kalem İlerleme Logu

(Her Kalem bitince burayı güncelle: ne yapıldı, test sayısı, commit hash)

### Kalem 1: PL011 — (in-progress)
…
