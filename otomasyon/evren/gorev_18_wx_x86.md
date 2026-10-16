# Tur 18 — x86 W^X: NX biti + komut getirme hatası (.kem)

YALNIZ `runtime/mimari/x86_64/kem_mmu_hw.kem` dosyasının SONUNA ekle: tek bir
```duzenle:runtime/mimari/x86_64/kem_mmu_hw.kem bloğu; ESKİ kısım dosyanın son işlevi
`kmmu_izolasyon_testi`nin TAMAMI (kesitte verildi, birebir), YENİ kısım aynı işlev + altına
yeni kod. Başka dosyaya DOKUNMA. Yalnız `mimari: x86_64` asm; tüm işlevler `çıplak işlev`.
Mevcut adları KULLAN, yeniden tanımlama: `kxm_y64`, `kxm_o64`, `kxm_cr3_oku`, `kmmu_as_gec`,
`kmmu_as_olustur`, `kxm_bekle_yaz`, `kxm_bekle_oku`, `kxm_cr2_yaz`, `kxm_cr2_oku`,
`KMMU_AS_TABAN`, `KMMU_AS_ADIM`, `KMMU_OZEL_VA`, `KMMU_OZEL_PA`, `KMMU_2MB`, `KMMU_SAYFA`.
Kesitteki `kxm_cr2_oku` erişim desenini (leaq + %r11) örnek al.

## Arka plan (.S zaten hazır)
`.S` #PF kurtarma yolu artık hata kodunu `kdl_x86_pf_hata` (.data, 8 bayt) sembolüne yazar.
`kdl_x86_pf_bekle == 65535` ise "ret taklidi" yapar (komut getirme hatasında `call` edilen
hedeften çağırana döner).

## Sabitler (araçla doğrulandı, aynen)
```
sabit KXM_NX_BIT: tam64 = 0 - 9223372036854775807 - 1;  // bit 63 (= INT64_MIN), sarma tanımlı
sabit KXM_RET_TAKLIDI: tam64 = 65535;
sabit KXM_PF_NX_HATA: tam64 = 17;                       // P(1) | I/D(16): NX komut getirme
```

## İşlevler
1. `kxm_hata_yaz(v: tam64)` / `kxm_hata_oku() -> tam64` — `kdl_x86_pf_hata` için,
   `kxm_cr2_yaz`/`kxm_cr2_oku` ile birebir aynı desen (sembol adı değişir).
2. `kxm_nx_ac()` — EFER.NXE (bit 11) aç. Tek asm bloğu, şablon (satırlar ayrı):
   `movl $$3221225600, %ecx` / `rdmsr` / `orl $$2048, %eax` / `wrmsr`
   bozulan("~{eax}") bozulan("~{ecx}") bozulan("~{edx}") bozulan("~{memory}").
   (`$$` = asm içinde düz `$`.)
3. `kxm_cagir(va: tam64)` — asm `call *$0`, girdi("r", va), bozulan: ~{rax} ~{rcx} ~{rdx}
   ~{rsi} ~{rdi} ~{r8} ~{r9} ~{r10} ~{r11} ~{memory}. (Hedef yalnız `ret` içerir.)
4. `kmmu_wx_testi() -> tam32`:
   - `kxm_nx_ac();`
   - `değişken c: tam64 = kmmu_as_olustur(2); eğer c == 0 { ver 0; }`
   - `değişken pt: tam64 = KMMU_AS_TABAN + 2 * KMMU_AS_ADIM + 12288;`
   - `değişken pa: tam64 = KMMU_OZEL_PA + 2 * KMMU_2MB;`
   - iki sayfaya `ret` (195) yaz (çekirdeğin identity görüşünden):
     `kxm_y64(pa, 195); kxm_y64(pa + KMMU_SAYFA, 195);`
   - sayfa 0 = VERİ (NX): `kxm_y64(pt, KXM_NX_BIT + pa + 3);`
     sayfa 1 = KOD: `kxm_y64(pt + 8, pa + KMMU_SAYFA + 3);`
   - `değişken k: tam64 = kxm_cr3_oku(); kmmu_as_gec(c);`
   - `kxm_cagir(KMMU_OZEL_VA + KMMU_SAYFA);`   // KOD sayfası çalışmalı (fault yok)
   - `kxm_cr2_yaz(0); kxm_hata_yaz(0); kxm_bekle_yaz(KXM_RET_TAKLIDI);`
   - `kxm_cagir(KMMU_OZEL_VA);`                // VERİ sayfası → #PF (komut getirme)
   - `değişken bekle: tam64 = kxm_bekle_oku(); değişken cr2: tam64 = kxm_cr2_oku();`
     `değişken hata: tam64 = kxm_hata_oku();`
   - `kmmu_as_gec(k);`
   - `eğer (bekle == 0) ve (cr2 == KMMU_OZEL_VA) ve (hata == KXM_PF_NX_HATA) { ver 1; } ver 0;`
Her işlev üstüne kısa Türkçe yorum; bloğun başına `// [D-689] W^X: NX` başlık yorumu
(gerekçe: yazılabilir veri sayfası çalıştırılamaz; arm64 ikizi kmmu_kernel_wx_testi).
