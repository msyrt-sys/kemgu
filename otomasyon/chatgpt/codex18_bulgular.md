# Dalga 18 bulguları

## Ortam ve başlangıç

Çalışma ağacının tabanı `0ce56bf14226`. Başlangıç 2026-11-26T14:07:15+03:00.
Ön koşul ilk kez 12.42.13’te denetlendi. D-772, 14.07.14 yoklamasında görüldü. Saatler Türkiye saatidir.
Host ARM64, 20 mantıksal işlemci. QEMU 8.2.2, x86 konuğu için TCG ve q35.

| QEMU -smp | QMP vCPU indeksleri | QMP host iş parçacığı kimlikleri |
|---:|---|---|
| 1 | 0 | 4048390 |
| 2 | 0,1 | 4048395,4048396 |
| 4 | 0,1,2,3 | 4048400,4048401,4048402,4048403 |
| 8 | 0,1,2,3,4,5,6,7 | 4048407,4048408,4048409,4048410,4048411,4048412,4048413,4048414 |

Bu ortam denetiminde konuk CPU’ları bekleme durumundaydı. KEMGU AP açılışının ölçümü ayrıca yapılır.

## Bölüm 1 — üç taban ölçümü

Aynı çalışma ağacında arka arkaya üç kez `make -j10 --output-sync=target test_tumu_olcum` çalıştırıldı. Birinci tur temiz çalışma ağacında başladı. Sonraki turlar önbelleği devraldı. ARM smoke kapısı tamamlanınca o turun kernel.elf ürünü arşivlendi. Böylece koşullu uart_merhaba_bare_metal derlemesi üç turda da ölçüldü.

| Tur | Make süresi | Ölçülen duvar | Kapı | Kapı toplamı | Make rc | Hatalı hedefler |
|---:|---:|---:|---:|---:|---:|---|
| 1 | 37 dk 31 sn | 37 dk 31 sn | 155 | 4 sa 01 dk 10 sn | 0 | yok |
| 2 | 35 dk 00 sn | 35 dk 00 sn | 155 | 3 sa 57 dk 50 sn | 2 | calistir_eszamanli_kosum |
| 3 | 34 dk 18 sn | 34 dk 18 sn | 155 | 3 sa 51 dk 27 sn | 0 | yok |

İkinci turda semafor_n denetimi 20 çalıştırmadan birini reddetti. Mevcut harness yalnız son çalıştırmanın kodunu yazdı (42). Hata veren çalıştırmanın kodu bu kayıttan belirlenemiyor.

```text
✅ kilit — 20/20 tur, sayaç tam (3 koşucu × 500 = 1500)
  ✅ semafor — 20/20 tur, sayaç tam (3 koşucu × 500 = 1500)
  🔴 semafor_n — 20 turun 1'inde semafor n>1 sozlesmesi BOZUK (son cikis: 42; 1=n asildi 2=kilit gibi 3=olcum guvenilmez)
  ✅ bariyer — 20/20 tur, kimse erken gecmedi (4 katilimci x 5 bulusma, yeniden kullanim dahil)
=== eşzamanlılık ilkelleri koşumu: 3/4 modül (20 tur) ===
make[1]: *** [Makefile:4906: calistir_eszamanli_kosum] Error 1
```

### Taban 1 — özgün rapor

```text
=== ZAMAN RAPORU: duvar saati 37 dk 31 sn · 155 kapı · kapı toplamı 4 sa 01 dk 10 sn · derleme/diğer 0 dk 33 sn ===
       19 dk 25 sn  %51.7  calistir_bm_x86_ag_saglamlik
       12 dk 59 sn  %34.6  calistir_bm_x86_ps2_irq
       12 dk 58 sn  %34.6  calistir_bm_x86_kaynak
       11 dk 32 sn  %30.7  calistir_bm_x86_kabuk14
       10 dk 49 sn  %28.8  calistir_bm_x86_v1
       10 dk 36 sn  %28.3  calistir_bm_x86_ps2_karma
        9 dk 50 sn  %26.2  calistir_bm_x86_cok_surec
        9 dk 12 sn  %24.5  calistir_kem_os_pc_matris
        7 dk 23 sn  %19.7  calistir_bm_x86_ekran_fare
        7 dk 20 sn  %19.5  calistir_bm_x86_dosyalar
        6 dk 34 sn  %17.5  calistir_bm_x86_tftp_teshis
        6 dk 32 sn  %17.4  calistir_bm_x86_surec
        6 dk 14 sn  %16.6  calistir_bm_x86_tcp_verim
        5 dk 32 sn  %14.7  calistir_codegen_bootstrap
        5 dk 25 sn  %14.4  calistir_bm_x86_uefi_kabuk
  disk: build 1.3G · boş 454 GiB
```

### Taban 2 — özgün rapor

```text
=== ZAMAN RAPORU: duvar saati 35 dk 00 sn · 155 kapı · kapı toplamı 3 sa 57 dk 50 sn · derleme/diğer 0 dk 00 sn ===
       16 dk 47 sn  %47.9  calistir_bm_x86_ag_saglamlik
       13 dk 19 sn  %38.1  calistir_bm_x86_kaynak
       13 dk 04 sn  %37.3  calistir_bm_x86_ps2_irq
       11 dk 43 sn  %33.5  calistir_bm_x86_kabuk14
       10 dk 57 sn  %31.3  calistir_bm_x86_v1
       10 dk 42 sn  %30.6  calistir_bm_x86_ps2_karma
        9 dk 58 sn  %28.5  calistir_bm_x86_cok_surec
        8 dk 58 sn  %25.6  calistir_kem_os_pc_matris
        7 dk 29 sn  %21.4  calistir_bm_x86_dosyalar
        7 dk 24 sn  %21.2  calistir_bm_x86_ekran_fare
        6 dk 34 sn  %18.8  calistir_bm_x86_tftp_teshis
        6 dk 13 sn  %17.7  calistir_bm_x86_uefi_kabuk
        6 dk 12 sn  %17.7  calistir_bm_x86_tcp_verim
        5 dk 59 sn  %17.1  calistir_codegen_bootstrap
        5 dk 37 sn  %16.1  calistir_bm_x86_surec
  başarısız: calistir_eszamanli_kosum
  disk: build 1.8G · boş 456 GiB
```

### Taban 3 — özgün rapor

```text
=== ZAMAN RAPORU: duvar saati 34 dk 18 sn · 155 kapı · kapı toplamı 3 sa 51 dk 27 sn · derleme/diğer 0 dk 00 sn ===
       16 dk 44 sn  %48.8  calistir_bm_x86_ag_saglamlik
       13 dk 03 sn  %38.0  calistir_bm_x86_kaynak
       12 dk 49 sn  %37.4  calistir_bm_x86_ps2_irq
       11 dk 33 sn  %33.7  calistir_bm_x86_kabuk14
       10 dk 47 sn  %31.5  calistir_bm_x86_v1
       10 dk 29 sn  %30.6  calistir_bm_x86_ps2_karma
        9 dk 54 sn  %28.9  calistir_bm_x86_cok_surec
        8 dk 58 sn  %26.2  calistir_kem_os_pc_matris
        7 dk 23 sn  %21.5  calistir_bm_x86_ekran_fare
        7 dk 20 sn  %21.4  calistir_bm_x86_dosyalar
        6 dk 34 sn  %19.2  calistir_bm_x86_tftp_teshis
        6 dk 09 sn  %17.9  calistir_bm_x86_tcp_verim
        5 dk 40 sn  %16.5  calistir_codegen_bootstrap
        5 dk 34 sn  %16.2  calistir_bm_x86_uefi_kabuk
        5 dk 15 sn  %15.3  calistir_bm_x86_surec
  disk: build 2.2G · boş 456 GiB
```

### Kapı süreleri ve başlangıç bütçeleri

Süreler saniyedir. Sapma, üç ölçümün ortalamadan en büyük mutlak uzaklığıdır. Bütçe en uzun ölçümün 1,5 katının yukarı yuvarlanmış hâli ve en az 60 saniyedir.

| Kapı | Kategori | Tur 1 | Tur 2 | Tur 3 | Ortalama | En büyük sapma | Bütçe |
|---|---|---:|---:|---:|---:|---:|---:|
| `calistir_ag_es` | host | 6.767 | 6.906 | 6.743 | 6.805 | 0.100 | 60 |
| `calistir_ag_kosum` | host | 0.205 | 0.209 | 0.234 | 0.216 | 0.018 | 60 |
| `calistir_anahtar_kelime_kapisi` | derleme | 0.050 | 0.082 | 0.056 | 0.062 | 0.019 | 60 |
| `calistir_arena_test` | host | 0.012 | 0.013 | 0.011 | 0.012 | 0.001 | 60 |
| `calistir_arm64_test` | derleme | 0.058 | 0.107 | 0.063 | 0.076 | 0.031 | 60 |
| `calistir_asan_denetim` | host | 42.089 | 55.261 | 39.474 | 45.608 | 9.653 | 83 |
| `calistir_asan_matris` | host | 3.712 | 6.115 | 3.962 | 4.596 | 1.519 | 60 |
| `calistir_ast_test` | host | 0.011 | 0.018 | 0.012 | 0.014 | 0.004 | 60 |
| `calistir_baremetal_diff` | derleme | 22.692 | 23.206 | 23.064 | 22.987 | 0.295 | 60 |
| `calistir_belge_kapisi` | host | 0.044 | 0.051 | 0.036 | 0.044 | 0.008 | 60 |
| `calistir_bm_x86_acilis_suresi` | qemu-x86 | 50.668 | 50.850 | 50.639 | 50.719 | 0.132 | 77 |
| `calistir_bm_x86_ag_arac` | qemu-x86 | 111.435 | 112.922 | 111.334 | 111.897 | 1.025 | 170 |
| `calistir_bm_x86_ag_iki` | qemu-x86 | 253.605 | 163.340 | 164.606 | 193.850 | 59.754 | 381 |
| `calistir_bm_x86_ag_l3` | qemu-x86 | 102.682 | 103.215 | 102.222 | 102.706 | 0.509 | 155 |
| `calistir_bm_x86_ag_parca` | qemu-x86 | 25.017 | 24.786 | 24.445 | 24.749 | 0.304 | 60 |
| `calistir_bm_x86_ag_pc` | qemu-x86 | 159.047 | 172.523 | 170.067 | 167.212 | 8.166 | 259 |
| `calistir_bm_x86_ag_saglamlik` | qemu-x86 | 1164.557 | 1006.734 | 1003.504 | 1058.265 | 106.292 | 1747 |
| `calistir_bm_x86_ag_udp` | qemu-x86 | 94.758 | 95.364 | 94.980 | 95.034 | 0.330 | 144 |
| `calistir_bm_x86_ahci` | qemu-x86 | 13.835 | 17.291 | 14.217 | 15.114 | 2.177 | 60 |
| `calistir_bm_x86_apic` | qemu-x86 | 274.431 | 267.934 | 262.449 | 268.272 | 6.159 | 412 |
| `calistir_bm_x86_arac16` | qemu-x86 | 86.472 | 86.624 | 86.333 | 86.476 | 0.148 | 130 |
| `calistir_bm_x86_belge15` | qemu-x86 | 94.046 | 94.164 | 92.307 | 93.506 | 1.199 | 142 |
| `calistir_bm_x86_blok` | qemu-x86 | 27.825 | 11.480 | 7.710 | 15.672 | 12.153 | 60 |
| `calistir_bm_x86_boru` | qemu-x86 | 71.066 | 71.193 | 70.149 | 70.803 | 0.654 | 107 |
| `calistir_bm_x86_buyuk_elf` | qemu-x86 | 196.505 | 198.922 | 197.776 | 197.734 | 1.229 | 299 |
| `calistir_bm_x86_cok_surec` | qemu-x86 | 589.889 | 598.243 | 594.395 | 594.176 | 4.286 | 898 |
| `calistir_bm_x86_disk` | qemu-x86 | 104.731 | 104.468 | 104.643 | 104.614 | 0.146 | 158 |
| `calistir_bm_x86_disk_hata` | qemu-x86 | 66.709 | 67.947 | 67.625 | 67.427 | 0.718 | 102 |
| `calistir_bm_x86_donanim` | qemu-x86 | 39.316 | 39.719 | 39.798 | 39.611 | 0.295 | 60 |
| `calistir_bm_x86_dosyalar` | qemu-x86 | 439.738 | 448.899 | 439.792 | 442.810 | 6.089 | 674 |
| `calistir_bm_x86_duzenle` | qemu-x86 | 287.194 | 293.314 | 288.769 | 289.759 | 3.555 | 440 |
| `calistir_bm_x86_e1000` | qemu-x86 | 34.230 | 34.382 | 34.526 | 34.379 | 0.149 | 60 |
| `calistir_bm_x86_ekran` | qemu-x86 | 2.094 | 2.374 | 2.256 | 2.241 | 0.147 | 60 |
| `calistir_bm_x86_ekran_fare` | qemu-x86 | 442.656 | 444.448 | 442.684 | 443.262 | 1.185 | 667 |
| `calistir_bm_x86_girdi15` | qemu-x86 | 68.044 | 67.332 | 67.317 | 67.564 | 0.479 | 103 |
| `calistir_bm_x86_guc` | qemu-x86 | 146.173 | 55.010 | 39.283 | 80.155 | 66.018 | 220 |
| `calistir_bm_x86_http` | qemu-x86 | 221.927 | 222.107 | 222.891 | 222.308 | 0.582 | 335 |
| `calistir_bm_x86_istisna` | qemu-x86 | 56.583 | 58.115 | 55.141 | 56.613 | 1.502 | 88 |
| `calistir_bm_x86_kabuk` | qemu-x86 | 173.471 | 206.603 | 182.715 | 187.596 | 19.007 | 310 |
| `calistir_bm_x86_kabuk14` | qemu-x86 | 692.215 | 702.776 | 692.850 | 695.947 | 6.829 | 1055 |
| `calistir_bm_x86_kapat` | qemu-x86 | 33.701 | 34.811 | 34.473 | 34.329 | 0.627 | 60 |
| `calistir_bm_x86_kaynak` | qemu-x86 | 778.444 | 799.433 | 782.711 | 786.863 | 12.570 | 1200 |
| `calistir_bm_x86_klavye16` | qemu-x86 | 269.328 | 269.119 | 264.150 | 267.532 | 3.383 | 404 |
| `calistir_bm_x86_log` | qemu-x86 | 39.142 | 39.676 | 39.487 | 39.435 | 0.293 | 60 |
| `calistir_bm_x86_msi` | qemu-x86 | 120.071 | 30.681 | 30.750 | 60.501 | 59.570 | 181 |
| `calistir_bm_x86_nic_matris` | qemu-x86 | 142.127 | 143.165 | 142.658 | 142.650 | 0.523 | 215 |
| `calistir_bm_x86_ntp` | qemu-x86 | 17.455 | 17.368 | 17.501 | 17.441 | 0.073 | 60 |
| `calistir_bm_x86_nvme` | qemu-x86 | 14.808 | 18.381 | 15.163 | 16.118 | 2.264 | 60 |
| `calistir_bm_x86_paket` | qemu-x86 | 92.072 | 94.418 | 93.082 | 93.191 | 1.227 | 142 |
| `calistir_bm_x86_panik` | qemu-x86 | 3.986 | 4.648 | 4.224 | 4.286 | 0.362 | 60 |
| `calistir_bm_x86_pat` | qemu-x86 | 7.805 | 7.962 | 8.240 | 8.002 | 0.238 | 60 |
| `calistir_bm_x86_pci16` | qemu-x86 | 55.026 | 55.679 | 54.751 | 55.152 | 0.527 | 84 |
| `calistir_bm_x86_program15` | qemu-x86 | 59.528 | 60.042 | 60.103 | 59.891 | 0.363 | 91 |
| `calistir_bm_x86_ps2` | qemu-x86 | 220.155 | 254.216 | 229.678 | 234.683 | 19.533 | 382 |
| `calistir_bm_x86_ps2_irq` | qemu-x86 | 778.610 | 784.270 | 769.378 | 777.419 | 8.041 | 1177 |
| `calistir_bm_x86_ps2_karma` | qemu-x86 | 636.005 | 642.302 | 628.696 | 635.668 | 6.972 | 964 |
| `calistir_bm_x86_ramdisk` | qemu-x86 | 174.073 | 113.310 | 99.609 | 128.998 | 45.076 | 262 |
| `calistir_bm_x86_saglamlik` | qemu-x86 | 67.631 | 69.217 | 68.360 | 68.403 | 0.814 | 104 |
| `calistir_bm_x86_soket` | qemu-x86 | 29.754 | 29.455 | 28.760 | 29.323 | 0.563 | 60 |
| `calistir_bm_x86_surec` | qemu-x86 | 392.199 | 337.105 | 314.543 | 347.949 | 44.250 | 589 |
| `calistir_bm_x86_surec_abi` | qemu-x86 | 7.992 | 9.003 | 8.349 | 8.448 | 0.555 | 60 |
| `calistir_bm_x86_tcp_kayip` | qemu-x86 | 282.488 | 282.399 | 287.152 | 284.013 | 3.139 | 431 |
| `calistir_bm_x86_tcp_secenek` | qemu-x86 | 19.356 | 18.970 | 19.401 | 19.243 | 0.272 | 60 |
| `calistir_bm_x86_tcp_temel` | qemu-x86 | 43.228 | 43.796 | 42.988 | 43.337 | 0.458 | 66 |
| `calistir_bm_x86_tcp_verim` | qemu-x86 | 374.414 | 371.647 | 368.644 | 371.569 | 2.924 | 562 |
| `calistir_bm_x86_tftp_teshis` | qemu-x86 | 393.999 | 394.383 | 394.125 | 394.169 | 0.214 | 592 |
| `calistir_bm_x86_uefi` | qemu-x86 | 31.970 | 43.305 | 31.665 | 35.647 | 7.658 | 65 |
| `calistir_bm_x86_uefi_kabuk` | qemu-x86 | 324.899 | 372.752 | 334.099 | 343.917 | 28.835 | 560 |
| `calistir_bm_x86_uefi_pc` | qemu-x86 | 198.040 | 235.387 | 205.699 | 213.042 | 22.345 | 354 |
| `calistir_bm_x86_usb` | qemu-x86 | 10.905 | 12.563 | 11.104 | 11.524 | 1.039 | 60 |
| `calistir_bm_x86_usb_ag` | qemu-x86 | 206.350 | 207.124 | 207.740 | 207.071 | 0.722 | 312 |
| `calistir_bm_x86_usb_hub` | qemu-x86 | 80.015 | 81.007 | 79.709 | 80.244 | 0.764 | 122 |
| `calistir_bm_x86_v1` | qemu-x86 | 648.696 | 657.386 | 647.229 | 651.104 | 6.283 | 987 |
| `calistir_bolge_atama_test` | host | 0.013 | 0.012 | 0.010 | 0.011 | 0.001 | 60 |
| `calistir_bolge_operand` | host | 5.674 | 6.391 | 5.971 | 6.012 | 0.379 | 60 |
| `calistir_bolge_sayac_yarisi` | host | 0.964 | 1.386 | 1.012 | 1.120 | 0.265 | 60 |
| `calistir_bolge_test` | host | 0.012 | 0.017 | 0.010 | 0.013 | 0.004 | 60 |
| `calistir_bolge_yonlendirme` | host | 0.026 | 0.043 | 0.023 | 0.031 | 0.012 | 60 |
| `calistir_capability_test` | host | 0.027 | 0.031 | 0.026 | 0.028 | 0.003 | 60 |
| `calistir_cekirdek_onbellek` | derleme | 48.901 | 52.687 | 47.459 | 49.682 | 3.005 | 80 |
| `calistir_chatgpt_kopru` | host | 162.779 | 169.088 | 166.924 | 166.264 | 3.484 | 254 |
| `calistir_check_genis` | derleme | 2.983 | 3.184 | 2.966 | 3.044 | 0.140 | 60 |
| `calistir_check_kapisi` | derleme | 3.668 | 6.493 | 3.763 | 4.641 | 1.852 | 60 |
| `calistir_checker_diff` | derleme | 4.132 | 5.048 | 4.265 | 4.482 | 0.567 | 60 |
| `calistir_ciplak_region_free` | host | 0.800 | 0.977 | 0.642 | 0.806 | 0.171 | 60 |
| `calistir_codegen_bootstrap` | host | 331.676 | 359.237 | 339.585 | 343.500 | 15.738 | 539 |
| `calistir_codegen_diff` | host | 25.152 | 27.199 | 26.189 | 26.180 | 1.028 | 60 |
| `calistir_codegen_genis` | host | 19.297 | 20.649 | 19.972 | 19.973 | 0.676 | 60 |
| `calistir_ct_bariyer` | derleme | 1.281 | 1.381 | 1.388 | 1.350 | 0.069 | 60 |
| `calistir_derleme_olcum` | derleme | 100.807 | 100.434 | 100.456 | 100.566 | 0.242 | 152 |
| `calistir_dizi_perf_test` | host | 0.306 | 0.503 | 0.319 | 0.376 | 0.127 | 60 |
| `calistir_dizi_sinir_test` | host | 14.853 | 16.272 | 14.983 | 15.370 | 0.902 | 60 |
| `calistir_drf_gorunurluk` | host | 0.231 | 0.346 | 0.248 | 0.275 | 0.071 | 60 |
| `calistir_drf_test` | host | 0.027 | 0.027 | 0.025 | 0.026 | 0.001 | 60 |
| `calistir_e4k_bellek` | host | 1.605 | 1.632 | 1.606 | 1.614 | 0.018 | 60 |
| `calistir_escape_test` | host | 0.011 | 0.017 | 0.012 | 0.013 | 0.004 | 60 |
| `calistir_eszamanli_kosum` | host | 0.404 | 0.483 | 0.408 | 0.432 | 0.052 | 60 |
| `calistir_fuzz_advanced` | host | 0.721 | 0.811 | 0.740 | 0.757 | 0.053 | 60 |
| `calistir_fuzz_test` | host | 0.440 | 0.628 | 0.458 | 0.509 | 0.119 | 60 |
| `calistir_gorev_msan` | host | 0.522 | 0.800 | 0.521 | 0.614 | 0.186 | 60 |
| `calistir_gorev_rt_test` | host | 0.124 | 0.126 | 0.125 | 0.125 | 0.001 | 60 |
| `calistir_json_test` | host | 0.010 | 0.013 | 0.010 | 0.011 | 0.002 | 60 |
| `calistir_kanal_omru` | host | 0.611 | 0.636 | 0.611 | 0.619 | 0.016 | 60 |
| `calistir_kdl_bolge_test` | host | 0.009 | 0.009 | 0.010 | 0.009 | 0.000 | 60 |
| `calistir_kem_malloc_kompozisyon` | host | 0.815 | 1.067 | 0.759 | 0.880 | 0.187 | 60 |
| `calistir_kem_os_arm` | qemu-arm | 14.903 | 15.408 | 15.090 | 15.133 | 0.275 | 60 |
| `calistir_kem_os_arm_ag` | qemu-arm | 38.843 | 41.531 | 42.521 | 40.965 | 2.122 | 64 |
| `calistir_kem_os_ayristirici` | qemu-x86 | 42.257 | 41.836 | 41.986 | 42.026 | 0.230 | 64 |
| `calistir_kem_os_pc_matris` | qemu-x86 | 552.090 | 537.738 | 538.219 | 542.682 | 9.407 | 829 |
| `calistir_kripto_check` | derleme | 0.049 | 0.045 | 0.048 | 0.047 | 0.002 | 60 |
| `calistir_kripto_kosum` | host | 0.889 | 0.944 | 0.873 | 0.902 | 0.042 | 60 |
| `calistir_lambda_test` | host | 0.258 | 0.311 | 0.237 | 0.269 | 0.043 | 60 |
| `calistir_lean_sorry` | host | 0.082 | 0.121 | 0.084 | 0.095 | 0.026 | 60 |
| `calistir_lexer_diff` | host | 0.173 | 0.279 | 0.175 | 0.209 | 0.070 | 60 |
| `calistir_linear_test` | host | 0.031 | 0.035 | 0.030 | 0.032 | 0.003 | 60 |
| `calistir_llvm_dogrula_test` | host | 0.008 | 0.013 | 0.010 | 0.010 | 0.003 | 60 |
| `calistir_llvm_test` | host | 13.452 | 19.730 | 14.335 | 15.839 | 3.891 | 60 |
| `calistir_lsp_test` | host | 0.024 | 0.033 | 0.029 | 0.029 | 0.005 | 60 |
| `calistir_mmio_bare_metal` | derleme | 0.019 | 0.049 | 0.024 | 0.030 | 0.018 | 60 |
| `calistir_mmio_test` | host | 0.018 | 0.017 | 0.012 | 0.016 | 0.004 | 60 |
| `calistir_mmio_volatile` | derleme | 11.275 | 11.130 | 11.642 | 11.349 | 0.293 | 60 |
| `calistir_modul_codegen` | derleme | 6.419 | 6.816 | 6.710 | 6.648 | 0.229 | 60 |
| `calistir_otp_cli_test` | host | 0.062 | 0.123 | 0.069 | 0.085 | 0.038 | 60 |
| `calistir_panik_test` | host | 0.008 | 0.009 | 0.010 | 0.009 | 0.001 | 60 |
| `calistir_parser_diff` | host | 0.218 | 0.433 | 0.267 | 0.306 | 0.127 | 60 |
| `calistir_parser_test` | host | 0.016 | 0.025 | 0.017 | 0.020 | 0.006 | 60 |
| `calistir_perf_bellek` | host | 1.238 | 1.275 | 1.296 | 1.270 | 0.032 | 60 |
| `calistir_qemu_cekirdek` | qemu-x86 | 0.014 | 0.014 | 0.014 | 0.014 | 0.000 | 60 |
| `calistir_qemu_smoke` | qemu-arm | 8.014 | 8.012 | 8.015 | 8.013 | 0.001 | 60 |
| `calistir_qtest` | qemu-x86 | 14.607 | 15.697 | 15.226 | 15.177 | 0.570 | 60 |
| `calistir_ring3_page_test_x86` | qemu-x86 | 20.083 | 20.125 | 20.078 | 20.095 | 0.030 | 60 |
| `calistir_runtime_link_test` | host | 0.004 | 0.005 | 0.004 | 0.004 | 0.001 | 60 |
| `calistir_sabitsure_test` | host | 0.018 | 0.033 | 0.021 | 0.024 | 0.009 | 60 |
| `calistir_self_driver` | host | 188.455 | 211.134 | 195.076 | 198.222 | 12.913 | 317 |
| `calistir_sembol_test` | host | 0.023 | 0.024 | 0.021 | 0.022 | 0.001 | 60 |
| `calistir_sifir_bolme` | host | 24.952 | 25.419 | 25.110 | 25.160 | 0.259 | 60 |
| `calistir_sifir_uyari` | derleme | 1.748 | 1.936 | 1.789 | 1.824 | 0.112 | 60 |
| `calistir_simd_llvm_test` | host | 0.245 | 0.334 | 0.257 | 0.279 | 0.056 | 60 |
| `calistir_simd_test` | host | 0.019 | 0.018 | 0.019 | 0.019 | 0.000 | 60 |
| `calistir_snapshot_test` | host | 0.065 | 0.074 | 0.065 | 0.068 | 0.006 | 60 |
| `calistir_stdlib_check` | derleme | 1.026 | 1.179 | 1.052 | 1.086 | 0.094 | 60 |
| `calistir_stdlib_ithal` | derleme | 1.807 | 1.869 | 1.801 | 1.825 | 0.043 | 60 |
| `calistir_surucu_diff` | host | 3.298 | 3.836 | 3.672 | 3.602 | 0.304 | 60 |
| `calistir_surucu_kosum` | host | 2.019 | 2.058 | 2.102 | 2.060 | 0.042 | 60 |
| `calistir_tasma` | host | 0.581 | 0.618 | 0.625 | 0.608 | 0.027 | 60 |
| `calistir_tip_kontrol_test` | host | 0.051 | 0.060 | 0.050 | 0.053 | 0.006 | 60 |
| `calistir_tip_test` | host | 0.011 | 0.012 | 0.011 | 0.011 | 0.001 | 60 |
| `calistir_uart_16550_test` | host | 0.026 | 0.046 | 0.036 | 0.036 | 0.010 | 60 |
| `calistir_uart_merhaba_bare_metal` | derleme | 0.123 | 0.164 | 0.128 | 0.138 | 0.026 | 60 |
| `calistir_uart_pl011_test` | host | 0.037 | 0.030 | 0.041 | 0.036 | 0.007 | 60 |
| `calistir_uart_vtable_test` | host | 0.009 | 0.016 | 0.008 | 0.011 | 0.005 | 60 |
| `calistir_wcet_test` | host | 0.016 | 0.023 | 0.019 | 0.020 | 0.004 | 60 |
| `calistir_yapi_diff` | host | 3.880 | 4.714 | 4.346 | 4.313 | 0.434 | 60 |
| `calistir_yazdir_bare_test` | host | 0.010 | 0.015 | 0.008 | 0.011 | 0.004 | 60 |
| `calistir_yigin_geri` | host | 66.688 | 66.736 | 66.444 | 66.623 | 0.179 | 101 |

Yeni `calistir_kapi_butcesi` kapısının kendi üç koşumu 0,149 / 0,166 / 0,185 sn sürdü. Başlangıç bütçesi 60 sn, kategorisi host. Böylece izlenen tabloda 156 kapı bulunuyor.

### Bütçe denetiminin negatif deneyleri

| Deney | Gerçek çıktı | Çıkış kodu |
|---|---|---:|
| `calistir_mmio_bare_metal` tarifinin ayrı kopyasına `sleep 120` eklendi. Asıl derleme de çalıştı. | `⚠ BÜTÇE (120.022 sn > 60 sn)` | Rapor 3 |
| Aynı kapının satırı bütçe tablosunun ayrı kopyasından silindi. | `calistir_mmio_bare_metal  bütçe yok` | Rapor 0 |

Sıkı kipte yalnız süre aşımı raporu 3 ile bitirir. GNU Make tarif hatasını `Error 3` olarak gösterir ve çağıran sürece 2 döndürür. Ayrı bir tarif denemesinde hem takım hem rapor başarısız olduğunda takımın hatası öncelikli kaldı. Başarısız hedef raporda adıyla görüldü.

İkinci tabandaki eşzamanlılık kapısı değişiklik yapılmadan üçüncü tabanda 20/20 geçti. Ayrıca kabuk izi açık koşumda kilit, semafor, semafor_n ve bariyerin her biri 100/100 kez beklenen 42 kodunu verdi. İkinci tabanın çıkış kodu 2 olarak korundu. İlk hatanın nedeni kesinleşmedi.

## Bölüm 2 — geçici dosya envanteri

Envanter 67 adet `test/*.sh`, bir adet `test/olcum/*.sh` ve 76 adet `tests/qemu/*.py` dosyasını kapsar. Çalışma dizini altındaki desenler üretilen dosya aileleridir. “Üst harness” sütunda belirtilen dosyayı içeren koşum dizininin sahibidir. Başarısızlıkta dizinin yolu stderr’e veya derleyici turunun günlüğüne yazılır.

### test altındaki kabuk betikleri

| Betik | Oluşturduğu çalışma dizini ve dosyalar | Başarı / hata |
|---|---|---|
| `ag_kosum_harness.sh` | build/agkosum.XXXXXX — dinleyici.exe, $2.ll, $2.exe, $2.srv, $2.out | Kendi dizinini siler / korur. |
| `anahtar_kelime_kapisi.sh` | Kaynakları ve testleri tarar, sonucu stdout’a yazar. | Dosya üretmez. |
| `asan_e2e_denetim.sh` | build/asan_denetim.XXXXXX — a.ll, a.exe | Kendi dizinini siler / korur. |
| `asan_matris_calistir.sh` | build/asan_matris.XXXXXX — m.ll, n.exe, a.exe | Kendi dizinini siler / korur. |
| `baremetal_diff_harness.sh` | build/bmdiff.XXXXXX — os.kem, os_c.ll, os_c.err, os_s.ll, os_c.d, os_s.d, $b.kem, c.ll, c.err, s.ll, c.d, s.d, p.ll, x86.kem, x86_c.ll, x86_c.err, x86_s.ll, x86_c.d, x86_s.d, x86_c.a, x86_s.a | Kendi dizinini siler / korur. |
| `belge_kapisi_harness.sh` | JSON modülü ve korpus satırlarını okur, sonuç stdout’a gider. | Dosya üretmez. |
| `bm_x86_ahci_harness.sh` | build/_bm_ahci.XXXXXX — comb.kem, ham.ll, err, k.ll, k.o, cerr, k.elf, lerr, disk.img, disk.img,if=none,id=d0,format=raw, seri.txt | Kendi dizinini siler / korur. |
| `bm_x86_blok_harness.sh` | build/blok.XXXXXX | Kendi dizinini siler / korur. |
| `bm_x86_disk_harness.sh` | disk.XXXXXX ya da DALGA12_CALISMA_DIZINI; disk imajları, çekirdek ve QEMU kayıtları. | Kendi dizinini siler / korur. Açıkça verilen dizin çağırana aittir. |
| `bm_x86_ekran_harness.sh` | build/_bm_ekran.XXXXXX — comb.kem, ham.ll, err, k.ll, k.o, cerr, k.elf, lerr | Kendi dizinini siler / korur. |
| `bm_x86_guc_harness.sh` | build/guc.XXXXXX | Kendi dizinini siler / korur. |
| `bm_x86_istisna_harness.sh` | build/istisna.XXXXXX — k.elf, k.o, comb.kem, ham.ll, err, k.ll, cerr, lerr | Kendi dizinini siler / korur. |
| `bm_x86_kabuk_harness.sh` | build/_bm_kabuk.XXXXXX — comb.kem, ham.ll, err, k.ll, k.o, cerr, k.elf, lerr | Kendi dizinini siler / korur. |
| `bm_x86_mmio_volatile_harness.sh` | build/mmio.XXXXXX — k.ll, o2.ll | Kendi dizinini siler / korur. |
| `bm_x86_msi_harness.sh` | build/msi.XXXXXX | Kendi dizinini siler / korur. |
| `bm_x86_nic_harness.sh` | build/nic.XXXXXX — kem_os_x86_kabuk.kem, $program.elf, alicix.elf, arm-self, arm-self/kem_os.img, arp-kapali, arp-kapali/kem_os_x86_kabuk.kem, sunucu.elf, paket.elf, saat.elf, soketdeney.elf, ping.elf | Kendi dizinini siler / korur. |
| `bm_x86_nvme_harness.sh` | build/_bm_nvme.XXXXXX — comb.kem, ham.ll, err, k.ll, k.o, cerr, k.elf, lerr, disk.img, disk.img,if=none,id=d0,format=raw, seri.txt | Kendi dizinini siler / korur. |
| `bm_x86_panik_harness.sh` | build/_bm_panik.XXXXXX — k.elf, k.o, comb.kem, ham.ll, err, k.ll, cerr, lerr | Kendi dizinini siler / korur. |
| `bm_x86_pat_harness.sh` | build/pat.XXXXXX | Kendi dizinini siler / korur. |
| `bm_x86_ps2_harness.sh` | build/_bm_ps2.XXXXXX — k.elf, k.o, comb.kem, ham.ll, err, k.ll, cerr, lerr | Kendi dizinini siler / korur. |
| `bm_x86_surec_abi_harness.sh` | build/surec_abi.XXXXXX — comb.kem, k.elf, k.o, seri, ham.ll, k.ll | Kendi dizinini siler / korur. |
| `bm_x86_surec_harness.sh` | surec.XXXXXX, kullanıcı ELF’leri, disk, seri ve QMP; surec_gunluk.XXXXXX, derleyici günlükleri. SUREC_USB_CIKTI dış çıktıdır. | Her başarılı turu siler. Hatalı tur ve birleştirilmiş günlük dizini korunur. |
| `bm_x86_uefi_harness.sh` | uefi.XXXXXX, k.elf/IR ve ESP. Yalnız-derle kipinde _bm_uefi veya verilen dizin. İçeriğe göre anahtarlanan çekirdek önbelleği. | Kendi açılış dizinini siler / korur. Yalnız-derle ürünü çağırana aittir. |
| `bm_x86_usb_harness.sh` | build/_bm_usb.XXXXXX — comb.kem, ham.ll, err, k.ll, k.o, cerr, k.elf, lerr, disk.img, disk.img,if=none,id=u0,format=raw, seri.txt | Kendi dizinini siler / korur. |
| `bolge_operand_harness.sh` | build/bolge_op.XXXXXX — c.ll, s.ll, c.env, s.env, c.rho, s.rho | Kendi dizinini siler / korur. |
| `bolge_sayac_harness.sh` | build/bolge_sayac.XXXXXX — duz$EXE, duz.err, duz.out, p.c, p, tsan, tsan.err, tsan.out | Kendi dizinini siler / korur. |
| `bolge_yonlendirme_harness.sh` | Derleyicinin LLVM çıktısını kabukta inceler. | Dosya üretmez. |
| `calisma_dizini.sh` | Başlatanın verdiği dizine EXIT temizliği kurar. | Geçerse siler, hatada yolu bildirir. |
| `cekirdek_onbellek_harness.sh` | build/obkapi.XXXXXX | Kendi dizinini siler / korur. |
| `check_genis_harness.sh` | build/chkgenis.XXXXXX — c.chk, s.chk | Kendi dizinini siler / korur. |
| `check_kapisi.sh` | check-kapisi.XXXXXX içinde kem_*_kapi.kem birleşik kaynakları. | Siler / korur. |
| `checker_diff_harness.sh` | build/checkdiff.XXXXXX — oracle.txt, aday.txt | Kendi dizinini siler / korur. |
| `ciplak_region_free_harness.sh` | build/ciplak_rf.XXXXXX — self.ll, dongu.kem, c_dongu.ll, s_dongu.ll, icin.kem, s_icin.ll, normal.kem, s_normal.ll | Kendi dizinini siler / korur. |
| `codegen_bootstrap_harness.sh` | build/cgboot.XXXXXX — ref.ll, codegen.exe, cg.ll, r.txt, c.txt, stage0.ll, lexer_ref.exe, lexer_cg.exe, parser_ref.exe, parser_cg.exe, checker_ref.exe, checker_cg.exe, stage1.ll, codegen2.exe, stage2.ll | Kendi dizinini siler / korur. |
| `codegen_diff_harness.sh` | build/cgdiff.XXXXXX — $b.c.ll, $b.c.exe, $b.k.ll, $b.k.exe | Kendi dizinini siler / korur. |
| `codegen_genis_harness.sh` | build/cggenis.XXXXXX — $b.c.ll, $b.s.ll, $b.c.exe, $b.c.out, $b.s.exe, $b.s.out | Kendi dizinini siler / korur. |
| `ct_bariyer_harness.sh` | Derleyicilerin IR metni kabuk değişkenindedir. | Dosya üretmez. |
| `dizi_sinir_harness.sh` | build/dizisinir.XXXXXX — $ad.kem, $ad.ll, $ad.exe, $ad.out, $ad.err, $ad.c | Kendi dizinini siler / korur. |
| `drf_gorunurluk_harness.sh` | build/drf_gor.XXXXXX — d.ll, d.exe | Kendi dizinini siler / korur. |
| `e4k_harness.sh` | build/e4k.XXXXXX — $c.ll, $c.err, $c.y.ll, $c.bin, tsan_dene, rt_tsan.o, g_$c.ll, g_$c, g_$c.$r.log | Kendi dizinini siler / korur. |
| `eszamanli_kosum_harness.sh` | build/eszamanli.XXXXXX — $m.kem, $m.ll, $m.err, $m.out, $m.clang, hatali-turlar.log | Kendi dizinini siler / korur. |
| `gorev_msan_harness.sh` | build/gorev_msan.XXXXXX — p.c, p, t, e, o | Kendi dizinini siler / korur. |
| `kanal_omru_harness.sh` | build/kanal_kapi.XXXXXX — $et.ll, $et, ${et}a, $et.err, $et-n.ll, $et-n | Kendi dizinini siler / korur. |
| `kem_malloc_kompozisyon_harness.sh` | build/kemmalloc.XXXXXX — self.ll, $cg.ll, $cg.o, $cg.exe, $cg.out | Kendi dizinini siler / korur. |
| `kripto_kosum_harness.sh` | build/_kripto_kosum.XXXXXX — bundle.kem, check.log, bundle.ll, gen.log, bundle.exe, link.log | Kendi dizinini siler / korur. |
| `lambda_v2_harness.sh` | build/lambdav2.XXXXXX — $ad.ll, $ad.ce, $ad.exe, $ad.le | Kendi dizinini siler / korur. |
| `lean_aksiyom_harness.sh` | build/tmp.XXXXXX — $f.olean, $, son_hata.txt, aksiyom_denetim.lean, aks.txt | Kendi dizinini siler / korur. |
| `lean_sorry_harness.sh` | Lean kaynakları ve lakefile taranır, sonuç stdout’a yazılır. | Dosya üretmez. |
| `lean_tam_harness.sh` | Lake derleme önbelleği ve modül ürünleri. | Lake yönetir, derleme çıktısı korunur. |
| `lexer_bootstrap_harness.sh` | build/lexboot.XXXXXX — lex.ll, kemlex.exe, oracle.txt, aday.txt | Kendi dizinini siler / korur. |
| `lexer_diff_harness.sh` | build/lexdiff.XXXXXX — lex.ll, kemlex.exe, oracle.txt, aday.txt | Kendi dizinini siler / korur. |
| `modul_codegen_harness.sh` | build/modcg.XXXXXX — $b.c.ll, $b.s.ll, $b.c.exe, $b.c.out, $b.s.exe, $b.err, $b.s.out, $b.c.ad, $b.s.ad | Kendi dizinini siler / korur. |
| `olcum/derleme_harness.sh` | derleme-olcum.XXXXXX, çekirdek ve IR; build/derleme_olcum.json. | Ara ürünleri siler / korur. Başarıda JSON ölçümü yayımlanır. |
| `parser_bootstrap_harness.sh` | build/parseboot.XXXXXX — p.ll, kemparse.exe, oracle.txt, aday.txt | Kendi dizinini siler / korur. |
| `parser_diff_harness.sh` | build/parsediff.XXXXXX — p.ll, kemparse.exe, oracle.txt, aday.txt | Kendi dizinini siler / korur. |
| `perf_bellek_harness.sh` | build/perfbellek.XXXXXX — p.ll, p_$b | Kendi dizinini siler / korur. |
| `qtest_harness.sh` | build/_qtest.XXXXXX — disk.img,if=none,id=d0,format=raw, $d.ll, $d.err, $d, $d.cerr, disk.img, qemu.log, $d.out | Kendi dizinini siler / korur. |
| `selfhost_driver_harness.sh` | build/selfdrv.XXXXXX — self.ll, self2.ll, self2b.ll, o.txt, a.txt, llvm.out | Kendi dizinini siler / korur. |
| `sifir_bolme_harness.sh` | build/sifir_bolme.XXXXXX — a.ll, a.exe | Kendi dizinini siler / korur. |
| `sifir_uyari_harness.sh` | build/sifir_uyari.XXXXXX — uyari.log, o.o | Kendi dizinini siler / korur. |
| `stdlib_ithal_harness.sh` | build/_stdlib_ithal.XXXXXX — $m.kem, $m.c, $m.s | Kendi dizinini siler / korur. |
| `surucu_diff_harness.sh` | build/surdiff.XXXXXX — c.chk, s.chk, c.ll, s.ll, c.d, s.d | Kendi dizinini siler / korur. |
| `surucu_kosum_harness.sh` | Doğrulama alt sürecini başlatır. | Alt süreç kendi ürünlerini yönetir. |
| `tasma_harness.sh` | build/tasma.XXXXXX — a.ll, a_$O.exe | Kendi dizinini siler / korur. |
| `test_llvm.sh` | build/llvm-test.XXXXXX — ${ad}.exe, ${ad}.ll, ${ad}.out.actual | Kendi dizinini siler / korur. |
| `yapi_diff_harness.sh` | build/yapidiff.XXXXXX — c.ll, s.ll, c.d, s.d | Kendi dizinini siler / korur. |
| `yigin_geri_harness.sh` | build/_yigin_geri.XXXXXX — birim.kem, b.ll, err | Kendi dizinini siler / korur. |
| `zaman_kabuk.sh` | Tarifin hedefi, başlangıcı, bitişi ve kodu; KEMGU_ZAMAN_DOSYASI. | İstenen ölçüm dosyası korunur. |

### tests/qemu altındaki Python modülleri

| Modül | Ürünler veya erişim | Başarı / hata |
|---|---|---|
| `acilis_suresi.py` | Senaryo dizinindeki QMP/seri günlükleri, USB/NVMe/SATA test imajları | Üst harness siler / hata dizininde korur. |
| `acpi_uefi_kahin.py` | QMP bellek dökümleri acpi-ram-*.bin | Üst harness siler / hata dizininde korur. |
| `ag_arac_es.py` | Senaryo dizinindeki QMP/seri günlükleri, USB/NVMe/SATA test imajları, pcap | Üst harness siler / hata dizininde korur. |
| `ag_arac_kahin.py` | Senaryo dizinindeki QMP/seri günlükleri, USB/NVMe/SATA test imajları, pcap | Üst harness siler / hata dizininde korur. |
| `ag_arm_derle.py` | Çağıranın belirttiği ARM çekirdek dizininde birleşik kaynak, ham/süzülmüş IR, nesne ve ELF | Yolun sahibi çağırandır. |
| `ag_arm_kahin.py` | Çalışma dizininde disk, seri ve qemu.log; build altında geçici Eş soket dizini | Kendi geçici dizinini siler / yolunu bildirip korur. İstenen çıktı çağırana aittir. |
| `ag_arp_sessiz.py` | Verilen birleşik kaynak kopyasında görev senaryosu | Yolun sahibi çağırandır. |
| `ag_es.py` | Host ağ soketi ve bellek içi çerçeve izi | Yeni geçici dosya yok. |
| `ag_es_test.py` | build altında ham çerçeve/pcap fikstürleri | Kendi geçici dizinini siler / yolunu bildirip korur. İstenen çıktı çağırana aittir. |
| `ag_http_ham_es.py` | Host Ethernet eşi üzerinden çerçeveler, bellek içi HTTP durumu | Yeni geçici dosya yok. |
| `ag_http_kahin.py` | Senaryo dizinindeki QMP/seri günlükleri, USB/NVMe/SATA test imajları, pcap | Üst harness siler / hata dizininde korur. |
| `ag_iki_konuk.py` | İki ayrı konuk dizini, imajlar, pcap, kısa soket yolları için build içinde geçici dizinler | Kendi geçici dizinini siler / yolunu bildirip korur. İstenen çıktı çağırana aittir. |
| `ag_l3_kahin.py` | Senaryo dizinindeki QMP/seri günlükleri, USB/NVMe/SATA test imajları, pcap | Üst harness siler / hata dizininde korur. |
| `ag_ntp_kahin.py` | Senaryo dizinindeki QMP/seri günlükleri, USB/NVMe/SATA test imajları, pcap | Üst harness siler / hata dizininde korur. |
| `ag_paket_buyuk.py` | Paket deposunda buyuk.elf ve paketler.txt, konuk imajları | Üst harness siler / hata dizininde korur. |
| `ag_paket_kahin.py` | Test deposunda bozuk.elf ve paketler.txt, disk imajları | Üst harness siler / hata dizininde korur. |
| `ag_parca_kahin.py` | Senaryo dizinindeki QMP/seri günlükleri, USB/NVMe/SATA test imajları, pcap | Üst harness siler / hata dizininde korur. |
| `ag_pc_matris.py` | Senaryo dizinindeki QMP/seri günlükleri, USB/NVMe/SATA test imajları, pcap | Üst harness siler / hata dizininde korur. |
| `ag_saglamlik_tel.py` | Senaryo dizinindeki QMP/seri günlükleri, USB/NVMe/SATA test imajları, pcap | Üst harness siler / hata dizininde korur. |
| `ag_soket_kahin.py` | Senaryo dizinindeki QMP/seri günlükleri, USB/NVMe/SATA test imajları, pcap | Üst harness siler / hata dizininde korur. |
| `ag_tablo_imaj.py` | Verilen birleşik kaynak kopyasına test tablosu | Yolun sahibi çağırandır. |
| `ag_tcp_kahin.py` | Senaryo dizinindeki QMP/seri günlükleri, USB/NVMe/SATA test imajları, pcap | Üst harness siler / hata dizininde korur. |
| `ag_tcp_kayip.py` | Senaryo dizinindeki QMP/seri günlükleri, USB/NVMe/SATA test imajları, pcap | Üst harness siler / hata dizininde korur. |
| `ag_tcp_secenek.py` | Senaryo dizinindeki QMP/seri günlükleri, USB/NVMe/SATA test imajları, pcap | Üst harness siler / hata dizininde korur. |
| `ag_tftp_kahin.py` | Senaryo dizinindeki QMP/seri günlükleri, USB/NVMe/SATA test imajları, pcap | Üst harness siler / hata dizininde korur. |
| `ag_udp_kahin.py` | Senaryo dizinindeki QMP/seri günlükleri, USB/NVMe/SATA test imajları, pcap | Üst harness siler / hata dizininde korur. |
| `ag_usb_kahin.py` | Depoda veri.bin, USB imajları ve pcap | Üst harness siler / hata dizininde korur. |
| `apic_kahin.py` | apic-*.txt sayaç anlık görüntüleri | Üst harness siler / hata dizininde korur. |
| `arac16_kahin.py` | Senaryo dizinindeki QMP/seri günlükleri, USB/NVMe/SATA test imajları | Üst harness siler / hata dizininde korur. |
| `belge15_kahin.py` | Belgeden çıkarılan *.kem, bunların *.elf dosyaları ve kullanıcı diski | Üst harness siler / hata dizininde korur. |
| `blok_kahin.py` | Aygıt başına *.img, seri ve blok-qemu.log | Üst harness siler / hata dizininde korur. |
| `bm_ekran.py` | QEMU seri/QMP, ekran görüntüleri ve piksel kaydı | Üst harness siler / hata dizininde korur. |
| `bm_istisna.py` | QMP soketi ve seri günlük | Üst harness siler / hata dizininde korur. |
| `bm_kabuk.py` | qemu.log, qmp-schema.json, açılış başına *.seri ve disk imajları | Üst harness siler / hata dizininde korur. |
| `bm_panik.py` | QMP soketi, seri ve panik görüntüsü | Üst harness siler / hata dizininde korur. |
| `bm_ps2.py` | PS/2 QMP soketi ve kayıtları | Üst harness siler / hata dizininde korur. |
| `bm_uefi.py` | Firmware değişken kopyası, USB/NVMe/SATA imajları, qemu.log, *.seri ve RAM dökümleri | Üst harness siler / hata dizininde korur. |
| `boru_kahin.py` | Senaryo dizinindeki QMP/seri günlükleri, USB/NVMe/SATA test imajları | Üst harness siler / hata dizininde korur. |
| `bozuk_elf_kahin.py` | Senaryo dizinindeki QMP/seri günlükleri, USB/NVMe/SATA test imajları | Üst harness siler / hata dizininde korur. |
| `calisma_dizini.py` | build altındaki kemgu-py.* dizinlerinin yaşam döngüsü | Kendi geçici dizinini siler / yolunu bildirip korur. İstenen çıktı çağırana aittir. |
| `dalga12_fat.py` | Çağıranın disk imajındaki FAT girdi ve sektörleri | Yolun sahibi çağırandır. |
| `dalga12_imaj.py` | Verilen yolda EFI/GPT ve veri bölüm imajları | Yolun sahibi çağırandır. |
| `dalga12_kahin.py` | dalga12.seri, dalga12-qemu.log, QMP soketi, *.fsck.log, pcap ve bölüm kopyaları | Üst harness siler / hata dizininde korur. |
| `disk_hata_kahin.py` | Enjeksiyon yapılandırması ile senaryoya ait disk ve seri günlükleri | Üst harness siler / hata dizininde korur. |
| `donanim_kahin.py` | Senaryo dizinindeki QMP/seri günlükleri | Üst harness siler / hata dizininde korur. |
| `duzenle_kahin.py` | Üst senaryonun diskine editör kaydı, ekran örnekleri | Üst harness siler / hata dizininde korur. |
| `e1000_kahin.py` | Senaryo dizinindeki QMP/seri günlükleri, USB/NVMe/SATA test imajları, pcap | Üst harness siler / hata dizininde korur. |
| `fat_kahin.py` | Verilen dosya sistemi imajındaki sektörler | Yolun sahibi çağırandır. |
| `fat_yazici.py` | Verilen yola FAT imajı | Yolun sahibi çağırandır. |
| `girdi15_kahin.py` | Senaryo dizinindeki QMP/seri günlükleri, USB/NVMe/SATA test imajları | Üst harness siler / hata dizininde korur. |
| `guc_kahin.py` | guc-*.seri ve üst açılışın izleri | Üst harness siler / hata dizininde korur. |
| `kabuk14_kahin.py` | Üst konsolda kabuk komutları ve test diski | Üst harness siler / hata dizininde korur. |
| `kapat_kahin.py` | Senaryo dizinindeki QMP/seri günlükleri | Üst harness siler / hata dizininde korur. |
| `kaynak14_kahin.py` | Kaynak sayacı ölçümlerini içeren koşum dosyası | Üst harness siler / hata dizininde korur. |
| `klavye16_kahin.py` | Senaryo dizinindeki QMP/seri günlükleri, USB/NVMe/SATA test imajları, pcap | Üst harness siler / hata dizininde korur. |
| `log_kahin.py` | Senaryo dizinindeki QMP/seri günlükleri, USB/NVMe/SATA test imajları | Üst harness siler / hata dizininde korur. |
| `msi_kahin.py` | Senaryo dizinindeki QMP/seri günlükleri | Üst harness siler / hata dizininde korur. |
| `pat_kahin.py` | QEMU kayıtları ve bellek tipi ölçümleri | Üst harness siler / hata dizininde korur. |
| `pc_matris.py` | Senaryo dizinindeki QMP/seri günlükleri, USB/NVMe/SATA test imajları | Üst harness siler / hata dizininde korur. |
| `pcap_oku.py` | Mevcut pcap dosyasını okur | Yeni geçici dosya yok. |
| `pci16_kahin.py` | Senaryo dizinindeki QMP/seri günlükleri | Üst harness siler / hata dizininde korur. |
| `program15_kahin.py` | Senaryo dizinindeki QMP/seri günlükleri, USB/NVMe/SATA test imajları | Üst harness siler / hata dizininde korur. |
| `ps2_irq_kahin.py` | Üst konsolun IRQ, QMP ve seri kayıtları | Üst harness siler / hata dizininde korur. |
| `psf_kahin.py` | Verilen yolda QMP screendump çıktısı | Üst harness siler / hata dizininde korur. |
| `ramdisk_kahin.py` | İç ramdisk kopyası, dış bölüm imajları | Üst harness siler / hata dizininde korur. |
| `saglamlik_kahin.py` | Senaryo dizinindeki QMP/seri günlükleri, USB/NVMe/SATA test imajları | Üst harness siler / hata dizininde korur. |
| `surec_abi.py` | ABI belgesi, çalışma zamanı ve sarmalayıcı kaynaklarını okur | Yeni geçici dosya yok. |
| `surec_elf.py` | Geçici bağlayıcı/LLVM IR/nesne dizini; çağıranın istediği kullanıcı ELF’i | Kendi geçici dizinini siler / yolunu bildirip korur. İstenen çıktı çağırana aittir. |
| `surec_imaj.py` | Çağıranın dizininde kullanıcı programları içeren disk imajları | Yolun sahibi çağırandır. |
| `surec_kahin.py` | Senaryo dizinindeki QMP/seri günlükleri, USB/NVMe/SATA test imajları | Üst harness siler / hata dizininde korur. |
| `uefi_imaj.py` | Verilen yolda FAT/GPT imajı; build altındaki geçici FAT kopyası | Kendi geçici dizinini siler / yolunu bildirip korur. İstenen çıktı çağırana aittir. |
| `uefi_kabuk_kahin.py` | kabuk-*.seri ve kabuk test diski | Üst harness siler / hata dizininde korur. |
| `usb14_imaj.py` | Çağıranın seçtiği yolda kullanıcı USB imajı | Yolun sahibi çağırandır. |
| `usb_harness.py` | build içinde USB qtest dizini, disk.img, qemu.log, dur.bin ve derleyici başına IR/ikili | Kendi geçici dizinini siler / yolunu bildirip korur. İstenen çıktı çağırana aittir. |
| `usb_hub_kahin.py` | Senaryo dizinindeki QMP/seri günlükleri, USB/NVMe/SATA test imajları | Üst harness siler / hata dizininde korur. |
| `v1_kahin.py` | Üst konsolun disk, görüntü ve seri kayıtları | Üst harness siler / hata dizininde korur. |

### Disk kapısı ve MSI temizleme deneyi

Bölüm 1 sonunda `build/` altında önceki tam koşumlardan kalan 93 geçici dizin vardı. Ölçümde toplam 2.283.462.656 bayt ayrılmıştı. Dizinler adlarıyla arşivlendi. Bölüm 2 pozitif ölçümü başlarken kökte geçici dizin sayısı sıfırdı. Başarıda yayımlanan derleme ölçümü `build/derleme_olcum.json` dosyasında bulunur.

MSI harness’ının başarılı yolundaki `rm -rf "$w"` geçici olarak çıkarıldı. Gerçek MSI kapısı iki derleyici ve iki kesme kipinde tamamlandı (rc 0). Disk kapısının tarif kodu 1, GNU Make çıkışı 2 oldu. Ayrılmış alan 1.697.509.376 bayt arttı.

```text
Disk build: önce 2283470848 B, sonra 3980980224 B, büyüme +1697509376 B
Geçici dizin: önce 0, sonra 4, yeni 4
  yeni: build/msi.Tqlo7O
  yeni: build/msi.XVrfpD
  yeni: build/msi.aSa1Ar
  yeni: build/msi.t0pOsz
make: *** [Makefile:4782: calistir_disk_denetimi] Error 1
```

Kaynak geri alındı. Deney dizinleri adlarıyla arşivlendikten sonra aynı disk kapısı 0 geçici dizinle geçti. Kabuk ve Python yardımcıları ayrıca sınandı. Başarıda çalışma dizini silindi. Kontrollü hatada dosyalar ve tanı yolu korundu. Kabuktaki özgün çıkış kodu 23 kaldı.

### İlk tam disk ölçümündeki yedi başarısız kapı

`-j10` takımının ölçülen süresi 35 dk 47 sn, çıkış kodu 2. Aynı makinede başka bir çalışma ağacının da `-j10` tam takımı çalışıyordu. Saat 16.24’te bir dakikalık yük ortalaması 31,54 idi. Bu ağaçta 14, diğerinde 9 QEMU süreci sayıldı.

```text
  başarısız: calistir_bm_x86_cok_surec calistir_bm_x86_girdi15 calistir_bm_x86_guc calistir_bm_x86_kabuk14 calistir_bm_x86_msi calistir_bm_x86_pci16 calistir_bm_x86_ps2_irq
Disk build: önce 3981037568 B, sonra 7169925120 B, büyüme +3188887552 B
Geçici dizin: önce 0, sonra 13, yeni 13
```

Yedi başarısız kapının bildirdiği 13 dizin, disk kapısının listelediği 13 adla birebir eşleşti. Seri, ELF ve derleyici günlükleri arşivlendi. Güç kâhini kendini barındıran derleyicide kapatma sırasında konuktan “KABUK HATA ACPI S5 kapanmadi” aldı (QEMU rc 1). O ELF, başarılı taban koşumlarının kullandığı önbellek anahtarıyla bayt bayt eşitti (SHA-256 `3b558716096b9005ca9cb480c380a18b2644b2d7a76dfe44700ff21400034d80`). Girdi15 kâhininde 68 ms’lik tuş basışında beklenen 4–6 tekrarın 1’i görüldü. Çok süreç kâhininde seri gelmedi. Bu sonuçlar korunuyor.

Ayrıca `make calistir_disk_denetimi` üç kez kökte sıfır geçici dizinle geçti (0,129 / 0,139 / 0,127 sn). Bütçe 60 sn.

### İkinci tam disk ölçümü

`-j10` takımının çıkış kodu 2. Yalnız hatalı kapıların bildirdiği dizinler arşivlendi. Bu kapıların ayrı tekrarları geçti.

```text
=== ZAMAN RAPORU: duvar saati 34 dk 55 sn · 155 kapı · kapı toplamı 3 sa 51 dk 23 sn · derleme/diğer 00 dk 00 sn ===
  başarısız: calistir_bm_x86_program15
Disk build: önce 7170084864 B, sonra 7599353856 B, büyüme +429268992 B
Geçici dizin: önce 0, sonra 2, yeni 2
```

### Üçüncü tam disk ölçümü

İlk koşumda adı yazılan tanı dizinleri arşivlendi. Ayrı `calistir_kem_os_x86` kapısı geçti. Aynı denetimlerle tam takım yeniden koştu (rc 0).

```text
=== ZAMAN RAPORU: duvar saati 35 dk 57 sn · 155 kapı · kapı toplamı 3 sa 54 dk 12 sn · derleme/diğer 00 dk 00 sn ===
Disk build: önce 7599525888 B, sonra 7600599040 B, büyüme +1073152 B
Geçici dizin: önce 0, sonra 0, yeni 0
```

## Bölüm 3 — ortak çekirdek önbelleği

Dokuz giriş (UEFI + sekiz PVH harness'ı) aynı dört ürünü yayımlıyor. Bağlayıcı betiği ve başlangıç nesneleri girişten gelir. Derleyici ikilisi, son birleşik kaynak, awk süzgeci, bağlayıcı betiği, sıralı nesneler, clang bayrakları, clang/lld sürümleri ve içe aktarılan KEM modüllerinin içerikleri anahtara girer. Önceki UEFI anahtarı birleşik metinde adı geçen bir sürücünün kendi dosyasındaki değişikliği kapsamıyordu. Yeni kapıda ekran çekirdeğinin çalışma dizinindeki `drivers/ekran/psf.kem` kopyasına bir yorum eklenince yalnız bu tur yeniden derlendi.

Sekiz PVH harness'ının yalnız-derle girişinde önbellek kapalı koşum ve ardından sıcak isabet ölçüldü. Her satırda dört dosyanın tamamı bayt bayt karşılaştırıldı. ARM ana makinede tam takım da çalışıyordu. Süreler saniyedir.

| Harness | Derleyici | Kapalı | İsabet |
|---|---|---:|---:|
| kabuk | C | 12,133 | 0,108 |
| ahci | C | 0,726 | 0,076 |
| usb | C | 1,308 | 0,081 |
| panik | C | 0,784 | 0,080 |
| ps2 | C | 0,768 | 0,082 |
| ekran | C | 0,720 | 0,079 |
| nvme | C | 0,718 | 0,077 |
| istisna | C | 0,800 | 0,077 |
| istisna | self-host | 5,263 | 0,086 |
| **Toplam** | | **23,220** | **0,746** |

- Nesne sabotajında başlangıç nesneleri bağlayıcıya verildi, yalnız anahtardan çıkarıldı. Günlükte `uefi/nesne-farki beklenen=derlendi gelen=isabet` çıktı, gerçek kapı rc=1. Kayıt `build/d18/bolum3-son-nesne.*/komut.log`.
- Yama sabotajında UEFI anahtarı gerçekten MSI yamasından önce hesaplandı. Günlükte `uefi/msi-yamali beklenen=derlendi gelen=isabet` çıktı, gerçek kapı rc=1. Kayıt `build/d18/bolum3-son-yama.*/komut.log`. İki deney ayrı çalışma ve önbellek kökleri kullandı.
- Düzeltmeden sonra gerçek kapı UEFI, panik ve koordinat yaması uygulanmış ekran kaynaklarını sınadı. Kaynak, derleyici ve nesne değişikliklerinin her biri üç girişte de yeniden derlemeyi tetikledi. UEFI MSI yamasının ELF'i değiştirdiği ayrıca karşılaştırıldı.
- İlk geçiş koşumunda kaldırılmış `RT` tanımları sekiz girişin açılmasını engelledi. Özgün kaynak listeleri geri kondu. Ayrıca `ciplak_region_free` ile `kem_malloc_kompozisyon` aynı anda `build/codegen` yazabiliyordu. İlk kapı bu koşumda ikiliyi bulamadı. İkisi de artık kendi çalışma dizininde bağlar ve araç hatasını döndürür. Birlikte `make -j2` rc=0. Geçiş koşumu karşılaştırmadan çıkarıldı, 16 tanı dizini korundu.
- Genişleyen önbellek kapısı tam takım yükünde 91.788 sn sürdü (rc=0). Bölüm 1 kuralıyla güncel bütçesi 138 sn, eski UEFI kapısının taban bütçesi 80 sn idi.
- İlk tamamlanan Bölüm 3 tam koşumunda 155 kapıdan ikisi kırmızıydı (36 dk 38 sn, rc=2). PCI16 köprü topolojisinde konuk SATA dosyasına `HATA bulunamadi` döndürdü. Başarısız self-host turun birleşik kaynağı ve dört ürünü, aynı kaynağın önbelleksiz derlemesiyle eşleşti (ELF SHA-256 `3b558716096b9005ca9cb480c380a18b2644b2d7a76dfe44700ff21400034d80`). Kâhin, USB tuşları arasındaki sabit host beklemesi yerine projedeki konuk LAPIC tiklerini de denetleyen süreç konsoluna geçti. Dört topoloji × iki derleyici 64 sn, rc=0. Eski kâhinle ayrı tekrar da geçtiğinden, ilk hatanın nedenini tek başına bu tekrar belirlemiyor.
- Aynı koşumda çok süreç kâhini konuk kapanış iletisini gördü, QEMU S5'te SIGSEGV ile çıktı. Kapatma komutunun Enter basışından sonra bırakma veya konuk belleğine QMP isteği gönderilmesi kaldırıldı. Süreç çıkışının 0 olması ve tam kapanış iletisi hâlâ zorunlu. Çok süreç, boru ve program15 birlikte 573 sn, rc=0. Kırmızı koşumun dört tanı dizini `build/d18/bolum3-tam.Dhj9n5/tani/` altında.

Aynı Bash gözlemcisi dokuz girişte gerçek LLVM IR nesne derlemelerini ve önbellek olaylarını saydı. Her iki tam takımın çıkış kodu 0. Zorunlu soğuk ve kapalı derlemeleri yapan host önbellek kapısı önce 5, sonra 22 çağrı yaptı. Son iki sütunda bu çağrılar çıkarıldı. Her iki takım sıcak olağan çekirdeklerle başladı.

| Tam takım | Tüm isabet | Gerçek IR derlemesi | Önbelleğe yayın | Host önbellek kapısı hariç isabet | Host önbellek kapısı hariç derleme | Duvar (sn) |
|---|---:|---:|---:|---:|---:|---:|
| Önce | 123 | 15 | 3 | 122 | 11 | 2157 |
| Sonra | 137 | 18 | 14 | 133 | 0 | 2044 |

Tam takımın duvar süresindeki fark (sonra eksi önce) -113 sn. Ölçüm dosyaları `build/d18/ortak-onbellek-{once,sonra}.tsv`, son rapor `build/d18/bolum3-son-tam.hGGrHC/zaman_raporu.txt`. Makine yükü koşumlar arasında değişti.

## Bölüm 4 — kritik yol ve kapsam eşitliği

Üç başlangıç ölçümünün en yükseğinde 480 saniyeyi aşan sekiz kapı bölündü. Make şemsiyeleri eski adlarını korudu. Süreç ve PC parçaları, özgün iki derleyici turunu birbirinden bağımsız dizinlerde eşzamanlı çalıştırır. PS/2 karma imajı her çalışmanın kendi dizinindedir. Ağ aktarımı ve diğer uzun işler tam takımın ilk yuvalarını alır. İlk denemede ayrı bir Make önkoşul satırına verilen öncelik, tarifin bulunduğu ana satırın gerisine düşüyordu; `make -qp` ile görülünce bu deneme durduruldu. Aşağıdaki ölçüm düzeltilmiş sıranın tam koşumudur.

Taban `be1cd2c:Makefile` ve yeni Makefile ayrı ayrı `make -n` ile açıldı. 288 kayıt; betik, derleyici, senaryo, açılış numarası, UEFI seçimi, PC aygıt/klavye/RAM/tablo alanlarıyla sıralanmış TSV SHA-256 değeri ikisinde de `2513b097982acf44dba5ca8edb89910f28ee99cb7b5fc97ac7b87315ca90750f`. PC ağ matrisindeki iki Python çağrısı toplam on altı QEMU sonucu üretir. Gerçek iki tam koşum günlüğünde de derleyici ve senaryo bazında 302 sonuç kaydı eşleşti.

| Şemsiye | Eski ve yeni çağrı | Parçalar ve gerçek süre (sn) |
|---|---:|---|
| `calistir_bm_x86_ag_saglamlik` | 6 | c_aktarim 444.08, c_cerceve 46.57, c_zaman 15.25, self_aktarim 509.68, self_cerceve 47.90, self_zaman 16.03 |
| `calistir_bm_x86_cok_surec` | 60 | arka 201.32, cocuk 271.91, hesap 174.23 |
| `calistir_bm_x86_kabuk14` | 20 | 1 351.53, 6 352.06 |
| `calistir_bm_x86_kaynak` | 40 | cocuk 354.86, editor 423.92 |
| `calistir_bm_x86_ps2_irq` | 28 | c 371.40, self 383.75 |
| `calistir_bm_x86_ps2_karma` | 20 | c 307.57, self 308.50 |
| `calistir_bm_x86_v1` | 40 | kesinti 292.17, tam 377.26 |
| `calistir_kem_os_pc_matris` | 74 | gpt 299.12, mbr 301.11 |

Bir parçadan `ag_saglamlik_tel.py cerceve` çağrısı gerçekten çıkarıldı. Kapı `Eksik (1): calistir_bm_x86_ag_saglamlik kemgu ag_saglamlik_tel.py cerceve` bildirdi (make rc=2). Geri alınca 288 çağrı eşleşti. Süre bütçeleri ölçülen sürelerin 1,5 katı, alt sınır 60 sn; yeni QEMU parçalarında ayrıca 600 sn üst sınırı var.

=== ZAMAN RAPORU: duvar saati 25 dk 08 sn · 169 kapı · kapı toplamı 3 sa 53 dk 13 sn · derleme/diğer 00 dk 00 sn ===

## Bölüm 5 — olay bekleme, RTC ve süreler

RTC nesnesi QOM ağacında türünden bulunur. Ekran glifleri iki donanım tarihi okumasının arasına alınır. İlk geçerli çiftin ardından hem donanım saati hem görüntü daha ileri bir değer göstermeli. Birden çok saniyenin atlandığı yoğun koşum geçerlidir. Donanım durduğunda kâhin kırmızıya düşer.

- Gerçek `make -j20 --output-sync=target calistir_bm_x86_rtc_stres` koşumunda C 10/10, self-host 10/10 geçti. RTC ilerlemesi için host gözlem süresi en az 1.018, medyan 1.038, en çok 1.062 sn. Her tur ayrı disk ve QMP soketi kullandı.
- Konukta CMOS Status B SET yazılınca `HATA RTC` ve kullanıcı süreç çıkışı 2 görüldü, kapı rc=1. İkinci gerçek deneyde donanım aynı şekilde durduruldu, SET yalnız okunan örnekte maskelendi. Ekran ve iki QOM saati de 09:42:04 değerinde kaldı. 15 saniyelik ilerleme sınırı rc=1 verdi.
- Ekran kâhininde hazır işareti `EKRAN_HIC_GELMEYECEK` yapıldı. 60 saniyede rc=1 ve son seri durumu gerçek `EKRAN_HAZIR` satırıyla basıldı.

İlk aday tam koşumu 1386 sn, rc=2. Self-host USB Girdi15 tekrarı 68 tikte 3 glif gösterdi (beklenen 4–6). O sırada tuş basılı kalma süresini QEMU milisaniyesi belirliyordu. Yeni ölçüm, konuğun tuşu aldığını fiziksel bellekten doğrular, ardından LAPIC sayacıyla 20 veya 68 tik ölçüp tuşu bırakır. Beklenen glif sayıları korundu. Aynı self-host ELF ile PS/2 ve USB yeniden geçti (31 sn). Sekiz eşzamanlı açılışta (4 USB, 4 PS/2) 20 tikte bir, 68 tikte beş glif ölçüldü. İlk başarısız koşum ve iki tanı dizini `build/d18/bolum5-kirmizi-girdi.3vSwPN/` altında.

Önceki 68 çağrının konumları `b0d09ee` satırlarıdır. i olay/koşul, ii zamanlamayı ölçen protokol fikstürü, iii gereksiz bekleme. Sekiz saniyelik boşta sızıntı/hız gözlemlerinde denetim süresi korundu. Ortak koşul yardımcısındaki kısa yoklama aralığı yalnız koşul gerçekleşmediği sürece işler.
 | Bölüm 4 dosyası ve satırı | Sınıf | Ölçülen olay veya bekleme nedeni |
|---|---|---|
| `tests/qemu/acilis_suresi.py:27` | i | EFI devri ve hazır satırı, ardışık monotonic_ns örnekleri |
| `tests/qemu/ag_l3_kahin.py:93` | ii | ARP komşu kaydının 30 sn yaşlanma sınırını aş |
| `tests/qemu/ag_ntp_kahin.py:44` | ii | NTP eşinde gecikme ölçümü için isteğe kasıtlı ağ gecikmesi |
| `tests/qemu/ag_saglamlik_tel.py:56` | i | alınan paket sayacı ilerlesin |
| `tests/qemu/ag_saglamlik_tel.py:106` | ii | TCP yeniden iletim penceresi boyunca bağlantı aşağıda |
| `tests/qemu/ag_saglamlik_tel.py:217` | iii | ICMP yanıtı zaten alındığından tur sonundaki duraklama kaldırıldı |
| `tests/qemu/apic_kahin.py:66` | i | sekiz saniyelik ölçümde PIT sabit, sonunda LAPIC tık sayısını denetle |
| `tests/qemu/bm_ekran.py:35` | i | EKRAN_HAZIR seri işareti |
| `tests/qemu/bm_istisna.py:17` | i | QMP soketi |
| `tests/qemu/bm_istisna.py:40` | i | beklenen tam seri |
| `tests/qemu/bm_istisna.py:41` | i | HMP HLT=1 |
| `tests/qemu/bm_kabuk.py:33` | i | QMP soketi |
| `tests/qemu/bm_kabuk.py:62` | i | beklenen tam seri |
| `tests/qemu/bm_kabuk.py:64` | i | xHCI tüketici göstergesi ya da i8042 çıkış tamponu |
| `tests/qemu/bm_kabuk.py:74` | i | boşta kabuk gözlemi boyunca seri ve canlılık |
| `tests/qemu/bm_kabuk.py:84` | i | beklenen ekran pikselleri |
| `tests/qemu/bm_panik.py:17` | i | QMP soketi |
| `tests/qemu/bm_panik.py:31` | i | beklenen tam seri |
| `tests/qemu/bm_panik.py:32` | i | HMP HLT=1 |
| `tests/qemu/bm_ps2.py:12` | i | QMP soketi |
| `tests/qemu/bm_ps2.py:30` | i | PS2_HAZIR |
| `tests/qemu/bm_ps2.py:32` | i | set 1 tarama baytlarının seri sayısı, E0 önekleri dahil |
| `tests/qemu/bm_uefi.py:36` | i | QMP soketi |
| `tests/qemu/bm_uefi.py:54` | i | seri açılış işareti ya da beklenen EFI hatası |
| `tests/qemu/dalga12_kahin.py:60` | i | koşul ve son okuma ile ortak üst sınır |
| `tests/qemu/dalga12_kahin.py:218` | i | iki konuk LAPIC tiki |
| `tests/qemu/duzenle_kahin.py:58` | i | sekiz saniye boyunca heap sabit |
| `tests/qemu/duzenle_kahin.py:66` | i | kayıt tamamlandı ekranı |
| `tests/qemu/duzenle_kahin.py:88` | i | hareketten sonra konuk tikleri |
| `tests/qemu/duzenle_kahin.py:91` | i | düğme geçişinden sonra konuk tikleri |
| `tests/qemu/duzenle_kahin.py:113` | i | sekiz saniye boyunca heap sabit |
| `tests/qemu/girdi15_kahin.py:47` | i | tekerlek geçişinin ardından iki konuk tiki |
| `tests/qemu/guc_kahin.py:36` | iii | QMP sıralı olayları kabul ediyor, gereksiz host gecikmesi kalktı |
| `tests/qemu/guc_kahin.py:46` | i | QEMU süreci çıktı |
| `tests/qemu/kabuk14_kahin.py:47` | i | sekiz saniye boyunca heap sabit |
| `tests/qemu/kaynak14_kahin.py:39` | i | çocuk beklerken sekiz saniye heap ve son çerçeve sayısı |
| `tests/qemu/ps2_irq_kahin.py:15` | i | LAPIC ilerlemesi |
| `tests/qemu/ps2_irq_kahin.py:20` | i | sekiz saniye boşta sayaç ve seri denetimi |
| `tests/qemu/ps2_irq_kahin.py:27` | i | HLT=1 |
| `tests/qemu/ps2_irq_kahin.py:34` | i | beklenen tam seri |
| `tests/qemu/ps2_irq_kahin.py:44` | i | ekran pikselleri |
| `tests/qemu/ps2_irq_kahin.py:51` | i | konuk tikleriyle USB girdisi |
| `tests/qemu/ps2_irq_kahin.py:62` | i | i8042 kapasitesi içindeki tuş grubunun IRQ1 teslim sayısı |
| `tests/qemu/ps2_irq_kahin.py:72` | i | halka üretici ve tüketici eşitliği |
| `tests/qemu/ps2_irq_kahin.py:87` | i | bağımsız IRQ12 sayacı |
| `tests/qemu/surec_kahin.py:58` | i | konuk LAPIC tıkı |
| `tests/qemu/surec_kahin.py:152` | i | sekiz saniye boyunca seri değişmesin |
| `tests/qemu/surec_kahin.py:159` | i | konuk tuş yoklaması |
| `tests/qemu/surec_kahin.py:176` | i | uygulama pikselleri |
| `tests/qemu/surec_kahin.py:192` | i | sekiz saniye boyunca seri ve heap sabit |
| `tests/qemu/surec_kahin.py:200` | i | ilk fare hareketinin IRQ sayacı |
| `tests/qemu/surec_kahin.py:316` | i | sekiz saniye boyunca heap sabit |
| `tests/qemu/uefi_kabuk_kahin.py:22` | i | tam seri |
| `tests/qemu/uefi_kabuk_kahin.py:28` | i | boşta seri sabit |
| `tests/qemu/uefi_kabuk_kahin.py:40` | i | konuk tikleriyle USB girdisi |
| `tests/qemu/uefi_kabuk_kahin.py:50` | i | ekran pikselleri |
| `tests/qemu/usb_harness.py:92` | i | tek istemcili qtest portunun dinleme durumu |
| `tests/qemu/v1_kahin.py:24` | i | sekiz saniye boyunca heap sabit |
| `test/ag_kosum_harness.sh:67` | i | dinleyici HAZIR satırı |
| `test/asan_matris_calistir.sh:72` | i | yerel programın çalıştırılabilir olması, geçici 127 |
| `test/codegen_diff_harness.sh:57` | i | yerel programın çalıştırılabilir olması, geçici 127 |
| `test/codegen_diff_harness.sh:69` | i | ikinci kopyayı çalıştırma sonucu |
| `test/codegen_genis_harness.sh:74` | i | süre sınırıyla gerçek yürütme sonucu, geçici 127 |
| `test/drf_gorunurluk_harness.sh:61` | i | ilk gerçek yürütme sonucu, geçici 127 |
| `test/kripto_kosum_harness.sh:59` | i | boş çıktılı geçici 127, çıktılı panik ayrı |
| `test/modul_codegen_harness.sh:80` | i | ilk yürütme sonucu, geçici 127 |
| `test/qtest_harness.sh:69` | i | qtest portunun bağlantı tüketmeden dinleme durumu |
| `test/sifir_bolme_harness.sh:68` | i | gerçek yürütme sonucu, geçici 127 |


Aynı `-j10 --output-sync=target test_tumu_olcum` komutunun duvar süreleri (sn) 1508, 1545, 1504 → 1383, 1393, 1381. İkinci önce ölçümünde bağımsız aday QEMU denemeleri de ana makineyi kullandı. Tablo tüm ölçülen kapıları içerir. GNU make aynı anda birden fazla kapı çalıştırır.
 | Kapı | Önce 1 | Önce 2 | Önce 3 | Sonra 1 | Sonra 2 | Sonra 3 | Ortalama fark (sn) |
|---|---:|---:|---:|---:|---:|---:|---:|
| `calistir_ag_es` | 6.754 | 6.765 | 6.752 | 6.757 | 6.743 | 6.732 | -0.013 |
| `calistir_ag_kosum` | 0.221 | 0.227 | 0.217 | 0.216 | 0.205 | 0.197 | -0.016 |
| `calistir_anahtar_kelime_kapisi` | 0.050 | 0.053 | 0.053 | 0.054 | 0.052 | 0.052 | +0.001 |
| `calistir_arena_test` | 0.011 | 0.012 | 0.011 | 0.011 | 0.011 | 0.012 | -0.000 |
| `calistir_arm64_test` | 0.057 | 0.055 | 0.055 | 0.061 | 0.062 | 0.054 | +0.004 |
| `calistir_asan_denetim` | 35.492 | 34.834 | 34.777 | 35.787 | 36.567 | 36.177 | +1.142 |
| `calistir_asan_matris` | 3.704 | 3.497 | 3.622 | 3.656 | 3.619 | 3.615 | +0.022 |
| `calistir_ast_test` | 0.021 | 0.011 | 0.014 | 0.011 | 0.009 | 0.011 | -0.005 |
| `calistir_baremetal_diff` | 22.283 | 35.087 | 22.190 | 22.284 | 22.392 | 22.521 | -4.121 |
| `calistir_belge_kapisi` | 0.032 | 0.032 | 0.030 | 0.037 | 0.032 | 0.033 | +0.003 |
| `calistir_bm_x86_acilis_suresi` | 49.773 | 50.823 | 49.842 | 51.977 | 51.071 | 52.048 | +1.553 |
| `calistir_bm_x86_ag_arac` | 110.115 | 111.337 | 111.735 | 91.920 | 94.848 | 93.017 | -17.801 |
| `calistir_bm_x86_ag_iki` | 165.814 | 166.371 | 165.320 | 145.014 | 140.833 | 140.761 | -23.633 |
| `calistir_bm_x86_ag_l3` | 102.626 | 103.533 | 102.776 | 95.781 | 96.485 | 96.006 | -6.888 |
| `calistir_bm_x86_ag_parca` | 24.985 | 24.686 | 24.794 | 21.952 | 22.055 | 21.822 | -2.879 |
| `calistir_bm_x86_ag_pc` | 169.725 | 171.279 | 169.656 | 143.160 | 140.819 | 143.897 | -27.594 |
| `calistir_bm_x86_ag_saglamlik_c_aktarim` | 444.077 | 440.552 | 438.592 | 434.064 | 437.334 | 438.598 | -4.408 |
| `calistir_bm_x86_ag_saglamlik_c_cerceve` | 46.573 | 46.052 | 46.006 | 39.049 | 45.456 | 39.389 | -4.912 |
| `calistir_bm_x86_ag_saglamlik_c_zaman` | 15.249 | 15.404 | 15.229 | 15.083 | 15.238 | 15.083 | -0.159 |
| `calistir_bm_x86_ag_saglamlik_self_aktarim` | 509.684 | 517.749 | 512.255 | 517.645 | 516.576 | 517.367 | +3.967 |
| `calistir_bm_x86_ag_saglamlik_self_cerceve` | 47.901 | 47.849 | 47.684 | 39.619 | 46.674 | 40.963 | -5.393 |
| `calistir_bm_x86_ag_saglamlik_self_zaman` | 16.027 | 16.122 | 15.997 | 15.767 | 16.116 | 16.073 | -0.063 |
| `calistir_bm_x86_ag_udp` | 95.440 | 94.934 | 94.954 | 62.027 | 61.968 | 62.072 | -33.087 |
| `calistir_bm_x86_ahci` | 13.109 | 13.116 | 13.333 | 13.206 | 13.396 | 13.244 | +0.096 |
| `calistir_bm_x86_apic` | 260.442 | 272.263 | 260.773 | 262.352 | 262.909 | 263.056 | -1.720 |
| `calistir_bm_x86_arac16` | 86.269 | 86.020 | 85.328 | 76.587 | 76.886 | 77.039 | -9.035 |
| `calistir_bm_x86_belge15` | 93.328 | 94.884 | 94.825 | 78.820 | 78.011 | 78.728 | -15.826 |
| `calistir_bm_x86_blok` | 7.408 | 7.439 | 7.550 | 7.417 | 7.589 | 7.743 | +0.117 |
| `calistir_bm_x86_boru` | 69.586 | 71.863 | 70.793 | 65.703 | 63.931 | 66.816 | -5.264 |
| `calistir_bm_x86_buyuk_elf` | 192.712 | 197.324 | 193.380 | 168.978 | 170.852 | 170.498 | -24.363 |
| `calistir_bm_x86_cok_surec_arka` | 201.317 | 201.204 | 199.321 | 163.832 | 167.403 | 166.398 | -34.736 |
| `calistir_bm_x86_cok_surec_cocuk` | 271.914 | 272.973 | 270.967 | 254.801 | 254.628 | 256.060 | -16.789 |
| `calistir_bm_x86_cok_surec_hesap` | 174.231 | 175.762 | 172.887 | 158.086 | 156.865 | 157.038 | -16.964 |
| `calistir_bm_x86_disk` | 106.288 | 106.179 | 104.835 | 137.952 | 137.224 | 135.042 | +30.972 |
| `calistir_bm_x86_disk_hata` | 67.462 | 67.282 | 66.687 | 77.267 | 76.819 | 76.801 | +9.819 |
| `calistir_bm_x86_donanim` | 39.630 | 40.783 | 39.773 | 41.986 | 42.470 | 42.113 | +2.128 |
| `calistir_bm_x86_dosyalar` | 428.909 | 429.347 | 428.355 | 369.393 | 370.878 | 370.611 | -58.576 |
| `calistir_bm_x86_duzenle` | 285.983 | 291.516 | 284.387 | 257.470 | 255.856 | 256.461 | -30.700 |
| `calistir_bm_x86_e1000` | 34.408 | 34.145 | 33.603 | 30.888 | 30.622 | 30.403 | -3.414 |
| `calistir_bm_x86_ekran` | 1.346 | 1.382 | 1.406 | 1.358 | 1.234 | 1.377 | -0.055 |
| `calistir_bm_x86_ekran_fare` | 431.727 | 432.397 | 431.559 | 390.196 | 391.686 | 392.209 | -40.531 |
| `calistir_bm_x86_girdi15` | 68.482 | 68.812 | 66.993 | 61.566 | 60.565 | 60.839 | -7.106 |
| `calistir_bm_x86_guc` | 34.848 | 35.409 | 34.864 | 34.564 | 34.559 | 34.758 | -0.413 |
| `calistir_bm_x86_http` | 223.007 | 222.119 | 221.279 | 203.155 | 202.657 | 201.976 | -19.539 |
| `calistir_bm_x86_istisna` | 47.348 | 56.441 | 47.496 | 39.679 | 39.356 | 39.310 | -10.980 |
| `calistir_bm_x86_kabuk` | 162.582 | 162.466 | 162.164 | 100.354 | 100.681 | 100.350 | -61.942 |
| `calistir_bm_x86_kabuk14_1` | 351.532 | 357.790 | 351.980 | 307.673 | 308.296 | 308.179 | -45.718 |
| `calistir_bm_x86_kabuk14_6` | 352.062 | 356.560 | 350.767 | 307.700 | 307.098 | 307.987 | -45.535 |
| `calistir_bm_x86_kapat` | 34.040 | 34.111 | 33.836 | 35.850 | 36.441 | 36.351 | +2.218 |
| `calistir_bm_x86_kaynak_cocuk` | 354.861 | 359.977 | 358.796 | 291.431 | 292.633 | 291.750 | -65.940 |
| `calistir_bm_x86_kaynak_editor` | 423.920 | 427.870 | 425.753 | 327.455 | 328.027 | 327.359 | -98.234 |
| `calistir_bm_x86_klavye16` | 267.013 | 268.276 | 268.072 | 234.399 | 234.331 | 235.436 | -33.065 |
| `calistir_bm_x86_log` | 40.938 | 40.411 | 40.157 | 37.803 | 38.443 | 38.295 | -2.322 |
| `calistir_bm_x86_msi` | 31.063 | 31.054 | 31.068 | 39.015 | 38.940 | 39.338 | +8.036 |
| `calistir_bm_x86_nic_matris` | 142.685 | 142.999 | 142.787 | 126.051 | 127.068 | 126.764 | -16.196 |
| `calistir_bm_x86_ntp` | 17.419 | 17.245 | 17.602 | 15.608 | 15.929 | 15.859 | -1.623 |
| `calistir_bm_x86_nvme` | 13.780 | 13.789 | 13.862 | 13.940 | 13.935 | 14.074 | +0.173 |
| `calistir_bm_x86_paket` | 93.600 | 91.989 | 93.829 | 77.939 | 81.278 | 78.609 | -13.864 |
| `calistir_bm_x86_panik` | 3.351 | 3.386 | 3.342 | 2.691 | 2.688 | 2.545 | -0.718 |
| `calistir_bm_x86_pat` | 7.996 | 7.965 | 7.890 | 8.025 | 8.223 | 8.012 | +0.136 |
| `calistir_bm_x86_pci16` | 67.086 | 65.864 | 66.319 | 60.936 | 61.939 | 61.334 | -5.020 |
| `calistir_bm_x86_program15` | 59.301 | 60.528 | 59.245 | 59.301 | 56.930 | 58.672 | -1.390 |
| `calistir_bm_x86_ps2` | 197.598 | 197.727 | 197.314 | 102.163 | 102.286 | 102.598 | -95.197 |
| `calistir_bm_x86_ps2_irq_c` | 371.395 | 393.604 | 372.036 | 347.354 | 348.156 | 348.784 | -30.913 |
| `calistir_bm_x86_ps2_irq_self` | 383.753 | 409.680 | 383.335 | 373.226 | 373.159 | 374.447 | -18.645 |
| `calistir_bm_x86_ps2_karma_c` | 307.566 | 327.926 | 307.084 | 291.396 | 291.864 | 292.714 | -22.201 |
| `calistir_bm_x86_ps2_karma_self` | 308.502 | 329.047 | 309.327 | 292.878 | 293.237 | 293.905 | -22.286 |
| `calistir_bm_x86_ramdisk` | 93.269 | 95.036 | 94.455 | 86.726 | 86.381 | 87.338 | -7.439 |
| `calistir_bm_x86_saglamlik` | 66.861 | 68.807 | 67.406 | 64.560 | 66.068 | 64.230 | -2.739 |
| `calistir_bm_x86_soket` | 29.358 | 28.679 | 29.002 | 23.627 | 23.635 | 23.553 | -5.408 |
| `calistir_bm_x86_surec` | 312.720 | 311.390 | 311.314 | 241.490 | 241.589 | 244.046 | -69.433 |
| `calistir_bm_x86_surec_abi` | 7.720 | 7.793 | 7.516 | 7.770 | 7.863 | 7.771 | +0.125 |
| `calistir_bm_x86_tcp_kayip` | 288.074 | 299.052 | 286.388 | 281.361 | 289.984 | 273.427 | -9.581 |
| `calistir_bm_x86_tcp_secenek` | 19.592 | 19.215 | 19.435 | 17.479 | 17.410 | 17.539 | -1.938 |
| `calistir_bm_x86_tcp_temel` | 42.790 | 42.964 | 43.019 | 38.595 | 38.960 | 38.886 | -4.111 |
| `calistir_bm_x86_tcp_verim` | 387.203 | 445.234 | 389.126 | 396.127 | 396.565 | 389.066 | -13.268 |
| `calistir_bm_x86_tftp_teshis` | 395.802 | 395.817 | 395.239 | 381.634 | 380.560 | 383.078 | -13.862 |
| `calistir_bm_x86_uefi` | 31.485 | 31.577 | 31.324 | 31.151 | 31.353 | 31.480 | -0.134 |
| `calistir_bm_x86_uefi_kabuk` | 322.106 | 321.752 | 321.924 | 304.733 | 304.349 | 305.290 | -17.136 |
| `calistir_bm_x86_uefi_pc` | 196.862 | 197.126 | 197.149 | 184.256 | 185.395 | 184.933 | -12.184 |
| `calistir_bm_x86_usb` | 9.691 | 9.529 | 9.937 | 9.597 | 9.624 | 9.661 | -0.092 |
| `calistir_bm_x86_usb_ag` | 205.665 | 205.958 | 203.566 | 171.727 | 172.548 | 170.218 | -33.565 |
| `calistir_bm_x86_usb_hub` | 79.597 | 80.685 | 79.544 | 71.303 | 71.756 | 72.200 | -8.189 |
| `calistir_bm_x86_v1_kesinti` | 292.175 | 294.187 | 293.170 | 253.977 | 255.603 | 256.166 | -37.929 |
| `calistir_bm_x86_v1_tam` | 377.260 | 377.599 | 377.600 | 314.830 | 317.249 | 317.812 | -60.856 |
| `calistir_bolge_atama_test` | 0.009 | 0.011 | 0.010 | 0.012 | 0.011 | 0.011 | +0.001 |
| `calistir_bolge_operand` | 5.608 | 6.617 | 5.503 | 5.496 | 5.710 | 5.696 | -0.275 |
| `calistir_bolge_sayac_yarisi` | 1.003 | 0.958 | 0.962 | 0.984 | 0.971 | 1.007 | +0.012 |
| `calistir_bolge_test` | 0.010 | 0.010 | 0.011 | 0.010 | 0.010 | 0.011 | -0.000 |
| `calistir_bolge_yonlendirme` | 0.023 | 0.020 | 0.022 | 0.022 | 0.019 | 0.021 | -0.001 |
| `calistir_capability_test` | 0.027 | 0.023 | 0.025 | 0.018 | 0.018 | 0.018 | -0.007 |
| `calistir_cekirdek_onbellek` | 89.284 | 89.310 | 88.649 | 88.848 | 90.303 | 89.313 | +0.407 |
| `calistir_chatgpt_kopru` | 161.812 | 164.847 | 161.414 | 165.843 | 165.075 | 165.659 | +2.835 |
| `calistir_check_genis` | 2.837 | 3.193 | 2.609 | 2.765 | 2.765 | 2.680 | -0.143 |
| `calistir_check_kapisi` | 3.715 | 3.642 | 3.622 | 3.644 | 3.623 | 3.697 | -0.005 |
| `calistir_checker_diff` | 4.022 | 5.368 | 4.100 | 4.157 | 4.103 | 4.196 | -0.344 |
| `calistir_ciplak_region_free` | 0.639 | 0.635 | 0.632 | 0.601 | 0.632 | 0.624 | -0.016 |
| `calistir_codegen_bootstrap` | 330.291 | 330.097 | 329.107 | 332.458 | 334.781 | 334.388 | +4.044 |
| `calistir_codegen_diff` | 24.981 | 30.837 | 25.013 | 25.133 | 25.122 | 25.257 | -1.773 |
| `calistir_codegen_genis` | 19.036 | 26.585 | 19.002 | 19.179 | 19.119 | 19.236 | -2.363 |
| `calistir_ct_bariyer` | 1.303 | 1.636 | 1.193 | 1.240 | 1.252 | 1.299 | -0.114 |
| `calistir_derleme_olcum` | 100.254 | 102.321 | 101.077 | 100.507 | 101.289 | 101.309 | -0.182 |
| `calistir_dizi_perf_test` | 0.310 | 0.302 | 0.285 | 0.307 | 0.271 | 0.302 | -0.005 |
| `calistir_dizi_sinir_test` | 14.777 | 14.686 | 14.776 | 14.751 | 14.783 | 14.815 | +0.037 |
| `calistir_drf_gorunurluk` | 0.240 | 0.233 | 0.241 | 0.246 | 0.247 | 0.246 | +0.008 |
| `calistir_drf_test` | 0.026 | 0.024 | 0.017 | 0.023 | 0.018 | 0.018 | -0.002 |
| `calistir_e4k_bellek` | 1.576 | 1.650 | 1.646 | 1.742 | 1.642 | 1.737 | +0.084 |
| `calistir_escape_test` | 0.009 | 0.012 | 0.012 | 0.013 | 0.011 | 0.012 | +0.001 |
| `calistir_eszamanli_kosum` | 0.410 | 0.411 | 0.412 | 0.413 | 0.421 | 0.396 | -0.001 |
| `calistir_fuzz_advanced` | 0.735 | 0.717 | 0.732 | 0.735 | 0.733 | 0.718 | +0.000 |
| `calistir_fuzz_test` | 0.461 | 0.445 | 0.456 | 0.460 | 0.453 | 0.463 | +0.005 |
| `calistir_gorev_msan` | 0.504 | 0.456 | 0.499 | 0.513 | 0.484 | 0.464 | +0.001 |
| `calistir_gorev_rt_test` | 0.126 | 0.125 | 0.125 | 0.126 | 0.124 | 0.125 | -0.001 |
| `calistir_json_test` | 0.009 | 0.009 | 0.009 | 0.010 | 0.009 | 0.010 | +0.001 |
| `calistir_kanal_omru` | 0.586 | 0.757 | 0.623 | 0.602 | 0.615 | 0.615 | -0.044 |
| `calistir_kapi_butcesi` | 0.170 | 0.191 | 0.169 | 0.215 | 0.190 | 0.210 | +0.029 |
| `calistir_kapi_kapsam` | 0.167 | 0.155 | 0.153 | 0.160 | 0.136 | 0.178 | -0.000 |
| `calistir_kdl_bolge_test` | 0.010 | 0.009 | 0.007 | 0.008 | 0.011 | 0.011 | +0.001 |
| `calistir_kem_malloc_kompozisyon` | 0.770 | 0.730 | 0.741 | 0.751 | 0.731 | 0.768 | +0.003 |
| `calistir_kem_os_arm` | 14.931 | 14.906 | 14.909 | 14.961 | 15.061 | 14.961 | +0.079 |
| `calistir_kem_os_arm_ag` | 42.345 | 42.143 | 42.385 | 28.760 | 42.151 | 28.911 | -9.017 |
| `calistir_kem_os_ayristirici` | 42.311 | 42.452 | 42.353 | 44.094 | 44.177 | 44.191 | +1.782 |
| `calistir_kem_os_pc_matris_gpt` | 299.117 | 302.904 | 302.678 | 252.282 | 248.296 | 251.784 | -50.779 |
| `calistir_kem_os_pc_matris_mbr` | 301.108 | 304.518 | 300.985 | 251.268 | 249.337 | 252.960 | -51.015 |
| `calistir_kripto_check` | 0.046 | 0.038 | 0.050 | 0.039 | 0.042 | 0.041 | -0.004 |
| `calistir_kripto_kosum` | 0.873 | 0.887 | 0.900 | 0.866 | 0.864 | 0.869 | -0.020 |
| `calistir_lambda_test` | 0.224 | 0.246 | 0.237 | 0.227 | 0.231 | 0.244 | -0.002 |
| `calistir_lean_sorry` | 0.078 | 0.075 | 0.081 | 0.079 | 0.079 | 0.080 | +0.001 |
| `calistir_lexer_diff` | 0.176 | 0.167 | 0.165 | 0.163 | 0.157 | 0.160 | -0.009 |
| `calistir_linear_test` | 0.021 | 0.029 | 0.032 | 0.032 | 0.029 | 0.022 | +0.000 |
| `calistir_llvm_dogrula_test` | 0.009 | 0.008 | 0.013 | 0.010 | 0.006 | 0.007 | -0.002 |
| `calistir_llvm_test` | 13.578 | 13.097 | 13.532 | 13.563 | 13.318 | 13.727 | +0.133 |
| `calistir_lsp_test` | 0.022 | 0.030 | 0.029 | 0.033 | 0.032 | 0.030 | +0.005 |
| `calistir_mmio_bare_metal` | 0.024 | 0.021 | 0.024 | 0.023 | 0.022 | 0.017 | -0.003 |
| `calistir_mmio_test` | 0.016 | 0.012 | 0.015 | 0.012 | 0.011 | 0.013 | -0.002 |
| `calistir_mmio_volatile` | 11.811 | 11.596 | 11.442 | 11.718 | 11.465 | 11.399 | -0.089 |
| `calistir_modul_codegen` | 6.426 | 7.159 | 6.217 | 6.369 | 6.479 | 6.335 | -0.206 |
| `calistir_otp_cli_test` | 0.066 | 0.066 | 0.066 | 0.073 | 0.052 | 0.069 | -0.001 |
| `calistir_panik_test` | 0.007 | 0.008 | 0.009 | 0.008 | 0.009 | 0.008 | -0.000 |
| `calistir_parser_diff` | 0.239 | 0.239 | 0.235 | 0.238 | 0.229 | 0.231 | -0.005 |
| `calistir_parser_test` | 0.013 | 0.017 | 0.016 | 0.015 | 0.016 | 0.017 | +0.001 |
| `calistir_perf_bellek` | 1.228 | 1.356 | 1.222 | 1.249 | 1.248 | 1.205 | -0.035 |
| `calistir_qemu_cekirdek` | 0.013 | 0.015 | 0.013 | 0.014 | 0.014 | 0.014 | +0.000 |
| `calistir_qemu_smoke` | 8.015 | 8.013 | 8.013 | 8.013 | 8.012 | 8.014 | -0.000 |
| `calistir_qtest` | 14.014 | 14.334 | 14.168 | 13.925 | 14.021 | 14.077 | -0.165 |
| `calistir_ring3_page_test_x86` | 20.079 | 20.083 | 20.081 | 20.086 | 20.073 | 20.086 | +0.001 |
| `calistir_runtime_link_test` | 0.003 | 0.003 | 0.004 | 0.004 | 0.003 | 0.003 | -0.000 |
| `calistir_sabitsure_test` | 0.019 | 0.015 | 0.020 | 0.014 | 0.019 | 0.017 | -0.002 |
| `calistir_self_driver` | 187.392 | 186.601 | 186.820 | 188.957 | 190.533 | 191.259 | +3.312 |
| `calistir_sembol_test` | 0.021 | 0.018 | 0.022 | 0.021 | 0.021 | 0.018 | -0.001 |
| `calistir_sifir_bolme` | 24.880 | 37.866 | 24.846 | 25.013 | 25.061 | 24.941 | -4.192 |
| `calistir_sifir_uyari` | 1.627 | 1.666 | 1.696 | 1.679 | 1.629 | 1.704 | +0.008 |
| `calistir_simd_llvm_test` | 0.231 | 0.238 | 0.248 | 0.248 | 0.241 | 0.259 | +0.011 |
| `calistir_simd_test` | 0.017 | 0.017 | 0.019 | 0.014 | 0.017 | 0.016 | -0.002 |
| `calistir_snapshot_test` | 0.062 | 0.068 | 0.064 | 0.064 | 0.066 | 0.072 | +0.003 |
| `calistir_stdlib_check` | 0.999 | 1.054 | 0.980 | 0.947 | 1.043 | 1.023 | -0.007 |
| `calistir_stdlib_ithal` | 1.610 | 1.705 | 1.570 | 1.546 | 1.646 | 1.702 | +0.003 |
| `calistir_surucu_diff` | 3.252 | 4.954 | 3.334 | 3.382 | 3.370 | 3.321 | -0.489 |
| `calistir_surucu_kosum` | 1.884 | 2.841 | 1.801 | 1.886 | 1.854 | 1.812 | -0.325 |
| `calistir_tasma` | 0.542 | 0.710 | 0.581 | 0.601 | 0.612 | 0.534 | -0.029 |
| `calistir_tip_kontrol_test` | 0.032 | 0.033 | 0.053 | 0.032 | 0.046 | 0.032 | -0.002 |
| `calistir_tip_test` | 0.011 | 0.011 | 0.011 | 0.010 | 0.010 | 0.012 | -0.000 |
| `calistir_uart_16550_test` | 0.039 | 0.037 | 0.037 | 0.028 | 0.028 | 0.036 | -0.007 |
| `calistir_uart_pl011_test` | 0.036 | 0.026 | 0.039 | 0.036 | 0.034 | 0.027 | -0.001 |
| `calistir_uart_vtable_test` | 0.008 | 0.009 | 0.009 | 0.008 | 0.009 | 0.008 | -0.000 |
| `calistir_wcet_test` | 0.020 | 0.019 | 0.020 | 0.019 | 0.018 | 0.020 | -0.000 |
| `calistir_yapi_diff` | 3.994 | 4.185 | 3.985 | 3.972 | 3.994 | 3.964 | -0.078 |
| `calistir_yazdir_bare_test` | 0.009 | 0.008 | 0.010 | 0.008 | 0.007 | 0.009 | -0.001 |
| `calistir_yigin_geri` | 67.294 | 66.545 | 67.596 | 68.004 | 67.210 | 67.098 | +0.292 |

Doğrudan değişen kâhinleri çağıran örnek kapıların üç koşum ortalamaları (sn).

| Kapı | Önce | Sonra | Fark |
|---|---:|---:|---:|
| calistir_bm_x86_program15 | 59.7 | 58.3 | -1.4 |
| calistir_bm_x86_girdi15 | 68.1 | 61.0 | -7.1 |
| calistir_bm_x86_ps2_irq_self | 392.3 | 373.6 | -18.6 |
| calistir_bm_x86_v1_tam | 377.5 | 316.6 | -60.9 |
| calistir_bm_x86_ekran_fare | 431.9 | 391.4 | -40.5 |
| calistir_bm_x86_ag_saglamlik_c_cerceve | 46.2 | 41.3 | -4.9 |
| calistir_ag_kosum | 0.2 | 0.2 | -0.0 |

Her son koşumun en uzun kapısı ve ölçülen süre (sn).
- 1. koşumda calistir_bm_x86_ag_saglamlik_self_aktarim 517.6 sn sürdü.
- 2. koşumda calistir_bm_x86_ag_saglamlik_self_aktarim 516.6 sn sürdü.
- 3. koşumda calistir_bm_x86_ag_saglamlik_self_aktarim 517.4 sn sürdü.

Son üç ölçüm sırasında 60 saniyede bir sayılan QEMU süreçleri. Diğer QEMU sütunu aynı ana makinedeki başka çalışma dizinlerini içerir.

| Ölçüm | Örnek | Dalga 18 QEMU en çok | Diğer QEMU en çok | Bir dakikalık sistem yükü medyanı |
|---|---:|---:|---:|---:|
| 1 | 23 | 17 | 1 | 9.39 |
| 2 | 23 | 16 | 0 | 9.29 |
| 3 | 23 | 15 | 0 | 9.39 |

## Bölüm 6 — sabit kaynakların sahipliği ve paralel koşum

GNU make'in çözdüğü hedefler (isteğe bağlılar ve desen kuralları dahil), kabuk tarifleri ve Python AST'si tarandı. Kabuk çağrıları ve yerel Python içe aktarmaları boyunca erişilebilen kaynaklar tüketici hedeflere bağlandı. Aynı yardımcıya ulaşan iki hedef için kip koşulu çalıştırılmadan olası erişim sayılır. Denetim kabuğun tamamını yorumlamaz. Dinamik olarak üretilen adlar ve kullanıcı ortamından sağlanan özel yollar için koşum deneyi gerekir.

`python3 tools/kapi/paylasim_denetimi.py --envanter build/paylasim.tsv` her yazıcının hedef, kaynak satırı, tür ve gerekçesini ayrı verir. Aşağıdaki anlık envanter 428 yol/uç ailesi ve 1.283 sahiplik kaydı içerir. Kaynak adları `build/` dizinine göredir. Açılı ayraç içindeki bileşen derleme veya modül desenidir. Son taramada sabit host TCP/UDP dinleyicisi görülmedi.

| Ortak kaynak | Olası sahip | Birlikte kullanım biçimi |
|---|---:|---|
| `_bm_uefi` | 94 | Eski derleme-yalnız çıktısını üretenler aynı UEFI kilidini alır. Normal açılışlar özel dizinde koşar. |
| `kem_os_pc.img` | 94 | Yayımlanan PC imajını üreten tarif, QEMU okuması bitene kadar UEFI kilidini tutar. |
| `kilitler/uefi.lock` | 94 | Sabit UEFI ürünlerini kullanan üreticilerin ortak kilit dosyası. |
| `cekirdek_onbellek` | 104 | Üretim özel dizinde, anahtar içerikten, yayımlama atomik. |
| `zaman.tsv` | 232 | Ölçümün ana hedefi ve olası 231 alt tarif. Tam ölçüm boyunca `kilitler/zaman.lock`, tek kayıt eklerken `zaman.tsv.kilit` tutulur. |
| `zaman.tsv.kilit` | 231 | Tariflerin aynı günlüğe kayıt eklemek için tuttuğu dosya kilidi. |

**Tek sahipli kaynaklar.** Tam listedeki diğer 422 kaynak aşağıda aynı üreticiye göre gruplanmıştır.

| Üretici | Kaynaklar |
|---|---|
| `_test_tumu_olcum` | `disk_baslangic.json`, `zaman_raporu.txt` |
| `build/%.o` | `<desen>.o` |
| `build/bm_a64_bolge.o` | `bm_a64_bolge.o` |
| `build/bm_a64_bolge_kemregion.o` | `bm_a64_bolge_kemregion.o` |
| `build/bm_a64_gorev.o` | `bm_a64_gorev.o` |
| `build/bm_a64_heap.o` | `bm_a64_heap.o` |
| `build/bm_a64_heap_kemmalloc.o` | `bm_a64_heap_kemmalloc.o` |
| `build/bm_a64_kanal.o` | `bm_a64_kanal.o` |
| `build/bm_a64_kesme.o` | `bm_a64_kesme.o` |
| `build/bm_a64_metin.o` | `bm_a64_metin.o` |
| `build/bm_a64_mmio.o` | `bm_a64_mmio.o` |
| `build/bm_a64_mmio_kem.o` | `bm_a64_mmio_kem.o` |
| `build/bm_a64_mmu.o` | `bm_a64_mmu.o` |
| `build/bm_a64_panik.o` | `bm_a64_panik.o` |
| `build/bm_a64_start.o` | `bm_a64_start.o` |
| `build/bm_a64_uart.o` | `bm_a64_uart.o` |
| `build/bm_a64_virtio.o` | `bm_a64_virtio.o` |
| `build/bm_a64_virtio_net.o` | `bm_a64_virtio_net.o` |
| `build/bm_a64_yazdir.o` | `bm_a64_yazdir.o` |
| `build/bm_a64_yetki.o` | `bm_a64_yetki.o` |
| `build/bm_a64_zaman.o` | `bm_a64_zaman.o` |
| `build/bm_x86_bolge.o` | `bm_x86_bolge.o` |
| `build/bm_x86_gorev.o` | `bm_x86_gorev.o` |
| `build/bm_x86_heap.o` | `bm_x86_heap.o` |
| `build/bm_x86_kesme.o` | `bm_x86_kesme.o` |
| `build/bm_x86_panik.o` | `bm_x86_panik.o` |
| `build/bm_x86_start.o` | `bm_x86_start.o` |
| `build/bm_x86_uart.o` | `bm_x86_uart.o` |
| `build/bm_x86_yazdir.o` | `bm_x86_yazdir.o` |
| `build/bm_x86_zaman.o` | `bm_x86_zaman.o` |
| `build/codegen` | `codegen` |
| `build/codegen.ll` | `codegen.ll` |
| `build/kdl_bolge.o` | `kdl_bolge.o` |
| `build/kdl_runtime.o` | `kdl_runtime.o` |
| `build/kdl_runtime_mmio.o` | `kdl_runtime_mmio.o` |
| `build/kdl_uart_pl011_aarch64.o` | `kdl_uart_pl011_aarch64.o` |
| `build/kem_user.elf` | `kem_user.elf`, `kem_user.ll`, `kem_user.o` |
| `build/kemgu` | `kemgu` |
| `build/kullanici/%.elf` | `kullanici/<desen>.elf` |
| `build/kullanici/self/%.elf` | `kullanici/self/<desen>.elf` |
| `build/test_arena` | `test_arena` |
| `build/test_ast` | `test_ast` |
| `build/test_bench` | `test_bench` |
| `build/test_bolge` | `test_bolge` |
| `build/test_bolge_atama` | `test_bolge_atama` |
| `build/test_capability` | `test_capability` |
| `build/test_dizi_perf` | `test_dizi_perf` |
| `build/test_drf` | `test_drf` |
| `build/test_escape` | `test_escape` |
| `build/test_fuzz` | `test_fuzz` |
| `build/test_fuzz_advanced` | `test_fuzz_advanced` |
| `build/test_gorev_rt` | `test_gorev_rt` |
| `build/test_json` | `test_json` |
| `build/test_kdl_bolge` | `test_kdl_bolge` |
| `build/test_lexer` | `test_lexer` |
| `build/test_lexer.o` | `test_lexer.o` |
| `build/test_linear` | `test_linear` |
| `build/test_llvm` | `test_llvm` |
| `build/test_llvm_dogrula` | `test_llvm_dogrula` |
| `build/test_lsp` | `test_lsp` |
| `build/test_mmio` | `test_mmio` |
| `build/test_otp_cli` | `test_otp_cli` |
| `build/test_panik` | `test_panik` |
| `build/test_parser` | `test_parser` |
| `build/test_runtime_link` | `test_runtime_link` |
| `build/test_sabitsure` | `test_sabitsure` |
| `build/test_sembol` | `test_sembol` |
| `build/test_simd` | `test_simd` |
| `build/test_simd_llvm` | `test_simd_llvm` |
| `build/test_snapshot` | `test_snapshot` |
| `build/test_tip` | `test_tip` |
| `build/test_tip_kontrol` | `test_tip_kontrol` |
| `build/test_uart_16550` | `test_uart_16550` |
| `build/test_uart_16550_tx` | `test_uart_16550_tx` |
| `build/test_uart_pl011` | `test_uart_pl011` |
| `build/test_uart_pl011_tx` | `test_uart_pl011_tx` |
| `build/test_uart_vtable` | `test_uart_vtable` |
| `build/test_wcet` | `test_wcet` |
| `build/test_yazdir_bare` | `test_yazdir_bare` |
| `calistir_arm64_test` | `kernel.ll`, `kernel_aarch64.o` |
| `calistir_arp_scan_test_arm` | `arp_scan_arm.elf`, `arp_scan_arm.o`, `arp_scan_arm.out` |
| `calistir_capstone_arm` | `capstone_arm.elf`, `capstone_arm.o`, `capstone_arm.out` |
| `calistir_capstone_x86` | `capstone_x86.elf`, `capstone_x86.o`, `capstone_x86.out` |
| `calistir_crashfs_test_arm` | `crashfs_arm.elf`, `crashfs_arm.o`, `crashfs_arm.out`, `disk_crashfs.img` |
| `calistir_dhcp_lease_test_arm` | `dhcp_lease_arm.elf`, `dhcp_lease_arm.o`, `dhcp_lease_arm.out` |
| `calistir_dhcp_test_arm` | `dhcp_arm.elf`, `dhcp_arm.o`, `dhcp_arm.out` |
| `calistir_diag_heap_yaz_arm` | `diag_heap.elf`, `diag_heap.ll`, `diag_heap.o`, `diag_heap.out` |
| `calistir_dns_ptr_test_arm` | `dns_ptr_arm.elf`, `dns_ptr_arm.o`, `dns_ptr_arm.out` |
| `calistir_dns_resolver_test_arm` | `dns_resolver_arm.elf`, `dns_resolver_arm.o`, `dns_resolver_arm.out` |
| `calistir_dns_test_arm` | `dns_arm.elf`, `dns_arm.o`, `dns_arm.out` |
| `calistir_dosya_test_arm` | `dosya_arm.elf`, `dosya_arm.o`, `dosya_arm.out` |
| `calistir_fs_journal_test_arm` | `disk_fs_journal.img`, `fs_journal_arm.elf`, `fs_journal_arm.o`, `fs_journal_arm.out` |
| `calistir_geri_al_test_arm` | `geri_al_arm.elf`, `geri_al_arm.o`, `geri_al_arm.out` |
| `calistir_guvenlik_bombardiman_test_arm` | `guvenlik_bombardiman_arm.elf`, `guvenlik_bombardiman_arm.o`, `guvenlik_bombardiman_arm.out` |
| `calistir_guvenlik_kalici_test_arm` | `disk_guvenlik_kalici.img`, `guvenlik_kalici_arm.elf`, `guvenlik_kalici_arm.o`, `guvenlik_kalici_arm.out` |
| `calistir_guvenlik_oku_test_arm` | `guvenlik_oku_arm.elf`, `guvenlik_oku_arm.o`, `guvenlik_oku_arm.out` |
| `calistir_guvenlik_spawn_test_arm` | `guvenlik_spawn_arm.elf`, `guvenlik_spawn_arm.o`, `guvenlik_spawn_arm.out` |
| `calistir_guvenlik_test_arm` | `guvenlik_arm.elf`, `guvenlik_arm.o`, `guvenlik_arm.out` |
| `calistir_http_get_test_arm` | `http_get_arm.elf`, `http_get_arm.o`, `http_get_arm.out`, `http_get_arm.pcap` |
| `calistir_istisna_test_x86` | `istisna_x86.elf`, `istisna_x86.o`, `istisna_x86.out` |
| `calistir_kalici_test_arm` | `disk_kalici.img`, `kalici_arm.elf`, `kalici_arm.o`, `kalici_b1.out`, `kalici_b2.out` |
| `calistir_kanal_ipc_test_arm` | `kanal_ipc_arm.elf`, `kanal_ipc_arm.o`, `kanal_ipc_arm.out` |
| `calistir_kanal_test_arm` | `kanal_arm.elf`, `kanal_arm.o`, `kanal_arm.out` |
| `calistir_kem_os_arm` | `kem_os.elf`, `kem_os.img`, `kem_os.ll`, `kem_os.o`, `kem_os.out`, `kem_os_comb.kem`, `kem_os_disk.img`, `kem_os_ham.ll`, `kem_os_routed.o` |
| `calistir_kem_os_arm_ag` | `arm_ag` |
| `calistir_kem_os_dtb_arm` | `dtb_<MB>.out`, `hucre.out`, `hucre_2_1.dtb`, `soc.out`, `soc_ranges.dtb`, `ust2g.out` |
| `calistir_kem_os_x86` | `kem_os_x86.elf`, `kem_os_x86.ll`, `kem_os_x86.o`, `kem_os_x86.out`, `kem_os_x86_comb.kem`, `kem_os_x86_disk.img`, `kem_os_x86_ham.ll`, `kem_os_x86_ist.out`, `kem_os_x86_rx.in` |
| `calistir_kemgu_os_arm` | `kemgu_init_el0.o`, `kemgu_os_arm.elf`, `kemgu_os_arm.o`, `kemgu_os_arm.out`, `kemgu_os_disk.img`, `kemgu_shell_el0.o` |
| `calistir_kernel_dizi_bare_metal` | `kernel_dizi.elf`, `kernel_dizi.ll`, `kernel_dizi.o`, `kernel_dizi.out` |
| `calistir_kernel_dizi_x86_bare_metal` | `kernel_dizi_x86.elf`, `kernel_dizi_x86.ll`, `kernel_dizi_x86.o`, `kernel_dizi_x86.out` |
| `calistir_kripto_check` | `_kripto_<bname>.kem`, `_kripto_base.kem` |
| `calistir_ls_test_arm` | `ls_arm.elf`, `ls_arm.o`, `ls_arm.out` |
| `calistir_metin_test_arm` | `metin_arm.elf`, `metin_arm.o`, `metin_arm.out` |
| `calistir_minifs_crud_test_arm` | `disk_minifs_crud.img`, `minifs_crud_arm.elf`, `minifs_crud_arm.o`, `minifs_crud_arm.out` |
| `calistir_minifs_test_arm` | `disk_minifs.img`, `minifs_arm.elf`, `minifs_arm.o`, `minifs_arm.out` |
| `calistir_mmio_bare_metal` | `kdl_runtime_mmio_bare.o` |
| `calistir_ntp_test_arm` | `ntp_arm.elf`, `ntp_arm.o`, `ntp_arm.out`, `ntp_arm.pcap` |
| `calistir_panik_bare_metal` | `kdl_panik_aarch64.o` |
| `calistir_pci_enum_test_x86` | `pci_enum_x86.elf`, `pci_enum_x86.o`, `pci_enum_x86.out` |
| `calistir_ping_sweep_test_arm` | `ping_sweep_arm.elf`, `ping_sweep_arm.o`, `ping_sweep_arm.out` |
| `calistir_port_scan_test_arm` | `port_scan_arm.elf`, `port_scan_arm.o`, `port_scan_arm.out`, `port_scan_arm.pcap` |
| `calistir_preempt_test_x86` | `preempt_x86.elf`, `preempt_x86.o`, `preempt_x86.out` |
| `calistir_priority_test_arm` | `priority_arm.elf`, `priority_arm.o`, `priority_arm.out` |
| `calistir_qemu_smoke` | `qemu_smoke.out` |
| `calistir_recon_shell2_test_arm` | `recon_shell2_arm.elf`, `recon_shell2_arm.o`, `recon_shell2_arm.out` |
| `calistir_recon_shell_test_arm` | `recon_shell_arm.elf`, `recon_shell_arm.o`, `recon_shell_arm.out` |
| `calistir_ring3_page_test_x86` | `ring3_page_x86.elf`, `ring3_page_x86.o`, `ring3_page_x86.out` |
| `calistir_ring3_proc_test_x86` | `ring3_proc_x86.elf`, `ring3_proc_x86.o`, `ring3_proc_x86.out` |
| `calistir_ring3_test_x86` | `ring3_x86.elf`, `ring3_x86.o`, `ring3_x86.out` |
| `calistir_rtc_test_arm` | `rtc_arm.elf`, `rtc_arm.o`, `rtc_arm.out` |
| `calistir_rtc_test_x86` | `rtc_x86.elf`, `rtc_x86.o`, `rtc_x86.out` |
| `calistir_sched_test_arm` | `sched_arm.elf`, `sched_arm.o`, `sched_arm.out` |
| `calistir_sched_test_x86` | `sched_x86.elf`, `sched_x86.o`, `sched_x86.out` |
| `calistir_sched_x86_test` | `sched_x86_coop.elf`, `sched_x86_coop.o`, `sched_x86_coop.out` |
| `calistir_shell_script_test_arm` | `shell_script_arm.elf`, `shell_script_arm.o`, `shell_script_arm.out` |
| `calistir_sil_test_arm` | `sil_arm.elf`, `sil_arm.o`, `sil_arm.out` |
| `calistir_sleep_test_arm` | `sleep_arm.elf`, `sleep_arm.o`, `sleep_arm.out` |
| `calistir_smp4_test_arm` | `smp4_arm.elf`, `smp4_arm.o`, `smp4_arm.out` |
| `calistir_smp4_test_x86` | `smp4_x86.elf`, `smp4_x86.o`, `smp4_x86.out` |
| `calistir_smp_atomic_test_arm` | `smp_atomic_arm.elf`, `smp_atomic_arm.o`, `smp_atomic_arm.out` |
| `calistir_smp_barrier_test_arm` | `smp_barrier_arm.elf`, `smp_barrier_arm.o`, `smp_barrier_arm.out` |
| `calistir_smp_compute_test_arm` | `smp_compute_arm.elf`, `smp_compute_arm.o`, `smp_compute_arm.out` |
| `calistir_smp_mcs_test_arm` | `smp_mcs_arm.elf`, `smp_mcs_arm.o`, `smp_mcs_arm.out` |
| `calistir_smp_prodcons_test_arm` | `smp_prodcons_arm.elf`, `smp_prodcons_arm.o`, `smp_prodcons_arm.out` |
| `calistir_smp_queue_test_arm` | `smp_queue_arm.elf`, `smp_queue_arm.o`, `smp_queue_arm.out` |
| `calistir_smp_rwlock_test_arm` | `smp_rwlock_arm.elf`, `smp_rwlock_arm.o`, `smp_rwlock_arm.out` |
| `calistir_smp_seqlock_test_arm` | `smp_seqlock_arm.elf`, `smp_seqlock_arm.o`, `smp_seqlock_arm.out` |
| `calistir_smp_sort_test_arm` | `smp_sort_arm.elf`, `smp_sort_arm.o`, `smp_sort_arm.out` |
| `calistir_smp_test_arm` | `smp_arm.elf`, `smp_arm.o`, `smp_arm.out` |
| `calistir_smp_test_x86` | `smp_x86.elf`, `smp_x86.o`, `smp_x86.out` |
| `calistir_smp_ticket_test_arm` | `smp_ticket_arm.elf`, `smp_ticket_arm.o`, `smp_ticket_arm.out` |
| `calistir_stdlib_check` | `_run_<mod>`, `_run_<mod>.kem`, `_run_<mod>.ll`, `_stdlib_<mod>.kem` |
| `calistir_syscall_abi_test_x86` | `syscall_abi_x86.elf`, `syscall_abi_x86.o`, `syscall_abi_x86.out` |
| `calistir_syscall_test_x86` | `syscall_x86.elf`, `syscall_x86.o`, `syscall_x86.out` |
| `calistir_tcp_close_test_arm` | `tcp_close_arm.elf`, `tcp_close_arm.o`, `tcp_close_arm.out`, `tcp_close_arm.pcap` |
| `calistir_tcp_connect_test_arm` | `tcp_connect_arm.elf`, `tcp_connect_arm.o`, `tcp_connect_arm.out`, `tcp_connect_arm.pcap` |
| `calistir_tcp_test_arm` | `tcp_arm.elf`, `tcp_arm.o`, `tcp_arm.out`, `tcp_arm.pcap` |
| `calistir_timer_test_x86` | `timer_x86.elf`, `timer_x86.o`, `timer_x86.out` |
| `calistir_traceroute_test_arm` | `traceroute_arm.elf`, `traceroute_arm.o`, `traceroute_arm.out`, `traceroute_arm.pcap` |
| `calistir_uart_16550_bare_metal` | `kdl_uart_16550_x86_64.o` |
| `calistir_uart_echo_bare_metal` | `kernel_echo.elf`, `start_aarch64.o`, `uart_echo.ll`, `uart_echo.o`, `uart_pl011_bm.o`, `yazdir_bare_bm.o` |
| `calistir_uart_merhaba_bare_metal` | `kernel.elf`, `uart_merhaba.ll`, `uart_merhaba.o` |
| `calistir_uart_merhaba_x86_bare_metal` | `kernel_x86.elf`, `qemu_smoke_x86.out`, `uart_merhaba_x86.ll`, `uart_merhaba_x86.o` |
| `calistir_udp_test_arm` | `udp.pcap`, `udp_arm.elf`, `udp_arm.o`, `udp_arm.out` |
| `calistir_userspace_dhcp_test_arm` | `userspace_dhcp_arm.elf`, `userspace_dhcp_arm.o`, `userspace_dhcp_arm.out`, `userspace_dhcp_arm.pcap` |
| `calistir_userspace_dns_test_arm` | `userspace_dns_arm.elf`, `userspace_dns_arm.o`, `userspace_dns_arm.out`, `userspace_dns_arm.pcap` |
| `calistir_userspace_fiber_test_arm` | `userspace_fiber_arm.elf`, `userspace_fiber_arm.o`, `userspace_fiber_arm.out` |
| `calistir_userspace_http_test_arm` | `userspace_http_arm.elf`, `userspace_http_arm.o`, `userspace_http_arm.out`, `userspace_http_arm.pcap` |
| `calistir_userspace_jmp_test_arm` | `userspace_jmp_arm.elf`, `userspace_jmp_arm.o`, `userspace_jmp_arm.out` |
| `calistir_userspace_malloc_test_arm` | `userspace_malloc_arm.elf`, `userspace_malloc_arm.o`, `userspace_malloc_arm.out` |
| `calistir_userspace_net_test_arm` | `userspace_net_arm.elf`, `userspace_net_arm.o`, `userspace_net_arm.out` |
| `calistir_userspace_ping_test_arm` | `userspace_ping_arm.elf`, `userspace_ping_arm.o`, `userspace_ping_arm.out`, `userspace_ping_arm.pcap` |
| `calistir_userspace_post_test_arm` | `userspace_post_arm.elf`, `userspace_post_arm.o`, `userspace_post_arm.out`, `userspace_post_arm.pcap` |
| `calistir_userspace_sched_test_arm` | `userspace_sched_arm.elf`, `userspace_sched_arm.o`, `userspace_sched_arm.out` |
| `calistir_userspace_shm_test_arm` | `userspace_shm_arm.elf`, `userspace_shm_arm.o`, `userspace_shm_arm.out` |
| `calistir_userspace_tcp_test_arm` | `userspace_tcp_arm.elf`, `userspace_tcp_arm.o`, `userspace_tcp_arm.out`, `userspace_tcp_arm.pcap` |
| `calistir_userspace_test_arm` | `userspace_arm.elf`, `userspace_arm.o`, `userspace_arm.out` |
| `calistir_userspace_tftp_test_arm` | `tftp`, `tftp/dosya.txt`, `userspace_tftp_arm.elf`, `userspace_tftp_arm.o`, `userspace_tftp_arm.out`, `userspace_tftp_arm.pcap` |
| `calistir_yazdir_bare_bare_metal` | `kdl_yazdir_bare_aarch64.o` |
| `kem_os_paket_deposu` | `paket_deposu` |
| `kemgu_self` | `kemgu_self`, `kemgu_self.ll` |
| `test_tumu_olcum` | `kilitler/zaman.lock` |

**Çakışmalar ve koşum kanıtları**

- Önceki Makefile tarifleri iki PL011 derlemesinin aynı nesneyi yazdığını gösterdi. İki tüketici artık başlık ve C kaynağına bağımlı tek nesne hedefini kullanıyor. Her iki çapraz derleme kapısı geçti.
- ARM DTB kapısı ana ARM kapısının ürettiği çekirdek imajını kullanıyor. DTB önyüklemelerinde QEMU disk yazılarını kendi geçici katmanına alıyor. Yayımlanan UEFI PC imajı ile derleme-yalnız dizini, her iki ürünü de kullanan kipte tek kilitle korunuyor.
- Host TCP gidiş-dönüşünde 12 eşzamanlı kapı × iki derleyici 24 ayrı dinleyici portu kullandı. Altı qtest aygıtı iki derleyiciyle 19 saniyede geçti. QEMU port 0 ile kendisi dinliyor. Kâhin PID'den portu bulurken tek qtest bağlantısını tüketmiyor. USB kâhininin QMP soketi ve hata günlüğü koşum dizininde.
- İki bağımsız ölçüm üreticisi aynı dosya kilidinde sıraya girdi. Her alt make içinde dört işin aynı anda çalıştığı ölçüldü. 24 yazıcıdan gelen 128 süre kaydının tamamı doğru biçimde okundu. Bunlar hem shell `flock` hem Python POSIX kilidiyle aynı dosyaya yazıldı. Deney başarılı ve rc=7 döndüren tarifleri ayrı doğruladı.
- Sabotajda parser ve tip denetimi harness'larına aynı `build/_b6_ortak` çalışma dizini verildi. Statik kapı iki hedefin ve iki betiğin adını bildirdi (make rc=2). Aynı değişiklikle iki kapı gerçek `-j10` koşumunda yarıştı. Parser AST yerine tip denetiminin `CP006` satırını okudu. Parser 2/15, tip denetimi 311/321, make rc=2. Özel dizinler geri gelince aynı iki hedef 15/15 ve 321/321 geçti.
- İlk tam `-j20` denemesinde 703 saniye sonra make rc=2 döndü. Tek kırmızı hedef `calistir_bm_x86_cok_surec_cocuk` idi. İki derleyici de gerçek çocukların ayrı sayfalarını gördü ve yüz çocuğun tamamlanmasını bildiren son seri satırında eski 15 saniyelik sınıra ulaştı. İki QEMU çalışma dizini ve derleyici günlükleri ölçümün tanı dizininde saklandı.
- Çocuk kâhini artık QMP ile son tahsis edilen PID'yi ve gerçek seri çıktısını birlikte okuyor. PID ilerlemesi durursa 15 saniye, bütün deney için 90 saniye sınırı var. Başarı için PID 102, beklenen seri baytları, iki ayrı fiziksel sayfa ve geri verilen çerçeveler birlikte doğrulanıyor. İkinci stres denemesindeki 20 açılışta C çekirdeği 45.416, self-host 45.385 başlangıç çerçevesine döndü. Çocukların toplanması 11,69–13,05 saniye sürdü.
- İkinci tam `-j20` denemesinde 656 saniyede tek hata `calistir_bm_x86_apic` oldu. Altı C açılışı tamamlandı. Yedincide Unix soketi dosya sisteminde göründüğü anda yapılan QMP bağlantısı errno 111 döndürdü. Yerel Unix soketinde `bind` ve `listen` aralığı kontrollü olarak tekrarlandı. Bağlantıyı kabul etmeyen soket tanıda errno 111 ile zaman aşımına uğradı. Aynı sunucu 122 ms sonra dinlemeye geçtiğinde yeni kâhin ilk gerçek bağlantıdan iletilen üç baytı okudu. x86 ve ARM QMP başlatıcıları bu yordamı kullanıyor.
- QMP bağlantı onarımından sonraki ilk tam `-j20` koşumu 914 sn ve rc=0. Yüz çocuk için son PID=102 gözlenirken bazı kapanışlar 17 saniyeyi geçti. Eski 15 saniyelik ortak sınır, sağlıklı ve ilerlemekte olan konuğu bu yükte kesiyordu.
- Ardından `semafor_n` 12. turda rc=2 verdi. İlk tabandaki aralıklı hatanın bu kez çıkış kodu ve ikilisi saklandı. Aynı eski ikili tek host CPU'ya bağlandığında 30 turun 23'ünde rc=2, 7'sinde rc=42 döndü. Boş döngü iki izin sahibinin örtüşmesini garanti etmiyordu. Yeni fikstürde dört işçi her tur başında buluşuyor ve izin alan ilk işçi ikincisiyle buluşana kadar içeride kalıyor. Bağımsız kilitle tepe, 800 giriş ve kapanıştaki sıfır bakiye denetleniyor. İki derleyici tek host CPU üzerinde ayrı ayrı 100/100 geçti. Fikstürde izin sayısı 1 yapıldığında her iki ikili 2 sn deney sınırında takıldı. İzin sayısı 4 yapıldığında her iki ikili 20/20 turda üst sınır ihlalini rc=1 ile saptadı. Normal kapıda buluşma zaman aşımı 10 saniyedir.
- Başka bir `-j20` denemesinde standart kütüphane bariyerinin gözlemcisi hata verdi. Önceki üç katılımcılı örnekte iki işçi bariyere gelmeden önce aynı tam sayıyı kilitsiz okuyup artırıyordu. Aynı kayıtlı ikili 2.000 koşumda 24 kez “sayac 2 degil” dedi. Yeni koşum örneğinde her işçi kendi varış alanını yazar. Ana görev iki alanın da beklenen nesli gösterdiğini görevleri birleştirmeden önce okur. Tek katılımcıda iki buluşma ve üç katılımcıda beş yeniden kullanım ölçüldü. C ve self-host ayrı ayrı 500/500 geçti. Teste bağlanan geçici modülde buluşma atlandığında iki derleyici ayrı ayrı 20/20 rc=1 verdi. Eski kaynak tip denetiminden geçmeye devam ediyor.

| Koşum | Duvar saati | 100 çocuk, en kısa/uzun | make / tee |
|---|---:|---:|---|
| Seri, SUREC_PARALEL=0 | 4 sa 48 dk 11 sn (17291 sn) | 11.73 / 12.32 sn | 0 / 0 |
| -j10 ölçümlü | 23 dk 24 sn (1404 sn) | 11.97 / 13.06 sn | 0 / 0 |
| -j20 ilk | 14 dk 59 sn (899 sn) | 12.23 / 17.16 sn | 0 / 0 |
| -j20 ikinci | 15 dk 10 sn (910 sn) | 12.04 / 17.97 sn | 0 / 0 |

Ölçümlü alt takımda 170 kapı, en uzun calistir_bm_x86_ag_saglamlik_self_aktarim (521.2 sn). Başarıdan sonra kalan koşum dizini 0. §7 ortak x86 kapıları takım içinde geçti. Ek x86 çekirdek ve ARM DTB kapıları 55 sn, rc=0. Özel DTC 1.7.0 ile hücre genişliği ve GIC adı, 2 GiB bellek ve iç içe aygıt adresi çevirisi üçü de gerçek ARM konuğunda geçti (129 sn, rc=0). Bölüm süresi 6 sa 46 dk 00 sn.

| Örneklenen koşum | Dakikalık örnek | Yerel QEMU en çok | Diğer QEMU en çok | Sistem yükü medyanı |
|---|---:|---:|---:|---:|
| j20-bir | 15 | 29 | 1 | 23.33 |
| j20-iki | 15 | 27 | 1 | 23.53 |
| j10 | 23 | 17 | 1 | 9.84 |
| seri | 288 | 2 | 17 | 1.13 |

## Bölüm 7 — değişikliğe göre kapı seçimi

`test_secili` tabana göre çalışma ağacındaki, indeksteki ve izlenmeyen dosyaları okur. Yeniden adlandırma silme ve ekleme olarak değerlendirilir. Eşleme bulunmazsa sonuç `test_tumu` olur. Her yol için eşleşen desen basılır. Sürücü kuralı uygulanacaksa izlenen tablo, güncel kaynaklardan üretilen satırlarla karşılaştırılır. Fark varsa tam takım seçilir. Açık bir taban Git tarafından çözülemezse işlem hatayla sona erer.

Sürücü eşlemesi GNU make tarifleri ve önkoşullarından kabuk/Python girişlerini bulur. Doğrudan derlenen kaynakları, dosya desenlerini ve KEMGU `kullan` yollarını transitif olarak izler. Tablo yalnız `test_tumu` içinde erişilen kapıları içerir. Çalışma anında dal seçen ortak harness'lar için bütün olası kaynaklar kapsandığından bir sürücü satırı fazladan kapı seçebilir. Çekirdek önbelleğiyle aynı içe aktarma çözümü kullanılıyor. Önceki ve yeni modül özetleri 24 gerçek çekirdek ve qtest girdisinde aynı çıktı. GNU make'in Windows değişkenleri ve QEMU/OVMF önkoşulu kapalıyken üretilen tablo da aynı çıktı. Bu karşılaştırma Linux üzerinde yapıldı.

| Sürücü alanı | Seçilen kapı |
|---|---:|
| acilis | 78 |
| acpi | 80 |
| ag | 80 |
| ahci | 82 |
| bellek | 78 |
| blok | 84 |
| disk | 81 |
| duzenle | 58 |
| ekran | 86 |
| elf | 79 |
| fs | 85 |
| girdi | 76 |
| nvme | 82 |
| ortak | 94 |
| pci | 79 |
| ps2 | 79 |
| sim | 1 |
| usb | 82 |
| uygulama | 59 |
| virtio | 3 |
| zaman | 81 |

Genel belgeler için JSON belge/ölçüm kapısı ve gerçek konukta program örneğini ve kabuk komutlarını ölçen `belge15` koşar. Dil kartı için köprü, gramer için anahtar kelime kapısı, ABI tablosu için doğrudan ya da soket senaryosunda ABI denetimini çağıran dört kapı eklenir.

**Geriye dönük ölçüm.** Dalın Bölüm 6 sonundaki 30 commit'i ilk ebeveynine göre karşılaştırıldı. Her commit'in kendi Git arşivinden Make önkoşulları ve kaynak tüketicileri okundu. Korunan ortak yapım girdilerinde o sürümün bütün QEMU kapıları gerekli sayıldı. Diğer yollarda gerekli kapılar, değişen kaynağı kullanan QEMU hedefleriyle kesişimden hesaplandı. 26 tam takım ve dört belge seçimi vardı. Bu aralıkta yalnız sürücü değiştiren gerçek commit yoktu. Beş sürücü alanı için bağımsız sabit beklentiler ayrıca kapıda denendi.

| Commit | Dosya | Seçim | Değişikliğe bağlı QEMU kapısı | Eksik |
|---|---:|---|---:|---:|
| 0cbdefd7 | 29 | tam takım | 98 | 0 |
| eec055b6 | 39 | tam takım | 98 | 0 |
| b0d09eef | 11 | tam takım | 98 | 0 |
| be1cd2c8 | 20 | tam takım | 71 | 0 |
| 2ce7ef0f | 74 | tam takım | 77 | 0 |
| abf1265f | 9 | tam takım | 77 | 0 |
| 0ce56bf1 | 9 | tam takım | 77 | 0 |
| 21a75819 | 1 | calistir_belge_kapisi calistir_bm_x86_belge15 | 0 | 0 |
| 56177d1e | 1 | tam takım | 27 | 0 |
| 37639955 | 2 | calistir_belge_kapisi calistir_bm_x86_belge15 | 0 | 0 |
| 14fc95c2 | 3 | tam takım | 77 | 0 |
| e6696869 | 2 | tam takım | 77 | 0 |
| 54f80372 | 1 | calistir_belge_kapisi calistir_bm_x86_belge15 | 0 | 0 |
| 7bd36a30 | 1 | calistir_belge_kapisi calistir_bm_x86_belge15 | 0 | 0 |
| 9070ec70 | 3 | tam takım | 77 | 0 |
| cdf62656 | 3 | tam takım | 77 | 0 |
| 38ffa38a | 124 | tam takım | 76 | 0 |
| 9c961b60 | 3 | tam takım | 47 | 0 |
| bd8e3b69 | 2 | tam takım | 76 | 0 |
| a574aa68 | 12 | tam takım | 76 | 0 |
| 4952ce15 | 10 | tam takım | 76 | 0 |
| 463a7348 | 16 | tam takım | 59 | 0 |
| b2acc00c | 10 | tam takım | 55 | 0 |
| 325dadbd | 10 | tam takım | 55 | 0 |
| 747df15d | 11 | tam takım | 76 | 0 |
| 7fa52376 | 12 | tam takım | 74 | 0 |
| 4b017441 | 13 | tam takım | 73 | 0 |
| 31e27dd7 | 14 | tam takım | 72 | 0 |
| 65674dfe | 19 | tam takım | 71 | 0 |
| 0cb5a22e | 13 | tam takım | 70 | 0 |
30 commit, 0 eksik QEMU kapısı.

**Eksik satır deneyi.** `drivers/sim/*` satırı gerçekten silindi. Seçici `drivers/sim/kabuk.kem` için tam takıma döndü. Sabit beklenti olan `calistir_chatgpt_kopru` tek başına seçilmediği için `calistir_kapi_secici` make rc=2 verdi. Satır geri konunca aynı kapı 1 sn içinde geçti. Git fikstürü ayrıca temiz ağaç, silinen dosya, izlenmeyen dosya ve adında boşluk ve Türkçe harf olan yeniden adlandırmayı denetler.

Gerçek Make girişinde özel Git indekslerinden üretilen iki karşılaştırma ağacı kullanıldı. Yalnız LOOP.md farkında iki belge kapısı 72 sn, yalnız sim sürücüsü farkında köprü kapısı 151 sn sürdü. İkisi de make/tee rc=0. §7 kapıları ve paylaşım/seçim denetimleri 317 sn, make/tee rc=0.

## Bölüm 8 — CI işlerinde paralellik

Taban commit'i `0ce56bf` için GitHub Actions koşumu 45807330890. Aşağıdaki süreler Actions iş ve adım zaman damgalarının farkıdır. İki ham iş günlüğü de indirilerek takımın gerçekten seri Make komutu ile çalıştığı doğrulandı. Günlükte komut ile son başarı satırı arası Linux için 2.099,94 sn, Windows için 3.764,97 sn sürdü.

| Platform | Eski iş süresi | Eski tam takım adımı | Sonuç |
|---|---:|---:|---|
| [Linux](https://github.com/msyrt-sys/kemgu/actions/runs/45807330890/job/139865903237) | 35 dk 17 sn | 35 dk 00 sn | success |
| [Windows](https://github.com/msyrt-sys/kemgu/actions/runs/45807330890/job/139865903738) | 1 sa 03 dk 43 sn | 1 sa 02 dk 45 sn | success |

Linux ve Windows'taki altışar Make tarifi paralel çalışacak şekilde değişti. Linux ölçümlü takım raporunu yazar ve alt sürecin hatasını korur. YAML karşılaştırmasında değişiklikler bu 12 tarifle sınırlı kaldı. Gerçek YAML'den çıkarılan komutlar bir argüman kaydedicisiyle Bash altında yürütüldü. Linux'ta make başarı/hata ve rapor var/yok bileşimlerinin dördü denendi. Windows komutundaki işlemci sayısı değişkeninin açılımı Linux'ta MSYS sözdizimi ile sınandı.

Sonraki CI süreleri gerçek draft PR koşumuna bağlı. Dal öneki push tetikleyicisine girmiyor ve kullanıcı draft PR'ı Bölüm 20 sonuna ayırdı. Bu yerel doğrulamalar Actions'ta geçen süre olarak tabloya yazılmadı.

## Bölüm 9 — x86 işlemci keşfi

OVMF q35, 256 MiB RAM ve x86 TCG çok iş parçacığı kipinde iki ayrı UEFI çekirdeği ölçüldü. CPU sırası, QOM APIC kimliği, seri kayıt, fiziksel kimlik sayfası ve ham MADT sağlama toplamı karşılaştırıldı. BSP kimliği LAPIC yazmacından geldi. Her satır kendi QEMU sürecidir.

| Derleyici | vCPU | APIC kimlikleri | QMP host thread-id |
|---|---:|---|---|
| C | 1 | [0] | 368271 |
| C | 2 | [0, 1] | 369675, 369676 |
| C | 4 | [0, 1, 2, 3] | 371064, 371065, 371066, 371067 |
| C | 8 | [0, 1, 2, 3, 4, 5, 6, 7] | 372467, 372468, 372469, 372470, 372471, 372472, 372473, 372474 |
| self-host | 1 | [0] | 375635 |
| self-host | 2 | [0, 1] | 376096, 376097 |
| self-host | 4 | [0, 1, 2, 3] | 376847, 376848, 376849, 376850 |
| self-host | 8 | [0, 1, 2, 3, 4, 5, 6, 7] | 377501, 377502, 377503, 377504, 377505, 377506, 377507, 377508 |

Bütün yerel APIC kayıtları tür 0 ve bayrak 1 idi. Ayrı host iş parçacıkları MTTCG çalışmasını doğruluyor. Keşif anında AP'lere henüz konuk işi gönderilmediğinden bu ölçüm AP yürütme süresini göstermez. ARM ana makinedeki x86 TCG serpiştirmesi gerçek x86 donanımından farklıdır.

**Bit 1 deneyi.** Özel kaynak ağacında `bayraklar & 3` gerçekten `bayraklar & 1` yapıldı. Dört QEMU topolojisi de yeşil kaldı. Sebep firmware'in yalnız etkin bitini vermesiydi. Çevrimiçi-yapılabilir tekil kaydı olan host fikstürü iki derleyicide de rc=3 döndürdü. İkinci gerçek deneyde etkin APIC 1 kaydı süzgeçten çıkarıldı. İki vCPU'lu konuk yalnız BSP'yi bildirdi, QMP iki CPU gösterdi ve kapı rc=1 döndü. Temiz kaynak ağacı deney süresince ayrı tutuldu.

İlk geliştirme denemesinde yalnız yazılan iki dahili küresel değişken LLVM iyileştirmesinde siliniyordu. Konuk listesi doğruydu ancak QMP fiziksel kayıt denetimi eksik sembolde durdu. Çekirdek numarasını doğrulayarak fiziksel listeden APIC kimliğini alan çalışma zamanı erişimcisi eklendi. Ardından iki derleyiciyle ELF sembolleri ve bellek değerleri doğrulandı.

Temiz keşif kapısı sıcak önbellekte 26 sn, iki derleyici için boş özel önbellekte 37 sn sürdü. Kapı bütçesi 60 sn. §7 ve sans-IO regresyonları dahil birleşik koşum 315 sn, rc=0.

## Bölüm 10 — UEFI'den uygulama işlemcilerini başlatma

Olağan UEFI çekirdeği ve süreç kabuğu aynı başlatma yolunu kullanıyor. QMP üzerinden firmware tanımını ve her vCPU'nun CR0, CR3, EFER, CS ve RSP yazmaçlarını okuduk. Her AP'nin fiziksel kaydındaki LAPIC kimliği QOM ile eşleşti. BSP'nin 512 KiB yığını ile AP'lerin 16 KiB yığınlarının sınırları ayrı ayrı karşılaştırıldı.

| Gerçek konuk değişikliği | Gözlem | Kapı rc |
|---|---|---:|
| İkinci SIPI gönderimi kaldırıldı | C ve self-host, 2/4/8 işlemcili altı konuk da açıldı. Bu QEMU sürümünde tek SIPI yeterli. | 0 |
| SIPI iki sayfa ileri, deney için ayrılmış ve yalnız durma döngüsü içeren sayfaya gitti | Konuk bir saniyelik AP onay süresini doldurdu. Seri günlükte zaman aşımı vardı, AP onayı yoktu. | 1 |
| Yığın adayı BSP'nin gerçek yığın tepesine değiştirildi | Konuk SIPI öncesinde `AP yigin siniri` verdi. İki aktif işlemciye aynı yığını vererek bellek bozulmasını tetikleme deneyi uygulanmadı. | 1 |
| Alçak bellekte uygun sayfa çifti bulunamadı | Seri günlük gerekçeyi verdi. QMP iki vCPU saydı, konuk fiziksel çevrimiçi sayacı birde kaldı ve BSP açılışa devam etti. | 0 |

Dört değişiklik birbirinden yalıtılmış derlemelerde sınandı. Trampolin, başlangıç nesnesinin gerçek girdisi ve önbellek anahtarına bu nesnenin içeriği giriyor. İlk bütünleşme kontrolü önceki bölümün tek başına tür denetiminden geçmeyen özel tanı girişini buldu. Keşif ve başlatma kapıları artık dağıtılan UEFI çekirdeğini açıyor. Muafiyet eklenmedi.

Tek vCPU'da 50 süreç tekrarı boyunca süreç heap artışı yine 0 bayt. C kabuğu 4788464 → 5247552 bayt, self-host kabuğu 5050928 → 5510016 bayt kullandı. Her iki çift taban koşumunda da görüldü. Serbest çerçeve sayısında §5.6 farkı var. Aynı kabuğun eski ve yeni ELF program başlıklarını doğrudan okuyunca firmware'in ayırdığı fiziksel aralık şu şekilde değişti.

| Derleyici | Önceki PT_LOAD kapsamı | Yeni PT_LOAD kapsamı | Önce / sonra boş çerçeve |
|---|---:|---:|---:|
| C | 7716 sayfa | 7717 sayfa | 45416 / 45415 |
| self-host | 7747 sayfa | 7750 sayfa | 45385 / 45382 |

Dört ELF de 0x2000000 adresinden başlıyor. C için son adres 0x3e24000 → 0x3e25000, self-host için 0x3e43000 → 0x3e46000. Farkın tamamı büyüyen yüklü imajla örtüşüyor. Tek işlemcide AP yığını ayrılmıyor. Bölüm 14'te aynı ölçüm yinelenecek. Mutlak eşitlik bu sürümde açık.

Ham tam koşumda AP başına zamanlar INIT gönderiminden onayın BSP tarafından okunmasına kadar ölçüldü. Her AP için zorunlu 10 ms INIT beklemesi dahildir. Yedi konuğun her biri 0x1000 fiziksel sayfasını kullandı.

| İmaj | vCPU | CR3 | Ayrı AP yığını | AP başına µs |
|---|---:|---:|---:|---|
| C UEFI | 2 | 0x203d000 | 1 | 10457 |
| C UEFI | 4 | 0x203d000 | 3 | 10454, 10248, 10259 |
| C UEFI | 8 | 0x203d000 | 7 | 10478, 10281, 10269, 10262, 10247, 10248, 10228 |
| self-host UEFI | 2 | 0x2044000 | 1 | 10483 |
| self-host UEFI | 4 | 0x2044000 | 3 | 10436, 10255, 10241 |
| self-host UEFI | 8 | 0x2044000 | 7 | 10501, 10363, 10279, 10268, 10259, 10261, 10278 |
| C kabuk | 4 | 0x2188000 | 3 | 10435, 10361, 10288 |

Dalga 14 tanı aracının kendi başına derlediği başlangıç nesnesi de olağan Make tarifini kullanıyor. Yalıtılmış kopyada değişiklik uygulanmadan yeniden üretilen nesne ana nesneyle bayt bayt aynıydı.

Ölçümlü tam takım 23:06, 173 kapı, en uzun kapı 8:27, bütçe aşımı yok. Geçici dizin 0 → 0. Ham takım 24:45. QEMU’suz PATH ile iki SMP hedefinde de hazırlık derlemesi yapılmadan atlama bildirildi.

## Bölüm 11 — çekirdek başı tablolar, GS ve LAPIC tıkları

BSP ve üç AP'de QMP ile okunan IDT tabanları eşit, dört GS, dört GDT ve dört TSS tabanı ayrıydı. Üç AP'nin GS kayıtları 0x142000, 0x153000 ve 0x164000 adresindeydi. Normal yığınlar, 12 IST aralığı ve kanaryalar fiziksel bellekten doğrulandı. İlk yük ölçümünde C tık farkları 159/159/159/159, self-host farkları 150/149/149/149 oldu. Ayrı bir ve dört vCPU'lu kullanıcı programında yedi sistem çağrısı, sıfır kullanıcı GS tabanı ve dönüşten sonra doğru çekirdek GS tabanı ölçüldü. ARM ana makinede x86 MTTCG kullanıldı.

AP saati kurulmayınca QMP 159/0/0/0 ölçtü (rc=1). Ayrı deneyde BSP'nin TSS'i AP için aday verildi. Özel sayfa sınırı denetimi bunu TR yüklenmeden reddetti, ilk AP onayı zaman aşımına uğradı (rc=1). Paylaşılan TSS'i birden çok işlemcide etkinleştirip yığın bozulması üretme sabotajı uygulanmadı.

D-757 tek vCPU kasıtlı çift hata senaryosu her iki derleyicide onar açılışta geçti. Özel yığını doğru kurulmuş AP2'de ayrıca kasıtlı çift hata denendi. QEMU kendi iothread kilit denetiminde sonlandı (rc=-6). AP2 için bu koşumdan çift hata kanıtı alınamadı. Ayrı, yalıtılmış bir tanı imajında donanımın IST1 seçimi dört çekirdekte de kesme kapısıyla ölçüldü. Her işleyicinin gerçek RSP'si kendi IST1 tepesinden 40 bayt aşağıdaydı, dört kanarya korundu. Bu ölçüm çift hata sonucu olarak sayılmadı.

İlk bütünleşmede ABI tarayıcısı 40 numaralı dalı bulamadı. Sürüm çağrısı asıl dağıtıcıda tutuldu ve kullanıcı GS değişimi bu çağrıyı da kapsadı. Tekil gerçek PVH imajı beklenen seri çıktıyı verirken iki bağımsız `make calistir_kem_os_x86` aynı IR, disk ve seri dosyaları üzerinde çakıştı. Kanıt tarifinin tamamı süreçler arası kilide alındı. Aynı anda başlatılan iki ayrı make bundan sonra 27 ve 55 sn'de tamamlandı (ikisi de rc=0). Yeni x86 IDT nesnesi kırmızı bölge kullanmadan derleniyor. Objdump'ta GDTR için yer ayrıldığı ve `cekirdek_no` gövdesinin tek GS okuması ile dönüşten oluştuğu doğrulandı.

Son §7 koşumunda iki derleyicinin tık ve ring3 kayıtları aşağıdadır. Keşif, AP, tık, istisna/IST, süreç ABI, seçim ve paylaşım kapıları dahil make/tee rc=0.

```text
SMP tık 4: GS=['0x2058d80', '0x142000', '0x153000', '0x164000'], tık farkları=[151, 150, 150, 150], oran=[1.0, 0.993, 0.993, 0.993]
SMP tık 4: GS=['0x2060d80', '0x142000', '0x153000', '0x164000'], tık farkları=[159, 159, 159, 159], oran=[1.0, 1.0, 1.0, 1.0]
GS ring3 1: kullanıcı tabanı=0, sistem çağrısı=7, BSP tabanı=0x21a4d80
GS ring3 4: kullanıcı tabanı=0, sistem çağrısı=7, BSP tabanı=0x21a4d80
```

## Bölüm 12 — çekirdek bilet kilidi

### C18-001 — yerleşik adla aynı imzayı taşımayan çıplak işlev

Küçük tekrar üretimi aşağıdadır.

```kem
çıplak işlev kilit_al(k: tam64) { }
işlev main() -> tam32 { kilit_al(8 olarak tam64); ver 0; }
```

Bu iki satır `ad.kem` olarak kaydedildiğinde `build/kemgu --check ad.kem` ve `build/kemgu --llvm ad.kem` rc=0 verir. `build/codegen --check ad.kem` rc=0 ile `T001 2 37` yazdı, `build/codegen --llvm ad.kem` aynı tanıyla rc=1 verdi. Self-host'ta metin alan yerleşik `kilit_al` ve `kilit_birak` imzaları var. İki derleyiciyle aynı sonucu almak için çekirdekte `smp_kilit_al` ve `smp_kilit_birak` kullanıldı. Derleyici veya muafiyet listesi değişmedi.

İki derleyicide sekiz istemcili 100.000 geçişli model FIFO kuyruğunu bağımsız sıra ve sahip kaydıyla karşılaştırdı. Bekleyen tüm biletler yeni iş eklenmeden sırasıyla tamamlandı. Donanım kapısında boş/dolu `kilit_dene`, iki 32 bit sayacın sarması ve kesmeler açıkken iç içe IF saklama sınandı.

| Derleyici | vCPU | Ortak sayı (QMP) | Sunulan ve sonraki bilet | Ölçüm (sn) |
|---|---:|---:|---:|---:|
| C | 4 | 4.000.000 | 4.000.003 | 1,515 |
| C | 8 | 8.000.000 | 8.000.003 | 2,524 |
| Self | 4 | 4.000.000 | 4.000.003 | 1,514 |
| Self | 8 | 8.000.000 | 8.000.003 | 3,026 |

Her çekirdekten ilk 8.192 artış ortak turda geldi. Kalan artışlarda sıra komşu çiftlere verildi. Kâhin kişi başı bir milyonu ayrıca okudu. Bu düzen `-j10` altında bir bilet sahibinin host zamanlayıcısını beklemesi yüzünden sekiz vCPU'nun birlikte oyalanmasını sınırlar. Ayrı C deneyinde **bütün** tekrarlar boyunca 4/8 çekirdek birlikte yarıştı. Süreler 2,059 ve 378,842 sn, sonuçlar 4.000.000 ve 8.000.000. Sekiz çekirdekte çekişmeli edinimler sırasıyla 999945, 999837, 999958, 998473, 999965, 999945, 999962, 999966. QMP sekiz ayrı TCG iş parçacığını doğruladı. Gerçek x86 işlemcide aynı gecikme veya serpiştirme beklenmez.

Sekiz vCPU'nun yalnız sayı artırma deneyinde kilidi tamamen çıkarınca toplam 2.494.650 oldu (beklenen 8.000.000). Bilet almadan yalnız `lock` önekini çıkaran ayrı imajda biletler 56.409/56.409, toplam 56.412 durumunda en az 35 saniye ilerleme olmadı, kapı rc=1. Hata ayıklama imajında 20→10 edinme seri günlüğe `KILIT TERS SIRA` yazdı, kapı rc=1. Her üçü ayrı imaj ve çalışma dizininde gerçekten uygulandı.

Son iki aşamalı kapıda da atomik önek çıkarıldı. Sekiz vCPU'da biletler 36.780/36.780, artırılan sayı 36.780, bitiş bayrağı 0 iken en az 35 saniye ilerleme olmadı (kâhin rc=1).

PVH ring3 seri giriş kapısında kısa yankı ölçüldü. Aynı imajla 30 tekrarda normal TCG 3, tek iş parçacıklı TCG 2 ve talimat sayımlı kip 2 kez satırı eksik aldı. PVH platform açılışında COM1 alıcı FIFO'su etkinleştirildi. Host dokuz baytı FIFO kurulduktan sonraki ilk blok kanıtı üzerine gönderiyor. FIFO eşiği bir baytken 17. denemede yalnız “K” okundu. Eşik 14 bayt olunca aynı gerçek QEMU düzeninde 30/30 tam yankı, ring3 başarı satırı ve son işaret alındı (13 sn). QEMU 8.2.2 alıcı boşken karakter aygıtına eşiğe kadar boş yer bildirir. [QEMU seri aygıtı](https://github.com/qemu/qemu/blob/v8.2.2/hw/char/serial.c#L519-L537)

Yüklü bir S5 koşumunda PM1 isteği ile QEMU kapanışı arasında port-0x80 yoklama döngüsü sona erdi ve hata dönüşü 1 geldi. Kalibrasyonu olan konukta S5'in 250 ms sınırı PIT ile ölçülmüş TSC'ye bağlandı. İlk önbellek denemesinde iki derleme arasında kaynak değişmişti, ikinci derleme isabet sayılmadı. Çakışan iki ayrı -j10 takımında ayrıca açılış süre aşımı ve kabuk komut/piksel uyuşmazlıkları kaydedildi. Hatalı koşumlar başarı sayısına dahil edilmedi.

UEFI PC regresyonunun bir çalışmasında USB aktarım zaman aşımı geldi. Başka bir çalışmada yedi komut, 70 satır ve son GOP karşılaştırması doğruydu, üçüncü açılışın 60 saniyelik duvar sınırı aşıldı. Aynı eşiği ve ekran denetimlerini korumak için Python ekran modelinde bir karedeki aynı renkli glifleri yeniden kullandık. Tam eşit RGB karelerinde üç kanalı topluca karşılaştırıyoruz. Farklı karede eski yol tüm pikselleri sayıyor ve ilk farklı koordinatı bildiriyor. Eski ve yeni model 640×480, 641×497 ve 1280×800'de boş ekran, Türkçe/kaydırmalı metin ve uzun girdiyle dokuz kez her pikselde aynıydı. Her görüntünün 15 rastgele kanal baytı değiştirildiğinde fark sayısı ve ilk yanlış piksel de eşleşti. 1280×800 model üretimi aynı makinede 155–156 ms'den 2–5 ms'ye indi.

Saat etiketli son `-j10` koşumunda kilit kapısının tarifleri 22,612 sn sürdü. Daha yüklü ayrı `-j10` koşumunda sekiz vCPU sayma süreleri C 173,729 sn ve self-host 100,712 sn idi. O koşumun takım başlangıcı 07:53:00, bitmiş dört ölçümün gözlendiği saat 07:59:06. Tarif bazında saat kaydı bulunmayan bu koşum için kapı süresinin üst sınırı 366 sn. Bütçe bu üst sınırın 1,5 katı, 549 sn.

Saf modelde sıradaki bilet denetimi çıkarıldığında model kâhini C çalıştırmasında 4 verdi (sarmalayıcı rc=1). Yeniden kurulan hata ayıklama imajında ters edinim deneyi de panik ve rc=1 üretti.

## Bölüm 13 — IPI teslimleri

Gerçek KEMGU çağrı ve kesme yollarında, QMP'nin fiziksel bellekten okuduğu her sonuç için gönderici, hedef, sıra, pozitif çevrim farkı ve hedefteki toplam teslim sayısı doğrulandı. Her gönderici sırayla bütün hedeflere ulaştı. Göndericinin kendi hedefi de matristedir. İkinci çağrı işlevi hedef çekirdeğin GS üzerinden okuduğu yerel tıkı döndürdü. Kâhin bu değeri hedefin güncel fiziksel sayacıyla karşılaştırdı.

| Derleyici | APIC kimlikleri | Çift × yineleme | Kayıp | Gidiş-dönüş medyan / p99 (µs) |
|---|---|---:|---:|---:|
| C | 0,1,2,3 | 16 × 1000 | 0 | 8,223 / 21,737 |
| C | 0,1,2,4,5,6 | 36 × 1000 | 0 | 7,783 / 20,543 |
| Self | 0,1,2,3 | 16 × 1000 | 0 | 6,983 / 20,220 |
| Self | 0,1,2,4,5,6 | 36 × 1000 | 0 | 9,261 / 25,539 |

ARM ana makinedeki TCG iş parçacıklarının ayrı kimlikleri QMP ile ölçüldü. Kesme girişi, işlev, haberleşme ve göndericinin onay okuması bu sürelere dahildir. Dolu kutu, eski bilet, geçersiz hedef/işlem ve sıfır süreli bekleme gerçek konukta sınandı.

IPI koçanının EOI yazımı ayrı bir imajda kaldırıldı. Kendine ilk yeniden zamanlama teslim edildi, sıradaki aynı öncelik sınıfındaki çağrı teslim edilemedi. QMP'de gönderen 0, hata −4, kapı rc=1. Hedef APIC kimliği yerine mantıksal numara yazıldığında dört çekirdek **sessiz** kaldı (16.000/16.000, rc=0). İki soket × üç çekirdeğin gerçek APIC kimlikleri 0,1,2,4,5,6 olunca aynı imajda teslim süre aşımı −4, rc=1. Her iki değişiklik de özel derleme ağaçlarındaydı.

Duruş vektörü de son kapıya alındı. BSP diğer çekirdeklere 82 gönderdi. QMP her AP'de gerçek HLT=1, RFLAGS.IF=0 ve kutuda duruş onayını okudu. Bu sürümde iki derleyici × iki topoloji kapısı toplam 31,937 sn sürdü. 4/6 vCPU için C medyan/p99 değerleri 8,158/29,372 ve 9,372/32,824 µs, self-host değerleri 9,490/32,935 ve 8,992/31,871 µs idi.

## Bölüm 14 — çerçeve ve heap sahipliği

Dört x86 vCPU aynı fiziksel ayırıcıyı ve çekirdek malloc/free girişlerini kullandı. İlk turda her vCPU kendi çerçevesini ve heap bloğunu diğer üçü de hazır olana dek tuttu. QMP, 65.536 sahip sözcüğünün sonda temiz olduğunu, iki fiziksel bitmap görüntüsünün bayt bayt eşitliğini ve 80.000 olayı ayrı ayrı doğruladı. Host olaylardan sahipleri yeniden hesapladı. Canlı çerçeve zirvesi dört, çifte sahip sıfır. Çerçeve bilet farkı 80.001 (son sayım dahil), heap farkı 80.000. C'de boş çerçeve 47.751 → 47.751, self-host'ta 47.749 → 47.749. İlk kâhin ham bitmap'i bitişik bayt sandığı için yanıldı. KEMGU dizisinde her bayt dört baytlık yuvanın ilk baytıdır. Süreç ve SMP kâhinleri artık aynı çözücüyü kullanıyor.

| 1 vCPU, 50 gerçek süreç | D-772 boş | B14 boş | D-772 / B14 ELF sayfası | Süreç heap artışı |
|---|---:|---:|---:|---:|
| C | 45.416 | 45.465 | 7.716 / 7.667 | 0 |
| Self-host | 45.385 | 45.456 | 7.747 / 7.676 | 0 |

Her dört imajda boş çerçeve ve yüklü ELF sayfalarının toplamı 53.132. İstenen mutlak boş çerçeve eşitliği sağlanmadı. C'de 49, self-host'ta 71 fazla boş çerçeve var. Boş çerçeve sayısındaki fark imajların PT_LOAD kapsamındaki fark kadar. Tek çekirdekte C kabuğu D-772'nin üç koşumundaki 4.788.464 → 5.247.552 bayt çiftini üretti. D-772 self-host kayıtlarında 5.050.928 → 5.510.016 bir kez ve 5.116.512 → 5.575.600 iki kez görüldü. Her iki çift de bu bölümde yeniden ölçüldü. Kâhin derleyiciye göre bu gerçek taban çiftlerini denetliyor.

Canlı fiziksel ayırıcının kilidini kaldırma sabotajı **uygulanmadı**. Ayrı imajda dört QEMU vCPU'su aynı sayısal aday için yarıştı. Sahiplik kontrolünde durumlar [1, −2, −2, −2], kapı rc=1. Bu deney fiziksel çerçeve dağıtmadı. Ölçüm ARM ana makinede ayrı iş parçacıklı x86 TCG ile yapıldı.

Heap algoritması aynı kaldı. Birleşik SMP imajı malloc/free sarmallarını iki mimariyi aynı asm birimine sokmadan x86 bilet kilidine bağlıyor. Ortak ve ARM imajları aynı heap işlevine doğrudan giriyor. İlk toplu koşum sürerken süreç betiğine derleyici etiketini ekledim. Çalışan Bash eski konumdan okumayı sürdürdüğü için beş alt hedef son satırda sözdizimi hatasıyla çıktı. Betiğin tamamı sabitken bütün bölüm kapıları yeniden çalıştırıldı.

Sabit betikle yapılan 357 sn'lik toplu koşumda iki USB klavye hatası kaldı. Bellek tekrarında komut bulunamadı, v1 kesinti koşumunda dosya gezginini başlatan komut eksik ulaştı. Tık sayacı USB raporunun geldiğini garanti etmiyordu. Bu iki kâhinde her tuşa karşılık DMA tamponundaki sekiz baytın gerçekten değişmesi bekleniyor. Aynı iki hedefin yeni davranışla toplu tekrarı 250 sn, make ve tee rc=0. İki derleyicinin bellek kapısı ayrıca tek başına da geçti.
