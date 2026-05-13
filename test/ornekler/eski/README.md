# test/ornekler/eski — Referans örnekler (uyumsuz)

Bu dizindeki `.kem` dosyaları **determined-cohen** Claude branch'inden geldi (konsolidasyon
sırasında ek dosya olarak alındı). Mevcut KEMGU compiler ile `--check`'ten geçmiyorlar —
referans amaçlı saklanıyor.

## Neden uyumsuz?

determined-cohen ayrı bir dil yolunda ilerlemişti (A-L harfli aşamalar). Aşağıdaki
özellikler bizim ana hatta (bold-curran + Linear Types V1) henüz yok veya farklı:

### 1. Eksik built-in fonksiyonlar (15 dosya)

determined-cohen'in stdlib seed'i şu adlarda fonksiyonlar tanımlıyor — bizim ana hat
sadece `yazdir(metin)` ve birkaç bellek built-in'ini tanıyordu (ADIM 27: libc `puts`
köprüsü). **2026-05-13 src-bugfix branch'inde 5 yeni I/O built-in eklendi:**

| Built-in | Imza | Eklendi mi |
|----------|------|------------|
| `yazdir(metin)` | metin -> tam32 | ✓ (libc puts) |
| `yazdir_tam(tam32)` | -> bos | ✓ (kdl_yazdir_tam) |
| `yazdir_tam64(tam64)` | -> bos | ✓ (kdl_yazdir_tam64) |
| `yazdir_satir()` | -> bos | ✓ (kdl_yazdir_satir) |
| `yaz_tam(tam32)` | -> bos | ✓ (kdl_yaz_tam) |
| `yaz_tam64(tam64)` | -> bos | ✓ (declare-only, runtime stub) |
| `yaz_metin(metin)` | -> bos | ✗ (stdlib/dosya.kem cakismasi) |
| `yazdır`, `yazdır_tam` (ı'lı) | — | ✗ (eski dosyalarda kullaniliyor) |
| `yaz` (4-char) | metin -> bos | ✗ (eklenmedi) |
| `uzunluk`, `min`, `maks` | — | ✗ (stdlib seed gerek) |
| `oku_dosya`, `yaz_dosya` | — | ✗ (stdlib/dosya cakisma + runtime) |
| `bellek_yarat`, `bellek_oku`, `bellek_yaz` | — | ✗ (stdlib seed gerek) |
| `kanal_yarat`, `kanal_gonder`, `kanal_al` | — | partial (runtime var, tip_kontrol yok) |

**Eski dosyalardan port durumu (2026-05-13):**
- 15/16 dosya halen --check'ten gecmiyor. Cogu Turkce I'li `yazdır` veya 4-char
  `yaz` kullaniyor, eklenmedi.
- ASCII versiyonlar (`yazdir_tam` vs. `yazdır_tam`) artik global scope'ta tanimli;
  bir eski dosyayi `sed -i 's/yazdır/yazdir/g'` ile port etmek artik mumkun.
- Ornek: `say.kem` icin `yazdır -> yazdir; yazdır_tam -> yazdir_tam` cevirimi yeterli.

**Daha fazlasi icin:** Stdlib seed (`stdlib/io/yaz.kem`, `stdlib/dizi/uzunluk.kem`,
vs.) yazmak gerek + LLVM backend'de libc çağrılarını veya runtime fonksiyonlarını
köprüleme. Yeşil iş ama büyük (~10-15 fonksiyon × test).

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
- kisitli_generic.kem (+ .out)  ← **PORT EDILDI** (test/ornekler/'da)
- monomorph.kem (+ .out)
- ozellik_uygula.kem (+ .out)
- say.kem (+ .out)
- secimlik.kem (+ .out)
- stdlib_karisim.kem (+ .out)
- tip_alias.kem (+ .out)

## Port denemesi sonucu (test altyapı genişletme)

16 dosyadan **1**'i port edilebildi (`kisitli_generic.kem`), 15'i için
çekirdek derleyici değişikliği gerekiyor:

| Dosya | Port durumu | Engel |
|-------|-------------|-------|
| **kisitli_generic** | ✓ port edildi | Sadece `yazdır`→`yazdir` (1 karakter) |
| arena_bellek | ✗ | `yazdır_tam` built-in yok |
| bootstrap | ✗ | `yaz_tam` + `yazdır` built-in yok |
| dizi_yazdir | ✗ | `yaz_tam`, `yazdır_tam`, `min`, `maks`, `uzunluk` yok |
| dosya_io | ✗ | `yazdır_satır`, dosya runtime yok |
| esles_basit | ✗ | `yazdır_tam` yok |
| fib_yazdir | ✗ | `yaz_tam`, `yazdır_tam` yok |
| heap_dizi_metin | ✗ | `yaz_tam`, `yazdır_tam` yok |
| icin_dongu | ✗ | `yazdır_tam` yok |
| kanal_basit | ✗ | `yazdır_tam` + concurrency syntax yok |
| monomorph | ✗ | `yazdır_tam` yok |
| ozellik_uygula | ✗ | `yaz_tam`, `yazdır_tam` yok |
| say | ✗ | `yazdir_tam`, `yazdır_tam` yok |
| secimlik | ✗ | `yazdır_tam` + pattern binding scope yok |
| stdlib_karisim | ✗ | `yazdır_mantıksal`, `yazdır_karakter` yok |
| tip_alias | ✗ | `tip Ad = ...` syntax yok + `yazdır_tam` yok |

**Port engeli analizi:**
- Runtime fonksiyonları **var** (`runtime/kdl_runtime.c`'de `kdl_yazdir_tam`,
  `kdl_yazdir_tam64`, `kdl_yazdir_kesirli`, `kdl_yazdir_mantiksal`,
  `kdl_yazdir_karakter`, `kdl_yazdir_satir`).
- Tip kontrol tarafında **built-in olarak kayıtlı değil** (`src/tip_kontrol.c`
  `tip_kontrol_baslat`'ta `yazdir`, `bellek_al/serbest/kopyala`,
  `metin_*` var; `yazdir_tam` vs. yok).
- Bu dosyaları port etmek için: `src/tip_kontrol.c`'ye 6+ yeni
  `EKLE_BUILTIN` çağrısı + `src/llvm.c`'ye karşılık gelen `declare`
  ekleme gerek. **Bu test altyapı görevinin kapsamı dışı** (src/
  dokunulamaz).

**Port edilebilirlik metriği:** %6 (1/16). Geriye kalan 15 dosya için
3-5 saatlik çekirdek yazıdır_* built-in genişletme görevi yeterli.
