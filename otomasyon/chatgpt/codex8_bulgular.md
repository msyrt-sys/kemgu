# Dalga 8 bulguları

## Tasarım ve ölçümler

- Başlangıç: D-745 kaydındaki 2/30 yırtık okuma önceki ölçümdür; bu dalganın başlangıç NVMe kapısı 20/20 yeşil.

## C8-001 — sonuç<mantıksal,metin> dönüş ABI farkı

A0 ortak yürütücü `GcIstek` modülünü birleşik x86 birimine getirince
`calistir_baremetal_diff` gerçek bir fark yakaladı: `gc_yanit_denetle` ve
`bayt.{be_yaz,yaz_be16,yaz_be32,yaz_be64}` C dönüşü `{i8,i1,ptr}`, self
`{i8,i32,ptr}`. İşlev/asm/volatile/noredzone sayıları aynı, dönüş tipi farklı.
Derleyici ve kapı değiştirilmedi. Yeni yürütücü gerçek NVMe 20/20, qtest 6/6,
köprü 122/122 ve check kapısından geçti. Bu ABI engeli son raporda açık kalacak.

Minimal örnek (iki --llvm çıktısında define satırını karşılaştır):
```kem
işlev f() -> sonuç<mantıksal,metin> { ver tamam(doğru); }
işlev main() -> tam32 { ver 0; }
```

A0 sabotajı: BellekOku adresi +8 kaydırıldı; 20 açılışta 20 kırmızı,
QEMU rc3 ve `nvme zaman asimi`; geri yüklemeden sonra 20/20 yeşil.

## SAHTE MODEL DÜZELTİLDİ — A2

NVMe ilk CQ görüntüsünde yeni faz+eski CID verir; 2 girdilik halka ve
16 tamamlanma ile sarımlar sınanır. Kalıcı CID, durum, faz ve kısa yanıt reddi korunur.
xHCI ve BOT ilk görüntüde eski adres+güncel DW3 üretir; yeniden okuma sonrası
ilerler. Gerçek QEMU enumerasyon izi C/self 205 istek, aynı SHA256:
`e78acb959fb473b7205a9d33d2e2a07ff8ab211c2a7aaeca87c59a3080ea94b9`.
Kayıtlı modelde arada aygıt işlemi olmayan aynı olayın ikinci okuması aynı DMA
belleğini görür; ilk görüntüye enjekte edilen kalıcı bozulma ikinci okumada
kaybolmaz. Eski olumsuz test beklentileri değiştirilmedi.
Sabotajda her üç yeniden okuma kaldırıldı; üç dogrula rc1. Gerçek aygıtta
yırtık yarışın yeniden üretildiği iddia edilmiyor: koruma belirlenimci modelde,
normal bare-metal NVMe 20/20.

## C8-002 — tam64 yapı alanına negatif literal

Yeni modelde `yirtik_adres: -1` self IR'ında `sub i32 0,1` ardından
`store i64 %...` üretti; clang reddetti. C derledi. Dolanma: DMA adreslerinin
pozitif olmasına dayanarak beklemeyen kayıt için 0 kullanıldı. Derleyici değişmedi.

## C bare-metal USB

Gerçek CPU, SeaBIOS sonrası xHCI reset ve BOT devir çalıştı. 64 MiB FAT32 diskten yalnız BPB, kök dizin, FAT sektörü ve dosyanın üç sektörü okundu. Python yazıcı/okuyucu KEMGU kullanmaz; fsck temiz, 7 küme kullanılıyor. Bare-metal runtime metin_esit sembolünü sağlamadığı için çekirdekte mevcut metin_bayt/uzunluk ilkelleriyle karşılaştırma yapıldı; C runtime değişmedi.

## DUR — D kabuk v0 / 16 MiB heap yetersizliği

Kullanıcının §3 kuralı uygulandı: heap yetmediği ölçülünce geliştirme durduruldu.
Linker, boot, derleyici ve C runtime değiştirilmedi.

Gerçek NVMe + aynı xHCI üzerinde USB klavye, QMP tuşları: `ls /`,
`cat /merhaba.txt`, `cat /yok` doğru seri çıktı verdi. Sonraki aktarımda
`xhci trb turu` / `xhci kisa olay`. DMA'daki gerçek olay hâlâ geçerliydi;
yürütücünün döndürdüğü dizinin boyu bir koşumda 0 idi.

Heap sayaçları: bump=18059696, son=18112512, kalan=52816 bayt.
Hata yolundan sonra 65560 bayt malloc denemesi serbest bloktan başarılı oldu;
bu ilk ölçüm OOM kanıtı sayılmadı. Ardından `bm_bellek_oku` girişindeki geçici
çıplak probe, `malloc(65536+24)` sonucunu doğrudan denetledi; aynı komut
noktasında NULL ve `HEAP_64K_YOK` ölçüldü. Probe geri alındı.

```
KEMGU-OS kabuk v0
DISK NVMe
KABUK_HAZIR
dalga8.txt
belgeler/
merhaba.txt
KABUK_HAZIR
Merhaba Dünya — ğüşıöç
KABUK_HAZIR
HATA bulunamadi
KABUK_HAZIR
HEAP_64K_YOK
```

Dolayısıyla D TAMAMLANMADI: NVMe/AHCI/USB için 5'er başarılı açılış yok.
`calistir_bm_x86_kabuk` kapısı bilerek kırmızı bırakıldı; komut sayısı veya
beklenen çıktı azaltılmadı. Kabuk varsayılanı 6 komut × 5 açılış × 3 disk.
E yapılmadı. BOT/HID ortak halka devir API'si bu kabuğa eklendi, fakat USB
kabuk konfigürasyonu henüz ölçülmedi; üretim için hazır olduğu iddia edilmiyor.

## D tasarım sınırları

GPT yalnız ilk 34 sektör ile mevcut çözücüden; FAT32 512 baytlık sektörleri
isteğe bağlı getirir. Yeni okuyucu en çok 256 sektör önbelleği ve 64 KiB
dosya/dizin sınırı uygular, eksik sektörü `gerekli` alanında bildirir. Eski
bellek-içi API değişmedi. Bütün disk hiçbir KEMGU dizisine alınmadı.
Python FAT32/GPT yazıcısı ve okuyucusu yalnız stdlib kullanır; LFN, iki FAT,
FSInfo ve GPT CRC'leri bağımsız üretilir. QMP qcode adları çalışan QEMU'nun
`query-qmp-schema` enum içeriğinden doğrulanır (enum adı anonimdir).

## C8-003 — sonuç dalındaki bayt kopyası

Sektör okuyucusu C'de 1300 baytlık Python deseniyle eşleşti; self'te 18.
baytta 129 yerine -127 görüldü. Sonuç dalına bağlanan `Dizi<dtam8>` üzerinden
`dizi_ekle(out,dizi_al(b,j))` kopyası açık `(oku_u8(b,j)&255) olarak dtam8`
ile yapıldı. Beklenen veri Python yazıcısından değişmeden kaldı. Derleyiciye
dokunulmadı. İlk yeni test taslağındaki not.txt uzunluk yazımı Python'ın
`len(b'Bagimsiz FAT32 yazicisi\n') == 24` hesabıyla düzeltildi; mevcut test
beklentisi değiştirilmedi.

## Küçük derleyici tekrar üretimleri

Komut: `build/{kemgu,codegen} --llvm <örnek.kem> > <örnek.ll>`; ardından
`clang -Wno-override-module -x ir <örnek.ll> -x none build/kdl_runtime.o -o <örnek>`.

C8-002 (iki LLVM rc0; C link0/çıktı -1; self link1, i32 değer i64 store):
```kem
yapı R { a: tam64; }
işlev main() -> tam32 { değişken r: R = R { a: -1 }; yazdir_metin(tam_metin(r.a)); ver 0; }
```

C8-003 (iki LLVM/link rc0; C çıktı129, self çıktı-127):
```kem
işlev al() -> sonuç<Dizi<dtam8>,metin> { değişken b: Dizi<dtam8> = [129 olarak dtam8]; ver tamam(b); }
işlev main() -> tam32 {
    değişken out: Dizi<dtam8> = [];
    eşleş al() { hata(e) => { ver 1; } tamam(b) => { dizi_ekle(out,dizi_al(b,0)); } }
    yazdir_tam(dizi_al(out,0) olarak tam32); ver 0;
}
```

## Son tam takım

`make test_tumu` rc2 (kabukta durdu). Ardından **seri** `make -k test_tumu`
rc2 ile bütün bağımsız hedefleri tamamladı; iki rc de PIPESTATUS[0] ile alındı.
Kırmızı hedefler yalnız `calistir_bm_x86_kabuk` ve `calistir_baremetal_diff`.
FIXPOINT ✓: 98132 satır. Check kapısı 340/353,13 muaf,0 RED. Köprü kart+126,
qtest altı dal C/self ayrı QEMU, bare-metal NVMe/AHCI/USB her biri20/20.
ASan132 PASS/0 FAIL/35 SKIP, ASan matrisi12/12. Yeni dört sürücü test dosyası
C+self yeşil; sektör veri kopyası sıfırlama sabotajı C koşumunda rc2 verdi
(dogrula rc1), geri yükleme sonrası tam köprüde geçti.

| Atlama sınıfı | Olay |
|---|---:|
| Codegen semantik, üç koşum: her birinde2 yabancı mimari+2 C link istisnası |12|
| Geniş codegen |8|
| Yapısal IR |2|
| Sürücü paritesi |1|
| Eski VirtIO host testleri |5|
| Bölge operandı |4|
| ASan E2E (yeni üç bare-metal örnek dahil) |35|
| **Korpus atlama toplamı** |**67**|

Ayrıca QEMU üst bellek senaryosu RAM≤1GiB nedeniyle1 atlama. Muafiyetler
24 yapısal+1 sürücü+18 geniş check+13 tip kontrol=56; ASan'ın4 bilinen ve
2 sızıntı istisnası ayrıca sayılır. Geniş host codegen günlüğündeki4
Segmentation fault satırı mevcut bare-metal örneklerin eşleşen host
çıkışlarıdır; gerçek sürücü başarısı yerine sayılmadı. Lean/lake yok: yalnız
32 dosyada0 sorry/admit metinsel denetimi, gerçek Lean doğrulaması yok.

Günlükler: `build/codex8/test-tumu.log`, `test-tumu-tam.log`,
`a0-sabotaj.log`, `a1-sabotaj-*.log`, `a2-*-sabotaj.log`,
`b-sabotaj.log`, `c-sabotaj.log`, `d-fs-sabotaj.log`, `d-heap-probe.log`.
