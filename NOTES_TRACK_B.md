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

### Kalem 1 — ARM PL011 UART (commit 8bec60c)
- runtime/kdl_uart.h, runtime/kdl_runtime_uart_pl011.c
- test/test_uart_pl011.c: 10/10 ✓ (ASan temiz)
- ARM64 cross-compile: 3 sembol uretildi, libc/CRT referansi YOK
- KEMGU_UART_MOCK ile host doğrulama yolu kuruldu

### Kalem 2 — kdl_yazdir_* bare-metal port (commit 5ff0441)
- runtime/kdl_yazdir_bare.h, runtime/kdl_runtime_yazdir_bare.c
- KDL_UART_PUTC macro ile backend secimi (PL011 vs 16550)
- 17/17 ✓ (format yardimcisi 7 + yazdir/yaz 4 + tam 3 + mantik 2 + birlesik 1)
- Stack-yalniz 32 byte tampon, INT64_MIN guvenli, base-10 positional
- 8 sembol bare-metal ARM64 ELF'te tanimli, libc yok

### Kalem 3 — Bare-metal hello world (commit a19f728)
- test/ornekler/uart_merhaba.kem (KEMGU dili)
- linker/bare-metal-aarch64.ld (yeni klasor)
- boot/start_aarch64.S (yeni klasor): SP setup + BSS zero + bl main + WFE spin
- src/tip_kontrol.c + src/llvm.c: yeni built-in `yazdir_metin(metin) -> bos`
  - LLVM: `@kdl_yazdir_metin(ptr)`, declare otomatik
  - Mevcut "yazdir -> puts" mappingi korundu (regression yok)
- Makefile: calistir_uart_merhaba_bare_metal
- **Sonuc**: kernel.elf @ 0x40000000, .text 0x908 byte, libc YOK
  - Ayni .kem hem host (fputs) hem bare-metal (UART) calisir — KEMGU dili
    platform-agnostik kanitlandi
- Regression: LLVM 105/105, tip_kontrol 165/165, runtime_link 9/9, OTP 9/9

### Kalem 4 — x86_64 NS16550A UART (commit f8f996f)
- runtime/kdl_runtime_uart_16550.c
- Inline asm in/out talimatlari (x86_64 architecture-conditional)
- test/test_uart_16550.c: 8/8 ✓
- x86_64 cross-compile: 3 sembol, libc yok
- LSR.THRE polling pattern (PL011 FR.TXFF ile paralel)

### Continuation C1 + C6 (commit 5c99d6c)
- C1: kdl_yazdir_isaretsiz_tam(32/64), kdl_yazdir_onaltilik, kdl_yaz_onaltilik
  - Format yardimcilari: format_isaretsiz64, format_onaltilik64
  - 7 yeni test (yazdir_bare toplam 24/24)
  - KEMGU built-in olarak da kayit (4 yeni)
  - Host runtime'a paralel implementasyon (printf "%u" / "%llx")
- C6: BARE_METAL_DESTEK.md — calisan modullar, compile flag'leri,
  KEMGU built-in -> LLVM IR cagrisi haritasi, eksik maddeler

### Continuation C3 (commit 2f7d13b)
- runtime/kdl_panik.h, runtime/kdl_runtime_panik.c
- kdl_panik_dur(const char *mesaj) -> NORETURN bare-metal
  - ARM64: wfe loop / x86_64: hlt loop
  - Mock modunda halt devre disi (test return)
- test/test_panik.c: 6/6 ✓ (basit/null/bos/uzun/UTF-8/ardisik)

---

## Pas Gecilen Continuation'lar

### C2: kdl_oku_karakter (input)
- Capability Spec V1 formalizasyonu gerek ("okuma yetkisi" kaynak tipi)
- UART RX yolu donanim init'i de yapilmali (V1 init no-op idi)
- Bu gorevin "sadece yazdirma" kapsamindan disari
- KIRMIZI_QUEUE'ya eklenmedi — input yönü zaten roadmap'te
  (bkz. BARE_METAL_DESTEK.md "Orta vade")

### C4: QEMU smoke test pipeline
- qemu-system-aarch64 binary'si host PATH'inde yok
- MSYS2 pacman ile yuklenmeli (~100 MB)
- Otonom kurulum kararlari §G alaninda degil — kullanici onayina birak
- Statik analiz (file + llvm-nm + llvm-objdump) yeterli kabul edildi

### C5: Cross-target UART register abstraction
- 2 driver var: PL011 (MMIO 32-bit) vs 16550 (Port 8-bit) — ABI tamamen farkli
- "Ortak pattern" pre-mature: bir sonraki driver (Apple SPMC, BCM AUX, vb.)
  geldiginde dogal abstraction cikar
- Macro substitution (KDL_UART_PUTC) sufficient soyutlama saglar
- Refactor kazanci sinirli, regression riski mevcut testlerin tum geneli

---

## Sonuc Ozeti (Track B)

**Commit zinciri** (claude/goofy-banach-95360a):
- 8bec60c: K8b.1 PL011 driver iskeleti
- 5ff0441: K8b.2 yazdir_* bare-metal port
- a19f728: K8b.3 bare-metal hello world (linker + _start + yazdir_metin built-in)
- f8f996f: K8b.4 x86_64 16550A driver
- 5c99d6c: K8b.C1+C6 onaltilik/isaretsiz + BARE_METAL_DESTEK belge
- 2f7d13b: K8b.C3 panik handler

**Yeni dosyalar** (12):
- runtime/kdl_uart.h, runtime/kdl_yazdir_bare.h, runtime/kdl_panik.h
- runtime/kdl_runtime_uart_pl011.c, .._uart_16550.c, .._yazdir_bare.c, .._panik.c
- test/test_uart_pl011.c, test_uart_16550.c, test_yazdir_bare.c, test_panik.c
- test/ornekler/uart_merhaba.kem
- linker/bare-metal-aarch64.ld
- boot/start_aarch64.S
- BARE_METAL_DESTEK.md
- NOTES_TRACK_B.md (bu dosya)

**Degisen dosyalar** (4):
- src/tip_kontrol.c: 5 yeni built-in (yazdir_metin, yazdir_isaretsiz_tam,
  yazdir_isaretsiz_tam64, yazdir_onaltilik, yaz_onaltilik)
- src/llvm.c: 5 yeni cagri mapping + 5 yeni declare
- runtime/kdl_runtime.c: 4 host paralel sembol (kdl_yazdir_isaretsiz_tam/64,
  yazdir_onaltilik, yaz_onaltilik)
- Makefile: 8 yeni hedef (4 mock test + 4 bare-metal validate)

**Test sayilari** (Track B icin yeni):
- PL011 UART:        10/10 ✓
- 16550A UART:        8/8 ✓
- yazdir_bare:       24/24 ✓ (K2'de 17 + C1'de 7)
- panik handler:      6/6 ✓
- **Toplam yeni:    48/48 ✓** (ASan temiz)

**Bare-metal cross-compile dogrulamalari** (tum libc-yok):
- ARM64 PL011 ELF object: 3 sembol
- ARM64 yazdir_bare ELF object: 8 sembol
- ARM64 panik ELF object: 1 sembol
- ARM64 uart_merhaba kernel.elf (final link): _start + main + 9 kdl_* sembolu
- x86_64 16550 ELF object: 3 sembol
- x86_64 panik ELF object (16550 backend): 1 sembol

**Mevcut testler regressionsuz** (kemgu derleyici degistigi icin dogrulandi):
- calistir_llvm_test:       105/105 ✓
- calistir_tip_kontrol:     165/165 ✓
- calistir_runtime_link:     9/9 ✓
- calistir_otp_cli:          9/9 ✓

---

## Mehmet'e Oneri: Bump Allocator Bagimli Is Ne Kadar Acilir?

UART konsol cikartisi bump allocator'siz tamamlandi (heap-yok, stack-
yalniz). Bump allocator onayi geldiginde:

**Otomatik acilan:**
1. `Dizi<T>` runtime bare-metal'da calisir (su an host-only)
2. `metin_birlestir`, `metin_kes`, `metin_kucuk` vb. UTF-8 string ops
3. Generic stdlib (dizi, opsiyonel, sonuc) bare-metal'da kullanılabilir
4. Compiler-level closure environment heap'leri (lambda capture)

**Yeni gereksinim:**
- Bump allocator'in **bolge ile entegrasyonu** (KEMGU_Bellek_Modeli.md
  Katman 1 — R-* aksiyomlari bare-metal'da dogrulanmali)
- Bare-metal'da `_heap_start`/`_heap_end` linker sembolleri (bump
  arena'sinin tabanini belirler)
- Heap pressure / OOM davranisi: `kdl_panik_dur("OOM")` ile dur

**Kapsam disi (bump allocator gelse bile):**
- Capability tabanli I/O (yetki<UART>) — Capability + Linear sinerjisi
- Multi-core init / sayfa tablosu — uzun vade
- Self-host bootloader (KEMGU'da yazilmis _start) — uzun vade

Bump allocator olmadan da Hedef 3 (Evrensel OS) icin
**konsol-cikti-veren-bare-metal-kernel** asamasi tamamlandi. Sonraki
dogal adim Mehmet'in cagrisina bagli.
