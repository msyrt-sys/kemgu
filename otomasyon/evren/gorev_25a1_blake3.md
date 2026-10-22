# Tur 25a1 — BLAKE3 ilkelleri: G, tur, sıkıştırma, blok kelimeleri (ağaç/parça mantığı BU TURDA YOK)

YALNIZ `stdlib/kripto/karma.kem` SONUNA TEK bir ```duzenle bloğu (ESKİ = dosyanın son satırları, kesitte).
Saf KEMGU, asm YOK. Gizli veri `sabitsüre<dtam32>` / `sabitsüre<dtam8>` içinde kalır — `ifşa`
KULLANMA. Deyimler (kesitte): `_karma_rotr_u32(x, n)` (sağa döndür, sabitsüre<dtam32>), `^ | & + >> <<`,
`sabitsüre_olustur(x)`, bayt→kelime `(b olarak sabitsüre<dtam32>)` (zext), `dizi_ekle(d, x)`,
`dizi_boyut(d)`, `d[i]`, `d[i] = x`, boş dizi `değişken d: Dizi<T> = [];`. İndeksler/sayaçlar
açık (`tam32`/`tam64`). `hata` anahtar kelimedir (değişken adı olamaz). Dizgi literalinde kaçış yok.
Mevcut `blake3_init()` (IV = SHA-256 H0 ile aynı 8 kelime) KALSIN ve kullanılsın.

## Algoritma (BLAKE3 spesifikasyonu, referans gerçekleme ile birebir; Python ile doğrulandı)
Sabitler: bayraklar CHUNK_START=1, CHUNK_END=2, PARENT=4, ROOT=8. Blok 64 bayt, parça 1024 bayt.
Mesaj permütasyonu PERM = [2, 6, 3, 10, 7, 0, 4, 13, 1, 11, 12, 5, 9, 14, 15, 8] (m'[i] = m[PERM[i]]).
- `_b3_g(s, a, b, c, d, x, y)` (s: Dizi<sabitsüre<dtam32>> yerinde): s[a]=s[a]+s[b]+x; s[d]=rotr(s[d]^s[a],16);
  s[c]=s[c]+s[d]; s[b]=rotr(s[b]^s[c],12); s[a]=s[a]+s[b]+y; s[d]=rotr(s[d]^s[a],8); s[c]=s[c]+s[d];
  s[b]=rotr(s[b]^s[c],7).
- tur: g(0,4,8,12,m0,m1) g(1,5,9,13,m2,m3) g(2,6,10,14,m4,m5) g(3,7,11,15,m6,m7)
       g(0,5,10,15,m8,m9) g(1,6,11,12,m10,m11) g(2,7,8,13,m12,m13) g(3,4,9,14,m14,m15).
- `_b3_sikistir(cv[8], m[16], sayac: tam64, blen: tam32, bayrak: tam32) -> Dizi<sabitsüre<dtam32>>` (16 kelime):
  s = cv[0..7] + IV[0..3] + [sayac düşük 32 bit, sayac yüksek 32 bit, blen, bayrak] (son dördü
  `sabitsüre_olustur((… olarak dtam32))`). 7 tur; her turdan sonra (sonuncusu HARİÇ) m permüte edilir.
  Sonra i<8 için s[i] = s[i] ^ s[i+8] ve s[i+8] = s[i+8] ^ cv[i]. 16 kelimeyi döndür.
  Zincir değeri (CV) = dönüşün ilk 8 kelimesi.
- Blok kelimeleri KÜÇÜK-UÇ: w = b0 | b1<<8 | b2<<16 | b3<<24; blok 64 bayttan kısaysa sıfırla doldur.

## Bu turda YAZILACAKLAR (yalnız bunlar)
- `_b3_g`, `_b3_tur(s, m)` (m: 16 kelime), `_b3_permute(m) -> Dizi<sabitsüre<dtam32>>`,
  `_b3_sikistir(cv, m, sayac: tam64, blen: tam32, bayrak: tam32) -> Dizi<sabitsüre<dtam32>>` (16 kelime),
  `_b3_blok_kelime(tampon: Dizi<sabitsüre<dtam8>>, bas: tam32, blen: tam32) -> Dizi<sabitsüre<dtam32>>`
  (tampon[bas..bas+blen) küçük-uç 16 kelime, eksik baytlar 0).
Bloğun başına `// [D-710] BLAKE3 ilkelleri` yorumu. Parça/ağaç/blake3_bayt SONRAKİ turda.
