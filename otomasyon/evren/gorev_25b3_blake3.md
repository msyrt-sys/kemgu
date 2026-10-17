# Tur 25b3 — BLAKE3 blake3_bayt (yeni dosya; sözde kod satır satır verildi — DÜŞÜNMEDEN çevir)

TEK bir YENİ dosya: ```yeni:stdlib/kripto/blake3.kem. Başka dosya YOK. Aşağıdaki sözde kodu KEMGU'ya
BİREBİR çevir; algoritmayı yeniden tasarlama. Mevcut (karma.kem, aynı pakette):
`_b3_sikistir(cv: Dizi<sabitsüre<dtam32>>, m: Dizi<sabitsüre<dtam32>>, sayac: tam64, blen: tam32, bayrak: tam32) -> Dizi<sabitsüre<dtam32>>` (16 kelime),
`_b3_blok_kelime(tampon: Dizi<sabitsüre<dtam8>>, bas: tam32, blen: tam32) -> Dizi<sabitsüre<dtam32>>` (16 kelime),
`blake3_init() -> Dizi<sabitsüre<dtam32>>` (8 kelime IV).
KEMGU: `değişken x: T = ...;`, `iken k < n { ...; k = k + 1; }`, `eğer a { } değilse { }`, `ver x;`,
dizi `[]`, `dizi_ekle(d, v)`, `d[i]`, `dizi_boyut(d)`. `ifşa` YOK. Tamsayı sabitleri: CHUNK_START=1,
CHUNK_END=2, PARENT=4, ROOT=8.

```
// [D-710] BLAKE3 — parça + ağaç; ilkeller karma.kem'de (Python referansı + resmi vektörlerle doğrulandı).

işlev _b3_ilk8(d: Dizi<sabitsüre<dtam32>>) -> Dizi<sabitsüre<dtam32>>:
    r = []; i = 0; iken i < 8: dizi_ekle(r, d[i]); i++ ; ver r

işlev _b3_birlestir(a: Dizi<sabitsüre<dtam32>>, b: Dizi<sabitsüre<dtam32>>) -> Dizi<sabitsüre<dtam32>>:
    r = []; i = 0; iken i < 8: dizi_ekle(r, a[i]); i++
    i = 0; iken i < 8: dizi_ekle(r, b[i]); i++ ; ver r

işlev _b3_yigin_al(y: Dizi<sabitsüre<dtam32>>, k: tam32) -> Dizi<sabitsüre<dtam32>>:   // k. grup
    r = []; i = 0; iken i < 8: dizi_ekle(r, y[8*k + i]); i++ ; ver r

işlev _b3_yigin_yaz(y: Dizi<sabitsüre<dtam32>>, k: tam32, cv: Dizi<sabitsüre<dtam32>>) -> tam32:
    i = 0
    iken i < 8:
        eğer 8*k + i < dizi_boyut(y): y[8*k + i] = cv[i]
        değilse: dizi_ekle(y, cv[i])
        i++
    ver 0

işlev blake3_bayt(veri: Dizi<sabitsüre<dtam8>>, n: tam32) -> Dizi<sabitsüre<dtam32>>:
    parca_say: tam32 = (n + 1023) / 1024 ; eğer parca_say == 0: parca_say = 1
    yigin: Dizi<sabitsüre<dtam32>> = [] ; yigin_n: tam32 = 0
    // son parçanın "çıktısı":
    o_cv = blake3_init() ; o_m = _b3_blok_kelime(veri, 0, 0) ; o_sayac: tam64 = 0 ; o_blen: tam32 = 0 ; o_bayrak: tam32 = 3
    c: tam32 = 0
    iken c < parca_say:
        bas = c * 1024 ; uz = n - bas ; eğer uz > 1024: uz = 1024
        blok_say = (uz + 63) / 64 ; eğer blok_say == 0: blok_say = 1
        cv = blake3_init() ; b: tam32 = 0
        iken b < blok_say:
            bb = bas + b * 64 ; blen = uz - b * 64 ; eğer blen > 64: blen = 64
            bayrak: tam32 = 0 ; eğer b == 0: bayrak = bayrak + 1 ; eğer b == blok_say - 1: bayrak = bayrak + 2
            m = _b3_blok_kelime(veri, bb, blen)
            eğer b == blok_say - 1:
                o_cv = cv ; o_m = m ; o_sayac = c olarak tam64 ; o_blen = blen ; o_bayrak = bayrak
            değilse:
                cv = _b3_ilk8(_b3_sikistir(cv, m, c olarak tam64, 64, bayrak))
            b++
        eğer c < parca_say - 1:          // son OLMAYAN parça → CV'yi yığına ekle (birleştirerek)
            yeni = _b3_ilk8(_b3_sikistir(o_cv, o_m, o_sayac, o_blen, o_bayrak))
            toplam: tam32 = c + 1
            iken toplam % 2 == 0:
                yigin_n = yigin_n - 1
                yeni = _b3_ilk8(_b3_sikistir(blake3_init(), _b3_birlestir(_b3_yigin_al(yigin, yigin_n), yeni), 0, 64, 4))
                toplam = toplam / 2
            _b3_yigin_yaz(yigin, yigin_n, yeni) ; yigin_n = yigin_n + 1
        c++
    // bitiş: yığının tepesinden tabanına ebeveyn çıktıları
    iken yigin_n > 0:
        yigin_n = yigin_n - 1
        sag = _b3_ilk8(_b3_sikistir(o_cv, o_m, o_sayac, o_blen, o_bayrak))
        o_m = _b3_birlestir(_b3_yigin_al(yigin, yigin_n), sag)
        o_cv = blake3_init() ; o_sayac = 0 ; o_blen = 64 ; o_bayrak = 4
    ver _b3_ilk8(_b3_sikistir(o_cv, o_m, o_sayac, o_blen, o_bayrak + 8))
```
