# Dalga 18 ilerleme kaydı

Başlangıç 2026-11-26T14:07:15+03:00

Çalışma tabanı `0ce56bf` (D-772). Araç derlemesi geçti. İlk tam koşum 155 kapıda, 37 dk 31 sn ve `make rc=0` ile bitti.

## Bölüm 1 — ölçüm tabanı ve süre bütçeleri

Üç gerçek `-j10` koşumunda aynı 155 kapı ölçüldü. Süreler ve make çıkış kodları 37 dk 31 sn (0), 35 dk 00 sn (2), 34 dk 18 sn (0). İkinci koşumda `semafor_n` 20 yinelemenin birini reddetti. Aynı kaynakla üçüncü taban ve 100 ek yineleme geçti. Ham raporlar, her kapının ortalaması ve en büyük sapması bulgular dosyasında.

İzlenen bütçe tablosu 156 kapı içerir. Yeni konak kapısı sabit kayıtlarda süre toplamını, sınırda eşitliği, aşımı, eksik bütçeyi, başarısız hedefi ve boş kaydı doğruladı.

`calistir_kapi_butcesi`, `calistir_cekirdek_onbellek`, `calistir_bm_x86_v1`, `calistir_bm_x86_uefi_pc`, `calistir_bm_x86_saglamlik`, `calistir_bm_x86_cok_surec`, `calistir_bm_x86_boru`, `calistir_bm_x86_program15` ve `calistir_kem_os_x86` birlikte geçti.

İki sabotaj uygulandı. Gerçek 120 sn bekleme, 60 sn bütçeli kapıyı 120,022 sn yaptı ve sıkı rapor 3 döndürdü. Bütçe tablosunun kopyasından aynı kapı silinince rapor “bütçe yok” uyarısı verdi.

Bölüm süresi 1 sa 57 dk 15 sn. Son kapı takımı 10 dk 18 sn, make çıkış kodu 0.

## Bölüm 2 — geçici dizinler ve disk denetimi

67 kabuk betiği, derleme ölçüm harness’ı ve 76 QEMU Python modülü dosya sahipliği bakımından incelendi. Başarıda koşum dizinleri temizleniyor, hatada yol ve günlükler korunuyor. Ölçümlü takım disk büyümesini ve geride kalan dizinleri raporluyor.

Önceki koşumlardan kalan 93 dizin arşivlendi. MSI temizliğini kaldırma sabotajında gerçek QEMU kapısı geçti, disk kapısı 4 dizini adıyla bildirdi (tarif rc 1, make rc 2). Kaynak geri alındı.

Diğer bir tam takımla örtüşen ilk `-j10` ölçümünde 7 kapı başarısız oldu (rc 2). Bildirilen 13 tanı dizini saklandı. İkinci ölçümde program15 kapısının S5 kapanış denetimi hata verdi. Tekil tekrar geçti. Üçüncü `-j10` ölçümü rc 0 ile geçti. Bölümün standart QEMU ve önbellek kapıları bu tam koşumda, `calistir_kem_os_x86` ayrıca geçti.

```text
=== ZAMAN RAPORU: duvar saati 35 dk 57 sn · 155 kapı · kapı toplamı 3 sa 54 dk 12 sn · derleme/diğer 00 dk 00 sn ===
Disk build: önce 7599525888 B, sonra 7600599040 B, büyüme +1073152 B
Geçici dizin: önce 0, sonra 0, yeni 0
```

Bölüm süresi 2 sa 9 dk 20 sn.

## Bölüm 3 — ortak çekirdek önbelleği

UEFI ve sekiz PVH girişinin anahtar ve ürün kuralları ortak kütüphaneye taşındı. İçe aktarılan modül değişiklikleri de kaydı geçersiz kılar. Ekran yaması, MSI yaması, üç girişte kaynak/derleyici/nesne değişiklikleri ve sıcak ürünlerin önbelleksiz derlemeyle bayt eşitliği doğrulandı. İki gerçek sabotajda kapı rc=1 verdi (nesne farkında ve MSI yamasında yanlış isabet).

Sekiz PVH girişinde dokuz yolun toplamı 23,220 sn → 0,746 sn. Host önbellek deneyi dışındaki tam takımda isabet 122 → 133, gerçek çekirdek derlemesi 11 → 0. İlk tam koşumda görülen PCI16 ve S5 hatalarının günlükleri ve düzeltmeleri bulgular dosyasında. Yeni tam takım rc=0 ve §7 kapıları yeşil; ayrıca `calistir_kem_os_x86` rc=0, 28 sn. Bölüm süresi 1:19:29.

```text
=== ZAMAN RAPORU: duvar saati 34 dk 04 sn · 155 kapı · kapı toplamı 3 sa 49 dk 04 sn · derleme/diğer 00 dk 00 sn ===
       16 dk 48 sn  %49.3  calistir_bm_x86_ag_saglamlik
       12 dk 56 sn  %38.0  calistir_bm_x86_kaynak
       12 dk 40 sn  %37.2  calistir_bm_x86_ps2_irq
       11 dk 26 sn  %33.5  calistir_bm_x86_kabuk14
       10 dk 45 sn  %31.6  calistir_bm_x86_v1
       10 dk 20 sn  %30.3  calistir_bm_x86_ps2_karma
       09 dk 48 sn  %28.8  calistir_bm_x86_cok_surec
       09 dk 01 sn  %26.5  calistir_kem_os_pc_matris
       07 dk 20 sn  %21.5  calistir_bm_x86_ekran_fare
       07 dk 15 sn  %21.3  calistir_bm_x86_dosyalar
       06 dk 33 sn  %19.2  calistir_bm_x86_tftp_teshis
       06 dk 06 sn  %17.9  calistir_bm_x86_tcp_verim
       05 dk 30 sn  %16.2  calistir_codegen_bootstrap
       05 dk 24 sn  %15.8  calistir_bm_x86_uefi_kabuk
       05 dk 09 sn  %15.1  calistir_bm_x86_surec
  disk: build 9.2G · boş 444 GiB
Disk build: önce 9785061376 B, sonra 9786150912 B, büyüme +1089536 B
Geçici dizin: önce 0, sonra 0, yeni 0
```

## Bölüm 4 — kritik yolun bölünmesi

Sekiz eski hedef 21 bağımsız parçaya açıldı. Önce/sonra 288 kâhin işlemi aynı, gerçek QEMU günlüklerinde 302 sonuç eşit. Eksik senaryo sabotajı kapıyı adıyla kırmızıya düşürdü (make rc=2). En uzun parça 509.68 sn. Tam takım ve §7 kapıları rc=0, ayrıca `calistir_kem_os_x86` rc=0. Bölüm süresi 0:29:20.

```text
=== ZAMAN RAPORU: duvar saati 25 dk 08 sn · 169 kapı · kapı toplamı 3 sa 53 dk 13 sn · derleme/diğer 00 dk 00 sn ===
       08 dk 30 sn  %33.8  calistir_bm_x86_ag_saglamlik_self_aktarim
       07 dk 24 sn  %29.4  calistir_bm_x86_ag_saglamlik_c_aktarim
       07 dk 12 sn  %28.6  calistir_bm_x86_ekran_fare
       07 dk 09 sn  %28.4  calistir_bm_x86_dosyalar
       07 dk 04 sn  %28.1  calistir_bm_x86_kaynak_editor
       06 dk 36 sn  %26.2  calistir_bm_x86_tftp_teshis
       06 dk 27 sn  %25.7  calistir_bm_x86_tcp_verim
       06 dk 24 sn  %25.4  calistir_bm_x86_ps2_irq_self
       06 dk 17 sn  %25.0  calistir_bm_x86_v1_tam
       06 dk 11 sn  %24.6  calistir_bm_x86_ps2_irq_c
       05 dk 55 sn  %23.5  calistir_bm_x86_kaynak_cocuk
       05 dk 52 sn  %23.3  calistir_bm_x86_kabuk14_6
       05 dk 52 sn  %23.3  calistir_bm_x86_kabuk14_1
       05 dk 30 sn  %21.9  calistir_codegen_bootstrap
       05 dk 22 sn  %21.4  calistir_bm_x86_uefi_kabuk
  disk: build 9.2G · boş 444 GiB
```

## Bölüm 5 — beklemeler ve RTC

68 eski bekleme envanterde. QEMU seri, piksel, kesme, QMP soketi, HALT ve konuk zamanlayıcı koşulları üst sınırla denetleniyor. RTC iki derleyiciyle 20/20 geçti. İki RTC durdurma deneyi ve olanaksız hazır işareti doğru yerde kırmızıya düştü. Üçer ölçümlü tam takımda make ve tee çıkış kodları 0. Duvar süreleri (sn) 1508, 1545, 1504 → 1383, 1393, 1381. Ayrı ara `test_tumu`, tip kontrolü, bare-metal farkı ve x86 çekirdek kapısı rc=0. Süre 2:47:28.

```text
=== ZAMAN RAPORU: duvar saati 23 dk 01 sn · 169 kapı · kapı toplamı 3 sa 32 dk 08 sn · derleme/diğer 00 dk 00 sn ===
       08 dk 37 sn  %37.5  calistir_bm_x86_ag_saglamlik_self_aktarim
       07 dk 19 sn  %31.8  calistir_bm_x86_ag_saglamlik_c_aktarim
       06 dk 32 sn  %28.4  calistir_bm_x86_ekran_fare
       06 dk 29 sn  %28.2  calistir_bm_x86_tcp_verim
       06 dk 23 sn  %27.7  calistir_bm_x86_tftp_teshis
       06 dk 14 sn  %27.1  calistir_bm_x86_ps2_irq_self
       06 dk 11 sn  %26.8  calistir_bm_x86_dosyalar
       05 dk 49 sn  %25.3  calistir_bm_x86_ps2_irq_c
       05 dk 34 sn  %24.2  calistir_codegen_bootstrap
       05 dk 27 sn  %23.7  calistir_bm_x86_kaynak_editor
       05 dk 18 sn  %23.0  calistir_bm_x86_v1_tam
       05 dk 08 sn  %22.3  calistir_bm_x86_kabuk14_1
       05 dk 08 sn  %22.3  calistir_bm_x86_kabuk14_6
       05 dk 05 sn  %22.1  calistir_bm_x86_uefi_kabuk
       04 dk 54 sn  %21.3  calistir_bm_x86_ps2_karma_self
  disk: build 23G · boş 431 GiB
Disk build: önce 23757295616 B, sonra 23758569472 B, büyüme +1273856 B
Geçici dizin: önce 0, sonra 0, yeni 0
```

## Bölüm 6 — paralel kaynaklar ve stres

Paylaşım denetimi 428 sabit kaynak ailesini ve 1.283 sahiplik kaydını taradı. Kalan çakışma 0. PL011 nesnesi, DTB çalıştırmalarının diski, UEFI ürünleri, süre günlüğü ve QEMU/host dinleyicileri için üretim ve kullanım sınırları düzeltildi. Envanter ve tanı sonuçları bulgular belgesinde.

Aynı çalışma dizini verilen iki gerçek kapıyı statik denetim adlarıyla bildirdi. Aynı değişiklikle paralel koşumda parser 2/15, checker 311/321, make rc=2. Özel dizinler geri gelince 15/15 ve 321/321 geçti. Altı qtest aygıtı, iki derleyici ve 24 ayrı TCP dinleyicisiyle ek koşumlar geçti.

Streste QMP'nin dinlemeye başlaması, 100 çocuğun PID ilerlemesi ve iki host eşzamanlılık gözlemcisi düzeltildi. Son ağaçta arka arkaya iki tam `-j20` koşumu 14 dk 59 sn ve 15 dk 10 sn, ikisinde de make/tee rc=0. Ölçümlü `-j10` koşumu 23 dk 24 sn, make/tee rc=0. 170 kapı içinde en uzunu 8 dk 41 sn. Ölçüm sonunda geride kalan koşum dizini 0. Tek işli tam koşum 4 sa 48 dk 11 sn, make/tee rc=0. §7 ek x86 çekirdek ve ARM DTB kapıları 55 sn sürdü. Üç isteğe bağlı DTB deneyi ilk koşumda sistemde dtc bulunmadığı için atlandı. Çalışma dizininde dtc 1.7.0 ile yinelenen gerçek ARM kapısı, hücre genişliği/GIC adı, 2 GiB bellek ve aygıt adres çevirisini 129 sn içinde doğruladı. Bölüm süresi 6 sa 46 dk.

## Bölüm 7 — değişen dosyalardan kapı seçimi

21 sürücü alanı için tablo Make tariflerinden, harness girdilerinden ve KEMGU içe aktarmalarından üretiliyor. Eşlenmeyen dosya tam takımı seçiyor. Son 30 commit kendi kaynaklarıyla denetlendi. 26 tam takım, dört belge seçimi, eksik QEMU kapısı 0. Yalnız sürücü değiştiren commit bu aralıkta yoktu. Beş sürücü alanını ayrıca sabit örneklerle ölçtük.

Gerçek `make test_secili` girişinde yalnız `LOOP.md` farkı iki kapı seçti (72 sn), yalnız `drivers/sim/kabuk.kem` farkı tek köprü kapısı seçti (151 sn), ikisi de rc=0. Sürücü satırını silme deneyinde seçici tam takıma düştü, seçim denetimi beklenen tek kapının eksikliğini bildirdi (make rc=2). Satır geri gelince kapı yeşil. §7 ortak x86 kapıları, paylaşım denetimi ve yeni seçici birlikte 317 sn, make/tee rc=0. Bölüm süresi 10 dk 49 sn.

## Bölüm 8 — CI paralelliği ve test altyapısı belgesi

İki işte altışar yapım/test çağrısı paralel ve hedefe göre eşzamanlanmış çıktıyla çalışıyor. Linux ölçümlü tam takımın raporunu basıyor. YAML karşılaştırması ve gerçek tariflerden alınan komutların başarı/hata deneyi geçti. §7 x86 kapıları ve kapı seçici 313 sn, make/tee rc=0. Bölüm için ayrıca sabotaj tanımlanmamıştı. Ölçüm, bütçe, önbellek, paylaşım, kapı seçimi ve CI kullanımı `belgeler/KEMGU_Test_Altyapisi.md` içinde.

Taban Actions koşumunda Linux işi 35:17 (tam takım adımı 35:00), Windows işi 1:03:43 (adım 1:02:45). İki ham günlük ve adım zamanları incelendi. Değişiklik sonrası Actions süresi, Bölüm 20 sonunda açılacak draft PR ile ölçülebilir. Dal öneki mevcut push tetikleyicisine girmiyor. Yerel bölüm süresi 6 dk 11 sn.

## Bölüm 9 — işlemci keşfi ve SMP kâhini

BSP kimliği LAPIC yazmacından, işlemci listesi UEFI MADT'den geliyor. C ve self-host UEFI imajlarının her birinde 1/2/4/8 vCPU için ham MADT, QOM APIC kimliği, QMP fiziksel kayıt ve seri liste eşleşti. Her vCPU ayrı TCG host iş parçacığına sahipti. Bu koşum ARM host üzerinde x86 emülasyonudur.

Bit 1'i yok sayma sabotajı, gerçek firmware bütün kayıtlarda yalnız bit 0'ı verdiği için dört topolojide sessiz kaldı. Host'ta yalnız bit 1'i açık kayıt ile iki derleyici de rc=3 verdi. Ek deneyde etkin APIC 1 listeden çıkarılınca iki vCPU'lu QEMU kapısı rc=1 verdi. Normal keşif kapısı isabette 26 sn, yeni önbellekle 37 sn, bütçe 60 sn. §7 x86 kapıları, üç sans-IO kapısı, paylaşım ve seçim denetimleriyle 315 sn, make/tee rc=0. Bölüm süresi 9 dk 57 sn.

## Bölüm 10 — UEFI yolunda uygulama işlemcilerini başlatma

C ve self-host ile 2/4/8 vCPU üzerinde gerçek UEFI çekirdeği açıldı. QMP işlemci başına uzun kipi, CR3'ü, ayrı yığını ve gerçek APIC kimliğini doğruladı. Dört vCPU'lu süreç kabuğu da düzenli kapandı. EfiConventionalMemory sayfa çifti bulunamazsa BSP tek başına çalışmayı sürdürdü.

İkinci SIPI kaldırılınca altı konukta kapı sessiz kaldı. Ayrılmış yanlış vektör sayfasıyla gerçek AP onay zaman aşımı görüldü (rc=1). Gerçek BSP yığın tepesi aday olarak verildiğinde SIPI öncesi sınır denetimi düştü (rc=1). AP’yi canlı BSP yığınında çalıştırıp bellek bozma deneyi uygulanmadı. Tek vCPU'da süreç heap artışı yine 0 bayt. Mutlak boş çerçeve sayısı C'de 1, self-host'ta 3 azaldı. Yüklü ELF'nin fiziksel kapsamı sırasıyla 1 ve 3 sayfa büyüdü.

Ham `-j10 test_tumu` 1485 sn, ölçümlü takım 1386 sn, ek check/baremetal diff/x86 kapıları 28 sn. Her üçünde make/tee rc=0. SMP AP kapısı yüklü koşumda 35,36 sn. Önceki 57 sn'lik ölçümle bütçe 86 sn. QEMU'suz temiz PATH deneyi derleme yapmadan atlama mesajlarını verdi. Bölüm süresi 57 dk 55 sn.

```text
=== ZAMAN RAPORU: duvar saati 23 dk 06 sn · 173 kapı · kapı toplamı 3 sa 34 dk 08 sn · derleme/diğer 00 dk 00 sn ===
       08 dk 27 sn  %36.6  calistir_bm_x86_ag_saglamlik_self_aktarim
       07 dk 19 sn  %31.7  calistir_bm_x86_ag_saglamlik_c_aktarim
       06 dk 42 sn  %29.0  calistir_bm_x86_tcp_verim
       06 dk 31 sn  %28.2  calistir_bm_x86_ekran_fare
       06 dk 22 sn  %27.5  calistir_bm_x86_tftp_teshis
       06 dk 14 sn  %27.0  calistir_bm_x86_ps2_irq_self
       06 dk 10 sn  %26.7  calistir_bm_x86_dosyalar
       05 dk 50 sn  %25.2  calistir_bm_x86_ps2_irq_c
       05 dk 32 sn  %24.0  calistir_codegen_bootstrap
       05 dk 26 sn  %23.5  calistir_bm_x86_kaynak_editor
       05 dk 17 sn  %22.9  calistir_bm_x86_v1_tam
       05 dk 06 sn  %22.1  calistir_bm_x86_kabuk14_6
       05 dk 05 sn  %22.0  calistir_bm_x86_kabuk14_1
       05 dk 04 sn  %21.9  calistir_bm_x86_uefi_kabuk
       04 dk 55 sn  %21.3  calistir_bm_x86_ps2_karma_self
  disk: build 14G · boş 425 GiB
Disk build: önce 14470782976 B, sonra 14471913472 B, büyüme +1130496 B
Geçici dizin: önce 0, sonra 0, yeni 0
```

## Bölüm 11 — çekirdek başı tablolar ve yerel saat

C ve self-host ile ortak IDT, dört ayrı GS/GDT/TSS, 12 IST yığını ve fiziksel tık sayaçları doğrulandı. Dört vCPU'da C tık farkları 151/150/150/150, self-host 159/159/159/159. Normal kullanıcı ELF'sinde bir ve dört vCPU ile yedi sistem çağrısı ve doğru GS dönüşü ölçüldü. §7 kapıları ve ek istisna/ABI/keşif/başlatma kapıları 407 sn, make/tee rc=0.

AP saatleri kaldırılınca sayaçlar 159/0/0/0, kapı rc=1. BSP TSS adayı özel sayfa denetiminde reddedildi, AP onayı gelmedi (rc=1). Aynı TSS'i canlı çekirdeklere yükleyip yığın bozma sabotajı uygulanmadı. D-757 tek vCPU çift hata kapısı iki derleyicide 10/10 geçti. AP2 çift hata denemesinde QEMU'nun iothread denetimi süreci durdurdu. Ayrı bir deneyde gerçek IST1 kesmesi dört çekirdekte dört ayrı yığın ve sağlam kanarya gösterdi. PVH kapısının iki eşzamanlı çağrısındaki dosya çakışması süreçler arası kilitle giderildi. Bölüm süresi 26 dk 22 sn.

## Bölüm 12 — bilet kilidi ve gerçek çok çekirdekli sayaç

C ve self-host FIFO modelinde 100.000 geçiş, QEMU'da dört ve sekiz çekirdeğin her birinden bir milyon kilitli artış geçti. Dolu ve boş deneme, iç içe kesme durumu, 32 bit sarma ve biletler QMP ile doğrulandı. Sekiz vCPU tüm deney boyunca aynı anda yarıştığında C sayacı da 8.000.000 oldu (378,842 sn). Düzenli kapıda ilk 8.192 artış bütün çekirdeklerde, kalanlar ikili gruplarda gerçekleşti. Kilit kapısı son `-j10` ölçümünde 22,612 sn. TCG zamanlaması ve C18-001 yerleşik ad çakışması bulgu dosyasında.

Sayısal kontrolde kilit kaldırılınca 2.494.650 artış kaldı. Atomik önek kaldırılınca sayaç durdu (rc=1). 20→10 edinme gerçek hata ayıklama imajında panik verdi. Modelin FIFO denetimi kaldırılınca C ikilisi 4 döndü. Regresyonda PVH COM1 FIFO ve S5 beklemesi düzeltildi. Ekran kâhini hızlandırıldı ve eski görüntülerle her piksel karşılaştırıldı. §7'nin tamamı, iki sans-IO kapısı, ChatGPT köprüsü, seçici, paylaşım ve tip kontrolü son koşumda 319 sn, make/tee rc=0. Bölüm süresi 38 dk 28 sn.

## Bölüm 13 — işlemciler arası kesmeler

80/81/82 vektörleri, açılışta fiziksel RAM'den ayrılan çekirdek başı kutu, tek hedefe ve diğer çekirdeklere yayın, süreli onay ve dolu kutu hatası eklendi. Her derleyicide 4 vCPU ile 16.000, seyrek kimlikli 6 vCPU ile 36.000 çağrı sıralı ve eksiksiz ölçüldü. QMP tüm dönüşleri, hedeflerin sayaçlarını, yerel tıkı ve durdurulan AP'lerde IF=0/HLT=1 durumunu okudu. Son kapıda gidiş-dönüş medyanı 8,158–9,490 µs, p99 29,372–32,935 µs. ARM host üzerinde x86 TCG ölçümüdür.

EOI atlanınca sonraki teslim süre aşımı −4 ile kırmızı. Hedefe mantıksal numara gönderilince ardışık kimliklerde **sessiz**, 0,1,2,4,5,6 kimliklerinde kırmızı. §7, tip kontrolü, SMP tık/kilit ve IPI kapıları toplam 419 sn, make/tee rc=0. Bölüm süresi 13 dk 15 sn.

## Bölüm 14 — SMP bellek sahipliği

Çerçeve ayırıcı ve heap girişleri ayrı bilet kilitleri kullanıyor. Dört vCPU'da 40.000 tahsis/iade çifti, tüm olayların host'ta tekrarı, dört eşzamanlı sahip ve sonunda aynı fiziksel bitmap iki derleyicide doğrulandı. Tek vCPU'da 50 süreç için süreç heap artışı sıfır, kabuk heap değerleri D-772 örnekleriyle eşleşti. C'de 49 ve self-host'ta 71 ek boş çerçeve var. Her iki fark da ELF'in küçüldüğü sayfa sayısına eşit. Mutlak boş çerçeve sayısı D-772'den farklı.

Canlı ayırıcı kilidini kaldırma sabotajı uygulanmadı. Ayrı sayısal sahiplik deneyinde üç çekirdeğin çakışması kapıyı kırdı. §7, ARM çekirdek ve QEMU, tip, IPI ve bellek kapıları koşuldu. Toplu koşumdaki iki USB giriş kırmızısı için kâhin gerçek HID DMA raporunu bekliyor. İki hedef birlikte 250 sn, rc=0. Diğer hedefler 357 sn'lik koşumda yeşil. Ölçümler ayrı iş parçacıklı x86 TCG'de. Bölüm süresi 27 dk 52 sn.

## Bölüm 15 — TLB geçersizleme ve çerçeve iadesi

CPU0 ve CPU1 aynı kullanıcı sayfasını iki derleyicide de okudu. CPU0 yaprağı kaldırdıktan sonra AP1 uzak INVLPG ve onay verdi. Host sayfa tablolarını, ayırıcı bitini ve AP1'in sonraki erişiminde donanımın ürettiği #PF ile CR2'yi (0x8000000000) doğruladı. Önceden dolu IPI kutusu çerçeveyi tutuyor ve aynı işlem daha sonra tamamlanabiliyor. Dört vCPU'da W^X ve süreç yalıtımı geçti. Bölüm kapıları, §7, bellek, IPI, tip kontrolü ve bare-metal fark denetimi birlikte 466 sn, rc=0.

Uzak INVLPG'yi atlayarak eski eşlemeden okuma sabotajı uygulanmadı. Gerçek uzak geçersizlemeden sonra yalnız onay kaldırılınca kapı −4 bekleme sonucu ve ayrılmış çerçeve ile rc=1 verdi. Ara regresyonda USB giriş kaybı ortak kâhinde gerçek HID raporu beklenerek giderildi. TCP kapısı iki derleyici × iki pencere kipine bölündü. Önceki ve yeni Makefile aynı dört bağımsız kâhin çağrısını üretti. Ölçeksiz self-host çağrısının çıkarıldığı özel Makefile denetimde adıyla kırmızı oldu. Paket hazırlama sırasında geçici bölge tahsisi kaldırıldı. Her aktarımda 64 MiB veri, PCAP SHA-256 ve 0,25 MiB/s eşiği denetlendi.

Hub üzerinden ön plan dosya gezgininden kabuğa dönüşte basılı q tekrarının komut satırına sızması düzeltildi. İki derleyici × iki hub derinliği 193 sn, rc=0. Klavye tekrarında 68 tik isteyen host 87 tik ölçtüğünde 9 glif doğrudur. Kâhin gerçek LAPIC aralığıyla GOP'u karşılaştırıyor. PS/2 ve USB ile her iki derleyicide kısa basış tek, 68 tikte 5, 87 tikte 9 glif verdi. İlk gecikmeyi sıfırlayan çekirdek kısa basışta 6 glifle kırmızı oldu.

İlk ölçümlü takımda HTTP'nin son bağımsız senaryosu önceki bağlantılar TIME_WAIT'teyken sekiz TCP yuvasını doldurdu. Kâhin kapanmayı QMP'den bekliyor ve doğru isteğin sunucuya ulaştığını doğruluyor. İki derleyiciyle 283 sn, rc=0. Ölçümlü takımda 713 sn süren 100+100 MiB aktarım için alım DMA kopyası ve TCP'nin aygıt yoklamasındaki sırası düzeltildi. Ayrı self-host koşumunda aynı SHA-256, bağlantı kesintisi, yeniden iletim ve sabit kaynak sayılarıyla 466 sn ölçüldü.

Altıncı ham takımda konuk diskinde doğru olan 1 MiB TCP aktarımı, PCAP'taki geç gelen segment yüzünden reddedildi. İki yönde sıra numarası, yinelenen bayt eşitliği, kesintisiz 1 MiB ve FIN konumu doğrulandı. Aynı PCAP'tan bir segment çıkarıldığında 1460 baytlık boşluk bulundu. İki konuk kapısı iki derleyiciyle 333 sn, rc=0.

İkinci ölçümlü turda 181 işlev kapısı 20 dk 26 sn içinde geçti. Disk kapısı önceki kırmızı koşumların kökte kalan 33 tanı dizinini bildirdi. Dosya ve PCAP'lar bir dizin eşlemesiyle arşivlendi ve sıkı disk denetimi tekrarlandı.

Yedinci ham takımda yalnız Ethernet eşinin DHCP kirası başarısız oldu. Paketteki dört DISCOVER'a karşılık OFFER görülmedi. Eşin iki ucuna koşuma özel adres verildi. Host iş parçacığındaki ilk hata ve son konuk durumu görünür hale getirildi. Yeni kâhin dört DHCP mesajının sırasını ve işlem kimliğini PCAP'ta doğruluyor.

Ağ araçları kapısının iki derleyicili tekrarı 80 sn, rc=0. C için 0x173d59a0 ve self-host için 0x17406b0f işlem kimliklerinde DHCP paket türleri 1, 2, 3, 5. İki yönlü 1 MiB boru aktarımının iki SHA-256 değeri ve sekiz UDP sayaç artışı da doğrulandı.

64 MiB hız ölçümündeki host alıcısı geç gelen bir TCP segmenti nedeniyle 19 sıra dışı segmenti attı. Eş artık ilan ettiği pencereye uyan veriyi tutup kesintisiz aralığı onaylıyor. Eski kayıtta bu değişiklik o ana dek eksiksiz alınan veriyi 2.908 bayt artırdı. Hız eşiği, bağımsız PCAP, FIN ve SHA denetimleri korundu.

Canlı alıcı düzeltmesinin seçili tekrarı 108 sn, rc=0. 64 MiB için C ölçekli 48,572 sn (1,318 MiB/s) ve self-host ölçeksiz 95,887 sn (0,667 MiB/s). İkisi de aynı SHA-256 ile bitti, konuk heap'i ve boş çerçeve sayısı sabit kaldı.

x86–ARM canlı aktarımının bir koşumunda ARM'in 18 saniyelik ilerleme sınırı doldu. x86'nın sekiz tanımlayıcılı e1000 üzerinde 65.535 baytlık yankı penceresi ilan ettiği ölçüldü. Dahili yankı hizmetinin penceresi RX halkasıyla sınırlandı. Yeni ikilide QMP sekiz yuva, SYN-ACK 4.380 bayt gösterdi. Her iki yönde ve ARM diskinde 1 MiB doğruydu.

İki derleyiciyle temel TCP ve iki konuk denetimleri geçti. Önceki host eşinin ilan edilen pencereyi aşan açık aktarımı durduruldu. Pencereye uyan self-host 100+100 MiB tekrarı 510 sn, rc=0. Veri süresi 502,52 sn, SHA-256 `0742ba24…83d1e`, heap 4.066.976 ve boş çerçeve 45.455 olarak sabit kaldı.

Son ham -j10 koşumu 1309 sn, ölçümlü -j10 koşumu 1332 sn, her ikisi rc=0 ve tee=0. Bölüm süresi 5 sa 5 dk 39 sn.

```text
=== ZAMAN RAPORU: duvar saati 22 dk 10 sn · 181 kapı · kapı toplamı 3 sa 21 dk 58 sn · derleme/diğer 00 dk 00 sn ===
       08 dk 32 sn  %38.5  calistir_bm_x86_ag_saglamlik_self_aktarim
       06 dk 30 sn  %29.3  calistir_bm_x86_ekran_fare
       06 dk 23 sn  %28.8  calistir_codegen_bootstrap
       06 dk 12 sn  %28.0  calistir_bm_x86_tftp_teshis
       05 dk 30 sn  %24.8  calistir_bm_x86_ps2_irq_self
       05 dk 23 sn  %24.3  calistir_bm_x86_dosyalar
       04 dk 59 sn  %22.5  calistir_bm_x86_tcp_kayip
       04 dk 52 sn  %21.9  calistir_bm_x86_cok_surec_cocuk
       04 dk 41 sn  %21.2  calistir_bm_x86_apic
       04 dk 33 sn  %20.5  calistir_bm_x86_kaynak_cocuk
       04 dk 29 sn  %20.2  calistir_bm_x86_ps2_irq_c
       04 dk 21 sn  %19.6  calistir_bm_x86_v1_tam
       04 dk 19 sn  %19.5  calistir_kem_os_pc_matris_gpt
       04 dk 12 sn  %19.0  calistir_kem_os_pc_matris_mbr
       04 dk 09 sn  %18.7  calistir_bm_x86_klavye16
  disk: build 40G · boş 312 GiB
Disk build: önce 42618396672 B, sonra 42619535360 B, büyüme +1138688 B
Geçici dizin: önce 0, sonra 0, yeni 0
```

## Bölüm 16 — SMP süreç zamanlayıcısı ve ABI

Çekirdek başına hazır kuyruk, iş çalma, büyük çekirdek kilidi, yakınlık, dört yeni ABI çağrısı (91–94), `islemci` ve `donanim` satırları eklendi.

C ile dört hesap süreci tek vCPU'da 6.444 sn, dört vCPU'da 2.283 sn (2.823×). Self-host ile 5.646 ve 1.701 sn (3.319×). Tek çekirdek beş ayrı PID saydı. Dört farklı CPL3, CR3 ve PID aynı QMP duraklamasında okundu. Her çalışan vCPU'nun donanım TR tabanı ile özel TSS eşleşti, TSS.RSP0 hizalıydı ve o işlemcinin çekirdek yığını içindeydi. Dört sonuç bağımsız üçgensel sayı hesabına eşitti. x86 konuklar ARM hostta çok iş parçacıklı TCG ile koştu.

İş çalma kapalıyken yalnız CPU0 üzerinde bir çocuk görüldü, kapı rc=1 (20 sn). Göçte canlı GS güncellemesini kapatma sabotajı uygulanmadı. Eski çekirdek numarası göç denetimine verildiğinde kullanıcı girişinden önce `SMP goc cekirdek kimligi` ile kapı rc=1 (38 sn) verdi.

§7 ile dört vCPU'da eski hesap/arka/çocuk, boru, kabuk14, dosyalar, tık/TLB, iki bölüm düzeninde donanım, seçici, paylaşım ve tip denetimi 439 sn, make/tee rc=0. Bölüm süresi 19 dk 23 sn.

## Bölüm 17 — paralel kullanıcı programı

Bölütlü asal eleği iki derleyicide, 1/2/4/8 vCPU ve 1/2/4/8 çocukla 32 matris noktasında 2.063.689 verdi. Her dilim bağımsız Python eleğiyle eşleşti. 8 vCPU ve N=4 ile C 0,719 sn, self-host 0,622 sn. Tam tablolar bulgularda. Son parçayı toplamayan ELF 1.575.661 verdi, kâhin 7 sn içinde rc=1 döndü. Süre yalnız raporlanır.

Matris kapısı 83,58 sn. §7, tip, paylaşım ve seçici kapıları 271 sn, make/tee rc=0. Bölüm süresi 6 dk 50 sn.

## Bölüm 18 — kesme, kilit ve sistem çapında panik

Dört hesap PID'si çalışırken her derleyicide 32 fiziksel NVMe okuması, 32 MSI-X teslimi, 32 PS/2 kesmesi ve 21 ICMP cevabı doğrulandı. Kararlı C/self-host heap değerleri 4.591.680/4.788.528 bayt. Çerçeveler dört işlem toplandığında başlangıç değerlerine geri döndü. Panik sahibi ekranı çizdi. Dört vCPU'da IF=0, HLT=1 ve sabit yerel tıklar QMP ile doğrulandı.

Gerçek AP zamanlayıcı kesmesinde düz kilit ile deney sıra=40 sahip=0 bekleyen=1 tanısı geldi (rc=1). Sürüm karşı deneyinde sayaç 1, kabuk çalışır. NMI IPI çıkarılınca BSP durdu, AP tıkları beş saniyede 87/84/82’den 495/491/489’a ilerledi (rc=1).

§7, sıra/kilit, istisna, panik ve tip kapıları 430 sn'lik koşumda geçti. Aynı grubun yeni yük kapısı ilk PID dilimini beklemediği için make rc=2 verdi. Fiziksel süreç tablosunda ilk dilim koşulu ve yük boyunca ilerleme denetimiyle yeni kapı iki kez geçti (28 ve 27 sn, make/tee rc=0). İlk değişken kaynak koşumunun tanıları saklandı. Bölüm süresi 15 dk 20 sn.

## Bölüm 19 — PC matrisi, tek ve dört işlemcili regresyon

PC matrisinin 72 açılışı geçti. 1/2/4 vCPU başına 24 açılışta QMP, gerçek RAM, disk ve bölüm kontrolleri tamamlandı. Fiziksel makine kontrol listesine MADT, `islemci` ve bağımsız asal toplamı eklendi.

Varsayılan 1 vCPU ile tam takım 1.465 sn. Önceki x86 kapıları geçti. Yeni SMP bellek kâhini self-host kabuğunun başlangıç/son heap çiftini reddetti (make rc=2). Aynı kullanıcı dosyaları ve arşivdeki D-772 ELF'si sekiz açılışta iki ek çifti birebir verdi. Ölçülen çiftlerle düzeltilen bellek kapısı iki derleyicide 98 sn, rc=0. Süreç heap artışı 0 bayt. Soğuk 100+100 MiB self-host TCP kapısı 718,772 sn ile süre hedefini aştı.

Dört vCPU ile cok_surec, boru, kabuk14, dosyalar, donanim, log, program15, ag_l3, ag_udp ve ag_parca geçti. İlk grup 402 sn, düzeltilen arka plan senaryosu 169 sn. ARM, kapsam, seçici ve paylaşım 20 sn, ek x86 çekirdek 20 sn. Bu bölümde ayrıca sabotaj istenmedi. Bölüm süresi 36 dk 7 sn.

## Bölüm 20 — kullanıcı belgesi ve son regresyon

SMP mimarisi, üretim kilit sırası, IPI ve TLB, ABI 91–99, hızlanma ve gecikme ölçümleri belgelendi. Yol haritasında H10i ve H14'ün QEMU kapıları gösterildi. Fiziksel koşum açık. Sabotaj envanterinde sessiz sonuçlar ve beş uygulanmayan canlı deney ayrı belirtildi. Kabuk yardımı ve kitap örnekleri iki derleyicide konukta sınandı. Yakınlık örneği tek vCPU'da CPU 0 ve dört vCPU'da CPU 3 verdi. Bu bölümde ayrıca sabotaj istenmedi.

§7, belge, tip ve bare-metal fark denetimleri ile 100+100 MiB self-host TCP kapısı 7:49 sürdü. Ölçümlü `-j10` 22:53, ham `-j20` 18:43. Her ikisinde make/tee rc=0. Son ölçümde 187 kapı, en uzun 6:51. İlk 20 işçili denemede iki 64 MiB TCP verim kapısı süre sınırına takıldı. Gönderim halkası düzeltildikten sonra eşik ve PCAP özeti korundu. PS/2 hızlı yazma testinde süreyi her gerçek girişten başlattık. PIT kanal 0 gecikmesiyle yavaş kalan LAPIC için aynı 8 sn deneyinde eski imaj 199, kanal 2 referansıyla düzeltilen imaj 801 tık verdi. USB klavye kâhini sürücünün tükettiği raporu da bekliyor. 192 KiB/sn yazma hızındaki gerçek USB diskte iki eski qtest ikilisi zaman aşımına düştü. Süreyi xHCI MFINDEX üzerinden ölçen iki ikili de yazma, flush, geri okuma ve dış FAT denetimini geçti. Kısa tuş basışı deneyi, 800 ms host gecikmesinde QEMU’nun sanal saatiyle bir glif verdi. MSI beklemesi maskesiz yerel zamanlayıcı varsa HLT kullanıyor. Tam takımda süreçlerin ek derleyici turu dış işçi ve CPU sayısına göre dağıtılıyor. İki SIPI birlikte çıkarıldığında iki derleyicide de AP onayı zaman aşımı ve rc=1 görüldü. Bölüm süresi 2 sa 56 dk 4 sn.

```text
=== ZAMAN RAPORU: duvar saati 22 dk 53 sn · 187 kapı · kapı toplamı 3 sa 37 dk 17 sn · derleme/diğer 00 dk 00 sn ===
       06 dk 51 sn  %29.9  calistir_bm_x86_ag_saglamlik_self_aktarim
       06 dk 42 sn  %29.3  calistir_bm_x86_ekran_fare
       06 dk 22 sn  %27.8  calistir_codegen_bootstrap
       06 dk 11 sn  %27.1  calistir_bm_x86_tftp_teshis
       06 dk 11 sn  %27.0  calistir_bm_x86_ps2_irq_self
       05 dk 52 sn  %25.6  calistir_bm_x86_dosyalar
       05 dk 17 sn  %23.1  calistir_bm_x86_tcp_kayip
       04 dk 56 sn  %21.5  calistir_bm_x86_cok_surec_cocuk
       04 dk 49 sn  %21.1  calistir_bm_x86_kaynak_editor
       04 dk 45 sn  %20.8  calistir_bm_x86_kaynak_cocuk
       04 dk 44 sn  %20.7  calistir_bm_x86_apic
       04 dk 43 sn  %20.6  calistir_kem_os_pc_matris_mbr
       04 dk 42 sn  %20.5  calistir_kem_os_pc_matris_gpt
       04 dk 40 sn  %20.4  calistir_bm_x86_v1_tam
       04 dk 29 sn  %19.6  calistir_bm_x86_ag_saglamlik_c_aktarim
  disk: build 54G · boş 294 GiB
Disk build: önce 57848848384 B, sonra 57850281984 B, büyüme +1433600 B
Geçici dizin: önce 0, sonra 0, yeni 0
```

## Taslak PR ve CI ölçümü

[Taslak PR #257](https://github.com/msyrt-sys/kemgu/pull/257) açıldı. GitHub, bölüm 20 commit'i `4456ab0` ile `main` (`dea24dd`) arasında `CONFLICTING` bildirdi. PR denetimleri ve bu dala ait Actions koşum listesi boştu. `git merge-tree` dört dosyada çakışma gösterdi.

- `Makefile`
- `test/ciplak_region_free_harness.sh`
- `test/kem_malloc_kompozisyon_harness.sh`
- `tests/qemu/surec_kahin.py`

D-772'de Linux işi 35:17, Windows işi 1:03:43 sürmüştü. Sonraki CI süreleri Claude'un entegrasyonunu bekliyor. Yerel `-j10` ve `-j20` sonuçları yukarıda. [D-772 Actions günlüğü](https://github.com/msyrt-sys/kemgu/actions/runs/45807330890)
