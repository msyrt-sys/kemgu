# Tur 23a — stdlib/kripto: çok bloklu SHA-256 + HMAC-SHA256 (RFC 2104) + koşum testleri

YALNIZ `stdlib/kripto/karma.kem`'e TEK bir ```duzenle bloğu yaz; başka dosyaya DOKUNMA. Saf KEMGU (asm YOK). Bağlamdaki
`stdlib/kripto/karma.kem` kesitleri deyimleri gösterir: `sabitsüre<dtam32>` üzerinde `^ | & >> << +`,
`sabitsüre_olustur(x)`, `ifşa(s)` (gizliyi açığa çıkarır), `(x olarak dtam32)`. Dizi büyütme:
`dizi_ekle(d, x)`, boyut `dizi_boyut(d)`, okuma `d[i]`, yazma `d[i] = x`. Boş dizi:
`değişken d: Dizi<sabitsüre<dtam8>> = [];`. Dizgi literalinde KAÇIŞ YOKTUR. `hata` bir anahtar
kelimedir — değişken adı olarak KULLANMA. Sabitler aşağıda araçla (Python hashlib/hmac) doğrulandı.

## 1) stdlib/kripto/karma.kem — dosyanın SONUNA (ESKİ = son satırlar, kesitte `hmac_pad_uret`'in sonu)
Mevcut `sha256_blok_sikistir(state, blok_w)` (16 kelimelik blok, state'i yerinde günceller) ve
`sha256_H0()` kullanılacak; yeniden tanımlama YOK.
- `işlev sha256_bayt(veri: Dizi<sabitsüre<dtam8>>, n: tam32) -> Dizi<sabitsüre<dtam32>>` —
  FIPS 180-4 dolgusu: n bayt + 0x80 + sıfırlar + 8 baytlık BÜYÜK-UÇ bit uzunluğu (n*8; üst 32 bit 0
  yeter), toplam 64'ün katı. Her 64 baytlık blok için 16 kelime BÜYÜK-UÇ kur
  (w = b0<<24 | b1<<16 | b2<<8 | b3, baytlar `olarak sabitsüre<dtam32>` ile genişletilir) ve
  `sha256_blok_sikistir` çağır. Durum sha256_H0()'dan başlar (kesitteki test bunu gösteriyor). 8
  kelimelik durumu döndür.
- `işlev hmac_sha256(anahtar: Dizi<sabitsüre<dtam8>>, an: tam32, mesaj: Dizi<sabitsüre<dtam8>>, mn: tam32) -> Dizi<sabitsüre<dtam32>>`
  — RFC 2104, blok 64 bayt: an > 64 ise anahtar = sha256_bayt(anahtar, an) özetinin 32 baytı
  (büyük-uç), sonra 64'e sıfırla doldur. ic = (K ^ 0x36)[64] || mesaj → iç = sha256_bayt(ic, 64 + mn);
  dis = (K ^ 0x5C)[64] || iç özetin 32 baytı (büyük-uç) → ver sha256_bayt(dis, 96).
  Kelime→bayt: (w >> 24), (w >> 16), (w >> 8), w; her biri `& 255` ve `olarak sabitsüre<dtam8>`.

