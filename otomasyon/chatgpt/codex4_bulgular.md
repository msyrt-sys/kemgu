# Dalga 4 bulguları

## Derleyici bulguları

## QEMU’da doğrulanacak

Gerçek donanım doğrulaması bu saf mantık dalgasının dışındadır. Kaynağı bulunamayan alanlar aşağıya ayrı ayrı kaydedilecektir.

- A0: `virtio_blk_init_test.kem` seçili `blk_cfg_adres` ithalatı eklendi. C/self --check ve çalıştırma temiz. `surucu_diff` muafiyeti artık kaldırılabilir; harness yasağı gereği değiştirilmedi.

### F001 — seçili ithal tipin yapı alanında bulunamaması

Örnek (3 satır):
```kem
kullan drivers::acpi::tablo::{ Rsdp };
yapı Kapsayici { rsdp: seçimlik<Rsdp>; }
işlev main() -> tam32 { ver 0; }
```
Komutlar: `build/kemgu --check build/codex4/alan_ithali.kem` → T011, rc=1;
`build/codegen --check build/codex4/alan_ithali.kem` → OK, rc=0.
Dolanma: Multiboot2 yalnız sonuç taşıyan yerel AcilisBellek/AcilisRsdp kayıtlarına kopyalar;
bellek haritası ve RSDP doğrulaması mevcut modüllerde kalır. Derleyici değiştirilmedi.

- A3 VARSAYIM: komut satırı boşluk/sekme ile ayrılır, yinelenen anahtar reddedilir;
  `kok` tam 8 onaltılık hane, isteğe bağlı ortada tire kabul eder. Paletli ekran türü 0
  açık desteklenmiyor hatası verir; RGB ve EGA metin türleri kabul edilir. GNU metni ve
  örnek başlığı palet sayısının genişliğinde ayrıştığı için bu alan QEMU’da doğrulanacak.

### F002 — seçimlik kurucusunun doğrudan yapı alanına atanması

```kem
yapı S { x: seçimlik<tam64>; }
işlev main() -> tam32 {
    değişken s: S = S { x: hiç };
    s.x = değer(7 olarak tam64);
    eşleş s.x { hiç => { ver 1; } değer(v) => { eğer v == 7 { ver 0; } ver 2; } }
}
```
`build/{kemgu,codegen} --llvm build/codex4/secimlik_alan.kem` her ikisi rc=0;
çıktıları `clang -x ir ... -x none build/kdl_runtime.o` ile bağlayınca C:
`use of undefined value @değer`, self: `%10 defined with type i32 but expected {i8,i64}`,
her iki link rc=1. Dolanma: `değişken x: seçimlik<T> = değer(v); alan=x;`.

- A3 kaynak sınırları: toplam bilgi bloğu ≤1 MiB, etiket sayısı ≤4096, metin ≤4096 bayt;
  daha büyük girdiler açık hata döndürür. Fikstür bunları aşmak için küçültülmedi.

### F003 — yapı alanındaki dtam8 dizisinden kopyalamada işaret genişlemesi

```kem
kullan drivers::ortak::bayt::{ oku_u8 };
yapı S { b: Dizi<dtam8>; }
işlev main() -> tam32 {
    değişken a: Dizi<dtam8> = [128 olarak dtam8];
    değişken b: Dizi<dtam8> = [];
    değişken s: S = S { b: a };
    dizi_ekle(b,dizi_al(s.b,0));
    yazdir_tam(oku_u8(b,0));
    eğer oku_u8(b,0) == 128 { ver 0; } ver 1;
}
```
`build/{kemgu,codegen} --llvm build/codex4/dizi_alan.kem`, ardından
`clang -x ir çıktı.ll -x none build/kdl_runtime.o -o çıktı` ve çalıştırma:
C stdout `128`, rc=0; self stdout `-128`, rc=1. İkisi de derlenip bağlanır.
Dolanma: alan üzerindeki doğrudan `dizi_al` yerine tipi açık `oku_u8(...)
olarak dtam8`; yazımlarda mevcut `yaz_u8`. NVMe çekirdeği ve sahte modelde
F003 yorumuyla kullanıldı. Derleyiciye dokunulmadı.

### QEMU’da doğrulanacak — AHCI

- PxCMD ST/FRE kapatma, CR/FR bekleme ve başlatma sırası; SSTS DET/IPM, SATA/ATAPI
  imzaları, PxIS.TFES ve PxTFD.ERR/BSY/DRQ. Yerel kurulu başlıkta AHCI yazmaç tanımı
  yoktu; sabitler Linux v6.17 `drivers/ata/ahci.h` üzerinden Python ile doğrulandı.
- H2D FIS tür 0x27/C biti, 48 bit LBA ve count baytları; komut başlığı CFL/W/PRDTL,
  CTBA hizası; PRDT DBC/I ve 4 MiB parçalama. Bunlar sahte modelden bağımsız gerçek
  QEMU AHCI ile ayrıca doğrulanmalıdır; sahte e2e donanım doğrulaması sayılmıyor.

- C2 açıklık: görevde DBC için geçen “çift sayı”, aktarımın bayt sayısıdır. DBC=bayt−1 olduğundan 512 bayt için 511 yazılır; PRDT testi bunu bağımsız Python hesabıyla korur.

### QEMU’da doğrulanacak — xHCI

- HCCPARAMS1 CSZ ve 32/64 bayt bağlam yerleşimi; slot/endpoint alanları ve dequeue
  işaretçisi; Link TRB TC/Cycle ve olay halkası sarma; doorbell endpoint ID, ERDP/EHB.
- PORTSC RW1C değişim bitleri ile reset yazımları, BAR ofsetleri ve gerçek aygıt hızları.
  Linux v6.17 xhci.h/xhci-caps.h/xhci-port.h/xhci-ext-caps.h indirilen sabit kaynak
  olarak Python ile karşılaştırıldı; gerçek QEMU doğrulaması ayrıca gereklidir.
- D2: görev MaxPorts alanını 8 bit verir; Linux kök hub sınırı 127'dir. Alan 8 bit
  okunur, 128–255 açık destek sınırı hatasıyla reddedilir; 7 bite sessiz kırpılmaz.
