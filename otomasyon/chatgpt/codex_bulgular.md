# Codex bulguları

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
