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
