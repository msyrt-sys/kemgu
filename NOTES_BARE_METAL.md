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

(Her Kalem sonrası burası güncellenir.)

- [ ] **K0:** Keşif + NOTES + branch
- [ ] **K1:** CLI `--hedef` flag
- [ ] **K2:** LLVM emission hedef-duyarlı
- [ ] **K3:** `KEMGU_BARE_METAL` guard altyapısı + etiketleme
- [ ] **K4:** `BARE_METAL_DESTEK.md` stdlib uyumluluk haritası
- [ ] **K5:** `linker/bare-metal-aarch64.ld` + `_baslat` entry
- [ ] **K6:** `BUMP_ALLOCATOR_SPEC_TASLAK.md`
- [ ] **K7:** Test pipeline genişletme (objdump libc-symbol kontrol)

---

## Parking Lot

(Karar verilemeyen veya V2'ye saklı maddeler — Mehmet onayı sonrası ele alınır.)

(boş — başlangıç)

---

## Bulunan Çakışma / Halt Riskleri

(Halt olursa burası doldurulur.)

(boş — başlangıç)
