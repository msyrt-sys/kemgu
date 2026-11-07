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
