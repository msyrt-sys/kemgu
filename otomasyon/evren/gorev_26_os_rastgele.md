# Tur 26 — İşletim sistemi rastgele kaynağını stdlib'e bağla [D-713]

Yeni yerleşik (derleyiciye ZATEN eklendi, sen ekleme): `os_rastgele() -> dtam64` — işletim
sisteminin kriptografik rastgele kaynağından 64 bit; başarısızlıkta çalışma zamanı PANİK eder.

İKİ ```duzenle bloğu üret (ESKİ = kesitteki satırlar birebir). Başka dosya YOK.

## 1) `stdlib/kripto/rastgele.kem` — `secure_rastgele_doldur`
Gövdeyi (xorshift64 yer tutucusu) şununla DEĞİŞTİR: her 8 baytta bir `os_rastgele()` çağır,
kelimenin baytlarını KÜÇÜK-UÇ sırayla `sonuc[i]`ye yaz. Sözde kod:
```
k: dtam64 = 0 ; i: tam32 = 0
iken i < gereksinim:
    eğer i % 8 == 0: k = os_rastgele()
    orta: dtam32 = (k olarak dtam32)      // E004'ten kaçınmak için iki adımlı daraltma (mevcut desen)
    alt: dtam8 = (orta olarak dtam8)
    sonuc[i] = sabitsüre_olustur(alt)
    k = k >> 8
    i = i + 1
ver gereksinim
```
İşlevin üstündeki "V1 PLACEHOLDER / KRİPTO GÜVENSİZ / V2" yorumlarını kaldır, yerine:
`// [D-713] OS entropisi (Linux /dev/urandom, Windows BCryptGenRandom); başarısızlıkta PANİK — sessiz 0 YOK.`
Dosya başlığındaki "V1: xorshift64 fallback" satırları ve xorshift/xoshiro işlevleri KALSIN (test PRNG'si).

Ayrıca aynı dosyaya, `secure_rastgele_doldur`un hemen ALTINA ekle:
```
// [D-713] Gizli 64 bitlik rastgele kelime.
işlev os_rastgele_kelime() -> sabitsüre<dtam64> { ver sabitsüre_olustur(os_rastgele()); }
```

## 2) `test/stdlib/test_kripto_kosum.kem` — yeni sınama + bit 128
`main`'in hemen ÜSTÜNE ekle:
```
// [D-713] OS rastgele: 32 kelimenin OR'u TÜM bitleri, AND'i HİÇBİR biti taşımamalı
// (bir bitin 32 çekilişte sabit kalma olasılığı 2^-32). Sabit dönen kaynak ikisini de düşürür.
işlev kt_os_rastgele() -> tam32 {
    değişken veya_t: dtam64 = 0;
    değişken ve_t: dtam64 = 0;
    ve_t = ve_t - 1;                 // tüm bitler 1
    değişken hepsi: dtam64 = ve_t;
    değişken i: tam32 = 0;
    iken i < 32 {
        değişken x: dtam64 = os_rastgele();
        veya_t = veya_t | x;
        ve_t = ve_t & x;
        i = i + 1;
    }
    eğer veya_t != hepsi { ver 0; }
    eğer ve_t != 0 { ver 0; }
    ver 1;
}
```
`main` içinde BLAKE3 satırının altına: `    eğer kt_os_rastgele() == 1 { skor = skor + 128; }    // D-713`
ve son satırın yorumunu `// 255 = tam geçiş` yap.

KEMGU: `ifşa` YOK; dizgi literalinde kaçış YOK; `ve`/`veya` anahtar kelimedir (değişken adı olamaz —
bu yüzden `veya_t`/`ve_t`).
