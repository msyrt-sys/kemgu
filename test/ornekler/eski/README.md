# test/ornekler/eski — Referans örnekler (uyumsuz)

Bu dizindeki `.kem` dosyaları **determined-cohen** Claude branch'inden geldi (konsolidasyon
sırasında ek dosya olarak alındı). Mevcut KEMGU compiler ile `--check`'ten geçmiyorlar —
referans amaçlı saklanıyor.

## Neden uyumsuz?

determined-cohen ayrı bir dil yolunda ilerlemişti (A-L harfli aşamalar). Aşağıdaki
özellikler bizim ana hatta (bold-curran + Linear Types V1) henüz yok veya farklı:

### 1. Eksik built-in fonksiyonlar (15 dosya)

determined-cohen'in stdlib seed'i şu adlarda fonksiyonlar tanımlıyor — bizim ana hat
sadece tek bir `yazdir(metin)` built-in'ini tanıyor (ADIM 27: libc `puts` köprüsü):

| Eksik built-in | Hangi dosyalarda |
|----------------|------------------|
| `yazdır` (ı'lı) — bizdeki `yazdir` (i) | hello.kem (port edildi → ana dizinde) |
| `yaz`, `yaz_tam` | dizi_yazdir, fib_yazdir, say |
| `yazdır_tam`, `yazdır_tam64` | say, dizi_yazdir, fib_yazdir, tip_alias |
| `uzunluk`, `min`, `maks` | dizi_yazdir |
| `oku_dosya`, `yaz_dosya` | dosya_io |
| `bellek_yarat`, `bellek_oku`, `bellek_yaz` | arena_bellek, heap_dizi_metin |
| `kanal_yarat`, `kanal_gonder`, `kanal_al` | kanal_basit |

**Port etmek için:** Stdlib seed (`stdlib/io/yaz.kem`, `stdlib/dizi/uzunluk.kem`, vs.)
yazmak gerek + LLVM backend'de libc çağrılarını veya runtime fonksiyonlarını köprüleme.
Yeşil iş ama büyük (~10-15 fonksiyon × test).

### 2. Eksik dil özellikleri (2 dosya)

- **tip_alias.kem** — `tip Yas = tam32;` syntax. Bizim hat'ta tip alias yok. Eklemek
  için: yeni anahtar kelime `tip`, parser top-level handler, sembol kategorisi
  `SEMBOL_TIP_ALIAS`, tip kontrolünde isim çözümleme. Yeşil iş, ~50-100 satır.

- **secimlik.kem** — pattern matching kollarında binding (`değer(s) => s`). ADIM 25'te
  `hiç`/`değer` ifade desteği eklendi ama pattern binding kol gövdesi scope'una desen
  tanımlayıcılarını eklemiyor. Çok küçük bir genişleme — `eslesme.kem`'deki çalışan
  desenle aynı yaklaşım ama scope ekleme adımı eksik.

- **kisitli_generic.kem**, **monomorph.kem**, **ozellik_uygula.kem** — bizim
  compiler'ımızdaki constraint v2 (`uygula`/`özellik`) ile sözdizimsel olarak benzer
  ama bağımlı oldukları stdlib seed eksik (yaz, uzunluk, vs.).

- **esles_basit.kem** — basit `eşleş` örneği ama gövdesinde `yazdır_tam` çağrısı var.

- **icin_dongu.kem** — `için` döngüsü örneği ama gövdesinde `yazdır_tam` çağrısı var.

- **bootstrap.kem** — kapsamlı bootstrap demo, eksik built-in'lere bağımlı.

### 3. Kasıtlı hata-test dosyası

- **lineer_hata.kem** ana dizinde kalıyor (taşınmadı) — Linear Types Spec V1'in
  L001/L002/L004/LR002 hatalarını sergileyen referans dosya, `--check`'in hata
  döndürmesi *beklenen* davranış.

## Port stratejisi (sonraya bırakıldı)

1. **Önce stdlib seed:** `stdlib/io/`, `stdlib/dizi/`, `stdlib/sayisal/` altında
   `yaz`, `yazdır_tam`, `uzunluk`, `min`, `maks` vs. fonksiyonları yaz veya köprüle.
2. **Sonra tip alias:** `tip Ad = HedefTip;` syntax + sembol tablosu.
3. **Sonra pattern binding scope:** `eşleş` kollarında yapıcı desen tanımlayıcılarını
   kol gövdesi scope'una ekle (ADIM 25 genişletmesi).
4. Her geçen dosyayı `test/ornekler/eski/` → `test/ornekler/` taşı.

## Liste

- arena_bellek.kem (+ .out)
- bootstrap.kem (+ .out)
- dizi_yazdir.kem (+ .out)
- dosya_io.kem (+ .out)
- esles_basit.kem (+ .out)
- fib_yazdir.kem (+ .out)
- heap_dizi_metin.kem (+ .out)
- icin_dongu.kem (+ .out)
- kanal_basit.kem (+ .out)
- kisitli_generic.kem (+ .out)
- monomorph.kem (+ .out)
- ozellik_uygula.kem (+ .out)
- say.kem (+ .out)
- secimlik.kem (+ .out)
- stdlib_karisim.kem (+ .out)
- tip_alias.kem (+ .out)
