# Tur 20 — x86 ÇEKİRDEK W^X: .text salt-okunur, geri kalan NX (CR0.WP)

YALNIZ `runtime/mimari/x86_64/kem_mmu_hw.kem` SONUNA ekle: tek ```duzenle bloğu; ESKİ kısım
son işlev `kem_pvh_cmdline_esit`in TAMAMI (kesitte, birebir), YENİ kısım aynı işlev + altına yeni
kod. Başka dosyaya DOKUNMA. Yalnız `mimari: x86_64` asm; tüm işlevler `çıplak işlev`.
arm64 ikizi `kmmu_kernel_wx_testi` (kesitte). Mevcut adları KULLAN (yeniden tanımlama YOK):
`kxm_y64`, `kxm_o64`, `kxm_cr3_oku`, `kxm_cr3_yaz`, `kxm_cerceve`, `kxm_nx_ac`, `KXM_NX_BIT`,
`kxm_bekle_yaz`, `kxm_bekle_oku`, `kxm_cr2_yaz`, `kxm_cr2_oku`, `kxm_hata_yaz`, `kxm_hata_oku`.

## Sabitler (araçla doğrulandı, aynen)
```
sabit KXM_PT_K: tam64 = 58728448;      // 0x3802000: çekirdeğin 0..2 MB'ı için 4KB PT
sabit KXM_TEXT_BAS: tam64 = 1048576;   // 0x100000: imaj başı (.note.Xen + .text)
sabit KXM_CR0_WP: tam64 = 65536;       // CR0 bit 16
sabit KXM_PF_YAZ_HATA: tam64 = 3;      // P(1) | W(2): salt-okunur sayfaya çekirdek yazması
```

## İşlevler
1. `kxm_rodata_son() -> tam64` — asm `leaq __rodata_end(%rip), $0`, çıktı("=r", &x).
2. `kxm_kod_adr() -> tam64` — asm `leaq pvh_entry(%rip), $0`, çıktı("=r", &x) (.text içinde).
3. `kxm_wp_ac()` — asm (satırlar ayrı): `movq %cr0, %rax` / `orq $$65536, %rax` /
   `movq %rax, %cr0`; bozulan("~{rax}") bozulan("~{memory}").
4. `kxm_yaz_faulting(adr: tam64, v: tam64)` — SABİT YAZMAÇLI tek komut: şablon tam olarak
   `movq %rax, (%rdi)`, girdi("{di}", adr), girdi("{ax}", v), bozulan("~{memory}").
   (Kodlama 48 89 07 = 3 bayt; `.S` RIP'i bekle kadar ilerletir.)
5. `kmmu_kernel_wx_kur()` — çekirdek 0..2 MB'ı 2MB sayfadan 4KB PT'ye böl:
   - `kxm_nx_ac();` (ÖNCE: NXE kapalıyken bit 63 ayrılmıştır → her erişim fault olur)
   - `değişken ro_son: tam64 = kxm_cerceve(kxm_rodata_son());` (aşağı hizala: veri ile ortak
     sayfa YAZILABİLİR kalmalı)
   - her s < 512: `değişken pa: tam64 = s * 4096;`
     `eğer (pa >= KXM_TEXT_BAS) ve (pa < ro_son) { kxm_y64(KXM_PT_K + s * 8, pa + 1); }`
     `değilse { kxm_y64(KXM_PT_K + s * 8, KXM_NX_BIT + pa + 3); }`
     (+1 = P, salt-okunur ve çalıştırılabilir; diğerleri P|RW + NX)
   - `değişken pml4: tam64 = kxm_cerceve(kxm_cr3_oku());`
     `değişken pdpt: tam64 = kxm_cerceve(kxm_o64(pml4));`
     `değişken pd: tam64 = kxm_cerceve(kxm_o64(pdpt));`   // PDPT[0] → boot PD
     `kxm_y64(pd, KXM_PT_K + 3);`                          // PD[0] → 4KB PT
   - `kxm_wp_ac();` sonra `kxm_cr3_yaz(kxm_cr3_oku());` (TLB temizle)
6. `kmmu_kernel_wx_testi() -> tam32` — arm gövdesinin x86 karşılığı:
   - `hedef = kxm_kod_adr(); once = kxm_o64(hedef);`
   - `kxm_cr2_yaz(0); kxm_hata_yaz(0); kxm_bekle_yaz(3);`
   - `kxm_yaz_faulting(hedef, 3735928559);`
   - `bekle = kxm_bekle_oku(); cr2 = kxm_cr2_oku(); hk = kxm_hata_oku(); sonra = kxm_o64(hedef);`
   - `eğer (bekle == 0) ve (cr2 == hedef) ve (hk == KXM_PF_YAZ_HATA) ve (sonra == once) { ver 1; }`
     `ver 0;`  (DİKKAT: `hata` anahtar kelimedir, değişken adı olamaz → `hk`)
Bloğun başına `// [D-691] çekirdek W^X` başlık yorumu (gerekçe: .text yazılabilir olsaydı yazma
sessizce geçerdi; WP olmadan çekirdek salt-okunur sayfaya da yazabilir), işlev başına kısa yorum.
