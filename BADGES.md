# KEMGU Badge'leri

Bu dosya README için hazır kullanıma uygun shield.io badge'lerini içerir.
Oturum 4 (belgeler) tamamlandığında README'ye eklenecek. Şimdilik
bağımsız bir referans dosyası.

## Hazır badge'ler

### Kopyalanabilir Markdown blokları

```markdown
[![CI](https://github.com/msyrt-sys/kemgu/actions/workflows/ci.yml/badge.svg)](https://github.com/msyrt-sys/kemgu/actions/workflows/ci.yml)
[![Test](https://img.shields.io/badge/test-608%2F608%20%E2%9C%93-brightgreen)](https://github.com/msyrt-sys/kemgu)
[![Snapshot](https://img.shields.io/badge/snapshot-50%2F50-brightgreen)](https://github.com/msyrt-sys/kemgu/tree/main/test/snapshots)
[![Fuzz](https://img.shields.io/badge/fuzz-18000%20iter%2C%200%20crash-brightgreen)](https://github.com/msyrt-sys/kemgu/blob/main/test/test_fuzz.c)
[![Lines](https://img.shields.io/badge/lines-~25k-blue)](https://github.com/msyrt-sys/kemgu)
[![Dil](https://img.shields.io/badge/dil-T%C3%BCrk%C3%A7e%20%E2%9C%93-red)](https://github.com/msyrt-sys/kemgu)
[![C11](https://img.shields.io/badge/C-11-blue)](https://en.cppreference.com/w/c/11)
[![ASan](https://img.shields.io/badge/sanitizer-ASan%20%2B%20UBSan-orange)](https://github.com/msyrt-sys/kemgu)
[![Linear](https://img.shields.io/badge/linear%20types-V1-purple)](https://github.com/msyrt-sys/kemgu/blob/main/belgeler/KEMGU_Linear_Types_Spec_V1.md)
[![License](https://img.shields.io/badge/license-TBD-lightgrey)](#)
```

### Tek tek görsel önizleme

| Badge | Anlam | Kaynak |
|-------|-------|--------|
| ![CI](https://github.com/msyrt-sys/kemgu/actions/workflows/ci.yml/badge.svg) | GitHub Actions CI durumu | `.github/workflows/ci.yml` |
| ![Test](https://img.shields.io/badge/test-608%2F608%20%E2%9C%93-brightgreen) | Birim test sayısı | `make test_tumu` |
| ![Snapshot](https://img.shields.io/badge/snapshot-50%2F50-brightgreen) | Snapshot baseline sayısı | `test/snapshots/` |
| ![Fuzz](https://img.shields.io/badge/fuzz-18000%20iter%2C%200%20crash-brightgreen) | Fuzzer kapsamı | `test_fuzz.c` + `test_fuzz_advanced.c` |
| ![Lines](https://img.shields.io/badge/lines-~25k-blue) | Toplam satır sayısı | `tools/sayim.py` |
| ![Dil](https://img.shields.io/badge/dil-T%C3%BCrk%C3%A7e%20%E2%9C%93-red) | Türkçe DNA | Stratejik hedef |
| ![C11](https://img.shields.io/badge/C-11-blue) | Derleyici çekirdek dili | `Makefile` -std=c11 |
| ![ASan](https://img.shields.io/badge/sanitizer-ASan%20%2B%20UBSan-orange) | Test sanitizer | Clang64 + libasan |
| ![Linear](https://img.shields.io/badge/linear%20types-V1-purple) | Linear Types V1 | `belgeler/KEMGU_Linear_Types_Spec_V1.md` |
| ![License](https://img.shields.io/badge/license-TBD-lightgrey) | Lisans (henüz belirlenmedi) | TBD |

## README'ye eklemek için (oturum 4 sonrası)

Mevcut README başlığı:

```markdown
# KEMGU — Türkçe Sistem Programlama Dili
```

Hemen başlıktan sonra eklenecek:

```markdown
# KEMGU — Türkçe Sistem Programlama Dili

[![CI](https://github.com/msyrt-sys/kemgu/actions/workflows/ci.yml/badge.svg)](https://github.com/msyrt-sys/kemgu/actions/workflows/ci.yml)
[![Test](https://img.shields.io/badge/test-608%2F608%20%E2%9C%93-brightgreen)](https://github.com/msyrt-sys/kemgu)
[![Snapshot](https://img.shields.io/badge/snapshot-50%2F50-brightgreen)](https://github.com/msyrt-sys/kemgu/tree/main/test/snapshots)
[![Fuzz](https://img.shields.io/badge/fuzz-18000%20iter%2C%200%20crash-brightgreen)](https://github.com/msyrt-sys/kemgu/blob/main/test/test_fuzz.c)
[![Lines](https://img.shields.io/badge/lines-~25k-blue)](https://github.com/msyrt-sys/kemgu)
[![Dil](https://img.shields.io/badge/dil-T%C3%BCrk%C3%A7e%20%E2%9C%93-red)](https://github.com/msyrt-sys/kemgu)
[![License](https://img.shields.io/badge/license-TBD-lightgrey)](#)

Türkçe syntax'lı bir sistem programlama dili...
```

## Dinamik sayım

Test ve satır sayıları zamanla değişir. `tools/sayim.py --json` ile
güncel sayıları al, badge URL'lerindeki sayıları güncelle. Örnek:

```bash
python tools/sayim.py --json
```

→ JSON: `{ "test": { "tahmini_birim_test": 608, ... }, "toplam_satir": 25000, ... }`

Sonra BADGES.md'deki sayıları el ile veya bir CI script ile güncelle.

## Lisans

Kullanıcı (Mehmet) henüz lisans seçimi yapmamış. Yaygın seçenekler:

- **MIT** — En geniş kullanım, attribution gerek
- **Apache 2.0** — Patent korumalı, kurumsal dostu
- **BSD-3-Clause** — MIT benzeri ama daha açık
- **AGPL-3.0** — Açık kaynak garantisi (network kullanımı dahil)
- **Mozilla Public License 2.0** — Dosya bazlı copyleft

KEMGU'nun stratejik hedefleri (güvenlik + işletim sistemi vizyonu)
düşünüldüğünde Apache 2.0 veya MPL 2.0 makul. Karar Mehmet'in.
