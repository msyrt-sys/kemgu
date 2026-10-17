# Tur 22 — kul_fs_x86 — özet

- Durum: **TAMAM**
- finish: `stop` · blok: `4`
- Uygulanan bloklar (kuru):
  - DÜZENLE runtime/mimari/x86_64/kem_gorev_hw.kem: -1 +2 satır
  - DÜZENLE runtime/mimari/x86_64/kem_gorev_hw.kem: -1 +9 satır
  - DÜZENLE runtime/mimari/x86_64/kem_mmu_hw.kem: -1 +4 satır
  - DÜZENLE runtime/mimari/x86_64/kem_ring3_hw.kem: -5 +149 satır

## git diff --stat
```
runtime/kem_gorev.kem                  |  92 +--------------------
 runtime/kem_gorev_cekirdek.kem         |  99 +++++++++++++++++++++++
 runtime/mimari/arm64/kem_gorev_hw.kem  |   9 +++
 runtime/mimari/x86_64/kem_gorev_hw.kem |   9 +++
 runtime/mimari/x86_64/kem_mmu_hw.kem   |   3 +
 runtime/mimari/x86_64/kem_ring3_hw.kem | 144 +++++++++++++++++++++++++++++++++
 6 files changed, 267 insertions(+), 89 deletions(-)
```
