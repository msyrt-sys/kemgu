# Tur 16 — x86 MMU çeviri + kurtarılabilir #PF (.kem)

Tek bir YENİ dosya yaz: `runtime/mimari/x86_64/kem_mmu_hw.kem`. Başka dosyaya DOKUNMA.
Biçim: ```yeni:runtime/mimari/x86_64/kem_mmu_hw.kem bloğu.

Bağlamdaki `runtime/kem_mmu.kem` (arm64) ikizidir: aynı TEST ADLARI (`kmmu_ceviri_testi`,
`kmmu_fault_testi`) ve aynı test mantığı, x86 donanımıyla. Dosyada YALNIZ `mimari: x86_64`
asm olabilir (arm64 asm YASAK — AS001). Tüm işlevler `çıplak işlev`. Dizgi literalinde kaçış
yoktur. Sayılar ondalık yazılır (aşağıdakiler araçla doğrulandı, AYNEN kullan).

## Sabitler (aynen)
```
sabit KXM_PD: tam64 = 58720256;          // 0x3800000 (56 MB) yeni PD (PDPT[1] için, 1..2 GB)
sabit KXM_PT: tam64 = 58724352;          // 0x3801000 4KB-sayfa tablosu
sabit KMMU_CEVIRI_VA: tam64 = 1342177280; // 0x50000000 (PD indeksi 128, PT indeksi 0)
sabit KMMU_CEVIRI_PA: tam64 = 60817408;   // 0x3A00000 (58 MB) identity-eşli boş RAM
sabit KMMU_FAULT_VA: tam64 = 1610612736;  // 0x60000000 (PD[256] boş → #PF)
```

## İşlevler
1. `kxm_y64(adr: tam64, deger: tam64)` ve `kxm_o64(adr: tam64) -> tam64`: `*tam64` ham
   işaretçi ile yaz/oku (arm `kmmu_y64`/`kmmu_o64` ile birebir aynı gövde).
2. `kxm_cr3_oku() -> tam64`: asm `movq %cr3, $0`, çıktı("=r", &x).
3. `kxm_cr3_yaz(v: tam64)`: asm `movq $0, %cr3`, girdi("r", v), bozulan("~{memory}").
   (CR3 yeniden yazmak TLB'yi temizler.)
4. `kxm_cerceve(g: tam64) -> tam64`: tablo girdisinden çerçeve adresi = `g - (g % 4096)`.
5. `kdl_mmu_kur_x86()`:
   - KXM_PD'den başlayan 512 girdiyi ve KXM_PT'den başlayan 512 girdiyi 0 yap
     (`değişken i: tam64 = 0; iken i < 512 { ...; i = i + 1; }`, adres = taban + i * 8).
   - `kxm_y64(KXM_PT, KMMU_CEVIRI_PA + 3);`            // PT[0] = PA | P|RW
   - `kxm_y64(KXM_PD + 128 * 8, KXM_PT + 3);`          // PD[128] = PT | P|RW
   - `değişken pml4: tam64 = kxm_cerceve(kxm_cr3_oku());`
   - `değişken pdpt: tam64 = kxm_cerceve(kxm_o64(pml4));`  // PML4[0] → PDPT
   - `kxm_y64(pdpt + 8, KXM_PD + 3);`                   // PDPT[1] = PD | P|RW
   - `kxm_cr3_yaz(kxm_cr3_oku());`                      // TLB temizle
6. `kmmu_ceviri_testi() -> tam32`: arm sürümünün gövdesi BİREBİR
   (0xDEADBEEF = 3735928559 PA'ya yaz → VA'dan oku; 0xCAFEBABE = 3405691582 VA'ya yaz →
   PA'dan oku; ikisi tutarsa 1, yoksa 0). `kxm_y64`/`kxm_o64` kullan.
7. Kurtarma değişkenleri `.S`'de: `kdl_x86_pf_bekle` ve `kdl_x86_pf_cr2` (.data, 8 bayt).
   Erişim RIP-göreli: `kxm_bekle_yaz(v)` asm `leaq kdl_x86_pf_bekle(%rip), %r11` + yeni
   satır + `movq $0, (%r11)`, girdi("r", v), bozulan("~{r11}") bozulan("~{memory}").
   Aynı desenle `kxm_bekle_oku() -> tam64` (`movq (%r11), $0`, çıktı("=r", &x)),
   `kxm_cr2_yaz(v)`, `kxm_cr2_oku() -> tam64` (sembol kdl_x86_pf_cr2).
8. `kxm_oku_faulting(adr: tam64) -> tam64`: SABİT YAZMAÇLI tek komut — asm şablonu
   tam olarak `movq (%rdi), %rax`, girdi("{di}", adr), çıktı("={ax}", &x),
   bozulan("~{memory}"). Bu kodlama 3 bayttır (48 8B 07); `.S` RIP'i bu kadar ilerletir.
9. `kmmu_fault_testi() -> tam32`:
   - `kxm_cr2_yaz(0); kxm_bekle_yaz(3);`            // 3 = atlanacak komut uzunluğu
   - `değişken cop: tam64 = kxm_oku_faulting(KMMU_FAULT_VA);`
   - `değişken bekle: tam64 = kxm_bekle_oku(); değişken cr2: tam64 = kxm_cr2_oku();`
   - `eğer bekle == 0 ve cr2 == KMMU_FAULT_VA { ver 1; } ver 0;`

Her işlevin üstüne bir satırlık Türkçe yorum yaz; dosya başına kısa bir açıklama
(arm ikizi, [D-687], RAM yerleşimi: heap ~17 MB, zamanlayıcı 48–50 MB, MMU 56–58 MB).
