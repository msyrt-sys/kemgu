# Tur 16 — mmu_x86 — özet

- Durum: **DOGRULAMA_HATA**
- finish: `stop` · blok: `1`
- Uygulanan bloklar (kuru):
  - YENİ   runtime/mimari/x86_64/kem_mmu_hw.kem (175 satır)

## Hata (dogrulama: `cat runtime/mimari/x86_64/kem_zaman_hw.kem runtime/mimari/x86_64/kem_mmu_hw.kem > /tmp/claude-1000/evren/mmu_sonda.kem && build/kemgu --check --mimari x86_64-none /tmp/claude-1000/evren/mmu_sonda.kem`)
```
58 |     kem_port_yaz8(64 olarak dtam16, 46 olarak dtam8);    // bölen yüksek bayt
   |     ^

hata[T002]: tanimsiz sembol
   --> /tmp/claude-1000/evren/mmu_sonda.kem:66:5
   |
66 |     kem_tik = kem_tik + 1;
   |     ^

hata[T002]: tanimsiz sembol
   --> /tmp/claude-1000/evren/mmu_sonda.kem:66:15
   |
66 |     kem_tik = kem_tik + 1;
   |               ^

```

## git diff --stat
```
boot/start_x86_64.S | 27 +++++++++++++++++++++++++++
 1 file changed, 27 insertions(+)
```
