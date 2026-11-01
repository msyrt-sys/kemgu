# KEMGU dil kartı (dış model için)

KEMGU, Türkçe anahtar kelimeli bir sistem programlama dilidir. Bu kart, yalnız
**derleyiciden geçtiği ÖLÇÜLMÜŞ** şekilleri içerir. Burada olmayan bir sözdizimini
**uydurma**; emin değilsen kullanma ve yanıtının sonunda "VARSAYIMLAR" altında yaz.
Kartın sonundaki örnek program `otomasyon/chatgpt/dogrula.sh --kart` ile her
değişiklikte iki derleyicide de derlenip çalıştırılır.

## Temel
- Dosya uzantısı `.kem`, UTF-8. Her deyim `;` ile biter. Yorum `//` ve `/* */`.
- `işlev ad(p: tip, q: tip) -> dönüş_tipi { ... }` · dönüş: `ver ifade;`
- Dönüş tipi yoksa `-> ...` yazılmaz (boş döner).
- Yerel: `değişken x: tam32 = 0;` (tip annotasyonu HER ZAMAN yaz) · atama `x = x + 1;`
- Üst düzey sabit: `sabit AD: tam32 = 512;`
- Bir işlev, dosyada kendisinden SONRA tanımlanan işlevi çağırabilir (ileri
  referans çalışır). Ama **prototip/ileri bildirim YOK**: gövdesiz `işlev f();`
  yazma.
- Sayılar: `42`, `0x10`, `1_000`. Büyük değer tam64'e sığar.

## Tipler
- İşaretli: `tam8 tam16 tam32 tam64` · işaretsiz: `dtam8 dtam16 dtam32 dtam64`
- `mantıksal` (`doğru`/`yanlış`), `metin`, `kesirli32`, `kesirli64`
- `Dizi<T>` (heap, sınır kontrollü — taşma programı PANİKLE durdurur)
- `seçimlik<T>`: `değer(v)` / `hiç` · `sonuç<T, H>`: `tamam(v)` / `hata(e)`
- `yapı Ad { a: tam32; b: Dizi<dtam8>; }` — alanlar **`;`** ile ayrılır
- `çeşit Ad { A, B(tam32), C(tam32, tam32) }` — varyantlar **`,`** ile ayrılır
- Değiştirilebilir referans parametre: `işlev f(d: &değişken D)`, çağrı `f(&değişken d)`

## Dönüşüm — ÖRTÜK GENİŞLETME YOK
- Farklı tamsayı tipleri ASLA kendiliğinden karışmaz. `tam8` → `tam32` bile
  `olarak` ister: `(b olarak tam32)`. Unutmak T001 hatası verir.
- Bayt okuma kalıbı: `dizi_al(b, i) olarak tam32`
- Dizi elemanı yazarken tipi eşle: `dizi_ekle(b, 52 olarak dtam8);`
- Daraltma: `tam32`→`dtam8`/`tam16` serbest (kırpar). **`tam64`/`dtam64` →
  8/16 bit DOĞRUDAN YASAK** (E004 hatası) — iki adımda yaz:
  `((v & 255) olarak tam32) olarak dtam8`. `tam64` → `tam32` serbest.

## Operatörler
- Aritmetik `+ - * / %` · karşılaştırma `== != < > <= >=`
- Bit: `& | ^ << >>` (işaretsiz tipte `>>` mantıksal kaydırmadır)
- Mantık **yalnız kelimeyle**: `ve`, `veya`, `değil`
  — `&&`, `||`, `!x` YOK (hata verir).
- Sıfıra bölme ve tip genişliğinden büyük kaydırma programı PANİKLE durdurur.

## Akış
- `eğer koşul { ... } değilse eğer k2 { ... } değilse { ... }` (parantez gereksiz)
- `iken koşul { ... }` · `için x: dizi { ... }`
- `eşleş` dallarının HER BİRİ blok olmalı: `Desen => { ...; }`
  - çeşit: `Olay::Tus(k) => { ver k; }` · tamsayı: `1 => { ... }` · joker: `_ => { ... }`
  - sonuç: `tamam(v) => {...}` `hata(e) => {...}` · seçimlik: `değer(v) => {...}` `hiç => {...}`
  - Kapsayıcılık denetlenir (eksik varyant = M001 hatası).

## Dizi ve metin yerleşikleri (ölçüldü)
- `dizi_olustur(0)` boş dizi · `[1, 2, 3]` literal (annotasyonlu değişkene)
- `dizi_ekle(d, v)` · `dizi_al(d, i)` · `dizi_yaz(d, i, v)` · `dizi_boyut(d)`
- `metin_uzunluk(s)` (BAYT sayısı) · `metin_bayt(s, i)` (dtam8 değil tam8 → `olarak tam32`)
- `metin_esit(a, b)` · `metin_birlestir(a, b)` · `metin_kes(s, başlangıç, uzunluk)`
- `kod_metin(kod_noktası) -> metin` (UTF-8 kodlar) · `kod_gecerli(k) -> mantıksal`
- Çıktı: `yazdir_metin(s)`, `yazdir_tam(n)`
- **Dizgi literalinde kaçış YOK**: `"a\nb"` dört karakterdir. Satır sonu gerekirse
  `kod_metin(10)` kullan.

## Ad olarak KULLANILAMAYAN anahtar kelimeler (46)
boş, bölge, delege, değer, değil, değilse, değişken, doğru, dışa, eğer, eşleş,
genel, geri_al, gerçekzamanlı, görev, güvensiz, hata, hiç, iken, imha, için, işlev,
kanal, kendin, kullan, küresel, modül, olarak, sabit, sabitsüre, satıriçi_asm,
seçimlik, sonuç, tamam, tekkez, uygula, ve, vektör, ver, veya, yanlış, yapı, yetki,
çeşit, çıplak, özellik
(örn. parametre adı `deger`, `uygulayici` yaz; `değer`, `uygula` YASAK.)

## Modüller (sürücü dosyaları)
- Dışa açılan üye `genel` ile işaretlenir: `genel işlev`, `genel sabit`,
  `genel yapı`, `genel çeşit`. İşaretsiz üye dosyaya özeldir (T041 hatası).
- İçe aktarma (yol depo köküne göreli, `/` yerine `::`):
  ```
  kullan drivers::ps2::klavye_tr::{ tus_coz, KlavyeDurumu };
  ```
  İçe aktarılan adlar NİTELİKSİZ kullanılır (`tus_coz(...)`).

## Bu görevlerde YASAK
- `güvensiz`, `çıplak`, `satıriçi_asm`, `yetki`, `mmio_*`, `görev`, `kanal`,
  ham işaretçi `*T`. Görevler **saf mantıktır**: bayt dizisi girer, yapı/çeşit çıkar.
- `src/`, `selfhost/`, `runtime/`, `Makefile` dosyalarına dokunmak.

## Test dosyası biçimi
- Her test `işlev test_ad() -> mantıksal` ve `main` başarısızları sayar;
  **exit 0 = hepsi geçti**:
  ```
  işlev main() -> tam32 {
      değişken h: tam32 = 0;
      eğer değil test_a() { yazdir_metin("BASARISIZ test_a"); h = h + 1; }
      ver h;
  }
  ```
- Her test hem **olumlu** (geçerli girdi → doğru sonuç) hem **olumsuz** (bozuk
  girdi → `hata(...)`/`hiç`) şekil içermeli. Yalnız olumlu test, "her şeyi
  kabul et" türü bir hatayı yakalayamaz.
- Test verisini elle bayt bayt kur (`dizi_ekle` ile ya da literalle); sabit
  beklenen değerleri spesifikasyondan hesapla ve yorumda kaynağını yaz.

## Örnek program (DOĞRULANMIŞ)
<!-- ORNEK -->
```kemgu
// Dil kartı örneği: derleyiciden geçtiği ÖLÇÜLMÜŞTÜR.
sabit SEKTOR: tam32 = 512;

yapı Nokta { x: tam32; y: tam32; }

çeşit Olay {
    Bos,
    Tus(tam32),
    Hareket(tam32, tam32)
}

işlev le16(b: Dizi<dtam8>, i: tam32) -> tam32 {
    değişken lo: tam32 = dizi_al(b, i) olarak tam32;
    değişken hi: tam32 = dizi_al(b, i + 1) olarak tam32;
    ver lo | (hi << 8);
}

işlev bol(a: tam32, b: tam32) -> sonuç<tam32, metin> {
    eğer b == 0 {
        ver hata("sifira bolme");
    }
    ver tamam(a / b);
}

işlev ilk_sifir(xs: Dizi<tam32>) -> seçimlik<tam32> {
    değişken i: tam32 = 0;
    iken i < dizi_boyut(xs) {
        eğer dizi_al(xs, i) == 0 {
            ver değer(i);
        }
        i = i + 1;
    }
    ver hiç;
}

işlev olay_puan(o: Olay) -> tam32 {
    eşleş o {
        Olay::Bos => { ver 0; }
        Olay::Tus(k) => { ver k; }
        Olay::Hareket(dx, dy) => { ver dx + dy; }
    }
}

işlev topla(xs: Dizi<tam32>) -> tam32 {
    değişken t: tam32 = 0;
    için x: xs {
        t = t + x;
    }
    ver t;
}

işlev test_le16() -> mantıksal {
    değişken b: Dizi<dtam8> = dizi_olustur(0);
    dizi_ekle(b, 52 olarak dtam8);
    dizi_ekle(b, 18 olarak dtam8);
    ver le16(b, 0) == 4660;
}

işlev test_bol() -> mantıksal {
    değişken r: sonuç<tam32, metin> = bol(84, 2);
    eşleş r {
        tamam(v) => { ver v == 42; }
        hata(m) => { ver yanlış; }
    }
}

işlev test_bol_sifir() -> mantıksal {
    eşleş bol(1, 0) {
        tamam(v) => { ver yanlış; }
        hata(m) => { ver metin_esit(m, "sifira bolme"); }
    }
}

işlev test_secimlik() -> mantıksal {
    değişken xs: Dizi<tam32> = [3, 0, 5];
    eşleş ilk_sifir(xs) {
        değer(i) => { ver i == 1; }
        hiç => { ver yanlış; }
    }
}

işlev test_cesit() -> mantıksal {
    ver olay_puan(Olay::Hareket(40, 2)) == 42 ve olay_puan(Olay::Bos) == 0;
}

işlev test_yapi() -> mantıksal {
    değişken n: Nokta = Nokta { x: 40, y: 2 };
    ver n.x + n.y == 42 ve topla([1, 2, 3]) == 6 ve (SEKTOR >> 1) == 256;
}

işlev main() -> tam32 {
    değişken h: tam32 = 0;
    eğer değil test_le16() { yazdir_metin("BASARISIZ test_le16"); h = h + 1; }
    eğer değil test_bol() { yazdir_metin("BASARISIZ test_bol"); h = h + 1; }
    eğer değil test_bol_sifir() { yazdir_metin("BASARISIZ test_bol_sifir"); h = h + 1; }
    eğer değil test_secimlik() { yazdir_metin("BASARISIZ test_secimlik"); h = h + 1; }
    eğer değil test_cesit() { yazdir_metin("BASARISIZ test_cesit"); h = h + 1; }
    eğer değil test_yapi() { yazdir_metin("BASARISIZ test_yapi"); h = h + 1; }
    ver h;
}
```
