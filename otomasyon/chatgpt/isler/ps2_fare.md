# Görev: PS/2 fare paket çözücü (3 ve 4 bayt)

PS/2 fare her hareket için 3 bayt (IntelliMouse kipinde 4 bayt) gönderir. Saf
durum makinesi: baytlar teker teker girer, paket tamamlanınca olay çıkar.

## Üretilecek dosyalar
1. `drivers/ps2/fare.kem`
2. `tests/drivers/ps2/fare_test.kem`

## API (BİREBİR)
```
genel yapı FareDurumu { paket_boyu: tam32; n: tam32; b0: tam32; b1: tam32; b2: tam32; }
genel yapı FareOlayi {
    dx: tam32; dy: tam32; dz: tam32;            // dy: YUKARI pozitif (PS/2 doğal yönü)
    sol: mantıksal; sag: mantıksal; orta: mantıksal;
}
genel işlev fare_yeni(dort_bayt: mantıksal) -> FareDurumu
genel işlev fare_bayt(d: &değişken FareDurumu, b: tam32) -> seçimlik<FareOlayi>
```

## Bayt 0 bitleri
bit0 sol, bit1 sağ, bit2 orta, **bit3 daima 1**, bit4 X işareti, bit5 Y işareti,
bit6 X taşma, bit7 Y taşma.

## Davranış
- `n == 0` iken gelen baytın bit3'ü (8) 0 ise bu bayt **senkron dışıdır**:
  atılır, `hiç` döner, `n` 0 kalır (senkron kurtarma).
- Bayt 1 = X, bayt 2 = Y; 9 bitlik ikiye tümleyen: X işareti 1 ise
  `dx = b1 - 256`, değilse `dx = b1` (aynısı Y için bit5 ile).
- 4 bayt kipinde bayt 3'ün düşük 4 biti Z (tekerlek), 4 bitlik ikiye tümleyen:
  `z = b3 & 15; eğer z >= 8 ise z - 16`. 3 bayt kipinde `dz = 0`.
- Taşma biti (bit6 veya bit7) set ise paket **atılır** (`hiç`), durum sıfırlanır.
- Paket tamamlanınca `değer(olay)` döner ve `n = 0`.
- Ara baytlarda `hiç`.

## Test en az şunları ölçmeli
- `[8, 5, 3]` → dx 5, dy 3, butonlar kapalı.
- `[9, 0, 0]` → sol basılı. `[10,0,0]` sağ, `[12,0,0]` orta.
- Negatif: `[8 + 16 + 32, 251, 254]` → dx -5, dy -2.
- Taşma: `[8 + 64, 1, 1]` → `hiç`, sonraki geçerli paket normal çözülür.
- Senkron kurtarma: `[0, 8, 1, 2]` → ilk `0` atılır, `[8,1,2]` paketi dx 1 dy 2.
- 4 bayt: `[8, 0, 0, 1]` → dz 1; `[8, 0, 0, 15]` → dz -1.
- 3 bayt kipinde 4. bayt yeni paketin ilk baytı sayılır.

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
