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
