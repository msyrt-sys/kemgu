# CODEX GÖREVİ 9 — ekran konsolu tamamlama (gömülü 8×16 Türkçe font, canlı yankı, kaydırma, panik ekranı) + PS/2 klavye

Dalga 1'in kuralları geçerlidir (`otomasyon/chatgpt/CODEX_GOREV.md` §2, §3, §5 — yeniden
oku), aşağıdaki **izin genişletmesi** dışında. Dalga 6'nın gerçek-QEMU kâhin kuralı da
geçerlidir (`CODEX_GOREV_6.md` §0). Soru sorma, durma; takılırsan (60 dk) kaydet ve geç.

## 0. Hedef
KEMGU-OS kabuk v0 (D-747/D-749) bugün ekrana **8×8** test fontuyla yazıyor, yazılan
karakterleri ancak Enter'dan sonra gösteriyor, kaydırma yolu hiç ölçülmedi ve panikte ekran
boş kalıyor. Bu dalga yol haritasının H4c, H4d, H4f maddelerini ve H5'in (PS/2 klavye)
bare-metal yarısını kapatır. Sonunda kabuk: **8×16 Türkçe font**, yazarken canlı yankı,
dolunca kayan ekran, panikte kırmızı ekran, USB klavye yoksa PS/2 klavye.

## 1. Ön koşul ve çalışma alanı
```bash
cd /home/msyrt/Desktop/kemgu && git fetch origin
git log origin/main --oneline | grep D-750     # yoksa 5 dk'da bir, en çok 2 sa; sonra DUR, kaydet
git worktree add ../kemgu-codex9 -b codex/ekran-dalga9 origin/main && cd ../kemgu-codex9
make -j8 build/kemgu build/codegen build/kdl_runtime.o
make calistir_bm_x86_ekran calistir_bm_x86_kabuk   # taban sağlam mı (5/5 + 15/15)
date > otomasyon/chatgpt/codex9_ilerleme.md
```
Tek dal, tek draft PR.

## 2. Okunacaklar (bu dalganın temeli)
- `runtime/mimari/x86_64/kem_ekran_bga.kem` — DISPI kip, glif çizimi, `ekran_konsol_ciz`,
  **ayna** tamponu (seriye giden baytların ekran kopyası).
- `test/ornekler/kem_os_x86_kabuk.kem` — `KabukEkran`, `ke_ciz`, yankı (`ekran_ayna_yaz`).
- `tests/qemu/psf_kahin.py` — BAĞIMSIZ kâhin: font baytları, PSF ayrıştırma, `konsol()`
  modeli, `screendump`, `karsilastir`. `tests/qemu/bm_ekran.py`, `tests/qemu/bm_kabuk.py`.
- `drivers/ekran/{psf,konsol,yazi}.kem`, `drivers/ps2/klavye_tr.kem`, `drivers/girdi/satir.kem`.
- `LOOP.md` D-747 (heap: tuş başına ~3 KB kalıcı sızıntı, E4k), D-748, D-749, D-750.

## 3. İzin genişletmesi (yalnız bu dosyalarda)
- `güvensiz` / `çıplak` / ham işaretçi YALNIZ: `runtime/mimari/x86_64/kem_ekran_bga.kem`,
  yeni `runtime/mimari/x86_64/kem_ps2_hw.kem` (port G/Ç `kem_pci_hw.kem`'deki ilkellerle;
  8 bit port için `kem_port_oku8`/`kem_port_yaz8` zaten var — yeni satıriçi_asm YAZMA) ve
  `test/ornekler/kem_os_x86_*.kem`. `drivers/**` saf kalır.
- `runtime/mimari/x86_64/kem_mmu_hw.kem` içinde YALNIZ `kdl_panik` gövdesi (bkz. C).
- Yeni harness'lar: `test/bm_x86_*_harness.sh`, `tests/qemu/*` (Python dahil); fikstür
  üreteci `tools/codex/*.py`.
- Makefile: yeni `calistir_bm_x86_*` + `test_tumu` + `.PHONY`; x86 CAT listesine yeni
  runtime dosyası; `test/check_kapisi.sh` birleşik birimlere satır ekle (D-749'daki
  `kem_ekran_bga` deseni). Yeni `runtime/*.kem` sınıflandırılmazsa kapı KIRMIZI olur — ekle.
- Derleyici (`src/`, `selfhost/`), runtime C dosyaları, `boot/`, `linker/` YASAK. Derleyici
  hatası bulursan küçük tekrar üretimiyle `codex9_bulgular.md`'ye C9-xxx olarak yaz ve
  dolan (D-747'deki C8-001..003 gibi).
- Muafiyet listelerine (bolge_operand `ENV_MUAF`, check_genis `MUAF`, yapi_diff) satır
  EKLEME. Gerekiyormuş gibi görünürse önce nedenini ölç ve kaydet (D-749: temsil farkı
  fikstürü başka korpusa taşınarak çözüldü, muafiyet eklenmedi).

## 4. İş listesi
### A — gömülü 8×16 Türkçe font (H4c)
- Kaynak: `/usr/share/consolefonts/Uni2-Fixed16.psf.gz` (PSF1, 8×16, 512 glif, Unicode
  tablolu; Türkçe `çÇğĞıİöÖşŞüÜ`, kutu çizim `─│┌┐└┘├┤┬┴┼═║` ve U+FFFD İÇERDİĞİ ölçüldü).
  Lisans: console-setup copyright "All console fonts are public domain by nature"; X11
  `fixed` ailesi kamu malı — kaynağı ve gerekçeyi fikstür başlığına YAZ.
- Üreteç `tools/codex/font_tr16.py` → `drivers/ekran/font_tr16.kem` (`genel işlev
  font_tr16() -> Dizi<dtam8>`). Üretilen dosya DEPOYA girer (CI'da consolefonts yok).
  Biçimi ÖLÇEREK seç: mevcut `yaz_u8` deseni (psf_gercek.kem, 3618 bayt → 74 KB kaynak)
  10804 baytta ~220 KB olur; tek bir hex `metin` + `metin_bayt` ile çözme çok daha küçük
  olabilir. Derleme süresini ve çekirdek boot süresini ikisi için ölç, seçimi yaz.
- Kâhin BAĞIMSIZ kalmalı: `psf_kahin.font_baytlari` yeni fikstürü KEMGU derlemeden okusun
  (seçtiğin biçimin Python ayrıştırıcısı) ve ayrıca baytların `.psf.gz`'dekiyle aynı olduğunu
  (yerelde, dosya varsa) doğrula.
- `kem_os_x86_ekran.kem` + kabuk 8×16'ya geçsin; ekran kâhinine bir **kutu çizim** satırı ekle.
  Sabotaj: tek bir Türkçe glifin (ör. `ğ`) baytını boz → kâhin KIRMIZI.

### B — konsol: canlı yankı, geri silme, imleç, kaydırma (H4d)
- Yazılan karakter Enter'ı beklemeden ekranda görünsün (yalnız ekran; **seri çıktı birebir
  korunur** — bugünkü kâhin seriyi tam karşılaştırıyor). Geri silme ekranda da silsin.
- İmleç: imleç hücresi ters renk (ya da alt çizgi) — kuralı `psf_kahin.konsol()` modeline de
  ekle, ikisi aynı kuralı uygulasın.
- **Kaydırma yolu ölçülmeli:** satır sayısını aşan çıktı (ör. `ls` 70 girdi ya da 70 satırlık
  `cat`) — Python `konsol()` modeli zaten kaydırır; ekran birebir olmalı. Disk imajına bunun
  için bir dosya ekle (bağımsız Python yazıcısıyla — `tests/qemu/fat_yazici.py`).
- Performans: tam ekran yeniden çizim süresini ölç (H4h); gerekiyorsa yalnız kirli satır.
- ⚠ Heap: kabuk tuş başına ~3 KB sızdırıyor (E4k, dil kararı bekliyor). Testlerin tuş
  sayısı 16 MB'ı aşmamalı; aşıyorsa DUR ve kaydet, çözmeye çalışma.

### C — panik ekranı (H4f)
- Ekran açıksa `kdl_panik` (`kem_mmu_hw.kem`) kırmızı zemin + "PANIK" + mesajı ekrana da
  yazsın; seri davranışı değişmez. Ekran açık değilse (bayrak) bugünkü gibi yalnız seri.
- Kâhin: kasıtlı panik eden bir çekirdek (ör. dizi sınır ihlali) → screendump kırmızı zemin +
  metin birebir; seri `PANIK:` satırı. Panik sonrası sistem durmalı (asılı kalmak değil,
  `hlt` döngüsü; kâhin QEMU'yu öldürür).
- Sabotaj: bayrak yokken de ekrana yazmaya çalışırsa (ekran açılmadan) ne olduğunu ölç ve yaz.

### D — PS/2 klavye bare-metal (H5 yarısı)
- `kem_ps2_hw.kem`: i8042 (veri 0x60, durum/komut 0x64) yoklamalı okuma. QEMU q35'te i8042
  var; QEMU çevirme (translate) açıkken hangi tarama seti geldiğini ÖLÇ ve `klavye_tr.kem`
  ile uyumunu doğrula (varsayma).
- Kabuk: USB klavye bulunamazsa PS/2'den okusun. Kâhin: `-device usb-kbd` OLMADAN aynı
  komut dizisi (QMP `input-send-event` PS/2'ye gider — ÖLÇ), seri + ekran birebir, üç disk.
- Boşta bekleme (D-747 dersi): ilk açılışta en az 8 sn tuşsuz bekle, kabuk düşmemeli.

## 5. Kapılar
Her işten sonra: `calistir_bm_x86_ekran`, `calistir_bm_x86_kabuk` ve eklediğin
`calistir_bm_x86_*`; derleyiciye dokunmadığın için `codegen_diff`/`checker_diff` yalnız sonda.
Bitirince `make test_tumu` (rc PIPESTATUS ile, atlamalar sayılmış), `calistir_check_kapisi`,
`calistir_baremetal_diff` dahil. Her iş için sabotaj; aralıklı sınıflarda kaç açılışta kaç
kırmızı olduğunu YAZ.

**Ölçüm tuzakları (bu oturumlarda yaşandı):** derle-çalıştır döngüsünde ikiliyi her koşumdan
önce `rm -f` (clang başarısız olunca eski ikili koşup sahte yeşil verir); `echo "$(cmd) $?"`
yazma — `$?` önce açılan `$(...)`ın kodudur; kâhin komutu ANINDA yazarsa boşta bekleme
kusurlarını göremez.

## 6. Bitirince
Draft PR `codex/ekran-dalga9`, gövde `codex9_ilerleme.md`; `codex9_bulgular.md`: ölçülen
davranışlar (tarama seti, çizim süreleri, font biçimi seçimi), derleyici bulguları (C9-xxx),
tasarım seçimleri, kalanlar. Auto-merge AÇMA, merge ETME.
