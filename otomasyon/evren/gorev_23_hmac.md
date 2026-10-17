# Tur 23 — stdlib/kripto: çok bloklu SHA-256 + HMAC-SHA256 (RFC 2104) + koşum testleri

Üç dosyaya ```duzenle blokları yaz; başka dosyaya DOKUNMA. Saf KEMGU (asm YOK). Bağlamdaki
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

## 2) test/stdlib/test_kripto_kosum.kem (üç blok)
(a) ESKİ: `    eğer kt_sec_u64_ust_bit() == 1 { skor = skor + 8; }   // D-629`
    YENİ: o satır + altına
    `    eğer kt_sha256_cok_blok() == 1 { skor = skor + 16; }   // D-707`
    `    eğer kt_hmac_rfc4231() == 1 { skor = skor + 32; }     // D-707`
(b) ESKİ: `    ver skor;   // 15 = tam geçiş` → YENİ: `    ver skor;   // 63 = tam geçiş`
(c) ESKİ: `işlev main() -> tam32 {` → YENİ: aşağıdaki yeni işlevler + o satır:
- `kt_metin_bayt(s: metin) -> Dizi<sabitsüre<dtam8>>`: her i < metin_uzunluk(s) için
  `sabitsüre_olustur((metin_bayt(s, i) olarak dtam8))` ekle.
- `kt_tekrar(b: dtam8, n: tam32) -> Dizi<sabitsüre<dtam8>>`: n kez `sabitsüre_olustur(b)`.
- `kt_ozet_kontrol(d: Dizi<sabitsüre<dtam32>>, w0: tam64, w1: tam64, w7: tam64) -> tam32`:
  `ifşa(d[0])`, `ifşa(d[1])`, `ifşa(d[7])` değerleri (`olarak tam64`) w0/w1/w7'ye eşitse 1, değilse 0.
- `kt_sha256_cok_blok() -> tam32`: "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
  (56 bayt → İKİ blok dolgu) → w0=613247585, w1=3523623096, w7=433784513.
- `kt_hmac_rfc4231() -> tam32`: üç vektörün üçü de geçerse 1:
  TC1 anahtar kt_tekrar(11, 20), mesaj "Hi There" → 2956217441, 3638245459, 775081975.
  TC2 anahtar "Jefe", mesaj "what do ya want for nothing?" → 1541194054, 3210769742, 1693202499.
  TC6 anahtar kt_tekrar(170, 131) (BLOKTAN UZUN → önce hash), mesaj
  "Test Using Larger Than Block-Size Key - Hash Key First" → 1625567577, 518043263, 249790292.

## 3) test/kripto_kosum_harness.sh
ESKİ: `BEKLENEN=15` → YENİ: `BEKLENEN=63   # D-707: +16 çok bloklu SHA-256, +32 HMAC-SHA256 (RFC 4231)`
