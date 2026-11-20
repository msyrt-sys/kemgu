# CODEX GÖREVİ 13 — kullanıcı programları: sistem çağrısı arayüzü, ELF yükleme, süreç modeli, dosya ve olay çağrıları

Dalga 1'in kuralları geçerlidir (`otomasyon/chatgpt/CODEX_GOREV.md` §2, §3, §5 — yeniden
oku), aşağıdaki **izin genişletmesi** dışında. Dalga 6'nın gerçek-QEMU kâhin kuralı da
geçerlidir (`CODEX_GOREV_6.md` §0). Soru sorma, durma; takılırsan (60 dk) kaydet ve geç.
Bu dalga öncekilerden büyüktür; **zorunlu / önerilen** ayrımına uy: zorunlu kısım bitmeden
önerilene geçme; zaman bitince yarım kalan önerilen işi taslakta bırakma — ya kapılı bitir ya hiç
başlama.

---

## 0. Hedef

Bugün KEMGU-OS'ta her şey çekirdeğin içinde koşuyor: kabuk, dosya sistemi, sürücüler. Yeni bir program
eklemek çekirdeği yeniden derlemek demek. Bu dalganın amacı, **çekirdekten ayrı derlenmiş KEMGU
programlarını** diskten yükleyip çalıştırabilmek — sıradan bir işletim sisteminin "program çalıştırma"
özelliği. Sonunda kabukta şu çalışmalı:

```text
> calistir /diskler/usb0p2/bin/kedi.elf /diskler/usb0p2/notlar/günlük.txt
...dosyanın içeriği...
[kedi.elf pid=3 cikis=0]
> calistir /diskler/usb0p2/bin/say.elf /diskler/usb0p2/notlar/günlük.txt
12 satir 87 kelime 541 bayt
[say.elf pid=4 cikis=0]
>
```

Program kendi adres alanında kullanıcı kipinde koşar ve çekirdekle **yalnız sistem çağrılarıyla**
konuşur: konsol, dosya, klavye olayı, saat, çıkış. Program bir çalışma zamanı hatasıyla sonlanırsa
(ör. sıfıra bölme) kabuk bunu bildirir ve çalışmaya devam eder.

Bu dalga bir sonraki adımın (metin düzenleyici ve dosya gezgini birer kullanıcı programı olacak) temelidir.

Yol haritası: H9a (dosya çağrıları), H9b (olay çağrıları), H9c (ELF'i dosya sisteminden yükle), H9d
(süreç modeli), H12e (sistem çağrısı tablosu belgesi). LOOP: E3b, E3c, E3d.

**Kapsam notu:** hata yalıtımı testleri (H9j / E3f) bu dalgada YOK — Claude ayrı yapacak. Bu dalga
işlevsel kısmı kurar; sayfa izinlerini ve sistem çağrısı argüman denetimini doğru kurman yeterli,
onları zorlayan negatif test programları yazma.

---

## 1. Bugünkü durum (Claude ölçtü, 2026-11-19 — varsayma, yine de kendin doğrula)

**Kullanıcı kipi (x86) yalnız test biçiminde var** (`runtime/mimari/x86_64/kem_ring3_hw.kem`, D-699/D-705):
- GDT `0x18`/`0x20` kullanıcı kod/veri, `0x28` TSS (`kxr_tss_kur`, `ltr`); D-757'den beri TSS'te IST
  yığınları da var (`runtime/kdl_kesme.c` `kdl_ist_kur`).
- Giriş `kdl_x86_ring3_gir(rip, rsp)` (`boot/start_x86_64.S` ~416): `iretq` ile kullanıcı kipine geçer.
  Geri dönüş bir istisna ya da `cik` yoluyla `isr_ortak` → `kdl_x86_r3_don` üzerinden **senkron**:
  "çalıştır ve bitene kadar bekle" modeli. Zamanlayıcıyla kullanıcı süreçleri arasında geçiş YOK.
- Bugünkü test programları çekirdek kodunun içinde **elle yazılmış makine baytlarıyla** kuruluyor
  (`kxr_y8(pa+0, 184)` ...). Çekirdekten ayrı derlenmiş x86 kullanıcı ELF'i **YOK**.

**Sistem çağrısı (x86, `kdl_syscall_stub`, start_x86_64.S ~390):** `int 0x80`; numara `rax`,
1. argüman `rdi`, 2. argüman `rdx` (stub `rdi→rsi`, `rax→rdi` taşır, `rdx` 3. parametre olarak kalır) →
`kdl_syscall_isle(num, arg, arg2)`. **Yalnız iki argüman.** IDT 0x80 kapısı kullanıcı kipinden
çağrılabilir (0xEE). Dağıtıcı `runtime/mimari/x86_64/kem_gorev_hw.kem:65`; ortak servisler
`runtime/kem_gorev_cekirdek.kem` (arm64 ile PAYLAŞILIR):
- 10 `tik`, 11 `pid`, 13 `cik`, 14 `durum(pid)`;
- tampon alanlar (D-610 doğrulayıcılarıyla): 5 `yaz(metin)`, 17/18 tek dosyalı minifs yaz/oku,
  26 `satir_oku(buf, n)`;
- arm64'te ayrıca 7 `satir`, 12 `baslat`, 24/25 ağ, 27 `saat` (`runtime/kem_gorev.kem:181`).
- Tampon doğrulama yardımcıları: `kg_user_yaz_ptr_gecerli`, `kg_user_oku_str_gecerli`; kullanıcı
  penceresi `[0x54000000, 0x54200000)` (2 MiB).

**Adres alanları (`runtime/mimari/x86_64/kem_mmu_hw.kem` ~180-230):** `kmmu_as_olustur(i)` sabit
fiziksel adreslerle: tablo alanı `0x3900000` (57 MB), alan başına 16 KiB, **en çok 6 alan**, kullanıcı
fiziksel penceresi `0x3C00000 + i*2MB`, sanal adres `0x54000000` (PD indeksi 160, 512×4 KiB sayfa).
**6 yuvanın hepsi bugün testlerde kullanılıyor.** `kmmu_as_gec(cr3)` adres alanını değiştirir.
⚠ Bu sabit adresler PVH dönemine ait; UEFI yolunda fiziksel bellek UEFI haritasından gelir (D-755
çerçeve ayırıcı, `runtime/mimari/x86_64/kem_bellek_harita.kem`). 57-72 MB bölgesinin UEFI haritasında
boş olduğu **ölçülmedi**.

**ELF:** `drivers/elf/elf64.kem` (sans-IO) ELF64'ü ayrıştırır ve yükleme planı verir (PT_LOAD sınırları,
hizalama, örtüşme, giriş noktası; hem yazılabilir hem çalıştırılabilir segmenti reddeder). Belleğe
yüklemez. arm64'te ayrı kullanıcı ELF'i VAR: `test/ornekler/kem_kullanici.kem` → `linker/user-aarch64.ld`
→ `build/kem_user.elf` (Makefile ~1290: ET_EXEC, makine, giriş adresi denetlenir) — ama o program saf
satıriçi asm; KEMGU çalışma zamanı kullanmıyor. **x86 kullanıcı linker betiği YOK.**

**Dosya sistemi (D-759):** `/diskler/{nvme,sata,usb}<n>p<b>` bağlamaları, sektör tabanlı FAT32
okuma/yazma, salt okunur bağlamalar, ESP yazılmaz, kabuk komutları. Hepsi kabuk koduna gömülü —
programların çağırabileceği bir dosya arayüzü YOK.

**Klavye/fare (D-757):** PS/2 IRQ1/IRQ12 halkaları ve USB HID klavye; kabuk tüketiyor.

**Heap (E4k — açık; Claude bu dalgayla paralel çalışıyor):** çekirdek heap'i 28 MB; kabuk dosya
komutları komut başına 0,2-1,2 MB tahsis ediyor ve geri vermiyor (`mv` 1,18 MB, `cp` 1,05 MB,
`cat` 0,52 MB). ELF okumak ve süreç kurmak da tahsis edecek — ölç ve yaz.

---

## 2. Ön koşul ve çalışma alanı
```bash
cd /home/msyrt/Desktop/kemgu && git fetch origin
git log origin/main --oneline | grep D-760     # yoksa 5 dk'da bir, en çok 2 sa; sonra DUR, kaydet
git worktree add ../kemgu-codex13 -b codex/surec-dalga13 origin/main && cd ../kemgu-codex13
make -j8 build/kemgu build/codegen build/kdl_runtime.o
make calistir_kem_os_x86 calistir_bm_x86_uefi_pc calistir_bm_x86_kabuk calistir_bm_x86_disk calistir_bm_x86_istisna   # taban
date > otomasyon/chatgpt/codex13_ilerleme.md
```
Tek dal, tek draft PR.

⚠ **Paralel derleyici işi:** Claude bu dalga sırasında derleyicide (E4k) çalışıyor; main'e derleyici
değişikliği girebilir. **Dalını dalga ortasında main ile BİRLEŞTİRME** — Claude entegrasyonda birleştirir.
Heap ölçümlerini başta ve sonda yaz.

---

## 3. Okunacaklar
- `runtime/mimari/x86_64/kem_ring3_hw.kem`; `boot/start_x86_64.S` (`kdl_syscall_stub`,
  `kdl_x86_ring3_gir`, `isr_ortak`'ın kullanıcı kipi dalı, `kdl_x86_r3_blok` düzeni, IRQ0 / LAPIC
  koçanları ve D-685 bağlam kaydı); `runtime/kdl_kesme.c` x86 IDT bloğu.
- `runtime/kem_gorev_cekirdek.kem` (ortak sistem çağrısı servisleri, tampon doğrulayıcıları, görev
  tablosu `KG_*`), `runtime/mimari/x86_64/kem_gorev_hw.kem`, `runtime/kem_gorev.kem`.
- `runtime/mimari/x86_64/kem_mmu_hw.kem` (adres alanları, sayfa izin bitleri, `KXM_NX_BIT`),
  `runtime/mimari/x86_64/kem_bellek_harita.kem` (UEFI çerçeve ayırıcı), `kem_apic_hw.kem`.
- `drivers/elf/elf64.kem` + testi; `test/ornekler/kem_kullanici.kem`, `linker/user-aarch64.ld`,
  Makefile `$(BUILD)/kem_user.elf` hedefi ve onu diske yazan satırlar (~1773).
- `test/ornekler/kem_os_x86_kabuk.kem`, `kem_os_x86_blok_ortak.kem` (bağlama tablosu, yol çözümü,
  `fat32_sektor_yaz`), `drivers/fs/*.kem`, `runtime/kem_heap.kem`.
- `tests/qemu/dalga12_kahin.py`, `dalga12_imaj.py`, `dalga12_fat.py`, `bm_istisna.py`, `ps2_irq_kahin.py`.
- `LOOP.md` D-686, D-699, D-705, D-757, D-759; yol haritası H9.

---

## 4. İzin genişletmesi (yalnız bu dosyalarda)
- **Yeni** `linker/user-x86_64.ld` (yalnız bu; mevcut linker betiklerine dokunma).
- `boot/start_x86_64.S`: YALNIZ (a) `kdl_syscall_stub` (argüman sayısı) ve (b) kullanıcı kipinden gelen
  zamanlayıcı kesmesinde bağlam kaydet/yükle koçanları (C2). PVH/UEFI girişleri, mevcut kullanıcı kipi
  testi, IST ve kurtarılabilir-#PF yolları **birebir korunur** (`calistir_kem_os_x86`,
  `calistir_bm_x86_istisna`).
- `runtime/kdl_kesme.c`: YALNIZ `#if defined(__x86_64__)` IDT bloğu (ikili modda düzenle).
- `runtime/kem_gorev_cekirdek.kem`: sistem çağrısı tablosu — ⚠ **arm64 ile paylaşılır**; arm64
  numaraları/davranışı değişmez, `calistir_kem_os_arm` + `calistir_qemu_cekirdek` yeşil kalır.
- `runtime/mimari/x86_64/*.kem` (yeni dosyalar dahil), yeni `runtime/kullanici/x86_64/*.kem`.
- `drivers/**` saf kalır (sans-IO); yeni modüller serbest (ör. `drivers/surec/`, `drivers/vfs/`).
- `test/ornekler/kem_os_x86_*.kem`, yeni `test/kullanici/*.kem`, `test/bm_x86_*_harness.sh`,
  `tests/qemu/*`, `tools/codex/*`.
- Makefile (yeni hedefler + `test_tumu` + `.PHONY` + x86 CAT listesi), `test/check_kapisi.sh` ve
  `test/baremetal_diff_harness.sh` sınıflandırma satırları.
- **Yeni** `belgeler/KEMGU_OS_Syscall_ABI.md`.
- YASAK: `src/`, `selfhost/`, `boot/efi/`, diğer `linker/` dosyaları, diğer runtime C dosyaları.
  Derleyici hatası → `codex13_bulgular.md`'ye C13-xxx + küçük tekrar üretimi (C ve self-host komutu,
  beklenen/gerçek sonuç), dolan. Muafiyet listelerine satır EKLEME.

---

## 5. Tasarım ilkeleri
1. **Tek arayüz:** kullanıcı programı çekirdek sembolüne bağlanmaz; ELF'te `llvm-nm -u` boş (ya da yalnız
   kendi çalışma zamanının sembolleri). Bunu bir kapı denetlesin.
2. **Sistem çağrısı argümanları doğrulanır:** tampon adresi + uzunluk sürecin kendi penceresinde olmalı
   (mevcut `kg_user_*` yardımcılarını genişlet), metinler azami uzunlukla okunur; geçersiz argüman →
   belgelenmiş hata kodu, süreç devam eder.
3. **Sayfa izinleri ELF segment bayraklarından:** kod salt okunur + çalıştırılabilir, veri ve yığın
   yazılabilir + çalıştırılamaz (`KXM_NX_BIT`), hiçbir sayfa ikisi birden değil. Yığının altında
   eşlenmemiş bir sınır sayfası.
4. **Süreç hatası süreci sonlandırır, sistemi değil:** kullanıcı kipindeki bir istisnada süreç
   sonlandırılır, neden seri + ekrana yazılır, kabuk devam eder; çekirdek kipindeki istisna bugünkü
   panik ekranında kalır (D-757). İkisi CS'nin RPL'iyle ayrılır (bugünkü D-699 kuralı).
5. **Sessiz kısaltma yok:** tablo dolu, yol çok uzun, ELF pencereye sığmıyor → açık hata.
6. **Salt okunur bağlamalar ve ESP** (D-759) sistem çağrısı yolunda da aynı kuralla korunur.
7. **Arayüz belgelenir ve sürümlenir** (`belgeler/KEMGU_OS_Syscall_ABI.md`); belge tablosu, çekirdek
   dağıtıcısı ve kullanıcı sarmalayıcıları birebir tutarlı olmalı — bir kapı bunu ölçsün.

---

## 6. İş listesi

### A — sistem çağrısı arayüzü v2 (ZORUNLU)
- Dosya çağrıları üç argüman ister (`oku(fd, buf, n)`). Seç ve gerekçesini yaz: (a) stub'ı genişlet
  (ör. `rax` numara; `rdi`, `rsi`, `rdx`, `r10` argümanlar — Linux x86_64 geleneğine yakın; bugünkü
  testler 2. argümanı `rdx`'te veriyor: ya uyumluluk korunur ya da o testler aynı commit'te güncellenip
  yeşil kalır), ya da (b) stub'a dokunmadan fazla argümanları doğrulanmış bir yapı adresiyle geç.
- Mevcut numaralar (x86: 5, 10, 11, 13, 14, 17, 18, 26; arm64: 7, 12, 24, 25, 27) anlamını
  değiştirmeden korunur. Yeni çağrılar ayrı bir aralıkta (ör. 40-79). Bilinmeyen numara belgelenmiş bir
  hata kodu döner. Hata kodları negatif ve tablolu.
- `belgeler/KEMGU_OS_Syscall_ABI.md`: numara, ad, argümanlar, dönüş, hata kodları, hangi mimaride var;
  kullanıcı belleği düzeni (bölüm C) ve argv düzeni.

### B — kullanıcı çalışma zamanı + linker betiği + derleme (ZORUNLU)
- `linker/user-x86_64.ld`: ET_EXEC, giriş `_start`, adres kullanıcı penceresinde; kod / salt okunur
  veri / yazılabilir veri / bss ayrı ve **sayfa hizalı** segmentler.
- `runtime/kullanici/x86_64/`: sistem çağrısı sarmalayıcıları (`sis_yaz`, `sis_cik`, `sis_ac`,
  `sis_oku`, ...) ve programın KEMGU ile yazılabilmesi için gereken en küçük çalışma zamanı.
  **Ölç:** KEMGU'nun ürettiği IR hangi `kdl_*` sembollerini istiyor (bölge, dizi, metin, panik, memcpy)?
  `runtime/kem_heap.kem` kullanıcı ELF'ine olduğu gibi derlenebiliyor mu (heap tabanı kullanıcı
  penceresinde, linker betiğinden)? Derlenemiyorsa nedenini yaz ve en küçük kullanıcı karşılığını yaz;
  çekirdek dosyasını kullanıcı ihtiyacı için değiştirme.
- Kullanıcı tarafında `kdl_panik` / dizi sınır ihlali → `cik(134)` gibi anlamlı bir çıkış.
- Makefile: her `test/kullanici/<ad>.kem` için `build/kullanici/<ad>.elf`; denetimler arm64 hedefinin
  aynası: ET_EXEC, EM_X86_64 (62), giriş adresi, yazılabilir+çalıştırılabilir segment yok, `llvm-nm -u` temiz.
- **Pencere boyutu:** bugün 2 MiB (kod+veri+heap+yığın). KEMGU programı + çalışma zamanı sığıyor mu
  ölç; büyütmek gerekiyorsa bu bir tasarım kararıdır — boyutu, gerekçeyi ve doğrulayıcıların aynı sınırı
  kullandığını yaz.

### C — süreç modeli + ELF yükleme (ZORUNLU: C1; ÖNERİLEN: C2)
**C1 — çalıştır ve bekle:**
- Süreç tablosu: pid, durum (hazır / koşuyor / bitti / sonlandırıldı), adres alanı, fd tablosu,
  çalışma dizini, çıkış kodu ya da sonlanma nedeni, ebeveyn. Sabit kapasite (ör. 16); dolunca açık hata.
- Adres alanları **UEFI yolunda çerçeve ayırıcıdan** (D-755) ayrılır; sabit 57-72 MB adresleri yalnız PVH
  test yolunda kalabilir. Mevcut 6 test yuvası ve testleri bozulmaz (`calistir_kem_os_x86`).
- ELF yükleme (H9c): dosyayı bölüm D'nin dosya katmanından oku, `elf64_coz`, PT_LOAD'ları çerçevelere
  kopyala, bss'i sıfırla, sayfa izinleri segment bayraklarından, yığın + alt sınır sayfası, argc/argv'yi
  yığına belgelenmiş düzende koy (Türkçe UTF-8 argüman dahil).
- Kabuk komutu `calistir <yol> [arg...]`: süreci kurar, kullanıcı kipine geçer, süreç bitince çıkış
  kodunu ya da sonlanma nedenini basar; adres alanını, çerçeveleri ve açık dosyaları **geri verir**.
  Ölç: aynı programı 50 kez çalıştır — boş çerçeve sayısı ve heap başta/sonda.
**C2 — zaman paylaşımlı çok süreç (önerilen, C1 tam kapılıysa):**
- Kullanıcı kipinden gelen LAPIC zamanlayıcı kesmesinde süreç bağlamı (genel yazmaçlar, RIP, RSP,
  RFLAGS, CS/SS) kaydedilir, sıradaki hazır süreç yüklenir (adres alanı dahil). `baslat <yol> &`,
  `bekle <pid>`, `ps`. Kanıt: uzun hesap yapan bir program koşarken kabuk tuşlara yanıt verir; iki
  süreç aynı sanal adrese farklı değer yazar ve her biri kendi değerini okur.
- Bekleyen çağrılar (bölüm E) C2'de süreci uyutur, C1'de yoklamayla bekler — seçimini yaz.

### D — dosya çağrıları (ZORUNLU)
- Çekirdekte tek bir dosya katmanı: kabuğun bugünkü bağlama tablosu + yol çözümü + sektör yazıcısı bir
  API'nin arkasına alınsın; **kabuk komutları ve sistem çağrıları aynı kodu** çağırsın (D-407: aynı soruyu
  iki yerde ayrı yanıtlayan kod ayrışır).
- Çağrılar: `ac(yol, kip)` (oku / yaz / oluştur / sonuna ekle / kes), `kapat`, `oku(fd, buf, n)`,
  `yaz(fd, buf, n)`, `konumla(fd, ofset, nereden)`, `bilgi` (boy, dizin mi, zaman damgası),
  `dizin_oku` (biçimini belgele), `olustur_dizin`, `sil`, `sil_dizin`, `yeniden_adlandir`,
  `calisma_dizini_al/degistir`, `esitle`.
- Süreç başına fd tablosu (ör. 16; 0/1/2 konsol). Süreç bitince açık dosyalar kapanır ve yazma
  önbelleği boşaltılır (D-759'daki kirli bayrak kuralıyla tutarlı).
- Salt okunur bağlamaya ya da ESP'ye yazma → hata kodu (kabukla aynı kural ve gerekçe).

### E — olay ve zaman çağrıları (ZORUNLU: klavye + zaman; ÖNERİLEN: fare)
- `tus_bekle(olay)`: PS/2 halkasından ya da USB HID'den `TusOlayi` (kod, unicode, bas/bırak,
  değiştiriciler — yapıyı belgeye yaz). `fare_bekle(olay)` (dx, dy, düğmeler). `saat(yapi)` (RTC,
  D-759), `tik()`, `uyu(ms)`.
- Kabuk ve program aynı anda tuş beklemez: ön plandaki süreç girdiyi alır, bitince kabuk geri alır.

### F — kullanıcı programları (ZORUNLU) — `test/kullanici/*.kem`, KEMGU ile, her biri ayrı ELF
- `merhaba`: argümanları yazar, `cik(argc)` ile çıkar (çıkış kodu kabukta görünür).
- `kedi`: argümandaki dosyayı okuyup konsola yazar (Türkçe dosya adı ve içerik).
- `say`: satır / kelime / bayt sayar; büyük dosyada (≥200 KB) parça parça okur.
- `yazdene`: dosya oluşturur, yazar, kapatır, yeniden açıp okur ve karşılaştırır, yeniden adlandırır,
  siler; salt okunur bağlamada açma girişiminin hata kodunu doğrular.
- `tus`: 5 tuş olayı okuyup basar (kâhin QMP ile gönderir).
- `bolme`: sıfıra böler — çalışma zamanı denetimi (D-502) süreci sonlandırır, kabuk nedenini basar ve
  devam eder (C1'in "süreç hatası sistemi durdurmaz" kanıtı).
- Programlar diske bağımsız Python yazıcısıyla (`dalga12_imaj.py` / `fat_yazici.py`) `/bin/` altına konur.

### G — kâhinler ve kapılar (ZORUNLU)
- Yeni kapı(lar) `calistir_bm_x86_surec*`: QEMU + OVMF (+ PVH desteklediysen o da), C ve self-host
  derleyicileriyle (çekirdek ve programlar aynı derleyiciyle: hepsi C, hepsi self), her biri ≥10 açılış.
- Seri + ekran birebir, çıkış kodları, sonlanma nedenleri.
- Kapanış sonrası ana makinede `fsck.fat -n` + bağımsız Python okuyucu: `yazdene`'nin dosyaları doğru,
  diğer dosyalar değişmemiş, salt okunur diskler + ESP SHA256 aynı.
- Sayfa izinleri: QMP ile sayfa tablosu yürüyüşü (bağımsız Python) yüklenen her segmentin izinlerinin
  ELF bayraklarıyla eşleştiğini ve yığın altındaki sınır sayfasının eşlenmemiş olduğunu doğrular.
- Arayüz tutarlılık kapısı: belge tablosu ↔ çekirdek dağıtıcısı ↔ kullanıcı sarmalayıcıları.
- **Sabotajlar (en az şunlar, her biri gerçek QEMU'da kırmızı):** (1) argv düzenini kaydır → `merhaba`
  KIRMIZI; (2) bss sıfırlamayı atla → bss'e güvenen program KIRMIZI; (3) fd tablosu sınır denetimini
  kaldır → fd kapısı KIRMIZI; (4) salt okunur kuralını sistem çağrısı yolunda atla → `yazdene` salt okunur
  diske yazar → SHA256 KIRMIZI; (5) süreç bitince çerçeveleri geri verme → 50 tekrar kapısı KIRMIZI;
  (6) segment izinlerini bayraktan değil sabit ver → sayfa tablosu kapısı KIRMIZI; (7) kullanıcı ELF'ine
  bir çekirdek sembolü bağla → ELF denetimi KIRMIZI; (8) belgede bir numarayı değiştir → tutarlılık kapısı
  KIRMIZI; (9) `konumla` sonrası ofseti yanlış güncelle → `yazdene` KIRMIZI; (10) süreç bitince yazma
  önbelleğini boşaltma → kapanış sonrası fsck/içerik KIRMIZI.

### H — zaman kalırsa (sırayla)
1. C2, 2. `fare_bekle` + küçük bir fare programı, 3. `ps`, `sonlandir <pid>`, 4. programdan program
   başlatma (arm64 12'nin x86 karşılığı).

---

## 7. Kapılar (her işten sonra)
Yeni `calistir_bm_x86_surec*` kapıların + `calistir_kem_os_x86`, `calistir_bm_x86_uefi_pc`,
`calistir_bm_x86_uefi_kabuk`, `calistir_bm_x86_kabuk`, `calistir_bm_x86_disk`, `calistir_bm_x86_blok`,
`calistir_bm_x86_istisna`, `calistir_bm_x86_apic`, `calistir_bm_x86_ps2_irq`; `kem_gorev_cekirdek.kem`
değişirse `calistir_kem_os_arm` + `calistir_qemu_cekirdek`; sans-IO değişikliklerinde
`calistir_surucu_diff`, `calistir_surucu_kosum`, `calistir_chatgpt_kopru`. Bitirince `make test_tumu`
(rc PIPESTATUS ile, atlamalar sayılmış), `calistir_check_kapisi`, `calistir_baremetal_diff` dahil.
Her iş için sabotaj; aralıklı sınıflarda kaç açılışta kaç kırmızı olduğunu YAZ.

**Ölçüm tuzakları:**
- Derle-çalıştır döngüsünde ikiliyi ve **kullanıcı ELF'lerini** her koşumdan önce `rm -f` (bayat ELF
  diskte kalır, sahte yeşil verir); disk imajı her koşumda yeniden üretilir.
- `echo "$(cmd) $?"` yazma; `--checkdump` tanı bassa da 0 döner.
- QEMU üçlü hatada sessizce yeniden başlar: her açılışta `-no-reboot` + tek açılış işareti.
- Kullanıcı kipinden çekirdeğe geçişte yanlış TSS yığını üçlü hataya gider — açılış sayacını oku.
- Kâhin komutları anında gönderirse bekleme kusurları görünmez; en az bir senaryoda program tuş
  beklerken 8 sn hiçbir şey gönderme.
- ⚠ E4q: birleşik x86 birimi büyüdükçe self-host derleme süresi orantısız artıyor (Windows CI bütçesi
  240 sn). Birleşik birimlerin satır sayısını ve self-host `--llvm` süresini başta ve sonda yaz.
  Programlar ayrı derlendiği için birleşik birimi büyütmemeyi tercih et.

---

## 8. Bitirince
Draft PR `codex/surec-dalga13`, gövde `codex13_ilerleme.md`. `codex13_bulgular.md`: arayüz seçimi ve
gerekçesi, kullanıcı belleği düzeni (pencere, segmentler, yığın, argv), adres alanlarının UEFI'de nereden
ayrıldığı, kullanıcı çalışma zamanının istediği `kdl_*` sembolleri, ELF boyutları, süreç başına çerçeve
ve heap maliyeti, 50 tekrar ölçümü, C2 yapıldıysa bağlam düzeni; derleyici bulguları (C13-xxx);
tasarım seçimleri (VARSAYIM etiketiyle); kalanlar; birleşik birim boyutu ve derleme süresi.
Auto-merge AÇMA, merge ETME.

## 9. Bu dalganın DIŞINDA
- Hata yalıtımı test programları ve onları zorlayan denetimler (H9j / E3f): Claude.
- E4k (bölge modeli) ve E4q (derleme performansı): Claude.
- Boru hatları, sinyaller, paylaşılan bellek, dinamik bağlama, kullanıcı alanı sürücüler, SMP.
- Metin düzenleyici (H9f) ve fare programları (H9g): sonraki dalga.
- BIOS/GRUB (D-756), Secure Boot, ağ.
