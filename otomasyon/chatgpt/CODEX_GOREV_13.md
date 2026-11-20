# CODEX GÖREVİ 13 — kullanıcı programları: sistem çağrısı ABI'si, ELF yükleme, süreç modeli, dosya ve olay çağrıları, izolasyon

Dalga 1'in kuralları geçerlidir (`otomasyon/chatgpt/CODEX_GOREV.md` §2, §3, §5 — yeniden
oku), aşağıdaki **izin genişletmesi** dışında. Dalga 6'nın gerçek-QEMU kâhin kuralı da
geçerlidir (`CODEX_GOREV_6.md` §0). Soru sorma, durma; takılırsan (60 dk) kaydet ve geç.
Bu dalga öncekilerden büyüktür; §5'teki **zorunlu / önerilen** ayrımına uy: zorunlu kısım bitmeden
önerilene geçme, zaman bitince yarım kalan önerilen işi taslakta bırakma — ya kapılı bitir ya hiç
başlama.

---

## 0. Hedef

v1 bitiş tanımı (yol haritası H0f) madde 6: *"W^X + süreç yalıtımlı ELF kullanıcı programları"* ve
madde 7'nin ön koşulu (metin düzenleyici ve dosya gezgini birer KULLANICI programı olacak). Bugün
KEMGU-OS'ta her şey çekirdekte koşuyor: kabuk, dosya sistemi, sürücüler. Bu dalganın sonunda:

```text
> calistir /diskler/usb0p2/bin/kedi.elf /diskler/usb0p2/notlar/günlük.txt
...dosyanın içeriği...
[kedi.elf pid=3 cikis=0]
> calistir /diskler/usb0p2/bin/hata_adres.elf
[hata_adres.elf pid=4 SONLANDIRILDI: #PF adr=0x... (kullanici)]
>
```

Yani: **çekirdekten AYRI derlenmiş KEMGU programları** FAT32 diskten okunur, kendi adres alanlarına
W^X ile yüklenir, ring3'te koşar, çekirdekle **yalnız sistem çağrısı sınırından** konuşur (dosya,
konsol, klavye/fare olayı, saat, çıkış), hata yaparsa yalnız kendisi ölür ve kabuk devam eder.

Yol haritası: H9a (dosya syscall'ları), H9b (olay syscall'ları), H9c (ELF'i dosya sisteminden yükle),
H9d (süreç modeli), H9j (süreç izolasyonu kapısı), H12e'nin syscall tablosu belgesi. LOOP: E3b
(süreç modeli), E3c (süreç oluşturma), E3d (dosya tanımlayıcı tablosu), E3f (izolasyon kapısı).

---

## 1. Bugünkü durum (Claude ölçtü, 2026-11-19 — varsayma, yine de kendin doğrula)

**Ring3 (x86) VAR ama yalnız test biçiminde** (`runtime/mimari/x86_64/kem_ring3_hw.kem`, D-699/D-705):
- GDT: `0x18`/`0x20` DPL3 kod/veri, `0x28` 64 bit TSS; `kxr_tss_kur` + `ltr`. D-757'den beri TSS'te
  IST1/2/3 de var (`runtime/kdl_kesme.c` `kdl_ist_kur`).
- Giriş `kdl_x86_ring3_gir(rip, rsp)` (`boot/start_x86_64.S` ~416): iretq ile CPL3. Dönüş: ring3'te
  bir istisna → `isr_ortak` → `kdl_x86_r3_don` → çağırana **senkron** dönüş. Yani bugün "programı çalıştır,
  bitene/ölene kadar bekle" modeli var; zamanlayıcıyla ring3 süreçleri arasında geçiş YOK.
- Kullanıcı programları çekirdek kodunun içinde **elle yazılmış makine baytlarıyla** (`kxr_y8(pa+0, 184)`
  ...; llvm-mc ile üretilmiş) kuruluyor. Çekirdekten ayrı derlenmiş x86 kullanıcı ELF'i **YOK**.

**Sistem çağrısı ABI'si (x86, `kdl_syscall_stub`, start_x86_64.S ~390):** `int 0x80`; **numara `rax`,
1. argüman `rdi`, 2. argüman `rdx`** (stub `rdi→rsi`, `rax→rdi` taşır, `rdx` olduğu gibi 3. parametreye
kalır) → `kdl_syscall_isle(num, arg, arg2)`. **Yalnız İKİ argüman.** IDT 0x80 kapısı DPL3 (0xEE).
Dağıtıcı `runtime/mimari/x86_64/kem_gorev_hw.kem:65`; ortak servisler
`runtime/kem_gorev_cekirdek.kem` (arm64 ile PAYLAŞILIR):
- `kem_syscall_temel`: 10 `gettick`, 11 `getpid`, 13 `exit`, 14 `durum(pid)`.
- `kem_syscall_kul` (işaretçili, D-610 doğrulamalı): 5 `yaz(str)`, 17 `dosya_yaz` / 18 `dosya_oku`
  (tek dosyalı minifs), 26 `satir_oku(buf, n)`.
- arm64'te ayrıca 7 `satir`, 12 `spawn`, 24/25 ağ, 27 `saat` (`runtime/kem_gorev.kem:181`).
- İşaretçi doğrulama: kullanıcı penceresi `[0x54000000, 0x54200000)` (`KG_USER_MIN/MAX`, 2 MiB);
  `kg_user_yaz_ptr_gecerli`, `kg_user_oku_str_gecerli`.

**Adres alanları (`runtime/mimari/x86_64/kem_mmu_hw.kem` ~180-230):** `kmmu_as_olustur(i)` SABİT
fiziksel adreslerle: tablo arenası `0x3900000` (57 MB), alan başına 16 KiB, **en çok 6 alan**
(`KMMU_AS_AZAMI`), kullanıcı fiziksel penceresi `0x3C00000 + i*2MB` (60-72 MB), süreç VA'sı
`0x54000000` (PD indeksi 160, 512×4 KiB sayfa). Çekirdek PDPT/PD girdileri kopyalanır (gözetmen kalır),
U/S biti yalnız kullanıcının kendi tablolarında. **6 yuvanın hepsi bugün testlerde kullanılıyor**
(0/1 izolasyon, 2 W^X, 3 ring3, 4 fs, 5 rx). `kmmu_as_gec(cr3)` = CR3 yazımı. ⚠ Bu sabit adresler
PVH dönemine ait; UEFI yolunda fiziksel bellek UEFI haritasından gelir (D-755 çerçeve ayırıcı,
`runtime/mimari/x86_64/kem_bellek_harita.kem`). 57-72 MB bölgesinin UEFI haritasında boş olduğu
**ölçülmedi**.

**ELF:** `drivers/elf/elf64.kem` (sans-IO): ELF64 LE, ET_EXEC/ET_DYN, EM_X86_64 (62) ve AArch64 (183),
PT_LOAD sınırları, **W+X segment reddi**, hizalama, örtüşme, giriş noktası bir X segmentinde. Belleğe
YÜKLEMEZ.
- arm64'te çekirdekten ayrı kullanıcı ELF'i VAR: `test/ornekler/kem_kullanici.kem` →
  `linker/user-aarch64.ld` (0x54000000) → `build/kem_user.elf` (Makefile ~1290: ET_EXEC, AArch64,
  giriş 0x54000000 denetlenir; kernel sembolü bağlanmaz). Ama o program saf satıriçi asm — KEMGU
  çalışma zamanı (bölge, dizi, metin) kullanmıyor. **x86 kullanıcı linker betiği YOK.**

**Dosya sistemi (D-759):** `/diskler/{nvme,sata,usb}<n>p<b>` bağlamaları, sektör tabanlı FAT32
okuma/yazma, salt okunur bağlamalar, ESP hiç yazılmaz, kabuk komutları. Hepsi çekirdek içinde, kabuk
koduna gömülü — kullanıcı programının çağırabileceği bir VFS arayüzü YOK.

**Olaylar (D-757):** PS/2 klavye IRQ1 + fare IRQ12 kesme-güvenli halkalara yazar; USB HID klavye
yoklamalı; kabuk bunları tüketir.

**Heap (E4k — AÇIK, Claude bu dalgayla PARALEL çalışıyor):** çekirdek heap'i 28 MB; kabuk dosya
komutları komut başına 0,2-1,2 MB tahsis ediyor ve geri vermiyor (D-759 ölçümü: `mv` 1,18 MB,
`cp` 1,05 MB, `cat` 0,52 MB). ELF okumak ve süreç kurmak da tahsis edecek.

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

⚠ **Paralel derleyici işi:** Claude bu dalga sırasında E4k (bölge modeli) üzerinde çalışıyor; main'e
derleyici değişikliği (D-76x) girebilir. **Dalını dalga ortasında main ile BİRLEŞTİRME** — Claude
entegrasyon sırasında birleştirir. Heap ölçümlerini başta ve sonda yaz ki fark görülebilsin.

---

## 3. Okunacaklar
- `runtime/mimari/x86_64/kem_ring3_hw.kem` (tamamı), `boot/start_x86_64.S` (`kdl_syscall_stub`,
  `kdl_x86_ring3_gir`, `isr_ortak` ring3 dalı, `kdl_x86_r3_blok` düzeni, IRQ0 / LAPIC koçanları ve
  D-685 bağlam kaydı), `runtime/kdl_kesme.c` x86 IDT bloğu.
- `runtime/kem_gorev_cekirdek.kem` (syscall ortak servisleri, D-610 işaretçi doğrulayıcıları,
  zamanlayıcı/görev tablosu `KG_*`), `runtime/mimari/x86_64/kem_gorev_hw.kem`, `runtime/kem_gorev.kem`
  (arm64 dağıtıcı + Model A/B süreç notları).
- `runtime/mimari/x86_64/kem_mmu_hw.kem` (adres alanları, W^X testleri, NX biti `KXM_NX_BIT`),
  `runtime/mimari/x86_64/kem_bellek_harita.kem` (UEFI çerçeve ayırıcı), `runtime/mimari/x86_64/kem_apic_hw.kem`.
- `drivers/elf/elf64.kem` + testi; `test/ornekler/kem_kullanici.kem`, `linker/user-aarch64.ld`,
  Makefile `$(BUILD)/kem_user.elf` hedefi ve onu diske yazan satırlar (~1773).
- `test/ornekler/kem_os_x86_kabuk.kem` + `kem_os_x86_blok_ortak.kem` (D-759 VFS benzeri katman:
  bağlama tablosu, yol çözümü, `fat32_sektor_yaz`), `drivers/fs/*.kem`.
- `runtime/kem_heap.kem` (kullanıcı tarafında yeniden kullanılabilir mi — bölüm B).
- `tests/qemu/dalga12_kahin.py`, `dalga12_imaj.py`, `dalga12_fat.py`, `bm_istisna.py`, `ps2_irq_kahin.py`.
- `LOOP.md` D-699, D-705, D-610, D-686, D-757, D-759; yol haritası H9.

---

## 4. İzin genişletmesi (yalnız bu dosyalarda)
- **Yeni** `linker/user-x86_64.ld` (yalnız bu dosya; mevcut linker betiklerine DOKUNMA).
- `boot/start_x86_64.S`: YALNIZ (a) `kdl_syscall_stub` (argüman sayısını artırmak için) ve (b) yeni
  ring3 bağlam kaydet/yükle koçanları (zamanlayıcı kesmesi ring3'ten geldiğinde tam kullanıcı bağlamını
  kaydetmek). **Mevcut ring3 testi, kurtarılabilir-#PF, IST, PVH ve UEFI girişleri birebir korunur**;
  `calistir_kem_os_x86` ve `calistir_bm_x86_istisna` bunu ölçer.
- `runtime/kdl_kesme.c`: YALNIZ `#if defined(__x86_64__)` IDT bloğu (ikili modda düzenle).
- `runtime/kem_gorev_cekirdek.kem`: syscall tablosu ve doğrulayıcılar — ⚠ **arm64 ile paylaşılır**;
  arm64 numaraları ve davranışı değişmez, `calistir_kem_os_arm` ve `calistir_qemu_cekirdek` yeşil kalır.
- `runtime/mimari/x86_64/*.kem` (yeni dosyalar dahil), yeni `runtime/kullanici/x86_64/*.kem`
  (kullanıcı tarafı çalışma zamanı — bölüm B).
- `drivers/**` saf kalır (sans-IO). Yeni modüller serbest (ör. `drivers/surec/`, `drivers/vfs/`).
- `test/ornekler/kem_os_x86_*.kem`, yeni `test/kullanici/*.kem` (kullanıcı programları),
  `test/bm_x86_*_harness.sh`, `tests/qemu/*`, `tools/codex/*`.
- Makefile (yeni hedefler + `test_tumu` + `.PHONY` + x86 CAT listesi), `test/check_kapisi.sh` ve
  `test/baremetal_diff_harness.sh` sınıflandırma satırları.
- **Yeni** `belgeler/KEMGU_OS_Syscall_ABI.md` (bölüm A).
- YASAK: `src/`, `selfhost/`, `boot/efi/`, diğer `linker/` dosyaları, diğer runtime C dosyaları.
  Derleyici hatası → `codex13_bulgular.md`'ye C13-xxx + küçük tekrar üretimi (C ve self-host komutu
  ve beklenen/gerçek sonuç), dolan. Muafiyet listelerine (`bolge_operand ENV_MUAF`, `check_genis MUAF`,
  `yapi_diff`, `stdlib_ithal MUAF`) satır EKLEME.

---

## 5. Tasarım ilkeleri (pazarlık dışı)
1. **Tek arayüz:** kullanıcı programı çekirdek sembolüne BAĞLANMAZ. Kanıt: kullanıcı ELF'inde
   `llvm-nm -u` boş (ya da yalnız kendi çalışma zamanının tanımladığı semboller) ve çekirdek
   sembol adlarından hiçbiri ELF'te yok.
2. **Her işaretçi doğrulanır** (D-610 deseni): kullanıcıdan gelen her adres + uzunluk, o sürecin
   kullanıcı penceresinde ve taşmasız olmalı; metin argümanları azami uzunlukla taranır. Geçersiz
   → hata kodu, çekirdek belleği ne okunur ne yazılır. Çekirdek adresi kullanıcıya hiç dönmez.
3. **W^X:** kod segmentleri R-X, veri RW+NX, yığın RW+NX, hiçbir sayfa W+X değil (ELF ayrıştırıcısı
   zaten W+X segmenti reddediyor; sayfa tablosunda da ölç). Yığının altında **koruma sayfası**
   (eşlenmemiş) — taşma #PF olur, sessiz bozulma olmaz.
4. **Hata = yalnız o süreç ölür:** kullanıcı kaynaklı her istisna (#PF, #GP, #UD, #DE ...) süreci
   öldürür, nedeni seri + ekrana yazılır, kabuk devam eder. Çekirdek kaynaklı istisna bugünkü panik
   yolunda kalır (D-757 ekranı). İkisi KARIŞMAZ: CS RPL ile ayrılır (bugünkü D-699 kuralı).
5. **Sessiz kısaltma YOK:** fd tablosu doluysa, yol çok uzunsa, ELF pencereye sığmıyorsa → açık hata.
6. **Salt okunurluk ve ESP koruması** (D-759) sistem çağrısı yolunda da geçerlidir: kullanıcı programı
   kabuğun yazamadığı hiçbir yere yazamaz.
7. **ABI belgelenir ve sürümlenir:** numara, yazmaç, dönüş, hata kodu tablosu `belgeler/KEMGU_OS_Syscall_ABI.md`'de;
   kullanıcı çalışma zamanı ve çekirdek dağıtıcısı BU belgeden üretilmiş gibi birebir tutarlı olmalı
   (tutarlılığı bir kapı ölçsün: belge tablosu ↔ dağıtıcıdaki numaralar ↔ kullanıcı sarmalayıcıları).

---

## 6. İş listesi

### A — Sistem çağrısı ABI v2 (ZORUNLU)
- Argüman sayısı: dosya çağrıları üç argüman ister (`oku(fd, buf, n)`). İki yol var — seç, gerekçesini
  yaz, ölç: (a) stub'ı genişlet (ör. `rax` numara, `rdi`/`rsi`/`rdx`/`r10` argümanlar — Linux x86_64
  geleneğine yakın; ⚠ bugünkü testler 2. argümanı `rdx`'te veriyor, uyumluluk ya korunur ya da eski
  testler aynı commit'te güncellenip yeşil kalır), (b) stub'a dokunmadan üçüncü ve sonraki argümanları
  doğrulanmış bir kullanıcı yapısı işaretçisiyle geç.
- Numaralar: mevcutlar (5, 10, 11, 13, 14, 17, 18, 26 x86'da; 7, 12, 24, 25, 27 arm64'te) **anlamını
  değiştirmeden** korunur. Yeni çağrılar ayrı bir aralıkta (ör. 40-79). Bilinmeyen numara `-1` değil,
  belgelenmiş bir hata kodu (ör. `-HATA_NOSYS`); hata kodları negatif ve tablolu.
- Dönüş: `rax`. Çağrı sırasında hangi yazmaçların korunduğu belgede.
- `belgeler/KEMGU_OS_Syscall_ABI.md`: tablo (numara, ad, argümanlar, dönüş, hatalar, işaretçi
  kuralları, hangi mimaride var) + kullanıcı belleği düzeni (bölüm C) + argv düzeni.

### B — kullanıcı tarafı çalışma zamanı + linker betiği + derleme (ZORUNLU)
- `linker/user-x86_64.ld`: ET_EXEC, giriş `_start`, adres kullanıcı penceresinde; kod / salt okunur
  veri / RW veri / bss ayrı segmentlerde ve **sayfa hizalı** (W^X sayfa düzeyinde uygulanabilsin).
- `runtime/kullanici/x86_64/`: `int 0x80` sarmalayıcıları (`sis_yaz`, `sis_cik`, `sis_ac`, `sis_oku`,
  ...) ve programın KEMGU olarak yazılabilmesi için gereken en küçük çalışma zamanı. **ÖLÇ:** KEMGU'nun
  ürettiği IR hangi `kdl_*` sembollerini istiyor (bölge, dizi, metin, panik, memcpy)? `runtime/kem_heap.kem`
  kullanıcı ELF'ine **olduğu gibi** derlenebiliyor mu (heap tabanı kullanıcı penceresinde, `__heap_start`
  linker betiğinden)? Derlenemiyorsa nedenini yaz ve en küçük kullanıcı karşılığını yaz; çekirdek
  dosyasını kullanıcı ihtiyacı için DEĞİŞTİRME (kopyala-değiştir değil, gerçekten ortak olabiliyorsa ortak).
- Kullanıcı tarafı `kdl_panik` / dizi sınır ihlali → çekirdeğe `cik(kod)` ile anlamlı bir çıkış
  (ör. 134), sessiz döngü değil.
- Makefile: `build/kullanici/<ad>.elf` (her `test/kullanici/<ad>.kem` için), ELF denetimleri arm64
  hedefinin aynası: ET_EXEC, EM_X86_64, giriş adresi, W+X segment yok, `llvm-nm` kernel sembolü yok.
- ⚠ **Pencere boyutu:** kullanıcı penceresi bugün 2 MiB (kod+veri+heap+yığın). KEMGU programı +
  çalışma zamanı sığıyor mu ÖLÇ. Sığmıyorsa pencereyi büyütmek bir tasarım kararıdır: büyüklüğü,
  gerekçeyi ve D-610 doğrulayıcılarının da aynı sınırı kullandığını kapıyla göster.

### C — süreç modeli + ELF yükleme (ZORUNLU: C1; ÖNERİLEN: C2)
**C1 — senkron çalıştırma (zorunlu):**
- Süreç tablosu: pid, durum (hazır/koşuyor/bitti/öldürüldü), adres alanı (CR3), fd tablosu, çalışma
  dizini, çıkış kodu / ölüm nedeni, ebeveyn. Sabit kapasite (ör. 16) — dolunca açık hata.
- Adres alanları **UEFI yolunda çerçeve ayırıcıdan** (D-755) ayrılır; sabit 57-72 MB adresleri yalnız
  PVH test yolunda kalabilir — UEFI'de o bölgenin boş olduğunu ölçmeden KULLANMA. Mevcut 6 test yuvası
  ve testleri bozulmaz (`calistir_kem_os_x86`).
- ELF yükleme (H9c): dosyayı VFS'ten oku (bölüm D'nin çekirdek tarafı), `elf64_coz`, PT_LOAD'ları
  çerçevelere kopyala, `bss` sıfırla, sayfa izinleri segment bayraklarından (R-X / RW+NX), yığın +
  koruma sayfası, argc/argv'yi yığına belgelenmiş düzende yerleştir (Türkçe UTF-8 argüman dahil).
- Kabuk komutu `calistir <yol> [arg...]`: süreci kurar, ring3'e girer, süreç `cik` ya da ölüm ile
  biter, kabuk çıkış kodunu/nedenini basar ve adres alanını + çerçeveleri + açık fd'leri **geri verir**
  (sızıntıyı ölç: aynı programı 50 kez çalıştır, boş çerçeve sayısı ve heap başta/sonda).
**C2 — kesmeli çok süreç (önerilen, C1 tam kapılıysa):**
- Ring3'ten gelen LAPIC zamanlayıcı kesmesinde tam kullanıcı bağlamı (genel yazmaçlar, RIP, RSP,
  RFLAGS, CS/SS) kaydedilir, sıradaki hazır süreç yüklenir (CR3 dahil). `baslat <yol> &` arka planda;
  `bekle <pid>`; `ps`. Kanıt: sonsuz döngüdeki bir kullanıcı programı varken kabuk tuşlara yanıt verir;
  iki süreç aynı sanal adrese farklı değer yazar, ikisi de kendi değerini görür.
- Engelleyen çağrılar (bölüm E) C2'de süreci uyutur; C1'de yoklamayla bekler — hangisini seçtiğini yaz.

### D — dosya sistem çağrıları (ZORUNLU)
- Çekirdekte VFS sınırı: kabuğun bugün kullandığı bağlama tablosu + yol çözümü + sektör yazıcısı tek
  bir çekirdek API'si arkasına alınsın ki **kabuk komutları ile sistem çağrıları aynı kodu** çağırsın
  (aynı soruyu iki yerde ayrı yanıtlayan kod ayrışır — D-407).
- Çağrılar: `ac(yol, kip)` (okuma / yazma / oluştur / sonuna ekle / kes), `kapat(fd)`, `oku(fd, buf, n)`,
  `yaz(fd, buf, n)`, `konumla(fd, ofset, nereden)`, `bilgi(yol veya fd, yapi_ptr)` (boy, dizin mi, zaman
  damgası), `dizin_oku(fd veya yol, buf, n)` (biçimi belgele), `olustur_dizin`, `sil`, `sil_dizin`,
  `yeniden_adlandir`, `calisma_dizini_al/degistir`, `esitle` (sync).
- fd tablosu süreç başına (ör. 16 yuva; 0/1/2 konsol); süreç ölünce açık fd'ler kapanır ve yazma
  önbelleği boşaltılır (kirli bayrak kuralı D-759 ile tutarlı).
- Salt okunur bağlamaya ya da ESP'ye yazma → hata kodu (kabuktaki gerekçe metniyle aynı kural).

### E — olay ve zaman çağrıları (ZORUNLU: klavye + zaman; ÖNERİLEN: fare)
- `tus_bekle(olay_ptr)`: D-757 halkasından (PS/2) ya da USB HID'den `TusOlayi` (kod, unicode,
  bas/bırak, değiştiriciler — yapıyı ABI belgesine yaz). `fare_bekle(olay_ptr)` (dx, dy, düğmeler).
  `saat(yapi_ptr)` (RTC, D-759), `tik()`, `uyu(ms)`.
- Kabuk ile kullanıcı programı aynı anda tuş beklemez: ön plandaki süreç girdiyi alır, kabuk
  süreç bitince geri alır. Kuralı yaz.

### F — kullanıcı programları (ZORUNLU) — `test/kullanici/*.kem`, KEMGU dilinde, ayrı ELF
- `merhaba`: argv'yi yazar, `cik(argc)` — çıkış kodu kabukta görünür.
- `kedi`: argümandaki dosyayı `ac/oku/yaz(1,...)` ile konsola döker (Türkçe ad + içerik).
- `say`: satır / kelime / bayt sayar (`wc` benzeri); büyük dosyada (≥200 KB) parça parça okur.
- `yazdene`: yeni dosya oluşturur, yazar, kapatır, yeniden açıp okur, karşılaştırır, `yeniden_adlandir`,
  `sil`; salt okunur bağlamada açma girişiminin hata kodunu doğrular.
- `tus`: 5 tuş olayı okuyup basar (kâhin QMP ile gönderir).
- **Hata yalıtımı test programları (H9j — negatif testler; her biri kasıtlı olarak HATALI bir program,
  işletim sisteminin hatayı doğru sınırladığını ölçer):**
  `hata_adres` (kullanıcı penceresi dışındaki, eşli ama gözetmen olan bir adresi okur → beklenen: #PF,
  yalnız bu süreç sonlanır), `hata_parametre` (bir sistem çağrısına pencere dışı adres verir → beklenen:
  belgelenmiş hata kodu döner, süreç DEVAM eder, çekirdek verisi değişmez), `hata_wx` (kendi kod sayfasına
  yazar → beklenen: #PF, W^X doğrulaması), `hata_yigin` (sınırsız özyineleme → beklenen: koruma sayfası
  #PF), `hata_komut` (ayrıcalıklı `hlt` → beklenen: #GP), `hata_dongu` (C2 varsa: hiç bitmeyen döngü,
  beklenen: kabuk yanıt vermeye devam eder; C2 yoksa bu testi ATLA ve nedenini yaz).
- Programlar diske bağımsız Python yazıcısıyla (`dalga12_imaj.py` / `fat_yazici.py`) `/bin/` altına konur.

### G — kâhinler ve kapılar (ZORUNLU)
- Yeni kapı(lar) `calistir_bm_x86_surec*`: QEMU + OVMF (+ PVH desteklediysen o da), C ve self-host
  derleyicileriyle (çekirdek VE kullanıcı programları ikisiyle de derlenir — dört kombinasyon değil,
  iki: hepsi C, hepsi self), her biri ≥10 açılış.
- Seri + ekran birebir (mevcut ayna/piksel kâhinleri), çıkış kodları, ölüm nedenleri.
- Kapanış sonrası ana makinede `fsck.fat -n` + bağımsız Python okuyucu: `yazdene`'nin yazdığı dosyalar
  doğru, `hata_*` testleri hiçbir dosyayı değiştirmemiş, salt okunur diskler + ESP SHA256 değişmemiş.
- İzolasyon: QMP ile fiziksel bellek dökümü — `hata_parametre` sonrası çekirdek veri bölgesi ve başka
  sürecin sayfaları değişmemiş.
- ABI tutarlılık kapısı: belge tablosu ↔ çekirdek dağıtıcısı ↔ kullanıcı sarmalayıcıları (numara ve
  argüman sayısı).
- **Sabotajlar (en az şunlar, her biri gerçek QEMU'da kırmızı):** (1) işaretçi doğrulamasını kapat →
  `hata_parametre` beklenen hata kodunu almaz ve bellek dökümü farklılaşır → KIRMIZI; (2) kod sayfasına
  W ver → `hata_wx` beklenen #PF'yi vermez → KIRMIZI; (3) koruma sayfasını eşle → `hata_yigin` beklenen
  #PF'yi vermez → KIRMIZI;
  (4) süreç geçişinde CR3'ü değiştirme (C2) ya da ELF'i yanlış alana yükle → izolasyon KIRMIZI;
  (5) fd sınır denetimini kaldır → KIRMIZI; (6) salt okunur kuralını syscall yolunda atla →
  `yazdene` salt okunur diske yazar → SHA256 KIRMIZI; (7) süreç bitince çerçeveleri geri verme →
  50 tekrar sızıntı kapısı KIRMIZI; (8) argv düzenini kaydır → `merhaba` KIRMIZI;
  (9) kullanıcı ELF'ine bir çekirdek sembolü bağla → ELF denetimi KIRMIZI; (10) ABI belgesinde bir
  numarayı değiştir → tutarlılık kapısı KIRMIZI.

### H — zaman kalırsa (önerilen, sırayla)
1. C2 (kesmeli çok süreç), 2. `fare_bekle` + küçük bir fare programı, 3. `ps`, `oldur <pid>`,
4. kullanıcı programı içinden `calistir` (spawn çağrısı, arm64 12'nin x86 karşılığı).

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
  diskte kalır ve sahte yeşil verir); disk imajı her koşumda YENİDEN üretilir.
- `echo "$(cmd) $?"` yazma (`$?` önce açılan `$(...)`'ındır); `--checkdump` tanı bassa da 0 döner.
- QEMU üçlü hatada sessizce yeniden başlar: her açılışta `-no-reboot` + tek açılış işareti.
- Ring3'ten çekirdeğe geçişte yanlış TSS.RSP0 / IST üçlü hataya gider — "kapı geçti" demeden önce
  açılış sayacını oku.
- Kâhin komutları ANINDA gönderirse boşta bekleme ve engelleyen çağrı kusurları görünmez; en az bir
  senaryoda süreç tuş beklerken 8 sn hiçbir şey gönderme.
- ⚠ E4q: birleşik x86 birimi büyüdükçe self-host derleme süresi orantısız artıyor (Windows CI
  `baremetal_diff` bütçesi 240 sn). Birleşik birimlerin satır sayısını ve self-host `--llvm` süresini
  başta ve sonda ölç ve yaz. Kullanıcı programları ayrı derlendiği için bu dalga birleşik birimi
  büyütmek ZORUNDA DEĞİL — büyütmemeyi tercih et.

---

## 8. Bitirince
Draft PR `codex/surec-dalga13`, gövde `codex13_ilerleme.md`. `codex13_bulgular.md`:
- ölçülen davranışlar: ABI seçimi ve gerekçesi, kullanıcı belleği düzeni (pencere boyutu, segment
  yerleşimi, yığın + koruma sayfası, argv düzeni), adres alanlarının UEFI'de nereden ayrıldığı,
  kullanıcı çalışma zamanının hangi `kdl_*` sembollerini istediği, kullanıcı ELF boyutları,
  süreç başına çerçeve/heap maliyeti ve 50 tekrar sızıntı ölçümü, C2 yapıldıysa bağlam düzeni;
- derleyici bulguları (C13-xxx, küçük tekrar üretimiyle);
- tasarım seçimleri (VARSAYIM etiketiyle), kalanlar, birleşik birim boyutu + derleme süresi.
Auto-merge AÇMA, merge ETME.

## 9. Bu dalganın DIŞINDA (kaydedildi, yapma)
- E4k (bölge modeli) ve E4q (derleme performansı): Claude.
- Boru hatları, sinyaller, paylaşılan bellek, dinamik bağlama, kullanıcı alanı sürücüler, SMP.
- Metin düzenleyici (H9f) ve fare programları (H9g): E4k'ye ve bu dalganın ABI'sine bağlı — sonraki dalga.
- BIOS/GRUB (D-756), Secure Boot, ağ.
