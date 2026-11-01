# Görev: PS/2 klavye — Türkçe Q, scancode set 1 → Unicode

Gerçek donanımdaki PS/2 klavye denetleyicisi her tuş olayında bir veya birkaç
**bayt** verir (scancode set 1). Bu paket donanıma DOKUNMAZ: baytları teker teker
alan saf bir durum makinesi yazılacak. (Port okuma ayrı bir işte bağlanacak.)

## Üretilecek dosyalar
1. `drivers/ps2/klavye_tr.kem`
2. `tests/drivers/ps2/klavye_tr_test.kem`

## API (adlar ve imzalar BİREBİR bu olmalı)
```
genel yapı KlavyeDurumu {
    sol_shift: mantıksal; sag_shift: mantıksal; ctrl: mantıksal; alt: mantıksal;
    altgr: mantıksal; caps: mantıksal; e0: mantıksal;
}
genel çeşit TusOlayi {
    Yok,                       // değiştirici, bırakma, ya da yok sayılan bayt
    Karakter(tam32),           // Unicode kod noktası (ör. ğ = 287)
    Ozel(tam32, mantıksal)     // (OZEL_* kodu, basıldı mı)
}
genel işlev klavye_yeni() -> KlavyeDurumu
genel işlev klavye_bayt(d: &değişken KlavyeDurumu, b: tam32) -> TusOlayi
```
Özel kodlar (`genel sabit ...: tam32`):
`OZEL_ENTER = 1, OZEL_GERI = 2 (backspace), OZEL_TAB = 3, OZEL_ESC = 4,
OZEL_YUKARI = 5, OZEL_ASAGI = 6, OZEL_SOL = 7, OZEL_SAG = 8, OZEL_SIL = 9 (delete)`.

## Davranış (scancode set 1)
- Bırakma (break) kodu = make kodu + 128 (yani `b >= 128` ise bırakma, make = `b - 128`).
- `224` (0xE0) öneki: `d.e0 = doğru` yap, `Yok` döndür; SONRAKİ bayt genişletilmiş
  koddur, işlendikten sonra `d.e0 = yanlış`.
- Değiştiriciler (basınca `doğru`, bırakınca `yanlış`, olay `Yok`):
  sol shift 42, sağ shift 54, ctrl 29 (E0 29 = sağ ctrl, aynı `ctrl` alanı),
  alt 56, **E0 56 = AltGr** (`altgr` alanı).
- Caps Lock 58: yalnız **basışta** `caps` tersine döner; bırakma `Yok`.
- Özel tuşlar (basış → `Ozel(kod, doğru)`, bırakma → `Ozel(kod, yanlış)`):
  Enter 28, Backspace 14, Tab 15, Esc 1; E0 72 yukarı, E0 80 aşağı, E0 75 sol,
  E0 77 sağ, E0 83 delete.
- Karakter tuşları yalnız **basışta** `Karakter(kp)` üretir; bırakmada `Yok`.
- Shift = `sol_shift veya sag_shift`. Harflerde büyük harf = `shift` XOR `caps`
  (Caps açıkken Shift küçük harf verir). Harf olmayanlarda yalnız `shift` belirler,
  Caps etkisizdir.
- **Türkçe I kuralı:** 23 numaralı tuş küçükte `ı` (305), büyükte `I` (73);
  40 numaralı tuş küçükte `i` (105), büyükte `İ` (304). Bu tabloda zaten öyle.
- Tabloda olmayan her make kodu → `Yok` (panik YOK).
- `ctrl`/`alt`/`altgr` basılıyken de karakter normal üretilir (V1: kısayol yok).

## Tablo (Türkçe Q — kod noktaları araçla doğrulandı)
| make kodu (ondalık) | normal | shift | harf mi |
|---|---|---|---|
| 41 | `"` = 34 | `é` = 233 | hayır |
| 2 | `1` = 49 | `!` = 33 | hayır |
| 3 | `2` = 50 | `'` = 39 | hayır |
| 4 | `3` = 51 | `^` = 94 | hayır |
| 5 | `4` = 52 | `+` = 43 | hayır |
| 6 | `5` = 53 | `%` = 37 | hayır |
| 7 | `6` = 54 | `&` = 38 | hayır |
| 8 | `7` = 55 | `/` = 47 | hayır |
| 9 | `8` = 56 | `(` = 40 | hayır |
| 10 | `9` = 57 | `)` = 41 | hayır |
| 11 | `0` = 48 | `=` = 61 | hayır |
| 12 | `*` = 42 | `?` = 63 | hayır |
| 13 | `-` = 45 | `_` = 95 | hayır |
| 16 | `q` = 113 | `Q` = 81 | evet |
| 17 | `w` = 119 | `W` = 87 | evet |
| 18 | `e` = 101 | `E` = 69 | evet |
| 19 | `r` = 114 | `R` = 82 | evet |
| 20 | `t` = 116 | `T` = 84 | evet |
| 21 | `y` = 121 | `Y` = 89 | evet |
| 22 | `u` = 117 | `U` = 85 | evet |
| 23 | `ı` = 305 | `I` = 73 | evet |
| 24 | `o` = 111 | `O` = 79 | evet |
| 25 | `p` = 112 | `P` = 80 | evet |
| 26 | `ğ` = 287 | `Ğ` = 286 | evet |
| 27 | `ü` = 252 | `Ü` = 220 | evet |
| 30 | `a` = 97 | `A` = 65 | evet |
| 31 | `s` = 115 | `S` = 83 | evet |
| 32 | `d` = 100 | `D` = 68 | evet |
| 33 | `f` = 102 | `F` = 70 | evet |
| 34 | `g` = 103 | `G` = 71 | evet |
| 35 | `h` = 104 | `H` = 72 | evet |
| 36 | `j` = 106 | `J` = 74 | evet |
| 37 | `k` = 107 | `K` = 75 | evet |
| 38 | `l` = 108 | `L` = 76 | evet |
| 39 | `ş` = 351 | `Ş` = 350 | evet |
| 40 | `i` = 105 | `İ` = 304 | evet |
| 43 | `,` = 44 | `;` = 59 | hayır |
| 86 | `<` = 60 | `>` = 62 | hayır |
| 44 | `z` = 122 | `Z` = 90 | evet |
| 45 | `x` = 120 | `X` = 88 | evet |
| 46 | `c` = 99 | `C` = 67 | evet |
| 47 | `v` = 118 | `V` = 86 | evet |
| 48 | `b` = 98 | `B` = 66 | evet |
| 49 | `n` = 110 | `N` = 78 | evet |
| 50 | `m` = 109 | `M` = 77 | evet |
| 51 | `ö` = 246 | `Ö` = 214 | evet |
| 52 | `ç` = 231 | `Ç` = 199 | evet |
| 53 | `.` = 46 | `:` = 58 | hayır |
| 57 | ` ` = 32 | ` ` = 32 | hayır |

## Test dosyası en az şunları ölçmeli (her biri ayrı `test_` işlevi)
- `a` basış (30) → `Karakter(97)`; bırakma (158) → `Yok`.
- Shift basılı + 30 → `Karakter(65)`; shift bırakılınca tekrar 97.
- Caps açık + 30 → 65; Caps açık + Shift + 30 → 97.
- 23 → 305 (ı), Shift+23 → 73 (I), 40 → 105 (i), Shift+40 → 304 (İ),
  Caps+40 → 304.
- 26 → 287 (ğ), 39 → 351 (ş), 51 → 246 (ö), 52 → 231 (ç), 27 → 252 (ü).
- Harf olmayan: 2 → 49 ('1'), Shift+2 → 33 ('!'), **Caps+2 → 49** (Caps etkisiz).
- 28 → `Ozel(OZEL_ENTER, doğru)`, 156 → `Ozel(OZEL_ENTER, yanlış)`.
- 224 sonra 72 → `Ozel(OZEL_YUKARI, doğru)`; 224 sonra 56 → `altgr` doğru olur.
- E0 sonrası `e0` bayrağı temizlenmeli (224, 72, sonra 30 → 97).
- Bilinmeyen kod (ör. 100) → `Yok`, durum bozulmamalı.
- Caps bırakma (186) caps'i DEĞİŞTİRMEMELİ.

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
