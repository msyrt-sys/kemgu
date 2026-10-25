# Tur 27 — metin_eksikler — özet

- Durum: **DOGRULAMA_HATA**
- finish: `stop` · blok: `2`
- Uygulanan bloklar (kuru):
  - DÜZENLE stdlib/metin.kem: -9 +145 satır
  - DÜZENLE test/stdlib/test_metin.kem: -2 +62 satır

## Hata (dogrulama: `mkdir -p /tmp/claude-1000/evren && cat stdlib/metin.kem test/stdlib/test_metin.kem > /tmp/claude-1000/evren/m.kem && build/kemgu --check /tmp/claude-1000/evren/m.kem && build/kemgu --llvm /tmp/claude-1000/evren/m.kem > /tmp/claude-1000/evren/m.ll && clang -x ir /tmp/claude-1000/evren/m.ll -x none build/kdl_runtime.o -o /tmp/claude-1000/evren/m && /tmp/claude-1000/evren/m`)
```
HATA: /tmp/claude-1000/evren/m.kem — parser 0, yukleme 0, tip kontrol 1, realtime 0 (toplam 1 hata).
hata[T010]: dizi_olustur tam olarak bir arguman gerektirir
    --> /tmp/claude-1000/evren/m.kem:540:45
    |
540 |     değişken d: Dizi<tam32> = dizi_olustur();
    |                                             ^

```

## git diff --stat
```
stdlib/metin.kem           | 136 +++++++++++++++++++++++++++++++++++++++++++++
 test/stdlib/test_metin.kem |  60 ++++++++++++++++++++
 2 files changed, 196 insertions(+)
```
