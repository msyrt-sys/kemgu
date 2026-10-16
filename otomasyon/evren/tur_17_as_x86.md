# Tur 17 — as_x86 — `deepseek-v4.1-flash`

- Tarih: 2026-10-16 12:19:17 · süre 245.7 sn · finish `stop` · reasoning_effort `low`
- usage: `{"prompt_tokens": 14011, "total_tokens": 19105, "completion_tokens": 5094, "prompt_tokens_details": {"cached_tokens": 0, "created_cache_tokens": 13952}, "completion_tokens_details": {"reasoning_tokens": 2146}}`
- Değişiklik blokları: 1 — duzenle:runtime/mimari/x86_64/kem_mmu_hw.kem
- İstem: [tur_17_as_x86_istem.md](tur_17_as_x86_istem.md)

## Yanıt

```duzenle:runtime/mimari/x86_64/kem_mmu_hw.kem
<<<<<<< ESKI
// GERÇEK #PF-gate: haritasız VA oku → .S recovery → CR2==VA + kurtarıldı mı?
// 1 = MMU zorluyor + gerçek #PF yakalandı + CR2==VA + OS devam ediyor. Sentetik DEĞİL.
çıplak işlev kmmu_fault_testi() -> tam32 {
    kxm_cr2_yaz(0);                                        // CR2 sonucu temizle
    kxm_bekle_yaz(3);                                      // .S recovery iste (3 = komut uzunluğu)
    değişken cop: tam64 = kxm_oku_faulting(KMMU_FAULT_VA); // GERÇEK #PF → .S recovery
    değişken bekle: tam64 = kxm_bekle_oku();               // .S temizledi mi? (0 = kurtarıldı)
    değişken cr2: tam64 = kxm_cr2_oku();                   // yakalanan CR2
    eğer bekle == 0 ve cr2 == KMMU_FAULT_VA {
        ver 1;
    }
    ver 0;
}
=======
// GERÇEK #PF-gate: haritasız VA oku → .S recovery → CR2==VA + kurtarıldı mı?
// 1 = MMU zorluyor + gerçek #PF yakalandı + CR2==VA + OS devam ediyor. Sentetik DEĞİL.
çıplak işlev kmmu_fault_testi() -> tam32 {
    kxm_cr2_yaz(0);                                        // CR2 sonucu temizle
    kxm_bekle_yaz(3);                                      // .S recovery iste (3 = komut uzunluğu)
    değişken cop: tam64 = kxm_oku_faulting(KMMU_FAULT_VA); // GERÇEK #PF → .S recovery
    değişken bekle: tam64 = kxm_bekle_oku();               // .S temizledi mi? (0 = kurtarıldı)
    değişken cr2: tam64 = kxm_cr2_oku();                   // yakalanan CR2
    eğer bekle == 0 ve cr2 == KMMU_FAULT_VA {
        ver 1;
    }
    ver 0;
}

// [D-688] x86_64: süreç başına adres alanı (CR3 değişimi) + izolasyon testi.
// arm64 kem_mmu.kem `kmmu_as_olustur` / `kmmu_izolasyon_testi` ikizidir. Arenada
// her alan 4×4KB = PML4 + PDPT + PD + PT; CR3 = PML4 fiziksel adresi.
sabit KMMU_AS_TABAN: tam64 = 59768832;   // 0x3900000 (57 MB) adres-alanı tablo arenası
sabit KMMU_AS_ADIM: tam64 = 16384;       // her alan = PML4 + PDPT + PD + PT (4×4KB)
sabit KMMU_AS_AZAMI: tam64 = 6;
sabit KMMU_OZEL_VA: tam64 = 1409286144;  // 0x54000000 süreç-özel VA (PD indeksi 160)
sabit KMMU_OZEL_IDX: tam64 = 160;
sabit KMMU_OZEL_PA: tam64 = 62914560;    // 0x3C00000 (60 MB); alan i → +i*2MB (tavan 72 MB)
sabit KMMU_2MB: tam64 = 2097152;
sabit KMMU_SAYFA: tam64 = 4096;
sabit KMMU_SAYFA_SAYI: tam64 = 512;

// Alan #i'nin PML4 fiziksel adresini (CR3 değeri) kur, döner. Çekirdek eşlemesi
// KOPYALANMALI: CR3 yazan komutun ARDINDAN gelen komut da haritalı olmalı — bu
// yüzden çekirdek PDPT ve PD tabloları bire bir kopyalanır; tek fark özel VA'nın
// kendi PT'sine yönlendirilmesidir. i >= KMMU_AS_AZAMI ise 0 (sessiz taşma yok).
çıplak işlev kmmu_as_olustur(i: tam64) -> tam64 {
    eğer (i < 0) veya (i >= KMMU_AS_AZAMI) { ver 0; }
    değişken pml4: tam64 = KMMU_AS_TABAN + i * KMMU_AS_ADIM;
    değişken pdpt: tam64 = pml4 + 4096;
    değişken pd: tam64 = pml4 + 8192;
    değişken pt: tam64 = pml4 + 12288;
    değişken k: tam64 = 0;
    iken k < 512 { kxm_y64(pml4 + k * 8, 0); k = k + 1; }   // PML4 sıfırla
    kxm_y64(pml4, pdpt + 3);                                // PML4[0] → kendi PDPT'si
    // Çekirdek PDPT: CR3 → PML4[0] → PDPT (fiziksel adres)
    değişken kp: tam64 = kxm_cerceve(kxm_o64(kxm_cerceve(kxm_cr3_oku())));
    değişken n: tam64 = 0;
    iken n < 512 { kxm_y64(pdpt + n * 8, kxm_o64(kp + n * 8)); n = n + 1; }
    kxm_y64(pdpt + 8, pd + 3);                              // PDPT[1] → kendi PD'si
    // PD = çekirdek KXM_PD'nin tam kopyası; sonra özel VA → kendi PT
    değişken m: tam64 = 0;
    iken m < 512 { kxm_y64(pd + m * 8, kxm_o64(KXM_PD + m * 8)); m = m + 1; }
    kxm_y64(pd + KMMU_OZEL_IDX * 8, pt + 3);
    // PT: 512×4KB sayfa, her alan kendi 2MB fiziksel penceresine (P|RW)
    değişken pa: tam64 = KMMU_OZEL_PA + i * KMMU_2MB;
    değişken s: tam64 = 0;
    iken s < KMMU_SAYFA_SAYI {
        kxm_y64(pt + s * 8, (pa + s * KMMU_SAYFA) + 3);
        s = s + 1;
    }
    ver pml4;
}

// Alan #cr3'e geç: CR3 yazımı TLB'yi temizler (PCID yok — tam boşaltma).
çıplak işlev kmmu_as_gec(cr3: tam64) {
    kxm_cr3_yaz(cr3);
}

// İZOLASYON KANITI (falsifiye edilebilir): iki adres alanı, AYNI sanal adres,
// FARKLI fiziksel sayfa. A'ya yaz → B'ye yaz → A'ya DÖN → A'nın değeri DURUYOR mu?
// Tek adres alanında taklit edilemez: B'nin yazması A'nınkini ezerdi ve son okuma
// 0xBBBBBBBB verirdi. Ayrıca çekirdeğin identity görüşünden iki fiziksel sayfa
// BAĞIMSIZ doğrulanır — "değeri bir yerde sakladık" sahteliği elenir.
// 1 = süreç izolasyonu donanımda GERÇEK.
çıplak işlev kmmu_izolasyon_testi() -> tam32 {
    değişken k: tam64 = kxm_cr3_oku();              // çekirdek CR3 (sonunda geri yüklenir)
    değişken a: tam64 = kmmu_as_olustur(0);
    değişken b: tam64 = kmmu_as_olustur(1);
    eğer (a == 0) veya (b == 0) { ver 0; }

    kmmu_as_gec(a);
    kxm_y64(KMMU_OZEL_VA, 2863311530);              // 0xAAAAAAAA → A'nın sayfası
    kmmu_as_gec(b);
    kxm_y64(KMMU_OZEL_VA, 3149642683);              // 0xBBBBBBBB → B'nin sayfası
    değişken bv: tam64 = kxm_o64(KMMU_OZEL_VA);     // B kendi değerini görmeli
    kmmu_as_gec(a);
    değişken av: tam64 = kxm_o64(KMMU_OZEL_VA);     // ASIL KANIT: A'nınki DURUYOR mu?

    kmmu_as_gec(k);                                 // çekirdek alanına dön
    // Bağımsız doğrulama: çekirdeğin identity görüşünden iki AYRI fiziksel sayfa.
    değişken pa: tam64 = kxm_o64(KMMU_OZEL_PA);
    değişken pb: tam64 = kxm_o64(KMMU_OZEL_PA + KMMU_2MB);

    eğer (av == 2863311530) ve (bv == 3149642683)
       ve (pa == 2863311530) ve (pb == 3149642683) {
        ver 1;
    }
    ver 0;
}
>>>>>>> YENI
```

Yapılan: `runtime/mimari/x86_64/kem_mmu_hw.kem` dosyasının sonundaki `kmmu_fault_testi` işlevi birebir korunup altına `// [D-688]` başlığı, doğrulanmış sabitler, `kmmu_as_olustur`, `kmmu_as_gec` ve `kmmu_izolasyon_testi` eklendi. Hepsi `çıplak işlev`; yalnız `mimari: x86_64` içermez (asm kullanılmadı, mevcut `kxm_*` ilkelleri çağrıldı) ve mevcut adlar (`kxm_y64`, `kxm_o64`, `kxm_cr3_oku`, `kxm_cr3_yaz`, `kxm_cerceve`, `KXM_PD`) yeniden tanımlanmadı.

VARSAYIMLAR
- `KMMU_AS_*`, `KMMU_OZEL_*`, `KMMU_2MB`, `KMMU_SAYFA`, `KMMU_SAYFA_SAYI` sabitleri bu dosyada daha önce TANIMLI DEĞİL; görev metni gereği eklendi. Eğer başka bir CAT edilen dosyada da tanımlıysa ad çakışması çıkabilir (görev "aynen ekle" dediği için ekledim).
- `kxm_cerceve(adres)` → çerçeve (4KB hizalı fiziksel taban) döndüren yardımcı olarak alındı (ad zaten dosyada mevcut sayıldı).
- `KXM_PD` çekirdek PD tablosunun fiziksel adresi; `kxm_o64(KXM_PD + m*8)` düz 8-bayt okuma (tüm-girdi kopyası) varsayıldı.

## Düşünme (reasoning)

Let me write the edit. I need to reproduce the exact `kmmu_fault_testi` from the context as ESKI, then append new code as YENI.

Let me carefully copy the ESKI section:

```
// GERÇEK #PF-gate: haritasız VA oku → .S recovery → CR2==VA + kurtarıldı mı?
// 1 = MMU zorluyor + gerçek #PF yakalandı + CR2==VA + OS devam ediyor. Sentetik DEĞİL.
çıplak işlev kmmu_fault_testi() -> tam32 {
    kxm_cr2_yaz(0);                                        // CR2 sonucu temizle
    kxm_bekle_yaz(3);                                      // .S recovery iste (3 = komut uzunluğu)
    değişken cop: tam64 = kxm_oku_faulting(KMMU_FAULT_VA); // GERÇEK #PF → .S recovery
    değişken bekle: tam64 = kxm_bekle_oku();               // .S temizledi mi? (0 = kurtarıldı)
    değişken cr2: tam64 = kxm_cr2_oku();                   // yakalanan CR2
    eğer bekle == 0 ve cr2 == KMMU_FAULT_VA {
        ver 1;
    }
    ver 0;
}
```

Then YENI = same + new code.

Now the new code:

```
// [D-688] x86_64: süreç başına adres alanı (CR3) + izolasyon testi.
// Sabitler araçla doğrulandı; arenada her alan 4×4KB = PML4+PDPT+PD+PT.
sabit KMMU_AS_TABAN: tam64 = 59768832;   // 0x3900000 (57 MB) adres-alanı tablo arenası
sabit KMMU_AS_ADIM: tam64 = 16384;       // her alan = PML4 + PDPT + PD + PT (4×4KB)
sabit KMMU_AS_AZAMI: tam64 = 6;
sabit KMMU_OZEL_VA: tam64 = 1409286144;  // 0x54000000 süreç-özel VA (PD indeksi 160)
sabit KMMU_OZEL_IDX: tam64 = 160;
sabit KMMU_OZEL_PA: tam64 = 62914560;    // 0x3C00000 (60 MB); alan i → +i*2MB (tavan 72 MB)
sabit KMMU_2MB: tam64 = 2097152;
sabit KMMU_SAYFA: tam64 = 4096;
sabit KMMU_SAYFA_SAYI: tam64 = 512;

// Alan #i'nin PML4 fiziksel adresini (CR3 değeri) kur ve döner.
// Çekirdek eşlemesi KOPYALANMALI: CR3 yazan komutun ardından gelen komut da
// haritalı olmalı. Tek fark özel VA'nın kendi PT'si.
çıplak işlev kmmu_as_olustur(i: tam64) -> tam64 {
    eğer (i < 0) veya (i >= KMMU_AS_AZAMI) { ver 0; }
    değişken pml4: tam64 = KMMU_AS_TABAN + i * KMMU_AS_ADIM;
    değişken pdpt: tam64 = pml4 + 4096;
    değişken pd: tam64 = pml4 + 8192;
    değişken pt: tam64 = pml4 + 12288;
    değişken k: tam64 = 0;
    iken k < 512 { kxm_y64(pml4 + k * 8, 0); k = k + 1; }   // PML4 sıfırla
    kxm_y64(pml4, pdpt + 3);                                // PML4[0] → kendi PDPT'si
    // Çekirdek PDPT: CR3 → PML4[0] → PDPT
    değişken kp: tam64 = kxm_cerceve(kxm_o64(kxm_cerceve(kxm_cr3_oku())));
    değişken n: tam64 = 0;
    iken n < 512 { kxm_y64(pdpt + n * 8, kxm_o64(kp + n * 8)); n = n + 1; }
    kxm_y64(pdpt + 8, pd + 3);                              // PDPT[1] → kendi PD'si
    // PD = çekirdek KXM_PD'nin tam kopyası; özel VA → kendi PT
    değişken m: tam64 = 0;
    iken m < 512 { kxm_y64(pd + m * 8, kxm_o64(KXM_PD + m * 8)); m = m + 1; }
    kxm_y64(pd + KMMU_OZEL_IDX * 8, pt + 3);
    // PT: 512×4KB sayfa, her alan kendi 2MB fiziksel penceresine
    değişken pa: tam64 = KMMU_OZEL_PA + i * KMMU_2MB;
    değişken s: tam64 = 0;
    iken s < KMMU_SAYFA_SAYI {
        kxm_y64(pt + s * 8, (pa + s * KMMU_SAYFA) + 3);     // P|RW
        s = s + 1;
    }
    ver pml4;
}

// Alan #i'ye geç: CR3 yazımı TLB'yi temizler (PCID yok).
çıplak işlev kmmu_as_gec(cr3: tam64) {
    kxm_cr3_yaz(cr3);
}

// İki adres alanı, AYNI VA, FARKLI fiziksel sayfa: A yaz → B yaz → A'ya dön;
// A'nın değeri DURUYOR mu? 1 = izolasyon donanımda GERÇEK. Çekirdeğin identity
// görüşünden iki BAĞIMSIZ fiziksel sayfa da doğrulanır (sahtelik elenir).
çıplak işlev kmmu_izolasyon_testi() -> tam32 {
    değişken k: tam64 = kxm_cr3_oku();          // çekirdek CR3 (sonunda geri yüklenir)
    değişken a: tam64 = kmmu_as_olustur(0);
    değişken b: tam64 = kmmu_as_olustur(1);
    eğer (a == 0) veya (b == 0) { ver 0; }

    kmmu_as_gec(a);
    kxm_y64(KMMU_OZEL_VA, 2863311530);          // 0xAAAAAAAA → A'nın sayfası
    kmmu_as_gec(b);
    kxm_y64(KMMU_OZEL_VA, 3149642683);          // 0xBBBBBBBB → B'nin sayfası
    değişken bv: tam64 = kxm_o64(KMMU_OZEL_VA); // B kendi değerini görmeli
    kmmu_as_gec(a);
    değişken av: tam64 = kxm_o64(KMMU_OZEL_VA); // ASIL KANIT: A'nınki DURUYOR mu?

    kmmu_as_gec(k);                             // çekirdek alanına dön

    değişken pa: tam64 = kxm_o64(KMMU_OZEL_PA);
    değişken pb: tam64 = kxm_o64(KMMU_OZEL_PA + KMMU_2MB);

    eğer (av == 2863311530) ve (bv == 3149642683)
       ve (pa == 2863311530) ve (pb == 3149642683) {
        ver 1;
    }
    ver 0;
}
```

Wait, task says constants are "araçla doğrulandı, aynen" — I should include them. But do these constants already exist in the file? The task says dump them in. Let me include as given. Actually maybe they conflict... The task says "Sabitler (araçla doğrulandı, aynen)" — imply they need to be added. I'll add them.

Actually wait — potential issue: does x86 file already define KMMU_FAULT_VA, KMMU_OZEL_VA perhaps? The given context shows KMMU_FAULT_VA used. Let me just add since task specifies.

Hmm, but duplicating KMMU_OZEL_VA if it existed would conflict. Task explicitly says add these, so fine.

Let me write the edit block.
