# Tur 12 — d1 — özet

- Durum: **DOGRULAMA_HATA**
- finish: `stop` · blok: `13`
- Uygulanan bloklar (kuru):
  - DÜZENLE src/ana.c: -1 +1 satır
  - DÜZENLE src/ana.c: -6 +10 satır
  - DÜZENLE src/ana.c: -1 +1 satır
  - DÜZENLE selfhost/codegen.kem: -1 +2 satır
  - DÜZENLE selfhost/codegen.kem: -1 +1 satır
  - DÜZENLE selfhost/codegen.kem: -7 +13 satır
  - DÜZENLE Makefile: -1 +3 satır
  - DÜZENLE test/check_kapisi.sh: -1 +1 satır
  - DÜZENLE test/check_kapisi.sh: -1 +1 satır
  - DÜZENLE test/check_kapisi.sh: -1 +1 satır
  - DÜZENLE test/baremetal_diff_harness.sh: -1 +1 satır
  - DÜZENLE test/baremetal_diff_harness.sh: -1 +1 satır
  - DÜZENLE test/baremetal_diff_harness.sh: -1 +1 satır

## Hata (dogrulama: `./build/kemgu --llvm --mimari x86_64-none runtime/kem_dtb.kem | grep -m1 'target triple'`)
```
hata[AS001]: satirici_asm mimari etiketi hedef mimariyle uyusmuyor (--mimari ile hedef sec)
   --> runtime/kem_dtb.kem:37:17
   |
37 |     güvensiz { satıriçi_asm { mimari: arm64 şablon: r#"adrp x9, kdl_dtb_adres
   |                 ^


HATA: runtime/kem_dtb.kem — tip kontrol 1 hata (LLVM IR URETILMEDI).
  Bilerek tip hatali kod derliyorsaniz: --tip-atla
```

## git diff --stat
```
Makefile                       |  4 +++-
 selfhost/codegen.kem           | 13 ++++++++++---
 src/ana.c                      | 10 +++++++---
 test/baremetal_diff_harness.sh |  6 +++---
 test/check_kapisi.sh           |  6 +++---
 5 files changed, 26 insertions(+), 13 deletions(-)
```
