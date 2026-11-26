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
