# Tur 23b — kripto koşum testleri: çok bloklu SHA-256 + HMAC-SHA256 (RFC 4231)

İKİ dosyaya ```duzenle blokları yaz (toplam 4 blok); başka dosyaya DOKUNMA. `sha256_bayt` ve
`hmac_sha256` artık stdlib/kripto/karma.kem'de VAR (imzalar aşağıda); koşum bundle'ı
stdlib/kripto.kem + stdlib/kripto/*.kem + bu test dosyasını birleştirir, yani doğrudan çağrılabilir.
`işlev sha256_bayt(veri: Dizi<sabitsüre<dtam8>>, n: tam32) -> Dizi<sabitsüre<dtam32>>`
`işlev hmac_sha256(anahtar: Dizi<sabitsüre<dtam8>>, an: tam32, mesaj: Dizi<sabitsüre<dtam8>>, mn: tam32) -> Dizi<sabitsüre<dtam32>>`
Dizgi literalinde KAÇIŞ YOKTUR; `hata` anahtar kelimedir. Sabitler Python hashlib/hmac ile doğrulandı.

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
