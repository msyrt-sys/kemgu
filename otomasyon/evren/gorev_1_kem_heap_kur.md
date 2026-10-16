## İş 5 — x86 boot'unda `kem_heap_kur` çağrısı + minimum x86 kanıt çekirdeği

### Sorun (ölçüldü)
`boot/start_aarch64.S` (adım 2c) `main`'den önce `kem_heap_kur(__heap_start,
__heap_end)` çağırır. `boot/start_x86_64.S` ÇAĞIRMAZ. Saf-`.kem` allocator
(`kem_heap.kem`) x86'da bağlandığında `kem_son = 0` kalır → her `malloc` null →
dizi işlevleri sessizce 0 döner. Ölçüm: aşağıdaki test programı bugün x86 QEMU'da
`55` yerine `0` basıyor, hata/panik YOK.

### İstenen değişiklikler (yalnız bu üç dosya)

1. **`boot/start_x86_64.S`** — `long_entry` içinde `call kdl_idt_kur` ile
   `call main` ARASINA `kem_heap_kur` çağrısı ekle. SysV: 1. argüman `%rdi`,
   2. argüman `%rsi`; değerler linker sembolleri `__heap_start` / `__heap_end`
   (bkz. `linker/bare-metal-x86_64.ld`). Yorumu aarch64'teki 2c yorumunun
   x86 karşılığı olarak yaz (neden gerekli, C-malloc'lu kernel'lerde
   `kdl_bare_heap.c`'deki ZAYIF no-op'un devreye girdiği — mevcut x86
   kernel'leri bozulmaz).

2. **`test/ornekler/kem_os_x86.kem`** (YENİ) — x86 kanıt çekirdeğinin `main`'i.
   `kem_heap.kem` ile CAT edilecek (tek birim). Normal (çıplak OLMAYAN)
   `işlev main() -> tam32` olmalı: prologu bölgeyi `kdl_global_bolge_al` →
   `kdl_bolge_olustur` → `malloc` (saf-.kem) üzerinden kurar — test tam bu
   zinciri ölçer. Gövde:
   - `Dizi<tam32>` oluştur, `dizi_ekle` ile 1..10 ekle, `için` ile topla.
   - Toplam 55 ise TEK satırda `X86 HEAP DIZI OK toplam=55` bas; değilse
     `X86 HEAP DIZI HATA toplam=<değer>` bas. Değeri SABİT yazma — gerçek
     toplamı `yazdir_tam` ile bas (D-264 dersi: marker değil DEĞER denetlenir).
   - Yazdırma yerleşikleri: `yazdir_metin(m)`, `yazdir_tam(t)`,
     `yazdir_satir()` (argümansız, satır sonu). Bunlar bu çekirdekte C
     `kdl_runtime_yazdir_bare.c` + 16550 UART üzerinden gider (İş 2 sonra
     `.kem` UART'a çevirecek — şimdilik dokunma).
   - Başına dosyanın amacını anlatan Türkçe başlık yorumu.

3. **`Makefile`** — `calistir_uart_merhaba_x86_bare_metal` hedefinin hemen
   SONRASINA yeni hedef `calistir_kem_os_x86` ekle ve `.PHONY`'nin İLK satırına
   (satır 130) adını ekle. Tarif (tab ile girintili), sırayla:
   - Bağımlılıklar: `$(BUILD)/kemgu$(EXE)` ve şu nesneler (ölçüldü, bağlama
     yeten en küçük küme; `bm_x86_heap.o` ve `bm_x86_bolge.o` BİLEREK YOK —
     allocator/bölge saf-.kem'den gelmeli):
     `$(BUILD)/bm_x86_start.o $(BUILD)/bm_x86_kesme.o $(BUILD)/bm_x86_zaman.o
      $(BUILD)/bm_x86_gorev.o $(BUILD)/bm_x86_uart.o $(BUILD)/bm_x86_yazdir.o
      $(BUILD)/bm_x86_panik.o`
   - `cat runtime/kem_heap.kem test/ornekler/kem_os_x86.kem > $(BUILD)/kem_os_x86_comb.kem`
   - `./$(BUILD)/kemgu$(EXE) --llvm --mimari x86_64 $(BUILD)/kem_os_x86_comb.kem > $(BUILD)/kem_os_x86_ham.ll`
     (AYRI satır — boruya bağlama; D-609: boru kemgu'nun çıkış kodunu maskeler)
   - `awk -f test/strip_defined_declares.awk $(BUILD)/kem_os_x86_ham.ll > $(BUILD)/kem_os_x86.ll`
   - `$(BM_X86) -O2 -Wno-override-module -x ir $(BUILD)/kem_os_x86.ll -c -o $(BUILD)/kem_os_x86.o`
   - `ld.lld -m elf_x86_64 -T linker/bare-metal-x86_64.ld -o $(BUILD)/kem_os_x86.elf $(BUILD)/kem_os_x86.o <nesneler>`
   - **Kanıt 1 (sembol kaynağı):** `llvm-nm $(BUILD)/kem_os_x86.o` içinde
     ` T malloc` ve ` T kem_heap_kur` OLMALI (saf-.kem tanımlıyor); yoksa
     `FAIL` + `exit 1`.
   - **Kanıt 2 (gerçek komut):** `llvm-objdump -d --no-show-raw-insn
     $(BUILD)/kem_os_x86.elf` çıktısında `kem_heap_kur`'a giden `call`
     satırını BUL ve O SATIRI OLDUĞU GİBİ yazdır (`echo` ile sabit metin değil,
     objdump'ın kendi satırı). Bulunamazsa `FAIL` + `exit 1`.
   - **Kanıt 3 (davranış):** QEMU: `timeout 8 qemu-system-x86_64 -kernel
     $(BUILD)/kem_os_x86.elf -display none -serial file:$(BUILD)/kem_os_x86.out
     2>/dev/null || true`; seri çıktıyı `cat` ile bas; `X86 HEAP DIZI OK
     toplam=55` YOKSA `FAIL` + `exit 1`. `qemu-system-x86_64` yoksa
     GÜRÜLTÜLÜ hata ver (`exit 1`) — sessiz atlama YASAK (D-486).
   - Diğer x86 hedeflerinin (`calistir_uart_merhaba_x86_bare_metal`) üslubunu
     izle (`@echo` başlıkları, Türkçe yorumlar).

### Kabul
- `start_x86_64.S` değişikliği yalnız ekleme; mevcut satırlar değişmez.
- Mevcut x86 kernel'leri (C malloc'lu) bozulmamalı — zayıf no-op sayesinde.

### Çapalar (ESKI metinleri — bunları AYNEN kullan, tahmin etme)
- Makefile hedef ekleme: bağlamdaki `otomasyon/evren/baglam/makefile_x86_kesit.txt`
  kesitinde `calistir_uart_merhaba_x86_bare_metal` hedefinin SON satırı
  `	else echo "QEMU yok — x86 hello atlandi."; fi` (başta TAB) ve hemen ardından
  boş satır + `# === Bare-Metal Dizi Kernel x86_64 (C1-x86)` yorumu gelir. ESKI
  olarak şu İKİ satırı kullan (birebir, TAB dahil):
  `	else echo "QEMU yok — x86 hello atlandi."; fi`
  `` (boş satır)
  `# === Bare-Metal Dizi Kernel x86_64 (C1-x86) — AYNI kernel, PVH boot ===`
  ve YENI'de aynı satırları koruyup yeni hedefi aralarına koy.
- `.PHONY`: ESKI = ` calistir_qemu_cekirdek kemgu_self` (depoda TEK kez geçer),
  YENI = ` calistir_qemu_cekirdek calistir_kem_os_x86 kemgu_self`.
- `start_x86_64.S`: ESKI = şu iki satır (birebir, girinti 4 boşluk):
  `    call    kdl_idt_kur            /* C3a: IDT kur (isr0..31 → istisna işleyici) */`
  `    call    main                   /* KEMGU kernel main() */`
