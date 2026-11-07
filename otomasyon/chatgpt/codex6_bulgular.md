# Dalga 6 bulguları

## Ölçülen QEMU davranışları

- Ham gerçek QEMU 8.2.2 ölçümü (`build/codex6/xhci_ham.py`): CAP=0x01000040, HCSPARAMS1=0x08001040, HCSPARAMS2=0xf, HCCPARAMS1=0x87001, DBOFF=0x2000, RTSOFF=0x1000. 32 bayt bağlam, 64 yuva, 8 port, 0 scratchpad. ERST boyu 4 → USBSTS=0x1001/HCE=1; reset+boy16 → USBSTS=1/HCE=0. Kaynakla tutarlı: QEMU v8.2.2 `hw/usb/hcd-xhci.c`, `xhci_er_reset` alt sınır 16.


## SAHTE MODEL DÜZELTİLDİ

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

## Kalan doğrulanmamışlar

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

- Gerçek QEMU aktarım halkasında TD sonundaki Link TRB, ardından hazır TD yokken henüz tüketilmemiş olabilir (`xhci_ring_chain_length` yalnız bakar). Link cycle’ını bir sonraki TD ile erken değiştirmek ikinci kontrol aktarımını durdurdu. Enumerasyon EP0 için mevcut 64 TRB üretici halkası kullanıldı; küçük BOT halkasında aynı davranış G3/G6’da ayrıca ölçülecek.

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
