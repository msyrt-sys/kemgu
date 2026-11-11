# Dalga 9 — ekran konsolu ve PS/2

Başlangıç: 2026-11-11T18:01:49.463286+00:00

- D-750 origin/main üzerinde; taban 3305918.
- Taban ekran 5/5, kabuk NVMe/AHCI/USB 15/15: rc=0.
- VARSAYIM: görev dosyası ana checkout’ta yok; otomasyon/codex-gorev9 dalındaki tam metin okundu.

- [x] A — gömülü 8×16 font; ekran 5/5, kabuk 15/15, köprü kart + 127 test; glif sabotajı gerçek QEMU 1/1 kırmızı ve dogrula exit=2. 2026-11-11T18:09:05.704444+00:00

- [x] B — canlı yankı, geri silme, ters imleç, 70 satır kaydırma; ekran 5/5, kabuk 15/15, köprü kart + 128 test. Yankı/kaydırma sabotajları 1/1 kırmızı; heap üst sınırı 3,28 MB. 2026-11-11T18:17:18.563180+00:00
