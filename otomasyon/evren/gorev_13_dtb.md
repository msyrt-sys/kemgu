## İş 13 — `kem_dtb.kem`'i mimariden bağımsız yap (tek asm bloğu mimari dosyasına)

`kem_dtb.kem`'in TEK `satıriçi_asm` bloğu `kdtb_adres()` içinde (arm64 `adrp
x9, kdl_dtb_adres …`, `_start`'ın sakladığı DTB adresini okur). Geri kalan 40
işlev saf FDT ayrıştırıcı. D2b deseni: `kdtb_adres` mimari dosyasına taşınır.
x86'da FDT YOK (PVH `start_info` verir; keşif `kem_pvh.kem`'de) → x86
`kdtb_adres()` **0** döner; `kem_dtb`'nin "adres 0 → keşif yok" yolu kullanılır.
CAT/kapı listesi eklemelerini ORKESTRATÖR yapar — onlara blok YAZMA.

### Değişiklikler (5 madde)
1. **`runtime/kem_dtb.kem`**: `kdtb_adres` işlevini (üstündeki 2 satırlık yorum +
   gövde) SİL; yerine tek yorum: adres okuyucu artık
   `runtime/mimari/<arch>/kem_kesif.kem`'de (D2b; x86 → 0). ÇAPA: bağlamdaki
   `// _start'ın sakladığı DTB adresini oku …` satırından işlevin kapanış `}`'una
   kadar (birebir).
2. **YENİ `runtime/mimari/arm64/kem_kesif.kem`**: silinen işlevin AYNISI (yorum dahil,
   gövde birebir) + Türkçe başlık (amaç, x86 ikizi, D2b).
3. **YENİ `runtime/mimari/x86_64/kem_kesif.kem`**: `çıplak işlev kdtb_adres() -> tam64 { ver 0; }`
   + başlık yorumu: x86 PVH FDT vermez, platform keşfi `kem_pvh.kem`'dedir; 0 =
   "DTB yok" sözleşmesi `kem_dtb.kem`'in kendi yoluyla uyumlu.
4. **`test/ornekler/kem_os_x86.kem`**: `kem_panik("X86 PANIK YOLU OK");` ÖNCESİNE:
   `kdtb_adres() == 0` ise `X86 DTB YOK pvh_kesif` satırını bas, değilse
   `X86 DTB BEKLENMEDIK` + değer (`olarak tam32`). Başlık yorumuna ekle (kem_dtb
   x86 biriminde derleniyor = mimariden bağımsızlığın kanıtı).
   ÇAPA: `    kem_panik("X86 PANIK YOLU OK");`
5. **`Makefile` Kanıt 14**: Kanıt 13'ün son satırından SONRA:
   `X86 DTB YOK pvh_kesif` TAM satırı yoksa FAIL; ayrıca
   `llvm-objdump -d --no-show-raw-insn $(BUILD)/kem_os_x86.elf` çıktısında
   `<kdtb_adres>:` etiketi OLMALI (x86 birimine girdi). Hedef yorumuna Kanıt 14.
   ÇAPALAR: `		echo "FAIL: DMA tabani yedek sabitten (32768 KB) buyuk degil: $$v"; exit 1; fi`
   (iki TAB) ve `#          türetilen DMA tabanı (kem_platform.kem). Değer de yazdırılır.`

Blok yolları: `runtime/kem_dtb.kem`, `runtime/mimari/arm64/kem_kesif.kem`,
`runtime/mimari/x86_64/kem_kesif.kem`, `test/ornekler/kem_os_x86.kem`, `Makefile`.
