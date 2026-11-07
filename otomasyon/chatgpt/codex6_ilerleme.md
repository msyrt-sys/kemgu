# Dalga 6 ilerleme

Başlangıç: 2026-11-07 22:01:33 UTC. D-743 (fd449cc) origin/main 4fc5e85 üzerinde doğrulandı; 21:25–22:01 arasında beş dakikalık fetch döngüsüyle beklendi. Worktree: `/home/msyrt/Desktop/kemgu-codex6`, dal: `codex/usb-dalga6`.

- Taban: derleme rc=0; `make calistir_qtest` rc=0, NVMe ve AHCI C+self dört gerçek QEMU koşumu ve bağımsız imaj kâhini geçti.
- VARSAYIM: Görev 6’nın açık yol/Makefile/harness izinleri eski genel AGENTS.md sınırının bu göreve özgü istisnasıdır. NVMe/AHCI case dalları korunacak.

- [x] G0 · F001/F002/F003/F004/F005 dolanmaları temizlendi; gerekli C001 alias’ları gerekçesiyle korundu. Mevcut 117 köprü testi + kart ve NVMe/AHCI qtest C+self yeşil. Sabotaj: NVMe bayt kopyasında bit7 silindi → gerçek qtest rc=2 (iki derleyicide bayt10), sahte çekirdek rc=1; kaynak geri kondu → iki kapı rc=0. Eski testlerin beklentileri değiştirilmedi. Günlükler `build/codex6/G0-*.log`.

- [x] G1 · Sans-IO xHCI çekirdeği; gerçek QEMU C+self iki aygıt enumerasyonu 1.07 sn. Reset/CNR, DCBAA/scratchpad kurma, komut/olay/EP0 halkaları, port reseti, Address Device, 8+18 bayt aygıt ve tam yapılandırma, Set Configuration, Configure Endpoint. Yeni başlangıç/olay testinde geçerli CC1/IN-CC13; hatalı kod/yön/artık/adres/taşıyıcı ve DMA sınırları. Sabotaj ERST64→4: qtest rc=2, sahte başlangıç testi rc=1; geri yükleme → qtest rc=0 ve köprü kart+118 test rc=0. C6-001/C6-002 derleyici bulgularına yalnız sürücü dolanması uygulandı.

- [x] G2 · Gerçek QEMU iki aygıtın VID/PID, sınıf ve uçları ölçülüp QEMU kaynak tanımlarıyla karşılaştırıldı; gerçek DMA çıkış bağlamları Configured/Running/type/MPS denetlendi. Her derleyici için ayrıca klavyesiz taze QEMU olumsuzu: aygıt sayısı hatası, rc=1. Dal 1.12 sn. Sabotaj endpoint add maskesi→1: qtest rc=2, gerçek endpoint bağlamı reddedildi. Geri yükleme → qtest rc=0, köprü kart+118 test rc=0.
