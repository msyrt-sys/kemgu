# ChatGPT köprüsü — elle paralel üretim

Amaç: saf mantık işlerini (bayt dizisi girer, yapı çıkar) ChatGPT'ye vererek
Claude/deepseek hattıyla **paralel** ilerlemek. ChatGPT'nin çıktısı doğrudan
depoya girmez; mevcut EVREN hattından geçer (biçim denetimi → uygula → iki
derleyicide derle + çalıştır). Kapılar ve entegrasyon Claude'da kalır.

## Kim ne yapar
| İş | Kim |
|---|---|
| Paket (görev + bağlam) hazırlamak, sabitleri araçla doğrulamak | Claude |
| Paketi ChatGPT'ye yapıştırıp yanıtı kaydetmek | **Sen (Mehmet)** |
| Kodu yazmak (sürücü mantığı + test) | ChatGPT |
| Uygulama + iki derleyicide doğrulama | `dongu.py --yanit` (otomatik) |
| Gözden geçirme, sabotaj, kapılar, donanıma bağlama, PR | Claude |

ChatGPT'ye **verilmeyen** işler: `src/`, `selfhost/`, `runtime/`, Makefile,
asm/`güvensiz`/MMIO, kapı betikleri — bunlar derleyici bilgisi ve ölçüm
disiplini ister.

## Akış (paket başına 3 adım)
1. **Paketi aç ve kopyala:** `otomasyon/chatgpt/giden/tur_cg_<iş>.md`
   dosyasının TAMAMINI ChatGPT'ye yapıştır (tek mesaj; ~15-18 bin karakter).
2. **Yanıtı kaydet:** ChatGPT'nin cevabını olduğu gibi (kod blokları dahil)
   `otomasyon/chatgpt/gelen/<iş>.md` dosyasına kaydet.
3. **İçe al ve doğrula:**
   ```bash
   python3 otomasyon/evren/dongu.py otomasyon/chatgpt/isler/<iş>.json --yanit otomasyon/chatgpt/gelen/<iş>.md
   ```
   Özet `otomasyon/evren/tur_cg_<iş>_ozet.md`'ye yazılır. `TAMAM` ise
   Claude'a "<iş> geldi" de; değilse hatayı (özetin son satırları) ChatGPT'ye
   geri yapıştırıp düzeltilmiş tam yanıtı iste, 2. adımdan tekrarla.

Durum kodları: `TAMAM` · `BLOK_YOK` (yanıtta ` ```yeni:yol ` bloğu yok — ChatGPT'den
dosyaları o biçimde vermesini iste) · `CAPA_HATA` (düzenle bloğu tutmadı) ·
`DOGRULAMA_HATA` (derleme/test kırmızı) · `ISARET_EKSIK`.

Düzeltme turunda dosyayı kirli bırakma: yeni yanıtı içe almadan önce
`git checkout -- <dosya>` ya da yeni dosyaysa `rm` (ya da Claude'a bırak).

## Paralellik
Paketler birbirinden bağımsızdır; altısını aynı anda farklı ChatGPT
pencerelerine verebilirsin. Yalnız `crc32_bolum` iki dosya üretir.

| Paket | Üretilen | Yol haritası |
|---|---|---|
| `ps2_klavye` | `drivers/ps2/klavye_tr.kem` + test | H klavye |
| `ps2_fare` | `drivers/ps2/fare.kem` + test | H fare |
| `crc32_bolum` | `drivers/disk/{crc32,bolum}.kem` + test | H disk |
| `fat32_bpb` | `drivers/fs/fat32_bpb.kem` + test | H FAT32 |
| `fat32_dizin` | `drivers/fs/fat32_dizin.kem` + test | H FAT32 |
| `acpi_tablo` | `drivers/acpi/tablo.kem` + test | H ACPI |

## Dosyalar
- `KEMGU_dil_karti.md` — dış modele verilen dil özeti. Yalnız ÖLÇÜLMÜŞ
  sözdizimi. İçindeki örnek `dogrula.sh --kart` ile iki derleyicide koşar;
  kartı değiştiren bunu da koşturmalı.
- `dogrula.sh <test.kem>` — C ve self-host: `--check` + derle + çalıştır
  (exit 0) + stdout eşitliği.
- `isler/<iş>.{md,json}` — görev metni ve EVREN iş tanımı (`"ortak": "kart"`).
- `giden/` — üretilmiş, yapıştırmaya hazır istemler
  (`dongu.py <json> --paket` ile yeniden üretilir).
- `gelen/` — senin kaydettiğin yanıtlar (git'e girmez).

## Yeni paket eklemek (Claude)
1. `isler/<iş>.md`: API'yi BİREBİR ver, her sabiti araçla doğrula (Unicode,
   CRC, checksum vektörleri), hem olumlu hem olumsuz test iste.
2. `isler/<iş>.json`: `{"tur":"cg","is":"<iş>","gorev":...,"baglam":[...],
   "ortak":"kart","dogrulama":["otomasyon/chatgpt/dogrula.sh <test>"],
   "isaretler":["CHATGPT_DOGRULA OK <test>"]}`
3. `python3 otomasyon/evren/dongu.py otomasyon/chatgpt/isler/<iş>.json --paket`
