# Dalga 13 — x86_64 kullanıcı programları

## Kapsam
- Yapılan: ABI v2, bağımsız kullanıcı çalışma zamanı ve ELF bağlantısı; UEFI çerçeve havuzu, C1 süreç çalıştırma, FAT32 dosya çağrıları, klavye/zaman çağrıları ve `calistir` kabuk komutu.
- Altı ayrı KEMGU programı: `merhaba`, `kedi`, `say`, `yazdene`, `tus`, `bolme`.
- Yapılmayan (kapsam dışı): H9j/E3f hata yalıtımı programları, derleyici/heap modeli değişiklikleri. C2, fare ve arka plan yönetimi önerilen kapsam olarak bırakıldı.
- Dal: `codex/surec-dalga13`; taban `b1b9470` (D-760). Main birleştirilmedi.

## Test
- `make calistir_bm_x86_surec_abi`: ABI belgesi/sarmalayıcı/dağıtıcı tutarlılığı; C ve self-host ile 10'ar PVH açılışı geçti.
- `SUREC_TEKRAR=1 SUREC_SENARYO='bolme yazma tekrar' make calistir_bm_x86_surec`: iki derleyicide geçti; kaynakları yeniden kullanarak 50 tekrar, Türkçe argv, dosya işlemleri, süreç çıkış kodu, seri/GOP ve bağımsız disk denetimleri.
- `make calistir_bm_x86_surec`: tam zincir içinde C ve self-host ile 10'ar OVMF açılışı geçti (toplam 20). USB/PS2 için sekiz saniye girdisiz bekleme ve canlı sayfa tablosu yürüyüşü dahil.
- `make test_tumu`: **GEÇTİ, rc=0** (`PIPESTATUS[0]`); son satır `Tum testler gecti!`. Kayıt: `build/codex13_test_tumu_son.log`, çıkış kodu: `build/codex13_test_tumu_son.rc`.
- `make calistir_kem_os_x86 calistir_kem_os_arm`: **GEÇTİ, rc=0**.
- Tam zincirde: tip kapısı 356 geçti / 21 mevcut muaf / 0 RED; bare-metal paritesi 23/23; sıfır uyarı 38 dosya; ChatGPT köprüsü 141 test; ASan E2E 133 başarılı / 0 başarısız ve ASan/UBSan matrisi 12/12.
- Mevcut atlamalar: 68 atlama olayı (tekil test sayısı değil); kapı bazlı nedenler ve mevcut muafiyetler `codex13_bulgular.md` içinde. Yeni süreç kapılarında atlama yok.
- `make -j8 build/kullanici/{merhaba,kedi,say,yazdene,tus,bolme}.elf build/kullanici/self/{merhaba,kedi,say,yazdene,tus,bolme}.elf`: 12 ELF üretildi, tüm sembol/segment denetimleri geçti.
- Sabotajlar: argv, BSS, fd sınırı, salt okunur yazma, çerçeve iadesi, segment izinleri, dosya konumu ve çıkış eşitlemesi gerçek QEMU'da 1/1 kırmızı. QEMU öncesi ABI ve ELF kapıları da 1/1 kırmızı. Yeniden üretim: `python3 tools/codex/surec_sabotaj.py`.
- Mimari: kullanıcı programları x86_64; mevcut arm64 kapıları regresyon kontrolüdür.
- Yeni test programı sayısı: 6. Yeni Make kapısı sayısı: 2. OVMF senaryo sayısı: 8; iki dosya senaryosu ek tekrarlarla toplam 10 açılışa tamamlanır.

## Dil özellikleri
- `tekkez` / `yetki`: yeni kullanım yok; yeni MMIO sürücüsü eklenmedi.
- `gerçekzamanlı`: yeni ISR yok; mevcut kesme yolları korunuyor.
- Kullanıcı çalışma zamanı mevcut KEMGU heap/dizi kodunu değişikliksiz kullanıyor. Derleyici ve dil değişikliği yok. C13-001/C13-002 dolanmaları bulgu dosyasında açıklanıyor.

## Ölçümler ve sınırlar
- Kullanıcı penceresi 2 MiB; her fiziksel yuva 516 sayfa, iki yuvalı havuz toplam 1.032 sayfa. Genel UEFI ayırıcısından açılışta alınır ve çekirdek ömrünce ayrılmış kalır; süreç çıkışında yuva havuza döner.
- 50 tekrar: iki derleyicide 1.032 kullanılabilir çerçeve, süreç kurma/temizleme heap artışı 0 bayt. C kabuk heap'i 4.657.328 → 16.003.360; self-host 4.788.592 → 16.003.456 bayt. Kabuk büyümesi ayrı E4k işidir.
- C1 tek canlı süreç çalıştırır. Dosya çağrılarında okuma parçası en çok 16 KiB, yazma parçası 4 KiB; mevcut FAT32 değiştirme ve ELF disk dosyası sınırı 64 KiB. `say`, 264.000 baytlık dosyayı parçalarla okur.
- Heap ve arayüz ayrıntıları: `otomasyon/chatgpt/codex13_bulgular.md`, `belgeler/KEMGU_OS_Syscall_ABI.md`.

## Dosya değişiklikleri (whitelist kontrol)
Görev 13 §4 izin genişletmesi esas alındı. Assembly farkı yalnız syscall stub içinde. Yeni tip denetimi muafiyeti yok.

```text
Makefile
belgeler/KEMGU_OS_Syscall_ABI.md
boot/start_x86_64.S
drivers/fs/fat32_dizin.kem
drivers/fs/fat32_sektor.kem
drivers/fs/fat32_sektor_yaz.kem
drivers/ps2/klavye_tr.kem
drivers/usb/hid_klavye.kem
linker/user-x86_64.ld
otomasyon/chatgpt/codex13_bulgular.md
otomasyon/chatgpt/codex13_ilerleme.md
runtime/kullanici/x86_64/kem_basla.kem
runtime/kullanici/x86_64/kem_sis.kem
runtime/mimari/x86_64/kem_bellek_harita.kem
runtime/mimari/x86_64/kem_gorev_hw.kem
runtime/mimari/x86_64/kem_surec_hw.kem
test/bm_x86_kabuk_harness.sh
test/bm_x86_surec_abi_harness.sh
test/bm_x86_surec_harness.sh
test/bm_x86_uefi_harness.sh
test/check_kapisi.sh
test/kullanici/bolme.kem
test/kullanici/kedi.kem
test/kullanici/merhaba.kem
test/kullanici/say.kem
test/kullanici/tus.kem
test/kullanici/yazdene.kem
test/ornekler/kem_os_x86_kabuk.kem
test/ornekler/kem_os_x86_surec_abi.kem
test/ornekler/kem_os_x86_vfs_ortak.kem
tests/qemu/dalga12_kahin.py
tests/qemu/surec_abi.py
tests/qemu/surec_elf.py
tests/qemu/surec_imaj.py
tests/qemu/surec_kahin.py
tools/codex/surec_check.py
tools/codex/surec_sabotaj.py
```
 Doğrudan `git diff origin/main` eski taban nedeniyle `LOOP.md` ve görev metninin upstream farklarını da gösterir; bu iki dosyada dal değişikliği yoktur. PR'ın `git diff origin/main...HEAD` farkı izinli yollardadır.

## Sonraki adım
İşlevsel kullanıcı programı temeli, metin düzenleyici ve dosya gezginini çekirdekten ayrı geliştirmeyi sağlar. Daha büyük kullanıcı programları için 64 KiB ELF okuma sınırının genişletilmesi sonraki doğal iştir. Claude'un ayrı hata yalıtımı ve E4k çalışmaları entegrasyonda birleştirilmelidir. Auto-merge kapalı tutulur; bu dal merge edilmez.

Tamamlanma: 2026-11-21 (Europe/Istanbul).
