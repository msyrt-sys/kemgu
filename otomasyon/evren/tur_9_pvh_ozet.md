# Tur 9 — pvh — özet

- Durum: **DOGRULAMA_HATA**
- finish: `stop` · blok: `10`
- Uygulanan bloklar (kuru):
  - DÜZENLE boot/start_x86_64.S: -3 +7 satır
  - DÜZENLE boot/start_x86_64.S: -2 +11 satır
  - YENİ   runtime/mimari/x86_64/kem_pvh.kem (48 satır)
  - DÜZENLE test/ornekler/kem_os_x86.kem: -2 +6 satır
  - DÜZENLE test/ornekler/kem_os_x86.kem: -1 +12 satır
  - DÜZENLE Makefile: -1 +4 satır
  - DÜZENLE Makefile: -1 +1 satır
  - DÜZENLE Makefile: -1 +7 satır
  - DÜZENLE test/check_kapisi.sh: -1 +1 satır
  - DÜZENLE test/check_kapisi.sh: -1 +1 satır

## Hata (dogrulama: `make calistir_kem_os_x86`)
```
  102d1e:      	mfence
  Kanit 5: QEMU davranis (X86 PANIK YOLU OK)...
  Kanit 6: objdump kem_uart_bayt (outb + inb, .kem UART)...
  100ca0:      	inb	%dx, %al
  100cb8:      	outb	%al, %dx
  Kanit 7: yerlesim (__heap_end < DMA tabani 0x02000000)...
    __heap_end = 0x0000000001111000 (17895424)
  Kanit 8: UC MMIO bayragi (objdump'da 0xc000009b)...
  103015:      	movl	$0xc000009b, %eax       # imm = 0xC000009B
  Kanit 9: QEMU davranis (DMA RAM BLK/NET + UC MMIO)...
  Kanit 10: QEMU davranis (microvm disk RW + net cihaz)...
  Kanit 11: QEMU davranis (kem_minifs x86 dosya sistemi)...
  Kanit 12: QEMU davranis (PVH start_info RAM) + pvh_entry %ebx...
FAIL: PVH OK sonrasi pozitif sayi satiri yok
make: *** [Makefile:2004: calistir_kem_os_x86] Error 1
```

## git diff --stat
```
Makefile                     | 11 ++++++++++-
 boot/start_x86_64.S          | 13 +++++++++++++
 test/check_kapisi.sh         |  4 ++--
 test/ornekler/kem_os_x86.kem | 15 +++++++++++++++
 4 files changed, 40 insertions(+), 3 deletions(-)
```
