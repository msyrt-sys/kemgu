2026-11-20T07:02:33+03:00

# Dalga 13 · x86_64 kullanıcı programları

| İş | Kanıt |
| --- | --- |
| A | ABI sürüm kâhini eski çekirdekte kırmızı. C ve self-host PVH açılışlarında yeşil. Sürüm 2→3 sabotajı gerçek QEMU'da 1/1 kırmızı. ChatGPT köprüsü 141/141. |
| B | C ve self-host ile bağımsız `merhaba.elf`, her biri 13.040 bayt. `llvm-nm -u` boş. Host ELF kapısı W+X program başlığını ve eklenen `kdl_syscall_isle` sembolünü ayrı koşularda reddetti. ELF'in gerçek açılış kanıtı C1 kâhininde. |
