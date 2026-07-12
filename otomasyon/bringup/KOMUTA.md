# KOMUTA — KEMGU REAL-OS BRING-UP LOOP (mekanik faz: B2-fs + net; FAZ-A HARİÇ)

Executor bu dosyaya her görev sonunda checkpoint JSON yazar (görev/durum/kanıt).
Loop harness (`bringup-loop.sh`) `🔴` veya `DUR:` görürse durur → insan/stratejist kararı.
FAZ-A (kesme/zaman/görev/EL0) bu loop'a DAHİL DEĞİL; `STOP-FAZ-A` görevinde loop temiz durur.

## Log
- `{"görev":"b2-fs","durum":"in-session-yürütüldü","kanıt":"[7] FS RW OK QEMU'da (format→dosya yaz+rastgele-pattern→oku→byte-eşleşme, inode indirection); vblk_kur re-init fix; commit D-272","gate":"taze-clone gate.sh bekliyor"}` (executor: in-session, loop machinery yok — claude/jq bu ortamda yok)
