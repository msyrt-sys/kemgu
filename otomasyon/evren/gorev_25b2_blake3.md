# Tur 25b2 — BLAKE3 parça + ağaç + blake3_bayt

TEK bir YENİ dosya yaz: ```yeni:stdlib/kripto/blake3.kem (duzenle bloğu YOK, başka dosyaya DOKUNMA). Bu dosya
kripto paketinde `stdlib/kripto/*.kem` ile BİRLEŞTİRİLİR; ilkeller `karma.kem`'de, buradan doğrudan çağrılır.
Dosya başına kısa bir açıklama yorumu (`// [D-710] BLAKE3 — parça + ağaç; ilkeller karma.kem'de`).
Saf KEMGU; gizli veri sabitsüre içinde kalır, `ifşa` YOK. `hata` anahtar kelimedir. Dizgi literalinde kaçış yok.
Bir önceki turda EKLENDİ (kesitte; yeniden tanımlama YOK):
`_b3_sikistir(cv, m, sayac: tam64, blen: tam32, bayrak: tam32) -> Dizi<sabitsüre<dtam32>>` (16 kelime;
CV = ilk 8), `_b3_blok_kelime(tampon, bas: tam32, blen: tam32) -> Dizi<sabitsüre<dtam32>>` (16 küçük-uç
kelime, eksik 0), `blake3_init()` (IV, 8 kelime). Bayraklar CHUNK_START=1, CHUNK_END=2, PARENT=4, ROOT=8.
Blok 64, parça 1024 bayt. Bu ilkeller "abc" ile ölçüldü (doğru).

## Algoritma
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


## Yazılacak
`işlev blake3_bayt(veri: Dizi<sabitsüre<dtam8>>, n: tam32) -> Dizi<sabitsüre<dtam32>>` (8 kelime) ve
gerekirse `_b3_` önekli yardımcılar (ör. `_b3_ebeveyn(sol8, sag8) -> 16 kelimelik m`, ilk 8'i alan bir
`_b3_ilk8`). CV yığını düz `Dizi<sabitsüre<dtam32>>` (8'lik gruplar); `dizi_cikar` YOK → mantıksal
uzunluk sayacı (`yigin_n`, grup sayısı) tut, okurken `yigin[8*k + i]`, itince yerinde yaz ya da ekle.
Son parçanın "çıktısı" beş parçadır: cv(8), m(16), sayac, blen, bayrak — ayrı değişkenlerde tut.
