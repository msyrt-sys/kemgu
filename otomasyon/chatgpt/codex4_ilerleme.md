# Dalga 4 ilerleme

Başlangıç: 2026-11-07T06:12:06.764000+00:00

- Ön koşul: D-739, `5aa7872`; 50 dakika / 10 periyodik fetch sonunda bulundu. Worktree tabanı `ac637ad`.
- VARSAYIM: Görev 4’ün açık kapsamı ve `codex/surucu-dalga4` dalı, genel AGENTS sürücü dalı kuralının bu görevdeki özel karşılığıdır.
- [x] A0 · 8 mevcut test · C/self T002×5 → temiz · kapasite +1 sabotajı → çalışma zamanı kırmızı · köprü 91/91 · 2026-11-07T06:14:15.988212+00:00
- [x] A1 · 2 test / tüm 9 istek varyantı · kısa DMA yanıtı sabotajı → kırmızı · self 0,002 sn · köprü 92/92 · 2026-11-07T06:16:39.096794+00:00
- [x] A2 · 3 test · BE bayt sırası sabotajı → kırmızı · self 0,002 sn · köprü 93/93 · 2026-11-07T06:19:00.045899+00:00
- [x] A3 · 4 test (296 kesit + 8 bozuk alan) · modül adres sırası sabotajı → kırmızı · self 0,003 sn · köprü 94/94 · F001/F002 dolanmaları · 2026-11-07T06:23:49.776448+00:00
- VARSAYIM A3: komut anahtarları tektir; kok 8 hex hane veya 4-4 biçimindedir. Bilgi 1 MiB / 4096 etiket / metin 4096 bayt ile sınırlıdır. Paletli ekran açık desteklenmiyor hatası verir.
- [x] B1 · 3 test · CQ kapı zili +1 kaldırma sabotajı → kırmızı · self 0,002 sn · köprü 95/95 · 2026-11-07T06:26:38.115001+00:00
- [x] B2 · 3 test / SQ-CQ bayt kâhini / zincirli PRP · faz biti kaydırma sabotajı → kırmızı · self 0,004 sn · köprü 96/96 · 2026-11-07T06:29:56.905035+00:00
- VARSAYIM B2: PRP planı bitişik fiziksel DMA alanı içindir; plan başına 16 MiB, sayfa 4–64 KiB. Büyük aktarım çekirdekte bölünür.
- [x] B3 · 3 test · seçili LBA biçimini yok sayma sabotajı → kırmızı · self 0,004 sn · köprü 97/97 · 2026-11-07T06:33:17.444280+00:00
- [x] B4 · 2 test / başlatma, 7 I/O, sarma, CFS, zaman aşımı · CQ faz sarma sabotajı → kırmızı · self 0,032 sn · köprü 98/98 · F003 dolanması · 2026-11-07T06:42:15.301508+00:00
- VARSAYIM B4: tek etkin istek, 2–64 yuva; çağıranın ayırdığı 86016 bayt bitişik DMA alanı; sayfa 4 KiB, veri tamponu 64 KiB; CAP.TO=0 için 500 ms. Metadata içeren LBA açık hata verir.
- [x] B5 · 2 test / GPT→FAT32 dosya oku-yaz-flush-yeniden oku + 6 hata · CID denetimi sabotajı → kırmızı · self 0,365 sn · köprü 99/99 · 2026-11-07T06:46:12.235022+00:00
- [x] C1 · 3 test · IPM denetimi sabotajı → kırmızı · self 0,002 sn · köprü 100/100 · 2026-11-07T06:49:57.095767+00:00
- [x] C2 · 3 test · DBC bayt−1 sabotajı → kırmızı · self 0,002 sn · köprü 101/101 · 2026-11-07T06:53:44.768116+00:00
- [x] C3 · 3 test · ATA kelime içi bayt sırası sabotajı → kırmızı · self 0,002 sn · köprü 102/102 · 2026-11-07T06:57:12.209301+00:00
- [x] C4 · 2 test / SATA tarama, boş yuva, 5 I/O · SATA imza sabotajı → kırmızı · self 0,064 sn · köprü 103/103 · 2026-11-07T07:02:14.004583+00:00
- VARSAYIM C4: her durum bir seçili SATA portuna aittir; port=-1 ilk uygun portu seçer, tüm uygun portlar listelenir. DMA alanı 73728 bayt; veri 64 KiB; beklemeler 1000×1 ms ile sınırlıdır.
- [x] C5 · 2 test / GPT→FAT32 dosya + 7 hata · TFES sabotajı → kırmızı · self 0,366 sn · köprü 104/104 · 2026-11-07T07:04:23.609001+00:00
- [x] D1 · 4 test / 57 kesit / USB-HID başlık kâhini · HID uzunluk sabotajı → kırmızı · self 0,002 sn · köprü 105/105 · 2026-11-07T07:08:20.392031+00:00
- [x] D2 · 3 test · CSZ bit konumu sabotajı → kırmızı · self 0,002 sn · köprü 106/106 · 2026-11-07T07:11:57.338793+00:00
- [x] D3 · 4 test / TRB, halka sarımı, olay, 32/64 B bağlam · cycle sabotajı → kırmızı · self 0,002 sn · köprü 107/107 · 2026-11-07T07:20:04.666062+00:00
- [x] D4 · 2 test / Türkçe metin, fark, rollover, Ctrl/Shift/AltGr · sağ Shift sabotajı → kırmızı · self 0,002 sn · köprü 108/108 · 2026-11-07T07:22:13.266539+00:00
- VARSAYIM D4: GUI değiştiricileri ve tanımsız kullanım kodları olay üretmez. 1–3 hata kullanım kodları son geçerli raporu korur.
- [x] D5 · int8 sınır tablosu, 3/4 bayt, düğmeler, geçersiz boylar · Y işareti sabotajı → kırmızı · self 0,002 sn · köprü 110/110 (D6 paket testi dahil) · 2026-11-07T07:26:31.364581+00:00
- [x] D6 · paket kâhini + 2 çekirdek test / 22 aktarım, 6 hata · CSW etiket sabotajı → kırmızı · self 0,114 sn · köprü 111/111 · 2026-11-07T07:29:11.558355+00:00
- VARSAYIM D6: xHCI slot/uç bağlamlarını çağıran kurar; özel olay halkası ve önceden sıfırlanmış 128 KiB DMA alanı gerekir. Tek etkin komut, LUN 0. Fiziksel taşıyıcı sözleşmesi bulgularda.
- [x] D7 · GPT→FAT32 dosya oku-yaz-flush-yeniden oku + 4 hata · CSW artık sabotajı → kırmızı · self 0,465 sn · köprü 112/112 · 2026-11-07T07:31:17.473390+00:00
- [x] E1 · LRU, kirli atım engeli, kopya yalıtımı, eski onay, aktarım sınırları · sürüm denetimi sabotajı → kırmızı · self 0,002 sn · köprü 114/114 (E2 testi dahil) · 2026-11-07T07:33:58.121451+00:00
- VARSAYIM E1: geri yazma planı LBA/veri/sürüm içerir; çağıran aygıt tamamlanmasından sonra aynı sürümü onaylar. Başarısız veya eski onay kirli biti temizlemez. Önbellek en çok 16 MiB, bölme planı 4096 parça.
- [x] E2 · seri/kök/yol, GPT+MBR ESP, bayrak, belirsizlik, tek-FAT dönüşü · ESP denetimi sabotajı → kırmızı · self 0,114 sn · köprü 114/114 · 2026-11-07T07:35:58.169190+00:00
- VARSAYIM E2: yinelenen aygıt/bölüm kimliği hata; aynı kök serisini taşıyan birden fazla bölüm ramdisk ve açık uyarı üretir. FAT32 olmayan aday bağlama kaydına alınmaz.
- [x] E3 · USB kök + NVMe GPT ESP/veri + AHCI, gerçek dosya kalıcılığı · ESP sabotajı → kırmızı · self 0,969 sn · köprü 116/116 (F1 dahil) · F005 dolanması · 2026-11-07T07:39:17.638973+00:00
- VARSAYIM E3: NVMe modelindeki ikinci ayrık disk aralığı büyük sıfır boşluğu ayırmadan geçerli iki FAT32 bölümünü temsil eder; ESP koruması modelde değil bağlama katmanındadır.
- [x] F1 · 4500 ikili + 300 metin örneği, boş/tek bayt, 14 geçerli fikstür mutasyonu · VID sabotajı → kırmızı · self 0,115 sn · köprü 117/117 (F2 dahil) · 2026-11-07T07:41:38.082978+00:00
- [x] F2 · 44 tablo satırı + BE/bağlam/slot sınırları · ilk test exit=52 → BE değer denetimiyle yeşil; denetim sabotajı → kırmızı · self 0,008 sn · köprü 117/117 · 2026-11-07T07:43:40.026445+00:00
- Son kâhin kontrolü: 18 Python üretici/doğrulayıcı rc=0; yeniden üretilen dosyalar bayt bayt aynı.

## Tamamlanma özeti

- A0–A3, B1–B5, C1–C5, D1–D7, E1–E3, F1–F2: **26/26 iş tamamlandı; atlanan iş yok.**
- **26 yeni test dosyası**, köprü **117/117** + dil kartı; C/self derleme, çalışma ve stdout eşliği. **26/26 derlenebilir sabotaj** çalışma zamanında yakalandı ve geri alındı.
- Her yeni test 5 saniye sınırıyla doğrulandı. Ölçülen en uzun self çalışma: depolama matrisi **0,968174 saniye**.
- `make test_tumu 2>&1 | tee build/tumu.log`, hemen ardından `rc=${PIPESTATUS[0]}`: **rc=0**; `Tum testler gecti!`. **FIXPOINT ✓**: 97.358 satır birebir; birleşik self-driver FIXPOINT de geçti.
- Başarısız kapı: **0**. Mevcut QEMU çekirdek kapıları: ARM64 **2/2**, x86_64 **1/1**. ASan E2E **PASS=128, FAIL=0**; ASan matrisi **12/12**.

### Mevcut atlama/muafiyet dökümü

Sayılar benzersiz dosya değil, kapı çalıştırması başınadır; tekrarlanan self-driver ölçümleri ayrı sayılır. Yeni ChatGPT sürücü köprüsünde atlama yoktur.

| Kapı | Atlama | Açıklama |
|---|---:|---|
| Codegen semantik (doğrudan) | 4 | 2 yabancı mimari + 2 C oracle modülü |
| Birleşik self-driver LLVM (iki çalıştırma) | 8 | her çalıştırmada aynı 2 mimari + 2 C oracle |
| Geniş codegen | 9 | mevcut oracle/bare-metal atlamaları |
| Yapısal IR | 2 | oracle IR/define yok |
| Sürücü yapısal paritesi | 1 | define yok |
| Eski VirtIO host-mock koşumu | 5 | C oracle --check reddediyor |
| Bölge operandı | 4 | harness atlamaları |
| ASan E2E | 32 | harness SKIP sayacı |
| **Toplam** | **65** | Mevcut harness davranışı; kapılar değiştirilmedi. |

- Ayrı muafiyetler: yapısal IR **24**, sürücü paritesi **1**, geniş check **18**, tip kontrol **9** → **52 muaf ölçüm**. ASan ayrıca **4 ALLOW(bilinen)** ve **2 SIZINTI-MUAF** bildiriyor.
- Lean: **32 dosyada 0 sorry/admit** taraması geçti; `lean/lake` olmadığı için gerçek Lean tip denetimi **yapılmadı**. Bu, yukarıdaki fikstür atlama toplamından ayrı bir doğrulama eksikliğidir.
- Self-driver alt logu `build/selfdrv.xcpYyi/llvm.out`, doğrudan codegen özetinde sayılmayan iki C-oracle modül atlamasını da gösteriyor.
- A0 düzeltmesinden sonra `virtio_blk_init_test` muafiyeti gereksiz kaldı; harness yasağı nedeniyle muafiyet listesi değiştirilmedi.

### Kapsam ve kalan doğrulama

- Kapsam denetimi temiz; Makefile farkı yalnız `CHATGPT_TESTLER`. Derleyici, runtime, kapı betikleri ve diğer worktree dosyaları değiştirilmedi.
- 18 Python kâhini/üreticisi rc=0; yeniden üretimde bayt farkı yok.
- Derleyici bulguları **F001–F005**, kullanılan dolanmalar ve **QEMU’da doğrulanacak** alanlar [bulgular dosyasında](https://github.com/msyrt-sys/kemgu/blob/codex/surucu-dalga4/otomasyon/chatgpt/codex4_bulgular.md). Yeni NVMe/AHCI/xHCI donanım doğrulaması sahte aygıt başarısıyla eş tutulmadı.
- Yerel kanıtlar: `build/codex4/*-{yesil,sabotaj,geri,kopru}.log`, `*-sure.txt`, `build/tumu.log`, `build/codex4/test_tumu.rc`.
- Tam test günlüğü SHA-256: `e8a0eaf4535c868d5e5fac506ff13bf47be71e8f67b2808ee42dbb3c35919312`.
- Bitiş: **2026-11-07T07:57:24.893729+00:00**. Worktree çalışma süresi **1 sa 45 dk**; ön koşul bekleme ayrıca **50 dk**.
- Dal `codex/surucu-dalga4`; tek draft PR. Auto-merge açılmadı, merge yapılmadı.
