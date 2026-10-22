# Tur 25a — stdlib/kripto/karma.kem: BLAKE3 (tam ağaç modu, 32 bayt çıktı)

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
- Parça (indeks c = parça sayacı): cv = IV; blokları sırayla; ilk blokta CHUNK_START, son blokta
  CHUNK_END bayrağı; son OLMAYAN her blok `cv = sıkıştır(cv, blok, c, 64, bayrak)[0..7]`. Son blok
  SIKIŞTIRILMAZ, "çıktı" olarak tutulur: (cv, blok kelimeleri, sayac=c, blen=son blok uzunluğu,
  bayrak). Boş girdi = tek parça, tek boş blok (blen 0, bayrak CHUNK_START|CHUNK_END).
- Ağaç (CV yığını): son parça HARİÇ her parçanın CV'si = çıktısının `sıkıştır(...)[0..7]`'si. CV'yi
  yığına eklerken: toplam = (bu parçanın indeksi + 1); `iken toplam % 2 == 0 { yeni = ebeveyn_cv(yığın_üstü_pop, yeni); toplam = toplam / 2; }`
  sonra yeni'yi it. ebeveyn_cv(sol, sağ) = sıkıştır(IV, sol(8)+sağ(8) = 16 kelime, 0, 64, PARENT)[0..7].
- Bitiş: çıktı = son parçanın çıktısı. Yığın boş değilse, yığının TEPESİNDEN tabanına doğru her CV
  için: çıktı = ebeveyn ÇIKTISI (cv=IV, m=yığın[k](8)+çıktı'nın CV'si(8), sayac 0, blen 64, bayrak
  PARENT) — yani son birleştirme sıkıştırılmaz, ROOT eklenerek sıkıştırılır.
- Kök: `sıkıştır(çıktı.cv, çıktı.m, çıktı.sayac, çıktı.blen, çıktı.bayrak | ROOT)` ilk 8 kelime = özet.

## İşlevler
`işlev blake3_bayt(veri: Dizi<sabitsüre<dtam8>>, n: tam32) -> Dizi<sabitsüre<dtam32>>` — 8 kelime
(özetin baytları bu kelimelerin KÜÇÜK-UÇ dizilişidir). Yardımcılar `_b3_` önekli. Yığın için
`Dizi<sabitsüre<dtam32>>` (8'lik gruplar halinde düz dizi) kullan; `dizi_cikar` YOKTUR — mantıksal
uzunluk sayacı tut. Bloğun başına `// [D-710] BLAKE3` başlık yorumu; mevcut "BLAKE3 Placeholder"
yorum bloğundaki "V2'de" ifadelerine dokunma.
