# KOMUTA — KEMGU REAL-OS BRING-UP LOOP (mekanik faz: B2-fs + net; FAZ-A HARİÇ)

Executor bu dosyaya her görev sonunda checkpoint JSON yazar (görev/durum/kanıt).
Loop harness (`bringup-loop.sh`) `🔴` veya `DUR:` görürse durur → insan/stratejist kararı.
FAZ-A (kesme/zaman/görev/EL0) bu loop'a DAHİL DEĞİL; `STOP-FAZ-A` görevinde loop temiz durur.

## Log
<!-- checkpoint'ler buraya eklenir -->
