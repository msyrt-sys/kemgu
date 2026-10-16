# Tur 17 — x86 süreç başına adres alanı (CR3 değişimi) + izolasyon testi

YALNIZ `runtime/mimari/x86_64/kem_mmu_hw.kem` dosyasının SONUNA ekleme yap: tek bir
```duzenle:runtime/mimari/x86_64/kem_mmu_hw.kem bloğu; ESKİ kısım dosyanın son satırı olan
`kmmu_fault_testi` işlevinin TAMAMI (kesitte verildi, birebir kopyala), YENİ kısım aynı işlev
+ altına yeni kod. Başka dosyaya DOKUNMA. Yalnız `mimari: x86_64` asm; tüm işlevler
`çıplak işlev`. Bağlamdaki arm64 `kmmu_as_olustur` / `kmmu_izolasyon_testi` ikizdir.
Dosyadaki mevcut `kxm_y64`, `kxm_o64`, `kxm_cr3_oku`, `kxm_cr3_yaz`, `kxm_cerceve`,
`KXM_PD` adlarını KULLAN (yeniden tanımlama YOK).

## Sabitler (araçla doğrulandı, aynen)
```
sabit KMMU_AS_TABAN: tam64 = 59768832;   // 0x3900000 (57 MB) adres-alanı tablo arenası
sabit KMMU_AS_ADIM: tam64 = 16384;       // her alan = PML4 + PDPT + PD + PT (4×4KB)
sabit KMMU_AS_AZAMI: tam64 = 6;
sabit KMMU_OZEL_VA: tam64 = 1409286144;  // 0x54000000 süreç-özel VA (PD indeksi 160)
sabit KMMU_OZEL_IDX: tam64 = 160;
sabit KMMU_OZEL_PA: tam64 = 62914560;    // 0x3C00000 (60 MB); alan i → +i*2MB (tavan 72 MB)
sabit KMMU_2MB: tam64 = 2097152;
sabit KMMU_SAYFA: tam64 = 4096;
sabit KMMU_SAYFA_SAYI: tam64 = 512;
```

## İşlevler
1. `kmmu_as_olustur(i: tam64) -> tam64` — alan #i'nin PML4 FİZİKSEL adresini döner (CR3
   değeri). `(i < 0) veya (i >= KMMU_AS_AZAMI)` ise 0 döner.
   - `pml4 = KMMU_AS_TABAN + i * KMMU_AS_ADIM; pdpt = pml4 + 4096; pd = pml4 + 8192;
     pt = pml4 + 12288;`
   - pml4'ün 512 girdisini 0 yap; sonra `kxm_y64(pml4, pdpt + 3);`
   - Çekirdek PDPT'si: `değişken kp: tam64 = kxm_cerceve(kxm_o64(kxm_cerceve(kxm_cr3_oku())));`
     (CR3 → PML4[0] → PDPT). pdpt = kp'nin 512 girdisinin TAM KOPYASI; sonra
     `kxm_y64(pdpt + 8, pd + 3);` (PDPT[1] → kendi PD'si).
   - pd = `KXM_PD`'nin 512 girdisinin TAM KOPYASI; sonra
     `kxm_y64(pd + KMMU_OZEL_IDX * 8, pt + 3);`
   - pt: `pa = KMMU_OZEL_PA + i * KMMU_2MB;` her s < 512 için
     `kxm_y64(pt + s * 8, (pa + s * KMMU_SAYFA) + 3);`  (P|RW)
   - `ver pml4;`
   Yorum: çekirdek eşlemesi KOPYALANMALI — CR3 yazan komutun bir sonraki komutu da
   haritalı olmalı (arm ikizindeki gerekçe). Tek fark özel VA'nın PT'si.
2. `kmmu_as_gec(cr3: tam64)` — `kxm_cr3_yaz(cr3);` (CR3 yazımı TLB'yi temizler; PCID yok).
3. `kmmu_izolasyon_testi() -> tam32` — arm gövdesinin x86 karşılığı:
   - `değişken k: tam64 = kxm_cr3_oku();` (çekirdek CR3, sonunda geri yüklenir)
   - a = kmmu_as_olustur(0), b = kmmu_as_olustur(1); biri 0 ise `ver 0`.
   - a'ya geç, KMMU_OZEL_VA'ya 2863311530 yaz; b'ye geç, 3149642683 yaz, bv oku;
     a'ya geç, av oku; `kmmu_as_gec(k);`
   - pa = kxm_o64(KMMU_OZEL_PA), pb = kxm_o64(KMMU_OZEL_PA + KMMU_2MB)
   - dördü de beklenen değerse 1, yoksa 0. (`ve` zinciri, arm'daki gibi.)
Her işlev üstüne Türkçe kısa yorum + bloğun başına `// [D-688]` başlık yorumu.
