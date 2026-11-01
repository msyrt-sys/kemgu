# Görev: FAT32 BPB + küme aritmetiği + FAT zinciri

## Üretilecek dosyalar
1. `drivers/fs/fat32_bpb.kem`
2. `tests/drivers/fs/fat32_bpb_test.kem`

## API (BİREBİR)
```
genel yapı Fat32Bpb {
    sektor_bayt: tam32; kume_sektor: tam32; ayrilmis: tam32; fat_sayisi: tam32;
    toplam_sektor: tam64; fat_sektor: tam64; kok_kume: tam64; fsinfo: tam32;
    ilk_veri_sektoru: tam64; kume_sayisi: tam64;
}
genel çeşit BpbHatasi { KisaGirdi, ImzaYok, SektorBoyu, KumeBoyu, Fat32Degil }
genel işlev bpb_coz(lba0: Dizi<dtam8>) -> sonuç<Fat32Bpb, BpbHatasi>
genel işlev kume_sektoru(b: Fat32Bpb, kume: tam64) -> tam64
genel çeşit FatGirdi { Bos, Sonraki(tam64), ZincirSonu, Bozuk }
genel işlev fat_girdi(fat: Dizi<dtam8>, kume: tam64) -> FatGirdi
genel işlev zincir(fat: Dizi<dtam8>, ilk: tam64, azami: tam32) -> sonuç<Dizi<tam64>, metin>
```

## BPB ofsetleri (önyükleme sektörü)
11 sektör baytı le16 · 13 küme başına sektör u8 · 14 ayrılmış sektör le16 ·
16 FAT sayısı u8 · 17 kök girdi sayısı le16 (FAT32'de 0) · 19 toplam16 le16 ·
22 FAT boyu16 le16 (FAT32'de 0) · 32 toplam32 le32 · 36 FAT boyu32 le32 ·
44 kök küme le32 · 48 FSInfo sektörü le16 · 510/511 = 85/170.
- 512'den kısa → `KisaGirdi`; imza yok → `ImzaYok`.
- sektör baytı 512/1024/2048/4096 değilse → `SektorBoyu`.
- küme başına sektör 0 ya da 2'nin kuvveti değilse (1..128) → `KumeBoyu`.
- kök girdi ≠ 0 ya da FAT boyu16 ≠ 0 ya da FAT boyu32 = 0 → `Fat32Degil`.
- toplam = toplam16 ≠ 0 ise o, değilse toplam32.
- `ilk_veri_sektoru = ayrilmis + fat_sayisi × fat_sektor`
- `kume_sayisi = (toplam - ilk_veri_sektoru) / kume_sektor`
- `kume_sektoru(b, c) = (c - 2) × kume_sektor + ilk_veri_sektoru`

## FAT girdisi
`fat[kume × 4]` le32, **üst 4 bit maskelenir** (`& 268435455`).
0 → `Bos`; ≥ 268435448 (0x0FFFFFF8) → `ZincirSonu`; 268435447 (0x0FFFFFF7) →
`Bozuk`; diğer → `Sonraki(v)`. Tablo dışı küme → `Bozuk` (panik değil).
`zincir`: `ilk`ten başlayıp `ZincirSonu`na kadar küme listesi; `azami` adımı
aşarsa (döngü) → `hata("dongu")`; `Bos`/`Bozuk`a çarparsa `hata("bozuk zincir")`.

## Test en az şunları ölçmeli
- Geçerli BPB: 512, 8, 32, 2, toplam32 1048576, FAT boyu32 1024, kök 2 →
  ilk veri 2080, `kume_sektoru(b,2)` 2080, `kume_sektoru(b,3)` 2088.
- Üst bit maskeleme: FAT girdisi 4026531843 (0xF0000003) → `Sonraki(3)`.
- Zincir 2→3→4→EOC → [2,3,4]; döngü 2→3→2 → `hata("dongu")`.
- Her hata kodu en az bir olumsuz testle.

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
