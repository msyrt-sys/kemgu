# Tur 23b — hmac — özet

- Durum: **DOGRULAMA_HATA**
- finish: `stop` · blok: `4`
- Uygulanan bloklar (kuru):
  - DÜZENLE test/stdlib/test_kripto_kosum.kem: -1 +65 satır
  - DÜZENLE test/stdlib/test_kripto_kosum.kem: -1 +3 satır
  - DÜZENLE test/stdlib/test_kripto_kosum.kem: -1 +1 satır
  - DÜZENLE test/kripto_kosum_harness.sh: -1 +1 satır

## Hata (dogrulama: `make calistir_kripto_kosum`)
```
FAIL: kripto koşum bundle --check geçmedi
hata[T002]: tanimsiz sembol
     --> build/_kripto_kosum_1396908/bundle.kem:1533:9
     |
1533 |         ekle(d, sabitsüre_olustur((metin_bayt(s, i) olarak dtam8)));
     |         ^

hata[T002]: tanimsiz sembol
     --> build/_kripto_kosum_1396908/bundle.kem:1543:9
     |
1543 |         ekle(d, sabitsüre_olustur(b));
     |         ^

HATA: build/_kripto_kosum_1396908/bundle.kem — parser 0, yukleme 0, tip kontrol 2, realtime 0 (toplam 2 hata).
make: *** [Makefile:956: calistir_kripto_kosum] Error 1
```

## git diff --stat
```
stdlib/kripto/karma.kem           | 132 ++++++++++++++++++++++++++++++++++++++
 test/kripto_kosum_harness.sh      |   2 +-
 test/stdlib/test_kripto_kosum.kem |  68 +++++++++++++++++++-
 3 files changed, 200 insertions(+), 2 deletions(-)
```
