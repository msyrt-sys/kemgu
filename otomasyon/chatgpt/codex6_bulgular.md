# Dalga 6 bulguları

## Ölçülen QEMU davranışları

- Ham gerçek QEMU 8.2.2 ölçümü (`build/codex6/xhci_ham.py`): CAP=0x01000040, HCSPARAMS1=0x08001040, HCSPARAMS2=0xf, HCCPARAMS1=0x87001, DBOFF=0x2000, RTSOFF=0x1000. 32 bayt bağlam, 64 yuva, 8 port, 0 scratchpad. ERST boyu 4 → USBSTS=0x1001/HCE=1; reset+boy16 → USBSTS=1/HCE=0. Kaynakla tutarlı: QEMU v8.2.2 `hw/usb/hcd-xhci.c`, `xhci_er_reset` alt sınır 16.


## SAHTE MODEL DÜZELTİLDİ

- G3 olay halkası: gerçek QEMU asgari16 ister; eski BOT tüketicisi ve model4’te sarıyordu. İkisi16’ya taşındı. Eski testlerin beklenen I/O/ack sayıları değiştirilmedi; 22 olaylık eski çekirdek testi yeni sarımı da geçer.
- G3 Link cycle: eski model Link cycle=data cycle şartını koyuyordu. Gerçek QEMU tüketicisi TD sonundaki Link üzerinde bekleyebilir; yeni TD hazırlanırken Link, geçilecek eski cycle ile kalır. Küçük OUT/IN/EP0 halkası ve sahte model bu gerçek gözleme göre düzeltildi. Önce gerçek BOT istek99’da zaman aşımı verdi; Link onarımından sonra sense6/41 görünür oldu.
- G3 ilk TUR Unit Attention: gerçek QEMU 6/41/0 (power-on reset) döndürdü. Sürücü yalnız ilk TUR için bir kez REQUEST SENSE sonrası tekrar eder. Modele isteğe bağlı `ua_kalan` eklendi; yeni test1 UA→başarı/13ack, 2 UA→hata/2sense ile sınırı korur. Eski genel aygıt fikstürleri UA’sız ve aynı beklentilerle kalır.


## Derleyici bulguları ve kalan dolanmalar

### C6-001 — alanın değiştirilebilir referansı geçiciye bağlanıyor

```kem
yapı I { n: tam32; }
yapı D { i: I; }
işlev artir(x: &değişken I) { x.n=x.n+1; }
işlev main() -> tam32 {
    değişken d: D = D { i: I { n: 3 } };
    artir(&değişken d.i);
    yazdir_tam(d.i.n);
    eğer d.i.n == 4 { ver 0; } ver 1;
}
```
`build/{kemgu,codegen} --llvm build/codex6/alan_ref.kem`, clang + `build/kdl_runtime.o`: iki IR/link rc=0; iki çıktı `3`, iki koşum rc=1. `xhci_olay_al(&değişken s.olay,...)` imleci ilerletmediği için gerçek port olayı sürekli tekrar okunuyordu. Derleyici değiştirilmedi; halka kaydı yerel değişkene kopyalanıp yordamdan sonra alana geri yazılıyor.


- G0: Depolama matrisinin nv/ah alias’ları C001/T024 örtük modül bağı çakışması nedeniyle korunur. F005 yalnız IR adını düzeltir, ad çözümlemeyi değiştirmez.

### C6-002 — çeşit dizisi alanına boş dizi ataması

```kem
kullan drivers::ortak::gc::{ GcIstek };
yapı I { n: tam32; }
yapı S { xs: Dizi<GcIstek>; i: I; }
işlev ekle(s: &değişken S, i: GcIstek) { dizi_ekle(s.xs,i); }
işlev f(s: &değişken S) -> GcIstek { ekle(s,GcIstek::Oku32((s.i.n olarak tam64)+4)); ver GcIstek::Yaz32(s.i.n olarak tam64,0); }
işlev al(s: &değişken S) -> GcIstek { değişken i: GcIstek = dizi_al(s.xs,0); ver i; }
işlev main() -> tam32 {
    değişken s: S = S { xs: [], i: I { n: 64 } };
    s.xs=[];
    değişken q: GcIstek = f(&değişken s);
    değişken i: GcIstek = al(&değişken s);
    eşleş i { GcIstek::Oku32(a) => { yazdir_metin(tam_metin(a)); eğer a == 68 { ver 0; } ver 1; } _ => { ver 2; } }
}
```
`build/{kemgu,codegen} --llvm build/codex6/enum_kuyruk6.kem` + clang/runtime: iki IR/link rc=0. C çıktı68/rc0; self yanlış payload (ölçümde 272412716344864)/rc1. `s.xs=[]` kaldırılınca ikisi68/rc0. Dolanma: `değişken bos: Dizi<GcIstek> = []; s.kuyruk=bos;`. Derleyici değişmedi.

- Gerçek QEMU aktarım halkasında TD sonundaki Link TRB, ardından hazır TD yokken henüz tüketilmemiş olabilir (`xhci_ring_chain_length` yalnız bakar). Link cycle’ını bir sonraki TD ile erken değiştirmek ikinci kontrol aktarımını durdurdu. Enumerasyon EP0 için mevcut 64 TRB üretici halkası kullanıldı; küçük BOT halkasında aynı davranış G3’te ayrıca ölçülüp düzeltildi.

### C6-003 — doğrudan dizi alanı üzerinde için gövdesi atlanıyor

```kem
yapı S { xs: Dizi<tam32>; }
işlev main() -> tam32 {
    değişken s: S = S { xs: [3,7,11] };
    değişken toplam: tam32 = 0;
    için a: s.xs { toplam=toplam+a; }
    yazdir_tam(toplam);
    eğer toplam == 21 { ver 0; } ver 1;
}
```
İki derleyici IR/link rc=0; beklenen21 yerine çıktı0/run rc=1. Dolanma: dizi alanını tipli yerel değişkene alıp onun üzerinde `için`, veya indeksli `iken`. Dil kartının `için` örneği yerel dizi kullanır; alan biçimi sessizce kabul edildiği için kaydedildi. Derleyiciye dokunulmadı.

## G2 gerçek aygıt kimlikleri

- C ve self ölçümü aynı: port1/slot1 hız4, VID 18164 (0x46f4), PID1, sınıf8/alt6/protokol80; uç129 bulk IN ve uç2 bulk OUT, paket1024. Port6/slot2 hız3, VID1575 (0x0627), PID1, sınıf3/alt1/protokol1; uç129 interrupt IN, paket8, bInterval7. `tests/qemu/xhci_qtest.kem` bunları ve QEMU’nun çıkış bağlamındaki Configured/Running, endpoint type/MPS alanlarını doğrular.
- Yerel `/usr/share/hwdata/usb.ids` yok. QEMU v8.2.2 `hw/usb/dev-storage.c` ve `hw/usb/dev-hid.c` ile karşılaştırıldı; Python sabit doğrulayıcı SHA256 ve Unicode/qcode/USB istek sabitlerini kaydeder. Kaynaklar: https://github.com/qemu/qemu/blob/v8.2.2/hw/usb/dev-storage.c ve https://github.com/qemu/qemu/blob/v8.2.2/hw/usb/dev-hid.c .

## G4 — gerçek HID ölçümü

QMP qcode sözlüğü QEMU v8.2.2 qapi/ui.json’dan doğrulandı. HLT BIOS ve çalışan VM ile her QMP tuş olayından sonra rapor alındı; 19 adet 8 baytlık rapor C/self birebir eşit. 16 bayt IN isteği her raporda CC13/artık8 verdi. Kodlar Python ord ile `[65,287,97,231,32,304,351,10]`; iki gerçek koşum 1.65 sn. SET_PROTOCOL(boot) + SET_IDLE(0) denetlendi. QMP ilk selamını beklerken qtest wait=on QEMU başlangıcını bekletti; yalnız yeni HID dalında qtest wait=off kullanıldı (VM yine -S olmadan çalışır).

### SAHTE MODEL DÜZELTİLDİ — HID

Önceki elle üretilmiş basış örneklerine ek olarak gerçek QEMU’nun modifier, basma ve bırakma raporları `hid_qemu_test.kem` içine alındı. Özellikle İ için sol Shift raporu ve ayrı bırakma raporları ölçüldü; mevcut eski test beklentileri değiştirilmedi. Kısa IN tamamlanması çekirdek sahte olay testinde de CC13/artık8 ile korunuyor.

## G5 — fare gerçek QEMU ölçümü

VID1575/PID1, interrupt IN129, paket4, bInterval7; QEMU dev-hid.c desc_iface_mouse2 ile aynı. QMP rel(+17,+9)+left down → ham `[1,17,9,0]`, rel(-5,-7)+left up → `[0,251,249,0]`; C/self aynı, dal1.18 sn. HID Y aşağı-pozitif, mevcut FareOlayi Y yukarı-pozitif olduğu için sonuçlar `(17,-9,true)` ve `(-5,7,false)`. Sürücüde yön düzeltmesi gerekmedi.

### SAHTE MODEL DÜZELTİLDİ — fare

Gerçek iki QMP raporu ayrı `fare_qemu_test.kem` fikstürüne alındı; önceki salt işaretli bayt testleri korunur. QEMU’nun boot protokolünde de 4 bayt döndürdüğü ölçüldü; çözücünün mevcut 3/4 bayt desteği doğru.

## Ek derleyici bulgusu

### C6-004 — doğrudan yapı dizisi sabiti dönüşü

```kem
yapı R { t: tam32; a: tam64; v: tam64; n: tam32; h: tam64; y: tam64; b: Dizi<dtam8>; }
işlev f() -> Dizi<R> { ver [R { t: 1, a: 0, v: 0, n: 0, h: 0, y: 16777280, b: [] }]; }
işlev main() -> tam32 {
    değişken xs: Dizi<R> = f(); değişken r: R = dizi_al(xs,0);
    yazdir_tam(r.t); yazdir_metin(tam_metin(r.v)); yazdir_tam(r.n); yazdir_metin(tam_metin(r.y));
    eğer r.t == 1 ve r.v == 0 ve r.n == 0 ve r.y == 16777280 { ver 0; } ver 1;
}
```
`build/{kemgu,codegen} --llvm build/codex6/kayit_dizi_donus.kem`, clang/runtime: IR/link rc=0. C `1,0,0,16777280`/rc0; self `1,0,-1022437784,263721319102200`/rc1 (bozuk alanlar çalıştırmaya göre değişiyor). Gerçek QEMU’nun C/self 187 istek izi birebir aynıyken yeniden oynatma fikstürünün ilk kaydında saptandı. Derleyici değişmedi; dönüşten önce `değişken kayitlar: Dizi<XhciIz> = [...]` kullanılır.

## SAHTE MODEL DÜZELTİLDİ — tam enumerasyon

G1’in kısmi başlangıç modeli artık gerçek QEMU’dan kaydedilen 187 istek/yanıt iziyle tamamlanır. C/self izlerinin kanonik SHA256’sı aynı: `d1f99eba7e7eb7971bf583cf35697e58bb3cb2142e4692ec26f966860f9e59f5`. Model istek türü, adres, değer, boy ve bütün yazılan baytları etkileyen modüler özeti karşılaştırır; gerçek kayıtlı MMIO/DMA yanıtlarını döndürür. Sürücünün TRB veya USB ayrıştırıcısını paylaşmaz. İki derleyicinin izleri ayrı tekrar edilir; hatalı tamamlanma kodu5 ve 15 baytlık kesilmiş olay hata vermelidir. Yenileme komutları fikstür başında; pozitif gerçek qtest geçmeden kayıt üretilmez.

Bu, genel bir xHCI emülatörü değil, ölçülen yürütmenin regresyon modelidir. İstek sırası değişirse önce gerçek QEMU’da tekrar ölçülür. G1 ERST boyu ve G2 endpoint add maskesi sabotajları artık tam sahte enumerasyon testinde de yakalanır. BOT, HID klavye ve fare farkları yukarıdaki ayrı modellerde korunur.

## Kalan doğrulanmamışlar ve kapsam sınırı

- Fiziksel xHCI donanımı, DMA cache/barrier/coherency ve interrupt glue bu sans-IO işin dışında. QEMU 8.2.2/q35 üzerinde ölçüldü.
- QEMU örneği scratchpad=0 ve 32 bayt bağlam bildiriyor. Sürücünün sıfır dışı scratchpad/64 bayt bağlam yolları gerçek aygıtta ölçülmedi; fiziksel doğrulama gerektirir.
- Kök port, yapılandırma1/alt ayar0, aygıt başına en çok bir IN/bir OUT ucu; hub/isochronous ve aynı yönde çoklu endpoint desteklenmez, hata döner. İlk16 yuva ve en çok32 scratchpad sayfası sınırı var.
- BOT’a geçiş, denetleyici olay halkasını özel BOT halkasına devreder. Bu API tek sahipli ve geri dönüşsüzdür: ardından aynı çekirdek durumu üzerinden HID başlatılmaz. Depo ve HID qtest’leri ayrı QEMU örnekleridir; eşzamanlı karma I/O iddiası yok.
- FAT32 gerçek testi mkfs.fat ile hazırlanmış düz64MiB imaj üzerinde; GPT, gerçek USB sökme/takma, güç kesintisi ve büyük disk bu dalgada ölçülmedi. Dosyanın tamamı ve iki FAT/FSInfo bağımsız doğrulanır.
- G0’da yalnız F001–F005 ile kapanan dolanmalar temizlendi. C001 ad bağı alias’ları ve burada belgelenen C6-001..004 dolanmaları durur; derleyici/runtime değiştirilmedi.
