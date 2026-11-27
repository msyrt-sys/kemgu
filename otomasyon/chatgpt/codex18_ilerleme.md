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
