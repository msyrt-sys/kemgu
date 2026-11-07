# Dalga 6 ilerleme

Başlangıç: 2026-11-07 22:01:33 UTC. D-743 (fd449cc) origin/main 4fc5e85 üzerinde doğrulandı; 21:25–22:01 arasında beş dakikalık fetch döngüsüyle beklendi. Worktree: `/home/msyrt/Desktop/kemgu-codex6`, dal: `codex/usb-dalga6`.

- Taban: derleme rc=0; `make calistir_qtest` rc=0, NVMe ve AHCI C+self dört gerçek QEMU koşumu ve bağımsız imaj kâhini geçti.
- VARSAYIM: Görev 6’nın açık yol/Makefile/harness izinleri eski genel AGENTS.md sınırının bu göreve özgü istisnasıdır. NVMe/AHCI case dalları korunacak.

- [x] G0 · F001/F002/F003/F004/F005 dolanmaları temizlendi; gerekli C001 alias’ları gerekçesiyle korundu. Mevcut 117 köprü testi + kart ve NVMe/AHCI qtest C+self yeşil. Sabotaj: NVMe bayt kopyasında bit7 silindi → gerçek qtest rc=2 (iki derleyicide bayt10), sahte çekirdek rc=1; kaynak geri kondu → iki kapı rc=0. Eski testlerin beklentileri değiştirilmedi. Günlükler `build/codex6/G0-*.log`.
