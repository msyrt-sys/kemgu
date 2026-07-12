# KOMUTA — KEMGU REAL-OS BRING-UP LOOP (mekanik faz: B2-fs + net; FAZ-A HARİÇ)

Executor bu dosyaya her görev sonunda checkpoint JSON yazar (görev/durum/kanıt).
Loop harness (`bringup-loop.sh`) `🔴` veya `DUR:` görürse durur → insan/stratejist kararı.
FAZ-A (kesme/zaman/görev/EL0) bu loop'a DAHİL DEĞİL; `STOP-FAZ-A` görevinde loop temiz durur.

## Log
- `{"görev":"b2-fs","durum":"YEŞİL","kanıt":"[7] FS RW OK; TAZE-CLONE gate.sh exit 0; commit D-272 e40cd4a","sıradaki":"virtio-net"}`
- `{"görev":"virtio-net","durum":"YEŞİL","kanıt":"[8] NET DEV OK; TAZE-CLONE gate.sh exit 0; commit D-273 783532d","sıradaki":"net-arp"}`
- `{"görev":"net-arp","durum":"YEŞİL","kanıt":"[9] NET ARP OK; TAZE-CLONE gate.sh exit 0; commit D-274 6af9a61","sıradaki":"net-icmp"}`
- `{"görev":"net-icmp","durum":"YEŞİL","kanıt":"[10] PING CANLI (ARP+IPv4+ICMP+RFC1071 checksum; SLIRP echo reply, payload KEMGU geri döndü); TAZE-CLONE gate.sh exit 0 (çekirdek+[6..10]+test_tumu+FIXPOINT); commit D-275 15df315","sıradaki":"STOP-FAZ-A"}`
- `{"görev":"STOP-FAZ-A","durum":"DURDU","not":"Mekanik kuyruk (b2-fs+virtio-net+net-arp+net-icmp) TAMAM — hepsi taze-clone gate + FIXPOINT yeşil. FAZ-A (kesme/zaman/görev/EL0 — preemptive+userspace) bu loop'a DAHİL DEĞİL → insan+stratejist kararı. Loop temiz durdu."}`
