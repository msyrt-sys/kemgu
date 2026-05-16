# Bare-Metal Hedef Genişletme — Çalışma Notları

**Branch:** `feature/bare-metal-genisletme` (Faz 2 zincirinden türev)
**Başlangıç:** 2026-05-15
**Mod:** OTONOM (Mehmet izniyle)
**Hedef:** Hedef 3 (Evrensel OS) — self-host bootstrap için bare-metal yol

---

## ADIM 0 — Keşif (özet)

### Mevcut durum

| Yer | Bulgu |
|-----|-------|
| `src/llvm.c:2224` | Hardcoded `target triple = "x86_64-pc-windows-gnu"` |
| `src/llvm.c:2229-2295` | Tüm libc + KDL declare'leri **unconditional** emit |
| `src/llvm.h` + `src/ana.c` + `test/test_bench.c` | `llvm_ir_uret(prog, out)` callsite'ları |
| `src/ana.c:31` | `Mod` enum + `--token/--parse/--check/--llvm/--lsp` pattern |
| `Makefile:493-503` | `calistir_arm64_test` — kernel.kem cross-compile (clang -target aarch64-unknown-none) |
| `runtime/kdl_runtime.c` | libc-bağımlı (printf/fopen/malloc/fread vs.) — pthread/CreateThread ifdef koruma var |
| `stdlib/temel/` | `matematik.kem`, `karsilastir.kem`, `sayisal.kem` — saf aritmetik (bare-metal-safe aday) |
| `stdlib/` üst düzey | `dosya.kem`, `metin.kem`, `dizi.kem`, `kripto.kem`, `opsiyonel.kem`, `sonuc.kem` — runtime'a bağımlı |
| `test/ornekler/kernel.kem` | Bare-metal örnek (bit op + tam64, libc-bağımsız) |
| `KIRMIZI_QUEUE.md` | Bare-metal ile ilgili **aktif madde yok** — çakışma yok |

### Karar Defteri

| # | Karar | Gerekçe |
|---|-------|---------|
| D1 | Yeni branch `feature/bare-metal-genisletme` | Faz 2 zinciri korunur, ayrı temiz çalışma alanı |
| D2 | CLI flag adı: `--hedef=<triple>` | Türkçe kimlik (Direktif §G ASLA: İngilizce karma) |
| D3 | API geriye uyumlu: `llvm_ir_uret(prog, out)` korunur + yeni `llvm_ir_uret_hedef(prog, out, triple)` | test_bench.c gibi eski caller'ları kırmamak |
| D4 | Default triple: `"x86_64-pc-windows-gnu"` (mevcut hardcoded korunur) | Regression riski sıfır |
| D5 | Bare-metal pattern: `*-none-*` ya da `*-unknown-none` substring kontrolü | aarch64-unknown-none, riscv64-unknown-none, x86_64-unknown-none |
| D6 | Guard etiket sözdizimi: `/* BARE-METAL-INCOMPATIBLE: <sebep> */` ve `/* BARE-METAL-SAFE */` | Grep-friendly, Türkçe açıklama ASCII anahtar |
| D7 | Linker entry: `_baslat` (Türkçe; `_start` POSIX pattern karşılığı) | Direktif §G; tam ad konvansiyonu Mehmet onayı bekler — yorum eklenecek |
| D8 | Bump allocator spec V1 — implementation YOK | Direktif §A 🔴 (yeni unsafe primitif) |

---

## Kalem Tamamlama Listesi

- [x] **K0:** Keşif + NOTES + branch `feature/bare-metal-genisletme`
- [x] **K1:** CLI `--hedef` flag (`ana.c`, 12 bilinen triple validasyon)
- [x] **K2:** LLVM emission hedef-duyarlı (`llvm_ir_uret_hedef`,
       `llvm_hedef_bare_metal_mi`); declare bloğu `*-none-*` / `*-unknown-none`
       hedefinde atlanır; %kdl_yetki tip korunur
- [x] **K3:** `KEMGU_BARE_METAL` guard zemini (#error host-runtime'da) +
       her bölüme BARE-METAL-* etiket + Makefile `CC_DEFINES` desteği
- [x] **K4:** `BARE_METAL_DESTEK.md` stdlib uyumluluk haritası — 8 SAF,
       2 KISMI, 3 BAĞIMLI modül; **2629 satırın %69'u (1816 satır) SAF**
- [x] **K5:** `linker/bare-metal-aarch64.ld` + `ENTRY(_baslat)` +
       Makefile `calistir_arm64_link_test` + `calistir_arm64_test`
       `--hedef=aarch64-unknown-none` + objdump libc-symbol kontrol
- [x] **K6:** `BUMP_ALLOCATOR_SPEC_TASLAK.md` — 6 açık soru + önerilen
       default'lar, KIRMIZI_QUEUE'ya madde eklendi (Mehmet onay bekler)
- [x] **K7:** 3 ek IR-level test (toplam **11/11** `test_hedef`) +
       `test_tumu`'a `calistir_arm64_test` eklendi + tam regression yeşil

---

## Parking Lot — V2/Faz 5+

1. **Bump allocator implementation** — BUMP_ALLOCATOR_SPEC_TASLAK.md
   onaylandıktan sonra `runtime/kdl_runtime_bare.c` yazılır
2. **UART/Console driver** (`kdl_yazdir_*` bare-metal port)
3. **HW RNG** (`hw_rastgele_u64` → RDRAND/RNDR/SBI)
4. **Block I/O driver** (`dosya_*` → AHCI/VirtIO/NVMe + FAT32 min FS)
5. **Cooperative scheduler** (`kdl_gorev_*`, `kdl_kanal_*` → IRQ-disable lock)
6. **`_baslat` ⇄ `main` alias** — V1'de manuel; V2'de `--hedef=*-none-*`'da
   otomatik bridge (KEMGU programcısı sadece `main()` yazar)
7. **Multi-arena bump** (V1 tek-arena; V2 `kdl_bolge_olustur_buf`)
8. **Linker symbol entegrasyonu** — `__heap_basi`/`__heap_sonu` runtime'a bağlanır
9. **Branch rename `feature/bare-metal-faz2`** (kullanıcı önerisi `-faz2`
   eki); `feature/bare-metal-genisletme` çalışma branch'i

---

## Bulunan Çakışma / Halt Riskleri

**Halt yaşanmadı.** Routine belirsizlikler "en mantıklı default" ile çözüldü
(NOTES Karar Defteri D1-D8).

Önemli karar:
- K1 build hatası (em-dash karakter) MinGW C11 parser tarafından stray
  karakter; düzeltildi (em-dash → `--`); 5 dakika içinde çözüldü, halt
  kriteri tetiklenmedi.
- K3 `KEMGU_BARE_METAL` define edildiğinde host-runtime `#error` ile
  derleme reddi — istenen davranış, regression değil.

---

## Commit Zinciri

```
b236725 Bare-metal: KIRMIZI_QUEUE bump allocator madde
a4e677a Bare-metal: bump allocator spec taslagi (K6)
c9c0f6f Bare-metal: linker sablonu + _baslat entry (K5)
1ffa001 Bare-metal: stdlib uyumluluk haritasi (K4)
a0ffc98 Bare-metal: KEMGU_BARE_METAL guard zemini + etiketleme (K3)
<K1+K2 commit>  Bare-metal: --hedef CLI flag + LLVM emission hedef-duyarli (K1+K2)
9132ba6  Faz 2 (Altyapi Bootstrap) MVP: dosya I/O 22/22  ← baz
```

## Toplam İstatistik

- **7 commit** (K1+K2 birleşik + K3-K7 + KIRMIZI_QUEUE update)
- **11/11** yeni `test_hedef` (Kalem 1+2+7 birleşik)
- **`test_tumu`** yeşil: 0 regression
- **3 yeni belge:** NOTES_BARE_METAL, BARE_METAL_DESTEK, BUMP_ALLOCATOR_SPEC_TASLAK
- **1 yeni linker script:** linker/bare-metal-aarch64.ld
- **2 yeni Makefile target:** `calistir_hedef_test`, `calistir_arm64_link_test`
- **1 yeni Makefile değişken:** `CC_DEFINES` (opsiyonel `-DKEMGU_BARE_METAL`)
- **calistir_arm64_test** yenilendi (objdump libc-symbol kontrol)
