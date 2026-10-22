# Tur 26 — os_rastgele — özet

- Durum: **TAMAM**
- finish: `stop` · blok: `2`
- Uygulanan bloklar (kuru):
  - DÜZENLE stdlib/kripto/rastgele.kem: -25 +22 satır
  - DÜZENLE test/stdlib/test_kripto_kosum.kem: -11 +30 satır

## git diff --stat
```
runtime/kdl_runtime.c             | 45 +++++++++++++++++++++++++++++++++++++++
 selfhost/checker.kem              |  2 ++
 selfhost/codegen.kem              |  5 +++++
 src/llvm.c                        |  5 +++++
 src/tip_kontrol.c                 |  4 ++++
 stdlib/kripto/rastgele.kem        | 19 +++++++----------
 test/stdlib/test_kripto_kosum.kem | 21 +++++++++++++++++-
 7 files changed, 89 insertions(+), 12 deletions(-)
```
