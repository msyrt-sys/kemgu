# Codex 3 karar ve ölçüm kayıtları

Görev 3 ve Görev 2 baştan sona yeniden okundu; CLAUDE UTF-8, ölçüm araçları ve tek checker kaynağı bölümleri, bulgu raporu, yetki ödünç tasarımının tamamı okundu. Görev 3’ün açık kapsamı önceki alan/dal kısıtlarının yerine uygulandı. Dondurulmuş selfhost/checker.kem değiştirilmez.

Doğrulama sınırı: ortamda lean/lake yok. make test_tumu içindeki Lean kapısı 32 dosyada 0 sorry/admit ve lakefile require artığı kontrolüdür; gerçek Lean tip denetimi olarak raporlanmaz.

## 1b / C007 — kalan T002’nin kökü

183d64c tabanındaki tests/drivers/virtio/virtio_blk_init_test.kem doğrudan C --check: önce rc=1, T011×1 + T002×5; C007 onarımından sonra rc=1, T011×0 + T002×5. Self --check iki ölçümde de rc=0 / OK.

T002’ler testin 98–102. satırlarındaki çıplak blk_cfg_adres çağrılarıdır. İşlev drivers/virtio/virtio_blk_config.kem:29 içinde genel tanımlıdır; testin seçili ithalatları bu işlevi getirmez. Ad alanlı ithalat kuralında geçişli yükleme bu çıplak adı görünür kılmaz. Görev gereği test/ithalat düzeltilmedi. Sürücü harness’ı kendi birleştirme bağlamında artık bu dosyayı eşleştiriyor (17/17, 1 muaf, 1 atlandı); doğrudan dosyanın T002’lerini çözmüş gibi sayılmadı. Harness’ın “muafiyetten çıkar” önerisi uygulanmadı; muafiyet/harness düzenleme yasağı korundu.

Kapsam kontrolü: bağımlılığı girişte önce yüklenen ama API dosyasında kendi yapı ithalatı eksik olan yerel probda C’nin T002 1:43 çıktısı onarım öncesi/sonrası birebir. Düz-ad imza fallback’i ve yapı kurucusu görünürlüğü değiştirilmedi.

## 1c — imza kapsamı ve sabotajla güçlendirilen fikstür

183d64c tabanında seçili dosya_sil(tam32) self checker’da yerleşik metin parametresine takılıyor; yanlış metin argümanı ise sessizce kabul ediliyordu. Nitelikli biçim doğruydu. metin_buyuk() tam64 dönüşü aynı adlı yerleşikle ikinci bir ayırt edici çakışmadır. Özel üye ithalatında C ve self T041 + yerleşik T001 verir; reddedilen üye yerleşiği gölgelememeli. Onarım yalnız seçili, kayıtlı ve özel olmayan üyenin kanonik imzasını arite ve dönüş sorgusuna taşır.

İlk dönüş-sorgusu sabotajı derlenip yeşil kaldı: karşılaştırma tek başına dönüş tipi denetimini korumuyordu. Koşum durduruldu; açık tam64 değişken bağlaması içeren yeni dosya eklendi. Sabotaj artık derleme rc=0 / checker 264/265 rc=2 (T001 3:5). Onarım yeşil 265/265, modül 47/47. Son sekiz kapı ve tam takım baştan çalıştırıldı; kesilen ara koşum başarı olarak sayılmadı.

## 2 — yetki ödüncü: C aşaması

VARSAYIM: tasarımdaki f(&y), canlı ve adlandırılmış sahip bağlamasını ödünç verir. Sahipsiz geçici üreticiden referans alma eski CP005 davranışında kalır. Yerel ödünç takma adı kaçış değildir; dönüşte ve kapsayıcılarda türü izlenir. Ek referans katmanı, dereference ile sahip kopyası, ham işaretçiye cast veya sonuç/seçimlik/çeşit yüküyle sarmalama CP006’yı aşamaz. Özdeş tipe cast kaçış sayılmaz.

C ilk fikstür kapısı: eski 40 test yeşil, yeni 16 test kırmızı (40/56, rc=2). Son C kapsamı 60/60; eski 40 test metni yedekle birebir. Korpusta 21 yeni dosya var. Tüm CP006 yasakları ve ilave sarmalama yolları ayrı ölçülür. Birim test tanı kodunun yanında toplam tanı sayısını da doğrular; küresel referans bildiriminin mevcut E011’i korunur.

16 ayrı C sabotajında derleyici build rc=0, birim kapı rc=2. Kanal sabotajında doğrudan guard ile ortak tüketim guard’ı birlikte kaldırılır: birinin kalması CP006’yı koruyordu. İlk yalnız-tanı-kodu testi, değerle parametre yolunda ek sahte T001’i kaçırıyordu; toplam tanı sayısı denetimi bu boşluğu kapattı. Sabotaj sonrası kaynak geri yüklendi, nesne/ikili dosyaları silindi, 60/60 yeniden yeşil.

Self-host checker son hedefli ölçüm: 277/277; LLVM davranış: 195/195 (2 yabancı mimari atlandı). Tam kaynak geri alma sabotajı build=0/checker=2, C ve self `delege` işaretçi yolu sabotajları ayrı ayrı build=0/codegen=2; yeni fikstür her ikisinde LINK-RED. Kaynaklar geri kondu ve ikililer yeniden üretildi.

Sürücü göçü: virtio_mmio_status_yaz, &yetki<MMIO> alır; beş çağıran alt-yetkisini çağrıdan sonra iptal eder. Eski yorum “iade” dese de eski gövde geri_al çağırıyordu (sahibe dönüş değil iptal). Testler ve alt-makefile aynen korundu; iki sürücü testi exit=0.

## 3 — FAT32 profili (onarım öncesi)

Taban 183d64c; derleme ve link çalışma süresinden ayrı ölçüldü. Üç koşumun tamamı exit=0, C/self stdout birebir.

- fat32_coklu_test / kemgu: [6.7711162969935685, 6.691078688018024, 7.063119848957285] saniye; medyan 6.771; derleme 0.010, link 0.060.
- fat32_coklu_test / codegen: [15.185748725081794, 14.839891278999858, 14.427259713993408] saniye; medyan 14.840; derleme 0.393, link 0.066.
- fat32_yaz_test / kemgu: [1.4755858379648998, 1.4742722329683602, 1.471567823085934] saniye; medyan 1.474; derleme 0.012, link 0.061.
- fat32_yaz_test / codegen: [12.251755483914167, 12.975106394966133, 13.066574057913385] saniye; medyan 12.975; derleme 0.444, link 0.062.

Yalnız build/ IR kopyalarına işlev giriş/çıkış sayacı eklendi. Sayaç aracı önce küçük programda 2 iç çağrı/1 main ile doğrulandı. Enstrümante stdout asılla birebir. Çoklu: oku_le32 9.338.769 çağrı, 7,479 s öz/%41,49; oku_u8 44.608.863 çağrı, 4,838 s/%26,84; imaj_fat_oku 9.313.644 çağrı. Yazma: oku_le32 2.273.947 çağrı, 6,387 s/%46,21; oku_u8 21.605.143 çağrı, 5,388 s/%38,98; CRC32 %7,57. Enstrümantasyon süreleri hedef ölçümü değildir. IR incelemesi bu tahsissiz okumalarda her işlev ve oku_le32 döngü turunda bölge oluşturma/serbest bırakma olduğunu doğruladı. İlk seçenek: mevcut çıplak qualifier ile yalnız bu iki sıcak okuma yardımcısında bölge maliyetini kaldırmak; algoritma, FAT tarama sayısı ve testler aynı kalacak.


## Son süreler (üç koşum medyanı, saniye)

| Test | Derleyici | Önce | Sonra | Hızlanma |
|---|---|---:|---:|---:|
| fat32_coklu_test | kemgu | 6.771 | 1.125 | 6.02× |
| fat32_coklu_test | codegen | 14.840 | 2.084 | 7.12× |
| fat32_yaz_test | kemgu | 1.474 | 0.722 | 2.04× |
| fat32_yaz_test | codegen | 12.975 | 1.478 | 8.78× |

Son self üçlü ölçümleri çoklu 2,080–2,127 s, yazma 1,478–1,479 s. C/self ve önce/sonra stdout birebir; iki yardımcıda her iki derleyici bölge oluşturma/serbest bırakma sayısı 0. Sabotaj build=0, performans rc=1; geri yükleme final kapısı rc=0.

Bağımsız kâhinler: fat32_kahin.sh rc=0, fsck.fat temiz (3 dosya, 3/68890 küme); fat32_coklu_kahin.sh rc=0, fsck.fat temiz (114 kayıt, 134/68890 küme), Python okuyucu 111 dosya + 2 alt dizin ve 68756 boş kümeyi doğruladı. mcopy/mdir kurulu olmadığından ilk kâhinde 3, çoklu kâhinde 2 ATLANDI satırı var; bu dış içerik kontrolleri yapılmış sayılmadı. Standart fsck ve bağımsız Python FAT/LFN/içerik denetimleri çalıştı.

Süreler bu ARM64 Linux çalışma alanında, harness ile aynı clang varsayılan optimizasyonuyla ölçüldü. 3 saniye kapısı duvar saatini ölçer; farklı donanım/yükte yeniden ölçülmelidir. Bu iki işlevin mevcut gövdeleri tahsis yapmaz; ileride gövdeye tahsis eklenirse bölge gereksinimi yeniden değerlendirilmelidir.
## 4 — özgün örneklerin yeniden ölçümü

Taban 183d64c. Kod blokları codex_bulgular.md içinden değiştirilmeden çıkarıldı; SHA256 ile kaydedildi. C009 çalıştırılmadan önce iki IR’da fat32_yaz.dosya_sil çağrısı ve host kdl_dosya_sil çağrısının yokluğu doğrulandı.

| Örnek | C check / LLVM / link / exit | Self check / LLVM / link / exit |
|---|---|---|
| C001 | T024×1 / 1 / — / — | OK / 0 / 0 / 0 |
| C002 | OK / 0 / 0 / 0 | OK / 0 / 0 / 0 |
| C003 | OK / 0 / 0 / 0 | OK / 0 / 0 / 0 |
| C004 | OK / 0 / 0 / 0 | OK / 0 / 0 / 0 |
| C005 | OK / 0 / 0 / 0 | OK / 0 / 0 / 0 |
| C006 | OK / 0 / 0 / 0 | OK / 0 / 0 / 0 |
| C007 | T011×9, T020×1 / 1 / — / — | OK / 0 / 0 / 0 |
| C008 | OK / 0 / 0 / 0 | OK / 0 / 0 / 0 |
| C009 | OK / 0 / 0 / 0 | OK / 0 / 0 / 0 |
| C010 | OK / 0 / 0 / 0 | OK / 0 / 0 / 0 |
| C011 | OK / 0 / 0 / 0 | OK / 0 / 0 / 0 |

C001 nitelikli çağrı korunur (karar T024). C007 açık yapı ithalatı korunur: PR #177 henüz bu tabanda yok; C’de T011×9 + T020×1. Self C001 paritesi #176’da ayrı düzeltilmiştir. Önceki dallardan commit taşınmadı.

VARSAYIM: bu iş davranış değişikliği değil kaynak dolanma temizliğidir. Yeni asıl API davranış fikstürünün eski kodda da yeşil olması beklenir; kırmızı/yeşil kabul kapısı eski API adlarının ve probe dizininin yokluğunu denetler. Mevcut harness değiştirilmez; bağımsız kaynak sözleşmesi eklenir.

Temizlik sonrasında özgün örnekler tekrar koşuldu: dokuz kapalı kusur iki derleyicide check/LLVM/link/exit=0; C001/C007 önceki tabloyla aynı. Kaynak kapısı ilk 242 ihlal/rc=1 → sıfır/rc=0. Yeni davranış fikstürü iki derleyicide exit=42; modül kapısı 41/41. Dört ayrı kaynak sabotajında compiler build ve C/self LLVM rc=0, kaynak kapısı rc=1. 20 mevcut testte sayı/metin sabitleri birebir; Python üreticilerinin yalnız ürettiği kaynak çağrıları/yorumları güncellendi, harness mantığı değişmedi. C001 ve C007 ithalat dolanmaları gerekçeli yorumla kaldı.


Ek doğrulama: 13 mevcut Python üreticisinde metin sabitleri dışındaki AST önce/sonra birebir; üreteç/harness kontrol akışı değiştirilmedi.
## Son durum ve kalan sınırlar

- İstenen altı uygulama işi tamamlandı; 60 dakika takılma nedeniyle atlanan iş yok. PR #176–#181 birbirinden bağımsız origin/main tabanlarından açıldı; hepsi oluşturulurken draft ve auto-merge=null olarak doğrulandı.
- C007 kapsam dışı T002×5, blk_cfg_adres için eksik seçili ithalattan gelir; görev gereği düzeltilmedi. Temizlik dalı #177’yi içermediği için C007 açık tip ithalatları korunur. C001 nitelikli çağrı zaten kabul edilen T024 tasarım kararıdır.
- CP006 yalnız yeni tanı kodudur. D numarası atanmadı; frozen checker, CLAUDE, LOOP, workflow veya mevcut harness/muafiyet değiştirilmedi. Ana çalışma ağacına ve codex/codex2 çalışma ağaçlarına yazılmadı.
- Lean/lake yok: yalnız mevcut metinsel Lean kontrolü çalıştı. mtools yok: FAT32 kâhinlerinin toplam beş ek dış içerik kontrolü atlama satırı raporlandı; iki fsck.fat ve bağımsız Python çoklu dosya denetimi geçti.
- Her tam takımda istenen grep 32 satır eşleştirdi; bu benzersiz atlanan test sayısı değildir. ASan özeti ayrıca 32 SKIP, 0 FAIL verir; diğer muafiyet/atlamalar her PR’ın ham özetinde vardır.
- Merge, main’e push ve auto-merge yapılmadı. Son özet PR yalnız bu iki raporu içerir; uygulama kodunu birleştirmez.

Son dal denetimi: altı uygulama dalının origin/main üç-nokta diff’lerinde selfhost/checker.kem, CLAUDE.md, LOOP.md, Makefile, .github/ veya mevcut *_harness.sh değişikliği yok. #176–#181 uzaktan tekrar doğrulandı: draft=true, state=open, merged_at=null, auto_merge=null.

## Son özet doğrulaması ve toplam süre

Özet dalı codex/d3-ozet, güncel origin/main 183d64c tabanından açıldı; yalnız iki rapor dosyası içerir. Bu dalda da sekiz kapı ve tam make test_tumu seri koşuldu: PIPESTATUS=0/0, FIXPOINT ✓ 95.843 satır. Checker 256/256, modül 40/40, self-driver checker 199/199 ve LLVM 194/194; köprü kart+91. QEMU ARM64 2/2+x86_64 1/1; ASan 128 geçti/0 başarısız/32 atlandı, matris 12/12. İstenen tarama 32 eşleşen atlama satırı, pozitif başarısızlık sayacı 0.

Son doğrulama bitişi: 2026-11-06T22:16:38.772215+00:00. Başlangıçtan itibaren toplam duvar saati **4 saat 2 dakika 33 saniye** (14553 s); yaklaşık 75 dakikalık ön koşul bekleyişi dahildir. Altı uygulama ve bir özet dalının rc/FIXPOINT kayıtları topluca yeniden doğrulandı. Açık uygulama işi veya takılma kaydı yok; raporlanan kapsam dışı/ortamsal sınırlar yukarıdadır.
