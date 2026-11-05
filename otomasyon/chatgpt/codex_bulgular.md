# Codex bulguları

Özet (E3): C001–C011 toplam 11 derleyici/araç zinciri uyumsuzluğu için küçük yeniden üretim ve mevcut dille dolanma kaydedildi. Derleyici değiştirilmedi. C009 host dosya API adı çakışması nedeniyle imaj APIlerinin güvenli adları kullanılır. S001 FAT etiket metadata tutarsızlığı dış fsck.fat kahiniyle düzeltilmiştir. E2 toplam 9000 bozuk girdi ve 26 sınır girdisinde yeni panik bulmadı.

## C001 — modülle aynı adlı işlevi seçili içe aktarma

```kemgu
kullan drivers::disk::crc32::{ crc32 };
işlev main() -> tam32 {
    değişken b: Dizi<dtam8> = dizi_olustur(0);
    eğer crc32(b, 0, 0) == 0 { ver 0; }
    ver 1;
}
```
Komutlar: `build/kemgu --check tests/drivers/probe/crc_ad.kem` → `hata[T024]: secili import: yerel tanimla ad cakismasi`; `build/codegen --check tests/drivers/probe/crc_ad.kem` → `OK`.
Dolanma: `kullan drivers::disk::crc32;` ve `crc32::crc32(...)`. Bu kart dışı biçim ayrı 6 satırlık probe ile iki derleyicide derlendi ve çalıştı: `bash otomasyon/chatgpt/dogrula.sh tests/drivers/probe/crc_ad.kem` → `CHATGPT_DOGRULA OK`. Genel API adı ve dosya yolu korundu. Derleyici değiştirilmedi.

## C002 — dizi_olustur yapı eleman genişliğini C tarafında kaybediyor

```kemgu
yapı Nokta { x: tam32; y: tam64; }
işlev uret() -> Dizi<Nokta> {
    değişken d: Dizi<Nokta> = dizi_olustur(0);
    dizi_ekle(d, Nokta { x: 12, y: 2048 });
    ver d;
}
işlev main() -> tam32 {
    değişken d: Dizi<Nokta> = uret();
    değişken n: Nokta = dizi_al(d, 0);
    yazdir_tam(n.x); yazdir_tam(n.y olarak tam32);
    eğer n.x == 12 ve n.y == 2048 { ver 0; }
    ver 1;
}
```
C komutu: `bash otomasyon/chatgpt/dogrula.sh tests/drivers/probe/yapi_dizi.kem` → `HATA[C çalıştır] exit=1`, stdout `12`, `-774316008` (bozuk ikinci alan değişken). IR: `kdl_dizi_olustur(..., i32 4)`.
Self komutu: `build/codegen --llvm tests/drivers/probe/yapi_dizi.kem > build/codex_probe.ll && clang -Wno-override-module -x ir build/codex_probe.ll -x none build/kdl_runtime.o -o build/codex_probe && build/codex_probe` → `12`, `2048`, exit 0.
Dolanma: `değişken d: Dizi<Nokta> = [];` aynı probe iki derleyicide yeşil. Kartın tip belirtilmiş dizi literalinin boş biçimi ölçüldü. Yapı dizilerinde bu biçim kullanılıyor; derleyici değiştirilmedi.

## C003 — sonuç eşleşmesinde yapı dizisi eleman tipi

```kemgu
yapı Nokta { x: tam32; y: tam64; }
işlev uret() -> sonuç<Dizi<Nokta>, metin> {
    değişken d: Dizi<Nokta> = [];
    dizi_ekle(d, Nokta { x: 12, y: 2048 }); ver tamam(d);
}
işlev main() -> tam32 {
    eşleş uret() {
        hata(e) => { ver 1; }
        tamam(v) => {
            değişken n: Nokta = dizi_al(v, 0);
            eğer n.y == 2048 { ver 0; } ver 1;
        }
    }
}
```
Komut şablonu (kemgu ve codegen): `build/<derleyici> --llvm tests/drivers/probe/sonuc_dizi.kem > build/codex_probe.ll && clang -Wno-override-module -x ir build/codex_probe.ll -x none build/kdl_runtime.o -o build/codex_probe && build/codex_probe`.
kemgu: stdout: ; exit=0
codegen: build/codex_sonuc_dizi_codegen.ll:192:16: error: '%12' defined with type 'i32' but expected '%Nokta = type { i32, i64 }'
  192 |   store %Nokta %12, ptr %13
      |                ^
1 error generated.

Dolanma: `tamam(v)` dalında önce `değişken xs: Dizi<Nokta> = v;`, sonra `dizi_al(xs, 0)`. Ayrı probe ve A3 testi iki derleyicide doğrulanır; derleyici değiştirilmedi.

## C004 — iç içe dizi_al ile dtam8 kopyasında işaret genişletme

```kemgu
kullan drivers::ortak::bayt::{ bayt_sifir, yaz_u8, oku_u8 };
işlev main() -> tam32 {
    değişken a: Dizi<dtam8> = bayt_sifir(1);
    değişken b: Dizi<dtam8> = [];
    yaz_u8(a, 0, 203);
    dizi_ekle(b, dizi_al(a, 0));
    yazdir_tam(oku_u8(b, 0));
    eğer oku_u8(b, 0) == 203 { ver 0; } ver 1;
}
```
Komut şablonu (kemgu ve codegen): `build/<derleyici> --llvm tests/drivers/probe/bayt_kopya.kem > build/codex_probe.ll && clang -Wno-override-module -x ir build/codex_probe.ll -x none build/kdl_runtime.o -o build/codex_probe && build/codex_probe`.
kemgu: stdout: 203; exit=0
codegen: stdout: -53; exit=1
Dolanma: `dizi_ekle(b, oku_u8(a, 0) olarak dtam8);`. Ayrı probe ve A3 testi iki derleyicide doğrulanır; derleyici değiştirilmedi.

## C005 — içe aktarılmış yapıdan sonraki tam64 sabit argüman genişliği

```kemgu
kullan drivers::fs::fat32_bpb::{ Fat32Bpb, kume_sektoru };
işlev main() -> tam32 {
    değişken b: Fat32Bpb = Fat32Bpb { sektor_bayt: 512, kume_sektor: 8,
        ayrilmis: 32, fat_sayisi: 2, toplam_sektor: 1048576, fat_sektor: 1024,
        kok_kume: 2, fsinfo: 1, ilk_veri_sektoru: 2080, kume_sayisi: 130812 };
    eğer kume_sektoru(b, 2) == 2080 { ver 0; } ver 1;
}
```
Komut: `build/<kemgu|codegen> --llvm tests/drivers/probe/yapi_arguman.kem > build/codex_arg.ll && clang -Wno-override-module -x ir build/codex_arg.ll -x none build/kdl_runtime.o -o build/codex_arg && build/codex_arg`.
kemgu: exit=0; ['%27 = call i64 @fat32_bpb.kume_sektoru(ptr %2, %Fat32Bpb %25, i64 %26)']
codegen: exit=1; ['%17 = call i64 @fat32_bpb.kume_sektoru(ptr %1, %Fat32Bpb %16, i32 2)']
İlk yerel yapı probe'unda fark oluşmadı; yukarıdaki içe aktarılan API örneğinde self i32, C i64 argüman üretir. A4 tam64 karşılaştırması self tarafında başarısızdı (alt 32 bit doğruydu). Dolanma: `2 olarak tam64`; beklentiler değişmedi.

## S001 — FAT32 boş kök ve birim etiketi
Görev B5 etiket parametresi ile boş kök ister. Microsoft FAT Specification (2005), §3.3 BS_VolLab ve §6 ATTR_VOLUME_ID uyarınca BPB etiketi kökteki 11 baytlık birim etiketiyle aynı olmalıdır. Kaynak: https://www.scs.stanford.edu/~zyedidia/docs/_other/fat.pdf (s.12,26). Bu nedenle etiket verildiğinde kökte tek metadata girdisi bulunur; dizin_coz etiketi atlar ve kullanıcı dosyaları bakımından kök boştur. Etiketsiz durumda kökün ilk baytı sıfırdır. B5, §3.5 gereği 65525 altı küme sayısını reddeder; A4 ham BPB ayrıştırıcısı mevcut kabul testlerini korur.


## C006 — İçe aktarılan yapı alanında boş dizi tipi

```kemgu
kullan drivers::fs::fat32_bpb::{ Fat32Bpb };
kullan drivers::fs::fat32_dizin::{ DizinGirdisi };
kullan drivers::fs::fat32_imaj::{ DizinResmi, FatKayit };
işlev main() -> tam32 {
    değişken r: DizinResmi = DizinResmi { baytlar: [], konumlar: [], zincir: [] };
    ver dizi_boyut(r.baytlar);
}
```
Komutlar: `build/<kemgu|codegen> --check tests/drivers/probe/bos_alan.kem` ve `build/<kemgu|codegen> --llvm tests/drivers/probe/bos_alan.kem > build/codex_bulgu.ll`, ardından `clang -Wno-override-module -x ir build/codex_bulgu.ll -x none build/kdl_runtime.o -o build/codex_bulgu`.
kemgu: OK: tests/drivers/probe/bos_alan.kem — tip kontrolu basarili.

link: 0 
codegen: T014	5	54
T014	5	68
T014	5	80

link: 1 /usr/bin/ld: /lib/aarch64-linux-gnu/Scrt1.o: in function `_start':
(.text+0x1c): undefined reference to `main'
/usr/bin/ld: (.text+0x20): undefined reference to `main'
clang: error: linker command failed with exit code 1 (use -v to see invocation)

Dolanma: boş diziyi önce açık tipli yerelde oluştur, yapı alanına bu yereli ata. Yerel tek alanlı yapı probe bu farkı üretmedi; içe aktarılan yapı gerekir.

## C007 — Geçişli içe aktarmada görünmeyen yapı tipleri

```kemgu
kullan drivers::fs::fat32_imaj::{ imaj_bpb };
işlev main() -> tam32 {
    değişken b: Dizi<dtam8> = [];
    eşleş imaj_bpb(b) { tamam(v) => { ver 1; } hata(e) => { ver 0; } }
}
```
Komutlar: `build/<kemgu|codegen> --check tests/drivers/probe/tasima_tipi.kem` ve `build/<kemgu|codegen> --llvm tests/drivers/probe/tasima_tipi.kem > build/codex_bulgu.ll`, ardından `clang -Wno-override-module -x ir build/codex_bulgu.ll -x none build/kdl_runtime.o -o build/codex_bulgu`.
kemgu: HATA: tests/drivers/probe/tasima_tipi.kem — parser 0, yukleme 0, tip kontrol 10, realtime 0 (toplam 10 hata).
hata[T011]: bilinmeyen tip
   --> tests/drivers/probe/tasima_tipi.kem:19:52
   |
19 | 
   |                                                    ^

hata[T011]: bilinmeyen tip
   --> tests/drivers/probe/tasima_tipi.kem:34:49
   |
34 | 
   |                                                 ^

hata[T011]: bilinmeyen tip
   --> tests/drivers/probe/tasima_tipi.kem:39:49
   |
39 | 
   |                                                 ^

hata[T011]: bilinmeyen tip
   --> tests/drivers/probe/tasima_tipi.kem:47:32
   |
47 | 
   |                                ^

hata[T011]: bilinmeyen tip
   --> tests/drivers/probe/tasima_tipi.kem:48:28
   |
48 | 
   |                            ^

hata[T011]: bilinmeyen tip
   --> tests/drivers/probe/tasima_tipi.kem:57:48
   |
57 | 
   |                   
link: 1 /usr/bin/ld: /lib/aarch64-linux-gnu/Scrt1.o: in function `_start':
(.text+0x1c): undefined reference to `main'
/usr/bin/ld: (.text+0x20): undefined reference to `main'
clang: error: linker command failed with exit code 1 (use -v to see invocation)

codegen: OK

link: 0 
Dolanma: dış çağıran, bağımlı yapı tiplerini açıkça içe aktarır.

## C008 — Eşleş sonrası sonuç dönüşü

```kemgu
yapı N { x: tam32; }
işlev f(r: sonuç<N, metin>) -> sonuç<N, metin> {
    eşleş r { tamam(v) => { ver tamam(v); } hata(e) => { ver hata(e); } }
}
işlev main() -> tam32 {
    değişken n: N = N { x: 1 };
    değişken r: sonuç<N, metin> = tamam(n);
    eşleş f(r) { tamam(v) => { ver 0; } hata(e) => { ver 1; } }
}
```
Komutlar: `build/<kemgu|codegen> --check tests/drivers/probe/sonuc_donus.kem` ve `build/<kemgu|codegen> --llvm tests/drivers/probe/sonuc_donus.kem > build/codex_bulgu.ll`, ardından `clang -Wno-override-module -x ir build/codex_bulgu.ll -x none build/kdl_runtime.o -o build/codex_bulgu`.
kemgu: OK: tests/drivers/probe/sonuc_donus.kem — tip kontrolu basarili.

link: 1 build/codex_bulgu.ll:202:21: error: integer constant must have integer type
  202 |   ret {i8, %N, ptr} 0
      |                     ^
1 error generated.

codegen: OK

link: 1 build/codex_bulgu.ll:183:21: error: integer constant must have integer type
  183 |   ret {i8, %N, ptr} 0
      |                     ^
1 error generated.

Dolanma: eşleşten sonra açık hata dönüşü; erişilemeyen default IR yerine geçerli dönüş.

## C009 — Sürücü API adlarının host yerleşikleriyle çakışması

```kemgu
kullan drivers::fs::fat32_bpb::{ Fat32Bpb };
kullan drivers::fs::fat32_dizin::{ DizinGirdisi };
kullan drivers::fs::fat32_imaj::{ DizinResmi, FatKayit };
kullan drivers::fs::fat32_yaz;
işlev main() -> tam32 {
    değişken b: Dizi<dtam8> = [];
    eşleş fat32_yaz::dosya_sil(b, "/X") {
        tamam(v) => { ver 1; } hata(e) => { ver 0; }
    }
}
```
Komutlar: `build/<kemgu|codegen> --check tests/drivers/probe/host_ad.kem` ve `build/<kemgu|codegen> --llvm tests/drivers/probe/host_ad.kem > build/codex_bulgu.ll`, ardından `clang -Wno-override-module -x ir build/codex_bulgu.ll -x none build/kdl_runtime.o -o build/codex_bulgu`.
kemgu: OK: tests/drivers/probe/host_ad.kem — tip kontrolu basarili.

  %8 = call {i8, i1, ptr} @fat32_yaz.dosya_sil(ptr %3, ptr %6, ptr %7)
Host bağlamalı ikili çalıştırılmadı.
codegen: OK

  %7 = call i32 @kdl_dosya_sil(ptr %5, ptr %6)
Host bağlamalı ikili çalıştırılmadı.
Dolanma: orijinal API korunuyor; imaja_dosya_yaz, imajda_dizin_olustur, imajdan_dosya_sil ek adları kullanılıyor. Nitelikli çağrı tek başına codegen host yönlendirmesini önlemiyor.

## C010 — İçe aktarılan çeşit kurucusunda self T016

Yalın yeniden üretim (5 satır):
```kemgu
kullan drivers::ps2::klavye_tr::{ TusOlayi };
işlev main() -> tam32 {
    değişken o: TusOlayi = TusOlayi::Karakter(65);
    eşleş o { TusOlayi::Karakter(k) => { ver k - 65; } _ => { ver 1; } }
}
```
`build/kemgu --check tests/drivers/probe/enum_aktar.kem`: OK; C LLVM/link/koşum exit=0. `build/codegen --check ...`: `T016 3 38`.
Dolanma: satir modülünde kurucuyu çağıran satir_karakter/satir_tus sarmalayıcıları; olay yine aynı satir_olay yolundan geçer. Düzeltilmiş probe ve bütün satır testleri iki derleyicide doğrulanır. C tarafında TusOlayi içe aktarımı satir modülünden önce olmalıdır (ters sırada T011/T001).

## C011 — tam32 alt sınırının yapı alanına atanması
```kemgu
yapı Sayac { n: tam32; }
işlev main() -> tam32 {
    değişken d: Sayac = Sayac { n: 0 };
    d.n = -2147483648;
    eğer d.n == -2147483648 { ver 0; } ver 1;
}
```
`bash otomasyon/chatgpt/dogrula.sh tests/drivers/probe/tam32_alt.kem`: C check/LLVM/link/koşum geçti; self check geçti, LLVM link `i64 ... expected i32` hatası. Dolanma: atamada açık `olarak tam32`; düzeltilmiş probe iki derleyicide yeşil.
