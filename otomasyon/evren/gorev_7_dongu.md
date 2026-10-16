## Görev: `otomasyon/evren/dongu.py` (YENİ dosya) — tek komutluk tur yürütücüsü

Amaç: orkestratörün bir turu tek komutla koşması ve yalnız KISA bir özet okuması.
Mevcut `otomasyon/evren/executor.py` bağlamda; onun `sor` / `uygula` alt
komutlarını SUBPROCESS ile çağır (import ETME — onun argparse'ı modül düzeyinde).

### Kullanım
`python3 otomasyon/evren/dongu.py <is_tanimi.json>`

`is_tanimi.json` alanları:
```json
{"tur": "8", "is": "minifs_x86",
 "gorev": "otomasyon/evren/gorev_8_minifs.md",
 "baglam": ["runtime/kem_minifs.kem"], "kesit": ["Makefile:1900-1950"],
 "effort": "low",
 "dogrulama": ["make calistir_kem_os_x86", "make calistir_check_kapisi"],
 "isaretler": ["X86 FS RW OK"]}
```
`baglam`, `kesit`, `effort`, `isaretler` isteğe bağlı (yoksa boş/None).

### Adımlar (sırayla; ilk başarısızlıkta DUR)
1. `executor.py sor --tur T --is I --gorev G [--baglam ...] [--kesit ...] [--effort E]`
   çalıştır. Çıkış kodu 3 → durum `KOTA`. Başka sıfır-dışı → `SOR_HATA`.
   Çıktının son satırından `finish=` ve `blok=` değerlerini ayrıştır
   (biçim: `tur T I: 12.3 sn, finish=stop, blok=4, usage={...}`).
   `blok=0` → durum `BLOK_YOK` (finish değerini özete yaz).
2. `executor.py uygula --tur T --is I --kuru` — çıktıda `HATA` geçen satır
   varsa → durum `CAPA_HATA` (o satırları özete yaz), UYGULAMA YAPMA.
3. `executor.py uygula --tur T --is I` (gerçek uygulama).
4. `dogrulama` listesindeki her komutu `bash -c` ile, depo kökünde, sırayla
   koş; stdout+stderr'i `otomasyon/evren/tur_<T>_<I>_dogrulama.log` dosyasına
   EKLE (komut başlığıyla). İlk sıfır-dışı çıkışta durum `DOGRULAMA_HATA`;
   o komutun çıktısının SON 15 satırını özete yaz.
5. Tüm komutlar geçtiyse: `isaretler`deki her dizgi doğrulama logunda
   geçmeli (`in`); eksik olan → durum `ISARET_EKSIK`. Hepsi varsa `TAMAM`.

### Çıktı
- `otomasyon/evren/tur_<T>_<I>_ozet.md`: durum, finish/blok, uygulanan
  bloklar (kuru çalıştırmanın satırları), `git diff --stat` çıktısı, hata
  ayrıntısı (varsa).
- stdout'a da AYNI özet (en çok ~40 satır).
- Çıkış kodu: `TAMAM` → 0, `KOTA` → 3, diğerleri → 1.

### Kurallar
- Yalnız standart kütüphane. Commit ATMAZ, push ETMEZ, kapı koşmaz.
- Depo kökü: `pathlib.Path(__file__).resolve().parents[2]`.
- Anahtar dosyasını OKUMAZ (executor okur).
- Türkçe yorumlar, executor.py üslubu.
