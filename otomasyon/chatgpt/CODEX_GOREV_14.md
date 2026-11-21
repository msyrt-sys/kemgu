# CODEX GÖREVİ 14 — çok süreç, büyük programlar, ekran ve fare çağrıları, metin düzenleyici, dosya gezgini, uçtan uca v1 senaryosu

Dalga 1'in kuralları geçerlidir (`otomasyon/chatgpt/CODEX_GOREV.md` §2, §3, §5 — yeniden
oku), aşağıdaki **izin genişletmesi** dışında. Dalga 6'nın gerçek-QEMU kâhin kuralı da
geçerlidir (`CODEX_GOREV_6.md` §0). Soru sorma, durma; takılırsan (60 dk) kaydet ve geç.

## ⏱ Bu dalga UZUNDUR — çalışma biçimi
- Dalga 13 bir saatte bitti ve "önerilen" işler yapılmadan kaldı. Bu dalgada **zorunlu / önerilen ayrımı
  YOK: §6'daki sekiz bölümün HEPSİ zorunludur ve sırayla yapılır.** Beklenen süre birkaç saattir (6+ saat
  normaldir). Bir bölümü bitirince **durma, raporlama yapıp bekleme — bir sonraki bölüme geç.**
- Her bölümün sonunda: (1) o bölümün kapıları + sabotajları yeşil/kırmızı beklendiği gibi, (2) ayrı bir
  commit (`SURUCU(codex): Dalga 14 bölüm N — ...`), (3) `codex14_ilerleme.md`'ye kısa not (ne yapıldı,
  ölçümler, sabotaj sonuçları, süre). Böylece dalga yarıda kesilse bile biten bölümler kullanılabilir.
- Gerçekten takılırsan (60 dk ilerleme yok): durumu `codex14_bulgular.md`'ye yaz, o bölümün yapılabilen
  kısmını kapılı bırak ve **sonraki bölüme geç**. Bütün dalgayı tek bir engel yüzünden bırakma.
- Draft PR'ı **yalnız sekiz bölüm bittikten sonra** (ya da her biri için takılma notu yazıldıktan sonra) aç.

---

## 0. Hedef

Dalga 13 (D-761) çekirdekten ayrı derlenen KEMGU programlarını çalıştırmayı kurdu: sistem çağrısı ABI v2
(`belgeler/KEMGU_OS_Syscall_ABI.md`), kullanıcı çalışma zamanı, ELF yükleme, ortak dosya katmanı, altı
program. Ama yalnız **tek süreç** (çalıştır ve bekle), ELF ≤ 64 KiB, 2 MiB pencere, ekrana çizme ve fare
çağrısı yok. Bu dalganın sonunda v1 bitiş tanımının (yol haritası H0f) son iki maddesi QEMU'da çalışır:

> (6) kullanıcı programları · (7) en az iki program: **metin düzenleyici + tıklanabilir dosya gezgini**

ve bütün v1 senaryosu tek bir QEMU açılışında baştan sona koşar; gerçek bir PC'ye yazılacak USB imajı ve
fiziksel deneme yönergesi hazır olur.

Yol haritası: H9b (fare), H9d (çok süreç), H9e (kabuk), H9f (`duzenle`), H9g (`dosyalar`), H9i (uçtan uca
senaryo), H11a (USB imaj üretici), H11b (fiziksel donanım kontrol listesi). LOOP: E3b, E3c, E3e.

---

## 1. Bugünkü durum (Claude ölçtü — varsayma, yine de kendin doğrula)

**Dalga 13 (D-761) çıktıları:**
- `belgeler/KEMGU_OS_Syscall_ABI.md`: ABI v2 — `int 0x80`, numara `rax`, argümanlar `rdi`, `rdx`, `rsi`,
  dönüş `rax`. Çağrılar 40-57: `abi_surumu`, `ac`, `kapat`, `oku`, `yaz`, `konumla`, `bilgi`, `dizin_oku`,
  `olustur_dizin`, `sil`, `sil_dizin`, `yeniden_adlandir`, `calisma_dizini_al/degistir`, `esitle`,
  `tus_bekle` (55), `saat` (56), `uyu` (57). 40-79 x86 v2'ye ayrılmış; 58-79 boş. Hata kodları tablosu orada.
- Bellek düzeni: pencere `[0x54000000, 0x54200000)`; ELF segmentleri `[0x54000000, 0x54100000)`; heap
  `[0x54100000, 0x541ef000)`; eşlenmemiş sınır sayfası `0x541ef000`; yığın `[0x541f0000, 0x54200000)`;
  argc/argv `0x541ff000`. **Diskteki ELF en çok 64 KiB** (`dosya siniri` hatası).
- Süreç: `runtime/mimari/x86_64/kem_surec_hw.kem` (`kxs_yuva_ayir/serbest`, `kxs_esle`, `kxs_aralik`,
  `kxs_hizmet`, `kxs_ring3`, `kxs_yukle`, `kxs_yigin` ...). **Süreç tablosu kapasitesi bir canlı süreç**;
  UEFI çerçeve ayırıcısından açılışta alınan 1.032 sayfalık havuz, iki 516 sayfalık yuva sırayla.
  Kullanıcı kipine giriş senkron; zamanlayıcıyla süreçler arası geçiş YOK.
- Ortak dosya katmanı: `test/ornekler/kem_os_x86_vfs_ortak.kem` (kabuk komutları ve sistem çağrıları aynı kod).
- Kullanıcı çalışma zamanı: `runtime/kullanici/x86_64/kem_basla.kem` (`_start`, heap kurulumu),
  `kem_sis.kem` (sarmalayıcılar); `linker/user-x86_64.ld`. `runtime/kem_heap.kem` değişikliksiz kullanılıyor.
- Programlar: `test/kullanici/{merhaba,kedi,say,yazdene,tus,bolme}.kem`.
- Kapılar: `calistir_bm_x86_surec`, `calistir_bm_x86_surec_abi`; kâhinler `tests/qemu/surec_*.py`.
- Kullanıcı kipinde kayan nokta/SIMD kapalı (`#NM`).

**Diğer altyapı:** ekran — `runtime/mimari/x86_64/kem_ekran_bga.kem` / `kem_ekran_gop.kem` (GOP 1280x800,
8x16 Türkçe font `drivers/ekran/font_tr16.kem`, kâhin `tests/qemu/psf_kahin.py`); fare — PS/2 IRQ12 halkası,
paket çözümleyici `drivers/ps2/fare.kem`, USB HID fare `drivers/usb/hid_fare.kem`, kabukta save-under imleç
(D-757); LAPIC zamanlayıcı UEFI yolunda (D-757); FAT32 sektör yazıcısı dosya başına 64 KiB değiştirme
planı sınırlı (D-759).

**Heap (E4k — açık, Claude'da):** çekirdek heap'i 28 MB; kabuk komutları ve dosya çağrıları geri
verilmeyen tahsis yapıyor (dalga 13: 50 tekrarda kabuk heap'i 4,7 → 16 MB). Uzun senaryolar bu bütçeye
takılabilir: ölç, yaz, gerekirse senaryoyu kısalt — derleyiciyi düzeltmeye çalışma.

---

## 2. Ön koşul ve çalışma alanı
```bash
cd /home/msyrt/Desktop/kemgu && git fetch origin
git log origin/main --oneline | grep D-762     # yoksa 5 dk'da bir, en çok 2 sa; sonra DUR, kaydet
git worktree add ../kemgu-codex14 -b codex/program-dalga14 origin/main && cd ../kemgu-codex14
make -j8 build/kemgu build/codegen build/kdl_runtime.o
make calistir_bm_x86_surec calistir_bm_x86_surec_abi calistir_bm_x86_uefi_pc calistir_kem_os_x86   # taban
date > otomasyon/chatgpt/codex14_ilerleme.md
```
Tek dal, tek draft PR. ⚠ Claude bu dalga sırasında derleyicide (E4k) çalışabilir; **dalını dalga ortasında
main ile birleştirme** — Claude entegrasyonda birleştirir.

---

## 3. Okunacaklar
- `belgeler/KEMGU_OS_Syscall_ABI.md` (tamamı), `otomasyon/chatgpt/codex13_bulgular.md` (dalga 13'ün
  ölçümleri ve VARSAYIM'ları), `runtime/mimari/x86_64/kem_surec_hw.kem`, `test/ornekler/kem_os_x86_vfs_ortak.kem`,
  `test/ornekler/kem_os_x86_kabuk.kem`, `runtime/kullanici/x86_64/*.kem`, `linker/user-x86_64.ld`, `test/kullanici/*.kem`.
- `boot/start_x86_64.S` (`kdl_syscall_stub`, LAPIC zamanlayıcı koçanı, D-685 bağlam kaydı), `runtime/kdl_kesme.c`
  x86 IDT bloğu, `runtime/mimari/x86_64/kem_apic_hw.kem`, `kem_ps2_hw.kem`, `kem_ekran_gop.kem`, `kem_ekran_bga.kem`.
- `drivers/ps2/fare.kem`, `drivers/usb/hid_fare.kem`, `drivers/girdi/*.kem`, `drivers/ekran/*.kem`.
- `tests/qemu/surec_*.py`, `dalga12_imaj.py`, `uefi_imaj.py`, `psf_kahin.py`, `ps2_irq_kahin.py`.
- Yol haritası `belgeler/KEMGU_OS_Donanim_Yol_Haritasi.md` H0f, H9, H11; `belgeler/ARM64_Fiziksel_Donanim_Kontrol_Listesi.md`
  (H11b belgesinin desen örneği).

---

## 4. İzin genişletmesi (yalnız bu dosyalarda)
- `boot/start_x86_64.S`: YALNIZ sistem çağrısı koçanı ve kullanıcı kipinden gelen zamanlayıcı kesmesinde
  süreç bağlamını kaydedip yükleyen koçanlar. PVH/UEFI girişleri, IST ve mevcut istisna yolları birebir korunur.
- `runtime/kdl_kesme.c`: YALNIZ `#if defined(__x86_64__)` IDT bloğu (ikili modda düzenle).
- `runtime/kem_gorev_cekirdek.kem`: sistem çağrısı tablosu — arm64 ile paylaşılır; arm64 davranışı değişmez.
- `linker/user-x86_64.ld` (pencere/segment düzeni değişirse).
- `runtime/mimari/x86_64/*.kem`, `runtime/kullanici/x86_64/*.kem` (yeni dosyalar dahil).
- `drivers/**` saf (sans-IO); yeni modüller serbest (ör. `drivers/duzenle/` metin tamponu, `drivers/arayuz/`).
- `test/ornekler/kem_os_x86_*.kem`, `test/kullanici/*.kem`, `test/bm_x86_*_harness.sh`, `tests/qemu/*`, `tools/codex/*`.
- Makefile (yeni hedefler + `test_tumu` + `.PHONY` + x86 CAT listeleri), `test/check_kapisi.sh`,
  `test/baremetal_diff_harness.sh` sınıflandırma satırları.
- `belgeler/KEMGU_OS_Syscall_ABI.md` (yeni çağrılar), yeni `belgeler/X86_Fiziksel_Donanim_Kontrol_Listesi.md`.
- YASAK: `src/`, `selfhost/`, `boot/efi/`, diğer `linker/` dosyaları, diğer runtime C dosyaları. Derleyici
  hatası → `codex14_bulgular.md`'ye C14-xxx + küçük tekrar üretimi (C ve self-host komutu, beklenen/gerçek
  sonuç), dolan. Muafiyet listelerine satır EKLEME.

---

## 5. Tasarım ilkeleri (dalga 13'ünkiler geçerli, ek olarak)
1. ABI geriye uyumlu büyür: mevcut 5-57 numaraları ve davranışları değişmez; yeni çağrılar 58-79'da;
   belge ↔ çekirdek dağıtıcısı ↔ kullanıcı sarmalayıcıları tutarlılık kapısı her yeni çağrıyı kapsar.
2. Kullanıcı programı yalnız sistem çağrısı kullanır; ekran/fare/klavye donanımına doğrudan dokunmaz.
3. Sessiz kısaltma yok; her tablo/kapasite sınırı belgelenmiş hata kodu döner.
4. Kabuk ve kullanıcı programları aynı sürücü/dosya katmanını kullanır (D-407: aynı soruyu iki yerde ayrı
   yanıtlayan kod ayrışır).
5. Ekran ve girdi bir anda tek bir ön plan sürecine aittir; kabuğa geri dönüş açık ve ölçülebilir.

---

## 6. İş listesi — sekiz bölüm, HEPSİ zorunlu, sırayla

### Bölüm 1 — zaman paylaşımlı çok süreç (H9d tamamı)
- Süreç tablosu kapasitesi en az 8. Süreç belleği sabit iki yuva yerine **süreç başına** UEFI çerçeve
  ayırıcısından ayrılır ve çıkışta geri verilir (havuz yaklaşımı kalacaksa gerekçesini ölçümle yaz).
- Kullanıcı kipinden gelen LAPIC zamanlayıcı kesmesinde tam süreç bağlamı kaydedilir (genel yazmaçlar,
  RIP, RSP, RFLAGS, CS/SS, adres alanı); sıradaki hazır süreç yüklenir. Hiç hazır süreç yoksa `hlt`.
- Bekleyen çağrılar (`tus_bekle`, `uyu`, yeni `bekle`) süreci uyutur; olay gelince uyanır (yoklama yok).
- Yeni çağrılar (numaraları belgeye ekle): `baslat(yol, argv)` → pid; `bekle(pid, sonuc_ptr)` (çıkış kodu
  ya da sonlanma nedeni); `pid` mevcut; `surec_listesi(tampon, n)` (`ps` için).
- Kabuk: `baslat <yol> [arg] &` arka planda; `bekle <pid>`; `ps` (pid, durum, ad, süre); `sonlandir <pid>`.
  Ön plan kuralı: girdi ve ekran yalnız ön plan sürecine; arka plan süreçleri konsola yazabilir (satır satır,
  karışmasın — kuralı yaz).
- Kanıtlar: (a) uzun hesap yapan bir program (`hesap`) arka planda koşarken kabuk tuşlara yanıt verir ve
  `merhaba` ön planda çalışır; (b) iki süreç aynı sanal adrese farklı değer yazar ve her biri kendi değerini
  okur; (c) 100 kez `baslat` + `bekle` döngüsünden sonra boş çerçeve sayısı başlangıçla aynı.
- Sabotajlar: bağlam kaydında bir yazmacı atla → `hesap` yanlış sonuç; adres alanını geçişte değiştirme →
  (b) kırmızı; çıkışta çerçeveleri geri verme → (c) kırmızı; uyuyan süreci hiç uyandırma → `uyu` testi asılır
  (zaman aşımıyla kırmızı).

### Bölüm 2 — büyük programlar
- ELF ≤ 64 KiB sınırını kaldır: dosya parça parça okunur (dalga 13 bir çağrıda 16 KiB okuyor), segmentler
  doğrudan çerçevelere kopyalanır; ELF'in tamamı çekirdek heap'ine alınmaz (E4k nedeniyle önemli — ölç).
- Kullanıcı penceresini büyüt (öneri: kod/veri 4 MiB, heap 8 MiB, yığın 1 MiB + sınır sayfası; kendi
  seçimini ölçümle gerekçelendir). `kxs_aralik` / doğrulayıcılar ve ABI belgesi aynı sınırları kullanır.
- Kanıt: ≥ 300 KiB'lık bir test programı (büyük salt okunur tablo + büyük bss) yüklenir, tablosunu ve
  bss'ini doğrular, çıkış 0; heap'te 4 MiB tahsis edip yazıp okuyan program.
- Sabotajlar: parça okumada ofseti kaydır → büyük program tablosu yanlış; bss sınırını eski boyutta bırak →
  büyük bss testi kırmızı.

### Bölüm 3 — ekran ve fare çağrıları (H9b tamamı)
- Metin hücresi ekran çağrıları (ön plan sürecine): `ekran_bilgi(yapi)` (sütun, satır, piksel boyu),
  `ekran_yaz(satir, sutun, metin_ptr, renk)` (UTF-8, mevcut 8x16 Türkçe font), `ekran_temizle(renk)`,
  `imlec(satir, sutun, gorunur)`. Gerekirse argüman yapısı (ABI v2'de üç argüman var).
- `fare_bekle(olay)`: dx, dy, mutlak konum (ekran içinde kenetli), düğme durumları; PS/2 ve USB HID ikisi.
- `olay_bekle(olay, maske, zaman_asimi_ms)`: tuş | fare | zaman aşımı — düzenleyici ve gezgin tek döngüyle
  çalışabilsin. `tus_bekle` / `fare_bekle` bunun özel durumları olarak kalabilir.
- Kabuk ön plan sürecine ekranı bırakır; süreç bitince kabuk kendi ekranını yeniden çizer.
- Kâhin: bağımsız Python modeli (`psf_kahin.py`) aynı çağrı dizisinden beklenen ekranı üretir, screendump
  ile piksel piksel karşılaştırılır; QMP `input-send-event` ile fare.
- Sabotajlar: satır/sütun sırasını ters çevir → piksel kırmızı; fare konumunu kenetleme → kenar testi kırmızı;
  süreç bitince kabuk ekranını yeniden çizme → piksel kırmızı.

### Bölüm 4 — metin düzenleyici `duzenle` (H9f)
- `test/kullanici/duzenle.kem`; metin tamponu mantığı saf modülde (`drivers/duzenle/` gibi, host'ta
  birim testli). Özellikler: dosya aç / yeni dosya, ok tuşları, Home/End, PgUp/PgDn, karakter ekle/sil
  (Backspace/Delete; Türkçe UTF-8 karakter tek birim), Enter ile satır böl, satır birleştir, Ctrl+S kaydet,
  Ctrl+Q çık (kaydedilmemiş değişiklikte uyarı + ikinci Ctrl+Q), durum satırı (dosya adı, satır:sütun,
  değişti işareti), uzun satırlarda yatay kaydırma, dosya ekrandan uzunsa dikey kaydırma.
- Sınır: FAT32 değiştirme planı dosya başına 64 KiB (D-759) — daha büyük dosyayı kaydetmek açık hata.
- Kâhinler: (a) host'ta tampon modülü için birim testleri (ekleme/silme/UTF-8/satır böl-birleştir/kaydırma);
  (b) QEMU'da QMP tuş dizisiyle Türkçe bir metin yazılıp kaydedilir → host'ta `fsck.fat -n` + bağımsız Python
  okuyucu içerik birebir; (c) ekran screendump'ı bağımsız modelle birebir; (d) 8 sn boşta bekleme.
- Sabotajlar: UTF-8 ikinci baytını sil → Türkçe karakter bozuk kaydedilir; satır birleştirmede bir karakter
  kaybet → içerik kırmızı; kaydetmeden önce eşitlemeyi atla → kapanış sonrası içerik kırmızı.

### Bölüm 5 — dosya gezgini `dosyalar` (H9g)
- `test/kullanici/dosyalar.kem`: bulunduğu dizinin listesi (ad, boy, dizin işareti), fare ile seçme, çift
  tık ya da Enter: dizine gir / dosyayı `duzenle` ile aç (`baslat` + `bekle`, Bölüm 1), `..` ile üst dizin,
  klavye ile de gezinme (ok tuşları), `q` ile çıkış. Bağlama noktaları (`/diskler/...`) gezilebilir; salt okunur
  bağlamada açılan dosya düzenleyicide salt okunur uyarısı verir.
- Kâhin: QMP fare hareket + tıklama dizisiyle bir dizine girip bir dosyayı açar, düzenleyicide bir satır
  ekleyip kaydeder, gezgine döner; host'ta dosya içeriği + ekran birebir.
- Sabotajlar: tıklama satırını bir satır kaydır → yanlış dosya açılır; `..` sonrası yolu güncelleme → liste kırmızı.

### Bölüm 6 — kabuk iyileştirmeleri (H9e)
- Komut geçmişi (yukarı/aşağı ok, en az 32 girdi), Tab ile yol/dosya adı tamamlama (birden çok aday → liste),
  `temizle`, `yardim`, program adıyla çalıştırma (`kedi dosya` → `/bin/kedi.elf` araması; arama yolunu belgele),
  son çıkış kodunu gösteren `$?` ya da eşdeğeri, satır düzenlemede sol/sağ ok ve ortadan ekleme.
- Kâhin: QMP tuş dizileri; seri + ekran birebir.
- Sabotajlar: geçmişte bir eksik kaydır → kırmızı; Tab tamamlamada ortak öneki yanlış hesapla → kırmızı.

### Bölüm 7 — uçtan uca v1 senaryosu + USB imajı + fiziksel donanım yönergesi (H9i, H11a, H11b)
- **Tek QEMU + OVMF açılışında** (USB bellek imajı + NVMe + AHCI aynı anda): kabuk açılır → `duzenle` ile
  `/diskler/usb0p2/notlar/günlük.txt`'ye Türkçe birkaç satır yazılıp kaydedilir → `dosyalar` ile dosya bulunup
  açılır ve bir satır daha eklenir → `say` ile sayılır → iç diskteki salt okunur bir dosya `kedi` ile okunur →
  `baslat hesap &` + `ps` + `sonlandir` → `kapat`. Kapanışta host: `fsck.fat -n` temiz, içerikler bağımsız
  okuyucuyla birebir, iç diskler + ESP SHA256 değişmemiş. Seri + seçili ekran anları birebir.
- `make kem_os_usb.img` (H11a): GPT, ESP (EFI stub + çekirdek), FAT32 veri bölümü (`/bin` altında bütün
  programlar, örnek Türkçe dosyalar). Bağımsız Python üreticiyle; `fsck.fat` temiz; QEMU'da açılır (yukarıdaki
  senaryo bu imajla koşar).
- `belgeler/X86_Fiziksel_Donanim_Kontrol_Listesi.md` (H11b, ARM64 listesinin deseni): imajı USB'ye yazma
  (`dd` — cihaz adı ASLA sabit değil, `lsblk` ile doğrulama adımı), firmware ayarları (Secure Boot kapalı,
  USB'den açılış), beklenen ekranlar sırayla (açılış satırları, kabuk istemi), adım adım denenecek komutlar,
  "şu görünmezse şuna bak" sorun giderme tablosu, sonuç kaydı şablonu (çalışan/çalışmayan aygıt tablosu).
- Sabotaj: senaryonun ortasında bir dosya yazma eşitlemesini kapat → kapanış kâhini kırmızı.

### Bölüm 8 — kaynak ve süre ölçümleri + sağlamlaştırma
- Süreç başına bellek (çerçeve + çekirdek heap), `baslat` süresi, `duzenle` açılış süresi, 100 süreç
  başlat/bitir ve 50 düzenleyici aç/kaydet/kapat döngüsünde çerçeve ve heap eğrisi (E4k'yi belgelemek için).
- Tüm yeni kapıların C ve self-host derleyicileriyle ≥10 açılış sonuçları; aralıklı kırmızı varsa kaç açılışta
  kaç kırmızı olduğu.
- Birleşik birimlerin satır sayısı ve self-host `--llvm` süresi başta ve sonda (E4q).

---

## 7. Kapılar (her bölümden sonra)
O bölümün yeni kapıları + `calistir_bm_x86_surec`, `calistir_bm_x86_surec_abi`, `calistir_bm_x86_uefi_pc`,
`calistir_bm_x86_uefi_kabuk`, `calistir_bm_x86_kabuk`, `calistir_bm_x86_disk`, `calistir_bm_x86_blok`,
`calistir_bm_x86_istisna`, `calistir_bm_x86_apic`, `calistir_bm_x86_ps2_irq`, `calistir_kem_os_x86`;
`kem_gorev_cekirdek.kem` değişirse `calistir_kem_os_arm` + `calistir_qemu_cekirdek`; sans-IO değişikliklerinde
`calistir_surucu_diff`, `calistir_surucu_kosum`, `calistir_chatgpt_kopru`. Sekizinci bölümden sonra
`make test_tumu` (rc PIPESTATUS ile, atlamalar sayılmış), `calistir_check_kapisi`, `calistir_baremetal_diff`.

**Ölçüm tuzakları:** derle-çalıştır döngüsünde ikiliyi ve kullanıcı ELF'lerini her koşumdan önce `rm -f`;
disk imajı her koşumda yeniden üretilir; `echo "$(cmd) $?"` yazma; `--checkdump` tanı bassa da 0 döner;
QEMU üçlü hatada sessizce yeniden başlar (`-no-reboot` + tek açılış işareti); kâhin komutları anında
gönderirse bekleme kusurları görünmez (her bölümde en az bir 8 sn boşta bekleme); zamanlamaya bağlı
kapılarda (Bölüm 1) tek açılışla yetinme.

---

## 8. Bitirince
Draft PR `codex/program-dalga14`, gövde `codex14_ilerleme.md` (sekiz bölümün her biri için: yapılanlar,
kapılar, sabotaj sonuçları, süre). `codex14_bulgular.md`: ölçülen davranışlar ve VARSAYIM'lar (zamanlayıcı
dilimi, bağlam düzeni, pencere düzeni, yeni ABI çağrıları, ekran/fare çağrı tasarımı, düzenleyici tampon
yapısı), kaynak ölçümleri, derleyici bulguları (C14-xxx), kalanlar. Auto-merge AÇMA, merge ETME.

## 9. Bu dalganın DIŞINDA
- E4k (bölge modeli), E4q (derleme süresi) ve hata yalıtımı testleri (H9j/E3f): Claude.
- Pencere yöneticisi, boru hatları, sinyaller, paylaşılan bellek, ağ, ses, SMP, BIOS/GRUB (D-756).
