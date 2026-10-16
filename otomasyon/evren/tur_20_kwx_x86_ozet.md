# Tur 20 — kwx_x86 — özet

- Durum: **DOGRULAMA_HATA**
- finish: `stop` · blok: `1`
- Uygulanan bloklar (kuru):
  - DÜZENLE runtime/mimari/x86_64/kem_mmu_hw.kem: -16 +96 satır

## Hata (dogrulama: `build/kemgu --check --mimari x86_64-none runtime/mimari/x86_64/kem_mmu_hw.kem`)
```
412 |     değişken c: tam64 = kem_pvh_oku64(bi + 24);      // cmdline_paddr
    |                           ^

hata[T002]: tanimsiz sembol
    --> runtime/mimari/x86_64/kem_mmu_hw.kem:417:34
    |
417 |         değişken ham: dtam64 = kem_pvh_oku64(c + (i olarak tam64)) olarak dtam64;
    |                                  ^

hata[T002]: tanimsiz sembol
    --> runtime/mimari/x86_64/kem_mmu_hw.kem:421:30
    |
421 |     değişken son: dtam64 = kem_pvh_oku64(c + (n olarak tam64)) olarak dtam64;
    |                              ^

```

## git diff --stat
```
runtime/mimari/x86_64/kem_mmu_hw.kem | 80 ++++++++++++++++++++++++++++++++++++
 1 file changed, 80 insertions(+)
```
