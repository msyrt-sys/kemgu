# KOMUTA — KEMGU REAL-OS BRING-UP LOOP (mekanik faz: B2-fs + net; FAZ-A HARİÇ)

Executor bu dosyaya her görev sonunda checkpoint JSON yazar (görev/durum/kanıt).
Loop harness (`bringup-loop.sh`) `🔴` veya `DUR:` görürse durur → insan/stratejist kararı.
FAZ-A (kesme/zaman/görev/EL0) bu loop'a DAHİL DEĞİL; `STOP-FAZ-A` görevinde loop temiz durur.

## Log
- `{"görev":"b2-fs","durum":"YEŞİL","kanıt":"[7] FS RW OK; TAZE-CLONE gate.sh exit 0; commit D-272 e40cd4a","sıradaki":"virtio-net"}`
- `{"görev":"virtio-net","durum":"YEŞİL","kanıt":"[8] NET DEV OK; TAZE-CLONE gate.sh exit 0; commit D-273 783532d","sıradaki":"net-arp"}`
- `{"görev":"net-arp","durum":"YEŞİL","kanıt":"[9] NET ARP OK (gerçek 2-yönlü: ARP request→SLIRP reply→gateway MAC eşleşme); TAZE-CLONE gate.sh exit 0 (çekirdek+[6..9]+test_tumu+FIXPOINT); commit D-274 6af9a61","sıradaki":"net-icmp"}` (executor: in-session)
