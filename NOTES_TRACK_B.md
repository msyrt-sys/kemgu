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

### Continuation C2 + C5 (commit 4f32fd4) — kullanici yeniden degerlendirme istedi

**C2 (UART RX yonu — driver seviyesi):**
- Onceki gerekceyi (Capability Spec gerek) duzelttim: driver-seviyesi RX
  guvenle eklenebilir; capability gating sadece KEMGU dili tarafinda
  formalize edilir.
- PL011: `kdl_uart_pl011_oku_karakter`, `_rx_hazir` (FR.RXFE poll + DR read)
- 16550: `kdl_uart_16550_oku_karakter`, `_rx_hazir` (LSR.DR poll + RBR read)
- Mock RX altyapisi: `kdl_uart_*_mock_rx_doldur`, `_mock_rx_buf`, `_mock_rx_pos`
- Test sayilari: PL011 10→16 (+6), 16550 8→13 (+5)
- Bare-metal cross-compile temiz, yeni sembol icin libc yok

**C5 (cross-target UART abstraction):**
- `KdlUartSurucu` struct (`init`, `putc`, `yaz`, `oku_karakter`, `rx_hazir`)
- `const KdlUartSurucu kdl_uart_pl011_surucu` ve `kdl_uart_16550_surucu`
  (R = ROdata sembolu)
- test/test_uart_vtable.c: 21 test (vtable non-NULL + sembol esitligi +
  PL011 + 16550 ortak generic test fonksiyonlari)
- Indirect call kazanci: ust katman kod tek bir `surucu->putc` cagrisi ile
  her iki driver'da calisir (kernel boot kodu, RTOS scheduler, vb.)

### Continuation C4 (commit pending) — opsiyonel QEMU pipeline
- Makefile `calistir_qemu_smoke` target
- `command -v qemu-system-aarch64` ile sorgular; yoksa graceful skip
- Varsa: `qemu-system-aarch64 -M virt -cpu cortex-a72 -nographic -kernel
  kernel.elf` calistirir, stdout'u yakalar, "Merhaba KEMGU" + "42"
  string aramasi yapar
- QEMU host'ta YOK (MSYS2 pacman ile yuklenebilir: `pacman -S
  mingw-w64-clang-x86_64-qemu`)
- Mevcut durum: target hazir, kullanici QEMU yuklerse otomatik aktif

### Continuation D1+D2+D3+D4 (kullanici 3. yeniden devam istegi) — kalan iki maddenin kapanmasi

**D1: kdl_yazdir_karakter bare-metal port (UTF-8 encode)**
- runtime/kdl_runtime_yazdir_bare.c: `kdl_utf8_encode` helper (RFC 3629)
- `kdl_yazdir_karakter(cp)`, `kdl_yaz_karakter(cp)` — 1-4 byte UTF-8
- 6 yeni test: ASCII (1 byte), Türkçe ğ (2 byte), euro € (3 byte),
  grinning 😀 (4 byte), no-newline yaz, Latin-1 ç
- KEMGU built-in: `yazdir_karakter(karakter) -> bos`, `yaz_karakter`

**D2: oku_karakter host + bare-metal + KEMGU built-in**
- Host runtime/kdl_runtime.c: `kdl_oku_karakter()` — fgetc(stdin) wrapper
- Bare-metal: KDL_UART_OKU_KARAKTER macro backend (default PL011)
- KEMGU built-in: `oku_karakter() -> karakter`
- 2 yeni test (basit + yuksek bit byte)

**D3: kdl_oku_metin line-buffered**
- Hem host hem bare-metal: max-1 byte oku, CR yut, LF/EOF durdur, NUL sonlandir
- C runtime fonksiyonu (KEMGU dili built-in olarak henüz yok — buffer
  parametresi tasarımı sonraki adım, Capability Spec ile bağlı)
- 4 yeni test: basit, CRLF normalize, max kesim, max=2 edge

**D4: uart_echo.kem bare-metal RX -> TX ornegi**
- test/ornekler/uart_echo.kem: `oku_karakter()` + `yaz_karakter()` + sat sonu
- Host calistirma: `echo "Z" | echo.exe` -> "Z" + "Tamam." ✓
- Bare-metal cross-link: kernel_echo.elf, _start + main + 6 kdl_* sembol,
  libc/CRT referansi YOK
- Makefile: calistir_uart_echo_bare_metal

**D fazi test toplami:** yazdir_bare 24 -> 36 (+12); echo host smoke OK.

---

## Pas Gecilen → Yeniden Degerlendirilen Continuation'lar

**Kullanici "önerilerini otomatik uygulayarak devam et" dedi.** Önceden pas
gecilen C2, C4, C5 yeniden incelendi ve uygun olanlar uygulandi.

### C2 — UYGULANDI (commit 4f32fd4)
- Onceki gerekce: "Capability Spec V1 formalizasyonu gerek"
- Duzeltme: driver-seviyesi RX safe (capability gating sadece dil tarafinda
  formalize edilir; C kodunun ham okumasi tehlikeli degil)
- Sonuc: PL011 + 16550 oku_karakter + rx_hazir API'leri tamamlandi

### C4 — UYGULANDI (kismi: QEMU yokken skip)
- QEMU host'ta yok ama Makefile target kuruldu: `calistir_qemu_smoke`
- QEMU yuklenince otomatik aktif olur (kullanici tarafi `pacman -S ...`)
- Otonom QEMU kurulumu yapilmadi (shared sistem modifikasyonu kullanici
  onayina baglidir, §6 destructive action karari)

### C5 — UYGULANDI (commit 4f32fd4)
- Onceki gerekce: "pre-mature, macro substitution yeterli"
- Duzeltme: const ROdata vtable (KdlUartSurucu) ek bellek maliyeti yok,
  ust katman icin gerçek deger var (RTOS scheduler, driver switching),
  21 test ile regression riski denetlendi

### Pas (gerçek halt) — uygulanmadi
- **KEMGU dili tarafinda `oku_karakter` built-in** — driver var ama
  tip_kontrol.c + llvm.c entegrasyonu Capability Spec ile bagli; ayri gorev.
- **Karakter UTF-8 encode bare-metal port** — host'ta var, bare-metal'da
  yok. Kucuk ama scope kayisi (utf8.c'nin bare-metal portu gerekir).

---

## Sonuc Ozeti (Track B)

**Commit zinciri** (claude/goofy-banach-95360a):
- 8bec60c: K8b.1 PL011 driver iskeleti
- 5ff0441: K8b.2 yazdir_* bare-metal port
- a19f728: K8b.3 bare-metal hello world (linker + _start + yazdir_metin built-in)
- f8f996f: K8b.4 x86_64 16550A driver
- 5c99d6c: K8b.C1+C6 onaltilik/isaretsiz + BARE_METAL_DESTEK belge
- 2f7d13b: K8b.C3 panik handler
- 159265f: ilk final rapor (NOTES + README)
- 4f32fd4: K8b.C2+C5 UART RX + vtable (yeniden degerlendirme)
- 938dfd9: K8b.C4 QEMU smoke target + ilk yeniden devam final raporu
- (pending): K8b.D1+D2+D3+D4 UTF-8 karakter + RX okuma + echo ornegi

**Yeni dosyalar** (14):
- runtime/kdl_uart.h, runtime/kdl_yazdir_bare.h, runtime/kdl_panik.h
- runtime/kdl_runtime_uart_pl011.c, .._uart_16550.c, .._yazdir_bare.c, .._panik.c
- test/test_uart_pl011.c, test_uart_16550.c, test_uart_vtable.c,
  test_yazdir_bare.c, test_panik.c
- test/ornekler/uart_merhaba.kem
- test/ornekler/uart_echo.kem
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

**Test sayilari** (Track B icin yeni — 3 yeniden devam istegi dahil):
- PL011 UART:        16/16 ✓ (K1'de 10 + C2'de 6 RX)
- 16550A UART:       13/13 ✓ (K4'te 8 + C2'de 5 RX)
- UartSurucu vtable: 21/21 ✓ (C5)
- yazdir_bare:       36/36 ✓ (K2'de 17 + C1'de 7 + D1+D2+D3'te 12)
- panik handler:      6/6 ✓ (C3)
- **Toplam yeni:    92/92 ✓** (ASan temiz)

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
