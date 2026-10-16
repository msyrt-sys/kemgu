## İş 4 — virtio-mmio x86 farkı: microvm tarama tabanı + işlevsel disk/ağ kanıtı

### Sorun (ölçüldü)
`vblk_bul` / `vnet_bul` cihazı `167772160 + i*512` (0x0a000000), `i < 32`
yuvada arar — QEMU aarch64 `virt`'e özgü. x86'da virtio-mmio YALNIZ
`-M microvm`'de var: taban `0xFEB00000` (4273995776), adım 0x200 (512),
**24 yuva** (ölçüldü). `pc` makinesinde hiç yok. QEMU varsayılanı legacy
(`force-legacy=true`, ölçüldü) ama sürücü v2 bekliyor → x86 QEMU satırına
`-global virtio-mmio.force-legacy=false` ŞART. Mevcut x86 kanıt çekirdeği
`microvm`'de sorunsuz boot ediyor (ölçüldü; tüm işaretler geliyor).
`0xFEB00000`, D-673'ün UC penceresi içinde (eşli).

### İstenen değişiklikler (yalnız bunlar — 5 madde)

1. **Platform dosyaları** — her birinde `vnet_dma` satırının ALTINA iki sabit:
   - `runtime/mimari/arm64/kem_platform.kem`:
     `sabit VM_TARAMA_TABAN: tam64 = 167772160;   // 0x0a000000 (QEMU virt)`
     `sabit VM_YUVA_SAYISI: tam32 = 32;`
   - `runtime/mimari/x86_64/kem_platform.kem`:
     `sabit VM_TARAMA_TABAN: tam64 = 4273995776;  // 0xFEB00000 (QEMU microvm)`
     `sabit VM_YUVA_SAYISI: tam32 = 24;`
   - Her dosyada sabitlerin üstüne kısa Türkçe yorum (adım 0x200 her iki
     makinede aynı; x86'da yalnız microvm; pc'de virtio-mmio yok).
2. **`runtime/kem_virtio_blk.kem` `vblk_bul`** ve **`runtime/kem_virtio_net.kem`
   `vnet_bul`**: döngüdeki `i < 32` → `i < VM_YUVA_SAYISI`, `167772160` →
   `VM_TARAMA_TABAN`. İşlevlerin üstündeki yorumdaki
   "32 slot × 0x200 @ 0x0a000000" ifadesini (yalnız blk'de var) platform
   dosyasına atıf yapacak şekilde güncelle.
3. **`test/ornekler/kem_os_x86.kem`** — `kem_panik("X86 PANIK YOLU OK");`
   satırından ÖNCE (DMA/MMIO testlerinden SONRA) iki test:
   - **Disk:** `değişken vb: tam64 = vblk_bul();` — 0 ise `X86 DISK YOK` bas.
     Değilse `vblk_kur(vb)` 0 değilse `X86 DISK KUR HATA` + kodu
     (`yazdir_tam`). Değilse: i=0..511 için `vblk_data_yaz(i,
     (((i % 251) + 5) olarak dtam8))`, `vblk_yaz(vb, 7)`; sonra i=0..511
     `vblk_data_yaz(i, 0 olarak dtam8)` ile tamponu SIFIRLA (geri okunan
     verinin tampondan değil DİSKTEN geldiği kanıtlansın); `vblk_oku(vb, 7)`;
     i=0..511 karşılaştır. Hepsi eşleşirse `X86 DISK RW OK` + ayrı satırda
     okunan baytların toplamı (`yazdir_tam`; değer denetimi, D-264). Yoksa
     `X86 DISK RW HATA` + ilk uyuşmayan indeks. `vblk_yaz`/`vblk_oku` dönüş
     kodu 0 değilse de HATA + kod.
   - **Ağ:** `değişken nb: tam64 = vnet_bul();` 0 ise `X86 NET YOK`;
     `vnet_kur(nb)` 0 değilse `X86 NET KUR HATA` + kod; değilse
     `X86 NET DEV OK mac_w0=` + ayrı satırda `vnet_mac_w0(nb)` değeri.
   - `vblk_bul`/`vblk_kur`/`vblk_yaz`/`vblk_oku`/`vnet_bul`/`vnet_kur`/
     `vnet_mac_w0` çıplak işlevler; normal `main`'den çağrılabilir.
   - Başlık yorumuna iki testi ekle.
4. **`Makefile` `calistir_kem_os_x86` QEMU satırı** — mevcut üç satırı
   (çapa aşağıda) şununla değiştir: önce disk imajı
   `dd if=/dev/zero of=$(BUILD)/kem_os_x86_disk.img bs=512 count=64
   2>/dev/null;` sonra `timeout 8 qemu-system-x86_64 -M microvm
   -display none -global virtio-mmio.force-legacy=false -drive
   file=$(BUILD)/kem_os_x86_disk.img,format=raw,if=none,id=d0 -device
   virtio-blk-device,drive=d0 -netdev user,id=n0 -device
   virtio-net-device,netdev=n0 -kernel $(BUILD)/kem_os_x86.elf -serial
   file:$(BUILD)/kem_os_x86.out 2>/dev/null || true;` — satır sonları ve `\`
   devamları mevcut üslupta (TAB girinti). Üstüne yorum: neden microvm (pc'de
   virtio-mmio yok), neden force-legacy=false (sürücü v2; QEMU varsayılanı
   legacy — aarch64 hedefi de aynı bayrağı kullanır).
5. **`Makefile` Kanıt 10** — Kanıt 9'un son satırından (çapa) SONRA:
   `X86 DISK RW OK` ve `X86 NET DEV OK mac_w0=` TAM satırları (`grep -qxF`);
   ayrıca disk OK satırının HEMEN ardındaki satır yalnız rakamlardan oluşan
   sıfırdan büyük bir sayı olmalı (`grep -A1 -xF ... | tail -1 | grep -qE
   '^[1-9][0-9]*$'`). Yoksa FAIL. Hedef başlık yorumuna Kanıt 10'u ekle
   (çapa: `#          okunabiliyor).`).

### Çapalar (ESKI — AYNEN; her biri dosyasında TEK kez geçer)
- arm platform: `küresel değişken vnet_dma: tam64 = 1182793728;   // 0x46800000`
- x86 platform: `küresel değişken vnet_dma: tam64 = 34603008;    // 0x02100000`
- kem_virtio_blk.kem VE kem_virtio_net.kem (her birinde bir kez, 2 satır):
  `    iken i < 32 {`
  `        değişken base: tam64 = 167772160 + (i olarak tam64) * 512;`
- kem_virtio_blk.kem yorum: `// Cihaz tara (DeviceID=2). Taban adresi döner (0 = yok). 32 slot × 0x200 @ 0x0a000000.`
- kem_os_x86.kem: `    kem_panik("X86 PANIK YOLU OK");`
- Makefile QEMU (3 satır, TAB girintili, sonlarında ` \`):
  `	@rm -f $(BUILD)/kem_os_x86.out; \`
  `	timeout 8 qemu-system-x86_64 -kernel $(BUILD)/kem_os_x86.elf -display none \`
  `		-serial file:$(BUILD)/kem_os_x86.out 2>/dev/null || true; \`
  (YENI'de `@rm -f ...; \` satırı KORUNSUN, `dd` ondan sonra gelsin; son
  satır yine ` \` ile bitsin — ardından gelen `echo "--- QEMU COM1 cikti
  ---"` satırları aynı kabuk komutunun parçası.)
- Makefile Kanıt 9 son satırı: `		echo "FAIL: 'X86 MMIO UC OK lapic_surum=' satiri yok"; exit 1; fi`
- Makefile yorum: `#          okunabiliyor).`

Blokları YALNIZ gerçek dosyalara yaz (`Makefile`, `runtime/...`, `test/...`) —
bağlamdaki `otomasyon/evren/baglam/*.txt` dosyaları YALNIZCA kesittir, hedef
DEĞİL.

### Yasak
`src/*.c`, `selfhost/*`, `runtime/kem_gorev.kem`, `runtime/kem_mmu.kem`,
`runtime/kem_zaman.kem`, `test/ornekler/kem_os.kem`, `boot/*.S`.
