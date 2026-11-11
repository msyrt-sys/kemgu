# Dalga 9 — ekran konsolu ve PS/2

Kabuk 8×16 Türkçe fontla canlı yankı, geri silme, ters renkli imleç ve kaydırma gösterir; USB klavye bulunamazsa PS/2 kullanır. `kdl_panik` ekran hazırken kırmızı zemin ve mesaj çizer, seri çıktıyı korur ve HLT ile durur.

## Kapsam

- **A:** kamu malı Uni2-Fixed16 fontu, ölçümle seçilen 64 baytlık hex parçaları; bağımsız PSF/SHA kâhini ve kutu çizim satırı.
- **B:** yalnız ekranda canlı girdi, yatay girdi penceresi, geri silme/imleç; bağımsız FAT32 yazıcısından 70 satırlık dosya ile kaydırma. Tam çizim ortalama 32,6 ms.
- **C:** ayırmasız `kdl_panik` çizimi, açık/kapalı ekran ve eski kayıt kanaryası; CPU HLT denetimi.
- **D:** gerçek i8042'de ölçülen set 1; USB klavye ve hatta xHCI denetleyicisi olmadığında PS/2 yedeği.

## Test

- **`make -j1 test_tumu`: rc=0**, `PIPESTATUS[0]` ile okundu. **FIXPOINT ✓, 99.048 satır.**
- Ekran **5/5**; USB klavye kabuk **15/15**; PS/2 kabuk **17/17**; panik **15/15**.
- qtest **6/6** (C/self ayrı QEMU); bare-metal NVMe/AHCI/USB **20'şer açılış**.
- Köprü kart + **129/129**; checker **312/312**; baremetal_diff **23/23**; tip kapısı **0 RED**.
- ASan **133 PASS / 0 FAIL / 38 SKIP**; ASan/UBSan matris **12/12**.
- Ek self-host QEMU kanıtı: panik **15/15**, aynı PS/2 ham izi ve üç diskle kabuk **3/3**.
- Yeni saf testler: font, girdi görünümü ve ölçülmüş PS/2 izi (**3 dosya**); yeni bare-metal kapılar: panik ve PS/2 (**2 hedef**).
- **65 atlama olayı** (64 korpus + 1 üst bellek); muafiyetler ve nedenleri [bulgular raporunda](https://github.com/msyrt-sys/kemgu/blob/codex/ekran-dalga9/otomasyon/chatgpt/codex9_bulgular.md). Lean/lake olmadığı için gerçek ispat tip denetimi yapılmadı.

## Sabotaj

| Bozma | Ölçülen kırmızı |
|---|---|
| `ğ` glifinin tek biti | QEMU 1/1, 2 piksel fark; saf test exit=2 |
| Canlı yankıyı kaldırma | QEMU 1/1, 268 piksel fark |
| Kaydırmada eski satırı kopyalama | QEMU 1/1, 7761 piksel fark |
| Panikte hazır bayrağını yok sayma | QEMU 1/1, kapalı ekran RAM kanaryası bozuldu |
| PS/2 tarama baytının bit 7'sini silme | QEMU 1/1, bırakma/E0 izi farklı |

Hepsi derlenen bozmalardır ve geri alınmıştır. Girdi görünümünün INT_MAX sınır testi ayrıca önce panik/134, düzeltmeden sonra C/self yeşil verdi.

## Dil özellikleri ve sınırlar

`drivers/` saf kalır. Ham erişimler yalnız izinli x86 runtime/çekirdek dosyalarındadır; `kem_mmu_hw.kem` içinde yalnız `kdl_panik` gövdesi değişti. Yeni asm, derleyici, C runtime, boot, linker veya CI değişikliği yok. Yeni `tekkez`, `yetki` veya ISR yok; PS/2 yoklamalıdır.

Panik ekranı **`kdl_panik` yolu içindir**. İzin dışındaki `kem_heap.kem` içindeki ayrı `kdl_dizi_oob` yoğun döngüsü değiştirilmedi; bütün dizi sınır hatalarının artık ekrana çıktığı iddia edilmiyor. E4k bölge sızıntısı ve ana dalın 28 MiB heap ayarı korunur. QMP ölçümünde bu testlerin üst sınırı 3,28 MB, self-host ek koşumda 2,83 MB altında; kapı 16 MiB'ı aşmayı reddeder.

## Dosya değişiklikleri — whitelist kontrolü

26 dosyanın tamamı görevde izin verilen alanlarda:

```text
Makefile
drivers/ekran/font_tr16.kem
drivers/ekran/girdi.kem
otomasyon/chatgpt/codex9_bulgular.md
otomasyon/chatgpt/codex9_ilerleme.md
runtime/mimari/x86_64/kem_ekran_bga.kem
runtime/mimari/x86_64/kem_mmu_hw.kem
runtime/mimari/x86_64/kem_ps2_hw.kem
test/bm_x86_kabuk_harness.sh
test/bm_x86_panik_harness.sh
test/bm_x86_ps2_harness.sh
test/check_kapisi.sh
test/ornekler/kem_os_x86_ekran.kem
test/ornekler/kem_os_x86_kabuk.kem
test/ornekler/kem_os_x86_panik.kem
test/ornekler/kem_os_x86_ps2.kem
tests/drivers/ekran/font_tr16_test.kem
tests/drivers/ekran/girdi_test.kem
tests/drivers/ps2/qemu_set1_test.kem
tests/qemu/bm_ekran.py
tests/qemu/bm_kabuk.py
tests/qemu/bm_panik.py
tests/qemu/bm_ps2.py
tests/qemu/fat_yazici.py
tests/qemu/psf_kahin.py
tools/codex/font_tr16.py
```

## Sonraki adım

Taslak PR incelemesi. Merge ve auto-merge yapılmadı; D numarası atanmadı.

## Çalışma kaydı

Başlangıç: 2026-11-11T18:01:49.463286+00:00

- D-750 origin/main üzerinde; taban 3305918.
- Taban ekran 5/5, kabuk NVMe/AHCI/USB 15/15: rc=0.
- VARSAYIM: görev dosyası ana checkout’ta yok; otomasyon/codex-gorev9 dalındaki tam metin okundu.

- [x] A — gömülü 8×16 font; ekran 5/5, kabuk 15/15, köprü kart + 127 test; glif sabotajı gerçek QEMU 1/1 kırmızı ve dogrula exit=2. 2026-11-11T18:09:05.704444+00:00

- [x] B — canlı yankı, geri silme, ters imleç, 70 satır kaydırma; ekran 5/5, kabuk 15/15, köprü kart + 128 test. Yankı/kaydırma sabotajları 1/1 kırmızı; heap üst sınırı 3,28 MB. 2026-11-11T18:17:18.563180+00:00

- [x] C — kırmızı panik ekranı, seri paritesi, HLT ve kapalı bayrak kanaryası. C ve self ayrı QEMU’da 15’er açılış; ekran 5/5, kabuk 15/15, köprü 128/128, check 0 RED, baremetal_diff 23/23. Bayrak sabotajı 1/1 kırmızı. 2026-11-11T18:24:32.872321+00:00

- [x] D — i8042/set 1 gerçek ölçümü ve PS/2 yedeği; PS/2 17/17, USB klavye 15/15, ekran 5/5, panik 15/15, köprü 129/129. Self-host PS/2 izi + üç diskle 3/3 kabuk ayrıca geçti. Tarama sabotajı 1/1 kırmızı. 2026-11-11T18:39:35.639197+00:00

Son rapor: 2026-11-11T19:02:54.211621+00:00. Tam doğrulama logu `build/codex9/test-tumu.log`; bütün aşama logları `build/codex9/` altında.
