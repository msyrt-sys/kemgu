# Görev: FAT32 dizin girdisi + uzun ad (LFN)

## Üretilecek dosyalar
1. `drivers/fs/fat32_dizin.kem`
2. `tests/drivers/fs/fat32_dizin_test.kem`

## API (BİREBİR)
```
genel yapı DizinGirdisi { ad: metin; ozellik: tam32; ilk_kume: tam64; boyut: tam64;
                          dizin_mi: mantıksal; }
genel işlev kisa_ad_saglama(b: Dizi<dtam8>, ofs: tam32) -> tam32
genel işlev dizin_coz(b: Dizi<dtam8>) -> sonuç<Dizi<DizinGirdisi>, metin>
```
`b` bir dizin kümesinin ham baytlarıdır (32'nin katı); `dizin_coz` tüm geçerli
girdileri sırayla döndürür.

## 32 baytlık girdi
0..10 8.3 ad · 11 özellik · 20 küme yüksek le16 · 26 küme düşük le16 · 28 boyut le32.
- ad[0] = 0 → dizinin sonu (dur). ad[0] = 229 (0xE5) → silinmiş, atla.
- özellik 15 (0x0F) → LFN girdisi. özellik & 8 (birim etiketi) → atla.
- `dizin_mi = (ozellik & 16) != 0`. `ilk_kume = (yüksek << 16) | düşük`.
- Kısa ad: 8 baytlık ad (sondaki boşluklar kırpılır) + uzantı varsa `.` + uzantı
  (kırpılmış). `"FOO     TXT"` → `"FOO.TXT"`, `"README     "` → `"README"`.

## LFN
- LFN girdileri kısa girdiden ÖNCE, **ters sırada** gelir; sıra baytı 0'da,
  `& 31` sıra no, `& 64` "son parça" bayrağı.
- Her LFN girdisi 13 UTF-16LE kod birimi taşır: 1..10 (5), 14..25 (6), 28..31 (2).
  0 ya da 65535 kod birimi adın sonudur/dolgudur.
- 13. bayt saglama toplamıdır: kısa adın 11 baytı üzerinde
  `s = ((s & 1) << 7) + (s >> 1) + bayt` (her adımda `& 255`).
  **Vektörler (araçla hesaplandı):** `"FOO     TXT"` → 101, `"UZUNDO~1TXT"` → 206.
- Toplanan LFN parçaları kısa girdinin saglamasıyla eşleşirse `ad` uzun addır;
  eşleşmezse (ya da sıra bozuksa) LFN atılır ve kısa ad kullanılır (panik değil).
- Kod birimi → `kod_metin`; vekil çiftleri V1'de `?` (63).

## Test en az şunları ölçmeli
- `kisa_ad_saglama("FOO     TXT")` = 101.
- Tek kısa girdi: ad `FOO.TXT`, küme yüksek 1 düşük 2 → 65538, boyut 1234.
- Silinmiş (229) ve birim etiketi atlanır; ad[0]=0 sonrası girdiler okunmaz.
- LFN: `"Uzun Döküman.txt"` (16 kod birimi → 2 LFN girdisi) + kısa
  `"UZUNDO~1TXT"`, saglama 206 → ad `"Uzun Döküman.txt"` (ö = 246, ü = 252).
- Saglaması yanlış LFN → kısa ad `"UZUNDO~1.TXT"`.
- Dizin bayrağı (16) → `dizin_mi` doğru.

## Genel kurallar (tüm paketler)
- Ortak bayt yardımcıları HAZIR ve doğrulanmış: `drivers/ortak/bayt.kem`
  (`bayt_sinirda`, `oku_u8`, `oku_le16`, `oku_le32`→tam64, `oku_le64`, `imza_esit`,
  `bayt_toplam8`, `bayt_sifir`, `yaz_u8/le16/le32/le64`, `yaz_ascii`). Bunları
  `kullan drivers::ortak::bayt::{...};` ile al; aynı işi yeniden YAZMA.
- Her okumadan önce `bayt_sinirda` ile sınırı denetle: bozuk/kısa girdi PANİK
  değil, `hata(...)` döndürmeli.
- Yalnız bu paketin adını verdiği dosyaları üret (`yeni:` blokları). Başka
  dosyaya dokunma.
- Kart dışı sözdizimi kullanma. Emin olmadığın her şeyi "VARSAYIMLAR" altında yaz.
- Doğrulama: `otomasyon/chatgpt/dogrula.sh <test dosyası>` iki derleyicide
  derleyip çalıştırır; test exit 0 vermeli.
