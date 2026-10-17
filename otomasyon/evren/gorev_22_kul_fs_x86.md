# Tur 22 — x86 kullanıcı kipinden dosya sistemi sistem çağrısı (arm64 [13] ikizi)

Mimariden bağımsız servisler HAZIR (runtime/kem_gorev_cekirdek.kem): `kem_syscall_kul_mi(num)`,
`kem_syscall_kul(num, arg, arg2)` (5 = yaz(str), 17 = dosya_yaz(ad, içerik), 18 = dosya_oku(ad, tampon))
ve D-610 işaretçi denetleyicileri. Bunlar şu MİMARİ KANCALARI ister (x86'da HENÜZ YOK):
`kg_kul_min()`, `kg_kul_max()`, `kg_kul_boy()` (-> tam64) ve `kis_buf_oku8`, `kis_buf_yaz8`.
Yalnız aşağıdaki 3 dosyaya ```duzenle blokları yaz; başka dosyaya DOKUNMA. Tüm işlevler
`çıplak işlev`; yalnız `mimari: x86_64` asm (bu görevde asm GEREKMİYOR). Sabitler araçla doğrulandı.

## 1) runtime/mimari/x86_64/kem_gorev_hw.kem
ESKİ: `    eğer kem_syscall_temel_mi(num) { ver kem_syscall_temel(num, arg); }`
YENİ: aynı satır + altına
`    eğer kem_syscall_kul_mi(num) { ver kem_syscall_kul(num, arg, arg2); }   // [D-705]`
İkinci blok: ESKİ = dosyanın son işlevi değil; `çıplak işlev kdl_syscall_isle(` satırının
HEMEN ÜSTÜNE eklemek için ESKİ olarak tam `çıplak işlev kdl_syscall_isle(num: tam64, arg: tam64, arg2: tam64) -> tam64 {`
satırını kullan, YENİ = aşağıdakiler + o satır:
```
// [D-705] kullanıcı penceresi (D-610 işaretçi doğrulaması): x86 kem_os ring3 süreç-özel
// 2 MiB pencere [0x54000000, 0x54200000) (kmmu_as_olustur PD[160]; kod sayfa 0, yığın 511).
sabit KG_USER_MIN: tam64 = 1409286144;   // 0x54000000
sabit KG_USER_MAX: tam64 = 1411383296;   // 0x54200000
sabit KG_USER_BOY: tam64 = 2097152;      // 2 MiB
çıplak işlev kg_kul_min() -> tam64 { ver KG_USER_MIN; }
çıplak işlev kg_kul_max() -> tam64 { ver KG_USER_MAX; }
çıplak işlev kg_kul_boy() -> tam64 { ver KG_USER_BOY; }
```

## 2) runtime/mimari/x86_64/kem_mmu_hw.kem
ESKİ: `çıplak işlev kis_satir() { kis_bayt(10); }` — YENİ: aynı satır + altına arm64 kem_mmu.kem'deki
iki satır BİREBİR (kesitte): `kis_buf_yaz8` ve `kis_buf_oku8` (başına `// [D-705] arm64 ikizi` yorumu).

## 3) runtime/mimari/x86_64/kem_ring3_hw.kem — dosyanın SONUNA yeni test
ESKİ: dosyanın son 5 satırı (kesitte, `kxr_ring3_testi`'nin sonu), YENİ: aynısı + altına:
```
sabit KXR_FS_AS: tam64 = 4;              // adres alanı #4 (0/1 izolasyon, 2 W^X, 3 ring3)
sabit KXR_FS_HLT: tam64 = 58;            // kullanıcı programında hlt ofseti
sabit KXR_FS_AD_OFS: tam64 = 256;        // "test" (0x54000100)
sabit KXR_FS_ICERIK_OFS: tam64 = 272;    // "KEMGU-X86" (0x54000110)
sabit KXR_FS_BUF_OFS: tam64 = 2093056;   // yığın sayfası 511 başı (0x541FF000) = okuma tamponu
```
`çıplak işlev kxr_fs_testi() -> tam32` — `kxr_ring3_testi`'nin ilk 11 satırının aynısıyla adres alanı
#KXR_FS_AS'i kur (PML4[0]/PDPT[1]/PD[160] U/S, sayfa 0 KXR_US_RX, sayfa 511 NX+KXR_US_RW; `pa` =
KMMU_OZEL_PA + KXR_FS_AS * KMMU_2MB). Sonra kullanıcı programını `kxr_y8(pa + i, …)` ile yaz — 59 bayt,
llvm-mc ile derlendi, BİREBİR bu sırayla (i = 0..58):
184,17,0,0,0,191,0,1,0,84,186,16,1,0,84,205,128,184,18,0,0,0,191,0,1,0,84,186,0,240,31,84,205,128,184,5,0,0,0,191,0,240,31,84,205,128,184,5,0,0,0,191,0,0,16,0,205,128,244
(Anlamı: 17 dosya_yaz("test","KEMGU-X86") · 18 dosya_oku("test", 0x541FF000) · 5 yaz(0x541FF000) ·
5 yaz(0x100000 = ÇEKİRDEK adresi → D-610 reddi, rax = -1) · hlt → #GP.)
Bir `Dizi` ya da döngü KULLANMA; 59 ayrı `kxr_y8` çağrısı yaz (mevcut test de böyle).
Dizgiler: pa+256'dan "test" baytları 116,101,115,116 + 0; pa+272'den "KEMGU-X86" baytları
75,69,77,71,85,45,88,56,54 + 0. Tamponun ilk 16 baytını önce 0 yap (pa + KXR_FS_BUF_OFS + j).
`kxr_tss_kur();` · `b = kxr_blok_adr()` · `k = kxm_cr3_oku()` · `kmmu_as_gec(as)` ·
`kxr_gir(KMMU_OZEL_VA, KXR_YIGIN_UST)` · sonra oku: `bk = kxm_o64(b)`, `v = kxm_o64(b + 16)`,
`r = kxm_o64(b + 32)`, `ax = kxm_o64(b + 48)` · `kmmu_as_gec(k)`.
Doğrula (hepsi `ve`): bk == 0, v == KXR_GP, r == KMMU_OZEL_VA + KXR_FS_HLT, ax == 0 - 1, ve tamponun
(çekirdek identity görüşü: pa + KXR_FS_BUF_OFS + j, bayt okuması için `kis_buf_oku8(adres, 0)`) 0..8
baytları sırasıyla 75,69,77,71,85,45,88,56,54 ve 9. bayt 0. Hepsi tutarsa `ver 1`, yoksa `ver 0`.
Başa `// [D-705]` başlık yorumu: ne kanıtlandığı (gerçek virtio diske kullanıcı kipinden yaz/oku,
D-610 çekirdek işaretçisi reddi) + arm64 ikizi (kem_os.kem [13] FS SYSCALL).
