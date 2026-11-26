# Dalga 18 ilerleme kaydı

Başlangıç 2026-11-26T14:07:15+03:00

Çalışma tabanı `0ce56bf` (D-772). Araç derlemesi geçti. İlk tam koşum 155 kapıda, 37 dk 31 sn ve `make rc=0` ile bitti.

## Bölüm 1 — ölçüm tabanı ve süre bütçeleri

Üç gerçek `-j10` koşumunda aynı 155 kapı ölçüldü. Süreler ve make çıkış kodları 37 dk 31 sn (0), 35 dk 00 sn (2), 34 dk 18 sn (0). İkinci koşumda `semafor_n` 20 yinelemenin birini reddetti. Aynı kaynakla üçüncü taban ve 100 ek yineleme geçti. Ham raporlar, her kapının ortalaması ve en büyük sapması bulgular dosyasında.

İzlenen bütçe tablosu 156 kapı içerir. Yeni konak kapısı sabit kayıtlarda süre toplamını, sınırda eşitliği, aşımı, eksik bütçeyi, başarısız hedefi ve boş kaydı doğruladı.

`calistir_kapi_butcesi`, `calistir_cekirdek_onbellek`, `calistir_bm_x86_v1`, `calistir_bm_x86_uefi_pc`, `calistir_bm_x86_saglamlik`, `calistir_bm_x86_cok_surec`, `calistir_bm_x86_boru`, `calistir_bm_x86_program15` ve `calistir_kem_os_x86` birlikte geçti.

İki sabotaj uygulandı. Gerçek 120 sn bekleme, 60 sn bütçeli kapıyı 120,022 sn yaptı ve sıkı rapor 3 döndürdü. Bütçe tablosunun kopyasından aynı kapı silinince rapor “bütçe yok” uyarısı verdi.

Bölüm süresi 1 sa 57 dk 15 sn. Son kapı takımı 10 dk 18 sn, make çıkış kodu 0.
