2026-11-20T07:02:33+03:00

# Dalga 13 · x86_64 kullanıcı programları

| İş | Kanıt |
| --- | --- |
| A | ABI sürüm kâhini eski çekirdekte kırmızı. C ve self-host PVH açılışlarında yeşil. Sürüm 2→3 sabotajı gerçek QEMU'da 1/1 kırmızı. ChatGPT köprüsü 141/141. |
| B | C ve self-host ile bağımsız `merhaba.elf`, her biri 13.040 bayt. `llvm-nm -u` boş. Host ELF kapısı W+X program başlığını ve eklenen `kdl_syscall_isle` sembolünü ayrı koşularda reddetti. ELF'in gerçek açılış kanıtı C1 kâhininde. |
| C1 | D-760 tabanında `calistir` kırmızıydı. UEFI USB diskinden ELF, W^X, NX, koruma sayfası ve Türkçe argv. QMP ile iki fiziksel alan. C ve self-host 50 çalıştırmada 1.032 boş çerçeve, süreç heap artışı 0 bayt. |
| D | Eksik dosya çağrısı `kedi`yi QEMU'da kırdı. İki derleyicide Türkçe FAT32 okuması, 264.000 baytlık `say`, `yazdene`, bağımsız FAT32 okuyucu ve `fsck.fat -n` yeşil. |
| E | Saat çağrısı olmadan `tus` QEMU'da kırmızı. USB HID ve PS/2 IRQ ile iki derleyicide sekiz saniye tuşsuz bekleme, beş basma/bırakma olayı ve zaman çağrıları. |
| F | `merhaba`, `kedi`, `say`, `yazdene`, `tus`, beş `hata_*` ve `sinir` ayrı KEMGU ELF'leri. Çıkış, süreç istisnası ve kabuğun devamı seri ve ekran kâhininde denetleniyor. |
