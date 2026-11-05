# CODEX GÖREVİ — KEMGU-OS saf-mantık sürücü dalgası (≈10 saat, kesintisiz)

Bu dosya senin (Codex) tek talimat kaynağındır. Baştan sona oku, sonra **soru
sormadan** çalış. Karar gerektiren her yerde spesifikasyona uyan EN BASİT
seçeneği seç ve `VARSAYIM` olarak ilerleme dosyasına yaz. Durma; bir işte
takılırsan (45 dk) kaydet ve sıradakine geç.

## 0. Bağlam (5 dk oku)
KEMGU, Türkçe sözdizimli bir sistem dili + onunla yazılan bir işletim sistemi.
Hedef: gerçek bir PC'de ekran + Türkçe klavye + fare + FAT32 çalışan OS. Senin
işin **donanıma dokunmayan saf mantık**: bayt dizisi girer, yapı/çeşit çıkar.
Donanıma bağlama (port/MMIO/kesme) Claude'da kalır.

Okunacaklar (sırayla):
1. `otomasyon/chatgpt/KEMGU_dil_karti.md` — **dilin TEK geçerli özeti.** Kart
   dışı sözdizimi uydurma. Şüphedeysen 10 satırlık bir probe yaz ve derle.
2. `otomasyon/chatgpt/README.md` — köprünün mantığı.
3. `drivers/ortak/bayt.kem` — hazır little-endian okuyucular. Yeniden YAZMA, `kullan` et.
4. `otomasyon/chatgpt/isler/*.md` — Faz A'nın ayrıntılı spesifikasyonları.
5. `drivers/virtio/status.kem` + `tests/drivers/virtio/virtio_status_test.kem` — desen örneği.

## 1. Çalışma alanı — ZORUNLU
Ana klasörde (`/home/msyrt/Desktop/kemgu`) başka oturumlar çalışıyor ve
`build/` paylaşılırsa birbirinizin derlemesini ezersiniz. **Kendi worktree'ni aç:**
```bash
cd /home/msyrt/Desktop/kemgu
git fetch origin
git worktree add ../kemgu-codex -b codex/surucu-dalga1 origin/main
cd ../kemgu-codex
make -j8 build/kemgu build/codegen build/kdl_runtime.o build/kdl_runtime_mmio.o
bash otomasyon/chatgpt/dogrula.sh --kart          # "CHATGPT_DOGRULA OK" görmelisin
date > otomasyon/chatgpt/codex_ilerleme.md         # başlangıç saati
```
Bundan sonra **yalnız `../kemgu-codex` içinde** çalış. Ana klasöre ve
`.claude/worktrees/`e dokunma.

## 2. Kesin kurallar
- **Dokunabileceğin yerler:** `drivers/**`, `tests/drivers/**`,
  `otomasyon/chatgpt/codex_*.md`, `tools/codex/**` (Python üreteçleri) ve
  Makefile'da YALNIZ `CHATGPT_TESTLER :=` satırındaki wildcard listesi
  (yeni test dizinini eklemek için).
- **YASAK:** `src/`, `selfhost/`, `runtime/`, `boot/`, `linker/`, `test/`
  (tekil — kapı korpusları), Makefile'ın geri kalanı, `.github/`, `CLAUDE.md`,
  `LOOP.md`. `güvensiz`, `çıplak`, `satıriçi_asm`, `yetki`, `mmio_*`, `görev`,
  `kanal`, ham işaretçi `*T` kullanma.
- **Derleyici hatası bulursan:** (C ile self-host farklı sonuç veriyor, ya da
  kartta geçerli bir şekil reddediliyor) derleyiciyi DÜZELTME. 15 satırı aşmayan
  minimal örnekle `otomasyon/chatgpt/codex_bulgular.md`'ye yaz (komut + iki
  çıktı), sürücüde zararsız bir dolanma yolu kullan, yorumda bulgu no'sunu belirt.
- **Testi zayıflatma, silme, beklenen değeri koda uydurma.** Test kırmızıysa
  kod yanlıştır ya da spesifikasyonu yanlış okudun — ikisini de kaydet.
- **Her sabit araçla:** CRC, sağlama, Unicode kodu, ofset tablosu, beklenen
  çıktı… hepsini önce `python3` ile hesapla; testin yorumuna hesap komutunu yaz.
  Bu metindeki ofsetler standarttan alındı ama standart KAZANIR — çelişki varsa
  standarda uy ve `codex_bulgular.md`'ye yaz.
- Türkçe UTF-8 dosyalarda `sed -i`/`awk`/`perl -i` KULLANMA (karakter bozar).
  Dosyaları editörünle ya da `python3` (encoding="utf-8") ile yaz.
- Hiçbir şeyi `main`'e push etme, merge etme, PR'ı onaylama.

## 3. Her iş için döngü (DEĞİŞMEZ)
1. Modül + test dosyasını yaz. Testte **hem olumlu hem olumsuz** şekiller olsun;
   bozuk/kısa girdi panik değil `hata(...)`/`hiç` vermeli.
2. `bash otomasyon/chatgpt/dogrula.sh tests/drivers/<dizin>/<ad>_test.kem`
   → `CHATGPT_DOGRULA OK` görene kadar düzelt. (C + self-host derle + çalıştır
   + stdout eşitliği; exit 0 = tüm testler geçti.)
3. **Sabotaj:** modülde testin koruduğu bir kuralı boz (ör. sağlama denetimini
   kaldır, işaret bitini yoksay) → aynı komut KIRMIZI olmalı → geri al.
   Kırmızı olmuyorsa test zayıftır: testi güçlendir, sabotajı tekrarla.
   Kod derlenmez hâle gelirse sabotaj geçersizdir; derlenen bir bozma seç.
4. `make calistir_chatgpt_kopru` yeşil (yeni dizin ise önce `CHATGPT_TESTLER`e ekle).
5. Commit: `git add <yalnız kendi dosyaların> && git commit -m "SURUCU(codex): <iş> — <tek cümle>"`.
6. `otomasyon/chatgpt/codex_ilerleme.md`'ye bir satır:
   `- [x] <iş> · <test sayısı> test · sabotaj: <ne bozuldu> → kırmızı · <saat>`
7. Her 5 işte bir: `git push -u origin codex/surucu-dalga1` (yedek).

## 4. İş listesi (sırayla; bağımlılıklar parantez içinde)

### Faz A — hazır spesifikasyonlu paketler
Her birinin ayrıntılı API'si `otomasyon/chatgpt/isler/<iş>.md`'de. API adlarını
BİREBİR koru (sonraki işler onlara dayanır).
- A1 `ps2_klavye` → `drivers/ps2/klavye_tr.kem`
- A2 `ps2_fare` → `drivers/ps2/fare.kem`
- A3 `crc32_bolum` → `drivers/disk/crc32.kem` + `drivers/disk/bolum.kem`
- A4 `fat32_bpb` → `drivers/fs/fat32_bpb.kem`
- A5 `fat32_dizin` → `drivers/fs/fat32_dizin.kem`
- A6 `acpi_tablo` → `drivers/acpi/tablo.kem`

### Faz B — metin ve dosya sistemi
- **B1 `drivers/ortak/utf8.kem`**: `utf8_coz(s: metin) -> Dizi<tam32>` (geçersiz
  dizi / aşırı uzun kodlama / vekil aralığı → 65533 U+FFFD, bir bayt ilerle),
  `utf8_kodla(k: Dizi<tam32>) -> metin` (`kod_metin` ile),
  `utf16le_coz(b: Dizi<dtam8>, ofs: tam32, birim: tam32) -> Dizi<tam32>`
  (vekil çiftleri birleşir; eşsiz vekil → 65533; 0 birimde dur),
  `utf16le_kodla(k: Dizi<tam32>) -> Dizi<dtam8>`. Test vektörlerini Python'un
  `encode/decode` ile üret (Türkçe + emoji 😀 = 128512 dahil).
- **B2 (A3,A5,B1) vekil yükseltmesi**: `bolum.kem` ve `fat32_dizin.kem`'deki
  V1 `?` dolanmasını `utf16le_coz` ile değiştir; testlere emoji'li ad ekle.
- **B3 `drivers/fs/yol.kem`**: `yol_bol(y: metin) -> sonuç<Dizi<metin>, metin>` —
  `/` ayırıcı, `.` atlanır, `..` bir üst (kökün üstü = kök), çift `/` tek sayılır,
  boş bileşen yok; göreli yol `hata("goreli")`; 255 baytı aşan bileşen hata.
  `yol_birlestir(Dizi<metin>) -> metin`.
- **B4 (A5) `drivers/fs/fat32_kisa_ad.kem`**: uzun addan 8.3 üret (Windows
  kuralı: büyük harf, `. ` baş/son kırp, geçersiz `"*+,/:;<=>?[\]|` ve ASCII
  dışı → `_`, gövde 6 + `~N`, uzantı 3), çakışma listesi verilince `~1..~9`,
  sonra gövde 5 + `~10..`. Ad 8.3'e zaten sığıyor ve tamamen büyük harfse LFN
  gerekmez (`lfn_gerekli_mi`).
- **B5 (A4,A5,B4) `drivers/fs/fat32_bicimle.kem`** — bellek içi mkfs:
  `fat32_bicimle(toplam_sektor: tam64, kume_sektor: tam32, etiket: metin) -> sonuç<Dizi<dtam8>, metin>`
  Önyükleme sektörü (BPB alanları `fat32_bpb` ile BİREBİR ayrıştırılabilmeli),
  FSInfo (imzalar 0x41615252 @0, 0x61417272 @484, 0xAA550000 @508; boş küme
  sayısı @488, sonraki boş @492), yedek önyükleme @6, 2 FAT, FAT[0]=0x0FFFFFF8,
  FAT[1]=0x0FFFFFFF, kök küme 2 = EOC, boş kök dizin. Bellek tasarrufu: imajı
  yalnız gerektiği kadar büyüt — çok büyükse `hata`.
- **B6 (B5) `drivers/fs/fat32_yaz.kem`**: imaj üzerinde
  `dosya_yaz(imaj, yol, veri: Dizi<dtam8>) -> sonuç<tam64, metin>` (küme
  ayır, zinciri iki FAT'e de yaz, LFN+8.3 dizin girdisi oluştur, alt dizin yoksa
  hata), `dizin_olustur(imaj, yol)` (`.` ve `..` girdileriyle), `dosya_sil`
  (zinciri serbest bırak, girdileri 229 ile işaretle), FSInfo sayaçlarını güncelle.
- **B7 (B6) `drivers/fs/fat32_oku.kem`**: `dosya_oku(imaj, yol) -> sonuç<Dizi<dtam8>, metin>`,
  `dizin_listele(imaj, yol) -> sonuç<Dizi<DizinGirdisi>, metin>`; ad
  karşılaştırması LFN'de tam, 8.3'te ASCII büyük/küçük harf duyarsız.
  Gidiş-dönüş testleri: biçimle → yaz → oku → bayt bayt eşit; sil → oku hata;
  çok kümeli dosya (≥3 küme); alt dizin içinde dosya; disk dolu → hata.
- **B8 (B5–B7) DIŞ KAHİN — `tools/codex/fat32_kahin.sh`**: KEMGU testinin
  ürettiği imajı stdout'a SEYREK hex olarak dök (yalnız sıfır olmayan
  sektörler: `sektor_no hexbaytlar`), Python ile dosyaya kur ve
  `fsck.fat -n imaj` ile doğrula (temiz olmalı) + `mdir`/`mcopy` varsa
  dosya içeriğini oku ve karşılaştır. Ters yön: `mkfs.fat -F 32` ile Python'da
  bir imaj üret, içine Linux araçlarıyla dosya koy (`mcopy` yoksa `mount`
  denemeyin — loop mount root ister; atla ve kaydet), seyrek .kem fikstürüne
  çevir (`tests/drivers/fs/veri/*.kem`, `genel işlev imaj() -> Dizi<dtam8>`),
  `fat32_oku` ile oku. Bu iş, kendi yazdığın kodun kendi testini geçmesinden
  ÖTE bir kanıt verir — atlama. Araç yoksa iş "ATLANDI: <neden>" diye kaydedilir.
- **B9 (A3) GUID biçimleme**: `drivers/disk/guid.kem` — 16 bayt →
  `"C12A7328-F81F-11D2-BA4B-00A0C93EC93B"` (ilk üç alan LE, son iki BE),
  tersi `guid_coz(metin)`, bilinen tür adları (EFI System, Microsoft basic
  data, Linux filesystem, Linux swap) — GUID'leri Python `uuid` ile doğrula.

### Faz C — platform tabloları ve bellek
- **C1 (A6) `drivers/acpi/madt.kem`**: imza `"APIC"`; 36 yerel APIC adresi le32,
  40 bayraklar, 44'ten girdiler (`tür u8, uzunluk u8`): 0 LAPIC (işlemci id,
  apic id, bayrak le32), 1 IOAPIC (id, -, adres le32, gsi tabanı le32), 2 ISO
  (veriyolu, kaynak, gsi le32, bayrak le16), 4 LAPIC NMI, 5 LAPIC adres
  geçersiz kılma (le64). Bilinmeyen tür uzunluğuyla atlanır; **uzunluk < 2 →
  hata** (sonsuz döngü koruması); tablo dışına taşma → hata.
- **C2 (A6) `drivers/acpi/fadt.kem`**: imza `"FACP"`; 36 FIRMWARE_CTRL, 40 DSDT,
  46 SCI_INT le16, 48 SMI_CMD, 56 PM1a_EVT_BLK, 64 PM1a_CNT_BLK, 116 RESET_REG
  (12 baytlık GAS: alan türü, bit genişliği, bit ofseti, erişim boyu, adres le64),
  128 RESET_VALUE, 140 X_DSDT le64 (yalnız uzunluk ≥ 148 ise). `dsdt_adresi`
  X_DSDT≠0 ise onu, değilse DSDT'yi döndürür.
- **C3 `drivers/pci/yapilandirma.kem`**: 256 baytlık tip-0 başlık: 0 satıcı
  le16 (65535 = aygıt yok → `hiç`), 2 aygıt, 4 komut, 6 durum, 8 rev, 9 prog_if,
  10 alt sınıf, 11 sınıf, 14 başlık türü (bit7 çok işlevli), 16..39 BAR0–5,
  60 kesme hattı, 61 kesme pini. BAR çözümü: bit0=1 G/Ç (adres & ~3);
  bellekte bit1-2 türü (2 = 64-bit, sonraki BAR'ı tüketir), bit3 önceden
  getirilebilir, adres & ~15. `bar_boyu(okunan_maske)` = tümleyen+1 (32 ve 64 bit).
  Sınıf adı tablosu: 01/06/01 AHCI, 0C/03/30 xHCI, 0C/03/20 EHCI, 03/00 VGA,
  02/00 Ethernet, 06/00 host köprüsü, 06/04 PCI köprüsü, 01/08/02 NVMe.
- **C4 `drivers/bellek/harita.kem`**: E820 / multiboot2 bellek haritası girdileri
  (taban le64, uzunluk le64, tür le32; mb2'de girdi boyu başlıktan) çöz;
  tabana göre sırala, aynı türden bitişik/örtüşenleri birleştir, farklı türler
  örtüşürse **kullanılamaz tür kazanır** (kullanılabilir alan kırpılır), sıfır
  uzunluk at. `kullanilabilir_bayt`, 4096 hizalı sayfa sayısı (taban yukarı,
  son aşağı yuvarlanır).
- **C5 (C4) `drivers/bellek/cerceve.kem`**: haritadan bit eşlemli sayfa
  ayırıcı (saf; bitmap `Dizi<dtam8>`): `ayir() -> seçimlik<tam64>`,
  `serbest(adres) -> sonuç<mantıksal, metin>` (hizasız / haritada yok / çift
  serbest → hata), `ayir_ardisik(n)`, `ayrilmis_isaretle(taban, uzunluk)`
  (çekirdek imajı için). İlk 1 MiB daima ayrılmış.
- **C6 `drivers/elf/elf64.kem`**: e_ident 0x7F 'E' 'L' 'F', sınıf 2, veri 1
  (LE), 16 e_type le16, 18 e_machine (62 x86_64, 183 AArch64), 24 e_entry,
  32 e_phoff, 54 e_phentsize (56 olmalı), 56 e_phnum. Program başlığı: 0 p_type
  le32 (1 = LOAD), 4 p_flags (1 X, 2 W, 4 R), 8 p_offset, 16 p_vaddr,
  32 p_filesz, 40 p_memsz, 48 p_align. Doğrula: filesz ≤ memsz, dosya içinde,
  **W ve X aynı anda → hata (W^X)**, LOAD segmentleri örtüşürse hata, giriş
  noktası bir X segmentinin içinde olmalı. Kahin: Python `struct` ile birkaç
  ELF başlığı üret ve ayrıca sistemdeki gerçek bir ikilinin (`/bin/true`)
  ilk 4 KiB'ını seyrek fikstüre çevirip ayrıştır (`readelf -lh` çıktısıyla karşılaştır).
- **C7 `drivers/fs/ustar.kem`** (initrd): 512 baytlık başlık: 0 ad (100),
  124 boyut (12 bayt sekizlik, NUL/boşlukla biter), 148 sağlama (8 bayt
  sekizlik; hesaplanırken bu alan 8 boşluk sayılır), 156 tür ('0' veya NUL =
  dosya, '5' = dizin), 257 `"ustar"`, 345 önek (155). İki sıfır blok = son.
  `tar_listele`, `tar_bul(ad) -> seçimlik<Dizi<dtam8>>`. Kahin: Python
  `tarfile` ile arşiv üret → seyrek fikstür.

### Faz D — ekran, metin, girdi
- **D1 `drivers/ekran/cerceve.kem`**: `yapı Cerceve { genislik; yukseklik; adim; bpp; veri: Dizi<dtam8>; }`
  (32 bpp, BGRX, `adim` bayt cinsinden ≥ genislik×4). `piksel_yaz/oku`,
  `dikdortgen_doldur` (kırpmalı), `kaydir_yukari(satir_px, renk)`,
  `kopyala(kaynak_cerceve, x, y)`. Ekran dışı her işlem sessizce kırpılır.
- **D2 `drivers/ekran/psf.kem`**: PSF1 (sihir 0x36 0x04; mod; karakter boyu;
  mod&1 → 512 glif; mod&2 → Unicode tablosu, UCS-2 LE, 0xFFFF ayırıcı) ve PSF2
  (sihir 0x72 0xB5 0x4A 0x86; 4 sürüm, 8 başlık boyu, 12 bayrak (1 = Unicode
  tablosu), 16 glif sayısı, 20 glif bayt, 24 yükseklik, 28 genişlik; Unicode
  tablosu glif başına UTF-8 dizileri, 0xFF ile biter, 0xFE dizi başlatır).
  `glif_indeksi(font, kod) -> seçimlik<tam32>`. Kahin:
  `/usr/share/consolefonts/` altındaki bir `.psf.gz`'yi Python ile aç, seyrek
  fikstüre çevir; `ğ ş ı İ ö ç ü` glif indekslerini Python'da hesaplayıp karşılaştır.
- **D3 (D1,D2) `drivers/ekran/yazi.kem`**: glifi çerçeveye çiz (satır başına
  (genislik+7)/8 bayt, MSB önce), ön/arka renk; `metin_ciz(cerceve, font, x, y, s)`;
  font'ta olmayan kod → 65533 glifi, o da yoksa `?`.
- **D4 `drivers/ekran/konsol.kem`**: hücre ızgarası (`Dizi<tam32>` kod noktası
  + `Dizi<tam32>` renk), imleç; 10 yeni satır, 13 satır başı, 9 sonraki 8'in
  katı, 8 geri sil; satır sonunda sar, altta kaydır; `kirli_satirlar()` yeniden
  çizilecek satırları döndürür ve temizler.
- **D5 (D4) `drivers/ekran/vt.kem`**: bayt bayt ANSI alt kümesi: `ESC [ n A/B/C/D`,
  `ESC [ satir;sutun H`, `ESC [ H`, `ESC [ 2 J`, `ESC [ K`, `ESC [ 0/1/3x/4x/9x m`;
  tanınmayan dizi yutulur (ekrana çöp basılmaz). UTF-8 baytları B1 ile birleştirilir.
- **D6 (A1,B1) `drivers/girdi/satir.kem`**: satır düzenleyici: `TusOlayi` alır;
  `Karakter` imleçte ekler, GERI imleçten öncekini, SIL imleçtekini siler,
  SOL/SAG, ENTER → `değer(satır metni)` ve temizler; YUKARI/ASAGI geçmiş
  (en çok 16, ardışık tekrar eklenmez); kapasite 256 kod noktası (aşınca yok sayar).
- **D7 (A2) `drivers/girdi/imlec.kem`**: `FareOlayi` biriktir, ekrana kırp,
  ekran y aşağı doğru (PS/2 dy ters çevrilir), hız çarpanı, düğme kenar
  olayları (`Basildi(dugme)`/`Birakildi(dugme)`), tekerlek kaydırma sayacı.
- **D8 `drivers/girdi/halka.kem`**: sabit kapasiteli halka tampon
  (`Dizi<tam64>`): `it` (dolunca en YENİYİ at ve `kayip` sayacını artır),
  `al -> seçimlik<tam64>`, `bos_mu`, `sayi`.
- **D9 (D1) `drivers/ekran/bmp.kem`**: "BM", 10 veri ofseti le32, 14 DIB boyu
  (≥ 40), 18 genişlik le32 (işaretli), 22 yükseklik le32 (negatif = yukarıdan
  aşağı), 26 düzlem = 1, 28 bpp (24/32), 30 sıkıştırma (0; 32 bpp için 3 de
  kabul — maskeler 54'ten), satır 4 bayta dolgulanır. `bmp_ciz(cerceve, bmp, x, y)`.
  Kahin: Python ile (Pillow yoksa elle `struct`) BMP üret.

### Faz E — bütünleştirme ve sağlamlık
- **E1 (hepsi) `tests/drivers/sim/masaustu_sim_test.kem`**: uçtan uca saf
  benzetim: B5-B6 ile bellek içi FAT32'ye `/belgeler/merhaba.txt`
  ("Merhaba Dünya — ğüşıöç") yaz; A1'e "ls /belgeler" + Enter, sonra
  "cat /belgeler/merhaba.txt" + Enter yazacak **scancode baytlarını** besle →
  D6 satır → küçük bir komut yorumlayıcı (`drivers/sim/kabuk.kem`: `ls`, `cat`,
  `echo`, bilinmeyen komut mesajı) → D4 konsol → D3 ile çerçeveye çiz (D2 fontu
  sentetik ya da B8/D2 fikstürü). Doğrula: konsol ızgarasındaki satırlar
  beklenen metin; çerçevede ilk karakterin glif pikselleri doğru renkte.
- **E2 (hepsi) panik-yok bulanıklaştırma**: `tests/drivers/bulanik/*_test.kem` —
  her ayrıştırıcıya (bolum, bpb, dizin, acpi, madt, fadt, pci, harita, elf,
  ustar, psf, bmp, vt) deterministik bir LCG (`x = (x*1103515245 + 12345) & 2147483647`)
  ile 300 rastgele girdi (boy 0..600) ve geçerli fikstürün 300 tek-bayt
  mutasyonu ver. Hiçbiri panik vermemeli (panik = süreç çıkışı ≠ 0 → test
  kırmızı). Bulduğun her panik bir **kusurdur**: düzelt, mutasyonu kalıcı
  olumsuz teste çevir. Tek koşum dogrula.sh'nin 30 sn sınırına sığmalı.
- **E3 `drivers/README.md`**: her modül için tek satır amaç + API listesi +
  durum + hangi kahinle doğrulandığı + bilinen sınırlar. `codex_bulgular.md`'yi
  derle ve özetle.

### Faz F — liste erken biterse (10 saat dolana kadar sırayla)
- F1 Kahin kapsamını genişlet: B8'i alt dizinler, 100+ dosya, silinmiş girdiler,
  ~N çakışmalarıyla tekrarla.
- F2 `drivers/fs/fat32_onar.kem`: FSInfo sayaçlarını FAT'ten yeniden hesapla,
  sahipsiz kümeleri bul (`fsck.fat` ile karşılaştır).
- F3 `drivers/girdi/klavye_us.kem`: ABD düzeni; düzen seçimini tablo
  parametresi yap (A1'e dokunmadan, ortak çekirdek çıkar).
- F4 Unicode büyük/küçük harf: Türkçe kurallı `buyuk_harf/kucuk_harf`
  (i↔İ, ı↔I) + `yol` karşılaştırmasında isteğe bağlı kullanım.
- F5 Her modülün testine sınır değer tabloları ekle (0, 1, en büyük, en büyük+1).

## 5. Bitirirken
- `git push -u origin codex/surucu-dalga1`
- `gh pr create --draft --base main --head codex/surucu-dalga1 --title "SURUCU(codex): saf-mantık sürücü dalgası 1" --body-file otomasyon/chatgpt/codex_ilerleme.md`
  (gh yoksa bu adımı atla ve kaydet). **Auto-merge AÇMA, merge ETME** — Claude
  gözden geçirir, kapıları koşar, D numarasını verir.
- `codex_ilerleme.md` sonuna özet: biten/atlanan işler, toplam test, sabotaj
  sayısı, bulgular, toplam süre (`date`).
