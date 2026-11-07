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
