# Tur 16 — mmu_x86 — `deepseek-v4.1-flash`

- Tarih: 2026-10-16 10:51:53 · süre 401.5 sn · finish `stop` · reasoning_effort `low`
- usage: `{"prompt_tokens": 15428, "total_tokens": 24666, "completion_tokens": 9238, "prompt_tokens_details": {"cached_tokens": 0, "created_cache_tokens": 15424}, "completion_tokens_details": {"reasoning_tokens": 5737}}`
- Değişiklik blokları: 1 — yeni:runtime/mimari/x86_64/kem_mmu_hw.kem
- İstem: [tur_16_mmu_x86_istem.md](tur_16_mmu_x86_istem.md)

## Yanıt

```yeni:runtime/mimari/x86_64/kem_mmu_hw.kem
// kem_mmu_hw.kem — x86_64 MMU çeviri + kurtarılabilir #PF donanım kancaları (D2b, [D-687]).
// ============================================================================
// arm64 ikizi: runtime/kem_mmu.kem (adrp/add + mrs ESR_EL1/FAR_EL1). Bu dosya AYNI
// TEST ADLARINI (kmmu_ceviri_testi, kmmu_fault_testi) ve AYNI test mantığını x86
// donanımıyla sağlar; arm64 asm YASAK (AS001) → yalnız `mimari: x86_64`.
//
// RAM yerleşimi (kem_os): heap ~17 MB; zamanlayıcı/korunmuş 48–50 MB (0x3000000+);
// MMU tabloları 56–58 MB — KXM_PD (0x3800000) yeni PD, KXM_PT (0x3801000) 4KB-sayfa
// tablosu, KMMU_CEVIRI_PA (0x3A00000) identity-eşli boş RAM. PML4[0]→PDPT zaten
// hazır (boot); bu dosya PDPT[1]'i kendi PD'mize bağlar → VA 1–2 GB aralığı
// (0x40000000–0x7FFFFFFF) bu PD üzerinden çevrilir; PD indeksi = (VA−0x40000000)/2MB.
// KMMU_CEVIRI_VA=0x50000000 → PD[128]; KMMU_FAULT_VA=0x60000000 → PD[256] (boş → #PF).

sabit KXM_PD: tam64 = 58720256;           // 0x3800000 (56 MB) yeni PD (PDPT[1] için, 1..2 GB)
sabit KXM_PT: tam64 = 58724352;           // 0x3801000 4KB-sayfa tablosu
sabit KMMU_CEVIRI_VA: tam64 = 1342177280; // 0x50000000 (PD indeksi 128, PT indeksi 0)
sabit KMMU_CEVIRI_PA: tam64 = 60817408;   // 0x3A00000 (58 MB) identity-eşli boş RAM
sabit KMMU_FAULT_VA: tam64 = 1610612736;  // 0x60000000 (PD[256] boş → #PF)

// Ham 64-bit yaz: adr → *tam64 (arm kmmu_y64 ikizi).
çıplak işlev kxm_y64(adr: tam64, deger: tam64) {
    değişken p: *tam64 = adr olarak *tam64;
    *p = deger;
}

// Ham 64-bit oku (arm kmmu_o64 ikizi).
çıplak işlev kxm_o64(adr: tam64) -> tam64 {
    değişken p: *tam64 = adr olarak *tam64;
    ver *p;
}

// CR3 oku (PML4 fiziksel tabanı) — `movq %cr3, $0`.
çıplak işlev kxm_cr3_oku() -> tam64 {
    değişken x: tam64 = 0;
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"movq %cr3, $0"#
            çıktı("=r", &x)
        }
    }
    ver x;
}

// CR3 yaz — `movq $0, %cr3`. Aynı değeri yeniden yazmak TLB'yi temizler.
çıplak işlev kxm_cr3_yaz(v: tam64) {
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"movq $0, %cr3"#
            girdi("r", v)
            bozulan("~{memory}")
        }
    }
}

// Tablo girdisinden çerçeve adresi: alt 12 bit (bayraklar) at → g - (g % 4096).
çıplak işlev kxm_cerceve(g: tam64) -> tam64 {
    ver g - (g % 4096);
}

// MMU kur (arm64 boot eşi): PD+PT temizle; PT[0]=PA|P|RW; PD[128]=PT|P|RW;
// PDPT[1]=PD|P|RW (mevcut PML4→PDPT üzerine); CR3 yeniden yaz (TLB flush).
çıplak işlev kdl_mmu_kur_x86() {
    değişken i: tam64 = 0;
    iken i < 512 { kxm_y64(KXM_PD + i * 8, 0); i = i + 1; }
    i = 0;
    iken i < 512 { kxm_y64(KXM_PT + i * 8, 0); i = i + 1; }
    kxm_y64(KXM_PT, KMMU_CEVIRI_PA + 3);               // PT[0] = PA | P|RW
    kxm_y64(KXM_PD + 128 * 8, KXM_PT + 3);             // PD[128] = PT | P|RW
    değişken pml4: tam64 = kxm_cerceve(kxm_cr3_oku());
    değişken pdpt: tam64 = kxm_cerceve(kxm_o64(pml4)); // PML4[0] → PDPT
    kxm_y64(pdpt + 8, KXM_PD + 3);                     // PDPT[1] = PD | P|RW
    kxm_cr3_yaz(kxm_cr3_oku());                        // TLB temizle
}

// 1 = non-identity çeviri donanımda ÇALIŞIYOR (iki farklı VA aynı PA'yı görüyor).
// Sentetik DEĞİL: identity olsaydı alias testleri tutmazdı.
çıplak işlev kmmu_ceviri_testi() -> tam32 {
    kxm_y64(KMMU_CEVIRI_PA, 3735928559);            // 0xDEADBEEF → identity VA (=PA)
    değişken a: tam64 = kxm_o64(KMMU_CEVIRI_VA);    // non-identity VA'dan oku → alias mı?
    kxm_y64(KMMU_CEVIRI_VA, 3405691582);            // 0xCAFEBABE → non-identity VA (ters yön)
    değişken b: tam64 = kxm_o64(KMMU_CEVIRI_PA);    // identity VA'dan oku → alias mı?
    eğer a == 3735928559 ve b == 3405691582 {
        ver 1;
    }
    ver 0;
}

// .S kurtarma-vari: kdl_x86_pf_bekle (atlanacak #PF komut uzunluğu) — RIP-göreli yaz.
çıplak işlev kxm_bekle_yaz(v: tam64) {
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"leaq kdl_x86_pf_bekle(%rip), %r11
movq $0, (%r11)"#
            girdi("r", v)
            bozulan("~{r11}") bozulan("~{memory}")
        }
    }
}

// kdl_x86_pf_bekle oku (kurtarma gerçekleştiyse 0'a düşer; .S temizler).
çıplak işlev kxm_bekle_oku() -> tam64 {
    değişken x: tam64 = 0;
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"leaq kdl_x86_pf_bekle(%rip), %r11
movq (%r11), $0"#
            çıktı("=r", &x)
            bozulan("~{r11}") bozulan("~{memory}")
        }
    }
    ver x;
}

// kdl_x86_pf_cr2 (yakalanan #PF CR2 değeri) — RIP-göreli yaz.
çıplak işlev kxm_cr2_yaz(v: tam64) {
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"leaq kdl_x86_pf_cr2(%rip), %r11
movq $0, (%r11)"#
            girdi("r", v)
            bozulan("~{r11}") bozulan("~{memory}")
        }
    }
}

// kdl_x86_pf_cr2 oku (kurtarma sonrası yakalanan CR2).
çıplak işlev kxm_cr2_oku() -> tam64 {
    değişken x: tam64 = 0;
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"leaq kdl_x86_pf_cr2(%rip), %r11
movq (%r11), $0"#
            çıktı("=r", &x)
            bozulan("~{r11}") bozulan("~{memory}")
        }
    }
    ver x;
}

// Haritasız VA'yı oku → GERÇEK #PF (TEK komut; .S RIP'i 3 bayt ilerletir; .S ELR/CS
// yerine RIP-göreli kurtarma). Dönen değer ÇÖP — çağıran atar. SABİT YAZMAÇ
// `movq (%rdi), %rax` = 48 8B 07 (tam 3 bayt; bu yüzden kxm_bekle_yaz(3)).
çıplak işlev kxm_oku_faulting(adr: tam64) -> tam64 {
    değişken x: tam64 = 0;
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"movq (%rdi), %rax"#
            girdi("{di}", adr)
            çıktı("={ax}", &x)
            bozulan("~{memory}")
        }
    }
    ver x;
}

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

Açıklama: `runtime/mimari/x86_64/kem_mmu_hw.kem` yeni dosyası, arm64 `runtime/kem_mmu.kem` ikizinin x86_64 karşılığı. Sabitler görevde verildiği gibi ondallık yazıldı; tüm işlevler `çıplak işlev`; dosyada yalnız `mimari: x86_64` asm var (AS001'e uygun). Test adları ve mantığı arm sürümüyle birebir (`kmmu_ceviri_testi`, `kmmu_fault_testi`). `kxm_oku_faulting` sabit yazmaçlı tek `movq (%rdi), %rax` (48 8B 07 = 3 bayt) şablonunu kullanır; `.S`'in RIP'i 3 bayt ilerletmesi bu sabitle uyumlu. Kurtarma değişkenleri `kdl_x86_pf_bekle` / `kdl_x86_pf_cr2` RIP-göreli erişilir.

VARSAYIMLAR
- `kxm_y64`/`kxm_o64`, `kxm_cerceve` ve ham işaretçi dönüşümleri bağlamdaki arm dosyasındaki `kmmu_y64`/`kmmu_o64`/çerçeve kalıbıyla aynı gövdeyi kullanır (o gövdeler kesitte gösterilmemişti; kalıp `*tam64 = adr olarak *tam64` benimsendi).
- `%` (mod) sabit bölenli (4096) olduğundan sıfıra-bölme kontrolünün ELENMESİ (D-502) beklenir; görev açıkça `g - (g % 4096)` istedi.
- `.S` tarafında `kdl_x86_pf_bekle`/`kdl_x86_pf_cr2` sembolleri ve #PF koçanının RIP'i kxm_bekle_yaz(3)'teki 3-baytlık adım kadar ilerletmesi bu turun kapsamı dışındadır (bu dosya yalnız `.kem` tarafını sağlar).
- `bozulan` listeleri: CR3 yazımı için yalnız `~{memory}`; RIP-göreli erişimler için `~{r11}` + `~{memory}`; `kxm_oku_faulting` için `~{memory}` (arm kalıbındaki yazım tutarlılığı).

## Düşünme (reasoning)

Let me write the file carefully following all the rules.

Key points:
- Only x86_64 asm
- All functions çıplak işlev
- No escapes in string literals
- Numbers decimal
- Constants as given
- Follow the pattern of arm64 twin

Let me examine the arm64 kmmu_y64/kmmu_o64 pattern - actually I don't see them in the excerpts but they should be simple pointer deref functions.

Let me write it.

For kxm_y64:
```
çıplak işlev kxm_y64(adr: tam64, deger: tam64) {
    değişken p: *tam64 = adr olarak *tam64;
    *p = deger;
}
```

For kxm_o64:
```
çıplak işlev kxm_o64(adr: tam64) -> tam64 {
    değişken p: *tam64 = adr olarak *tam64;
    ver *p;
}
```

For kxm_cr3_oku:
```
çıplak işlev kxm_cr3_oku() -> tam64 {
    değişken x: tam64 = 0;
    güvensiz { satıriçi_asm { mimari: x86_64 şablon: r#"movq %cr3, $0"# çıktı("=r", &x) } }
    ver x;
}
```

For kxm_cr3_yaz:
```
çıplak işlev kxm_cr3_yaz(v: tam64) {
    güvensiz { satıriçi_asm { mimari: x86_64 şablon: r#"movq $0, %cr3"# girdi("r", v) bozulan("~{memory}") } }
}
```

kxm_cerceve:
```
çıplak işlev kxm_cerceve(g: tam64) -> tam64 {
    ver g - (g % 4096);
}
```

kdl_mmu_kur_x86:
```
çıplak işlev kdl_mmu_kur_x86() {
    değişken i: tam64 = 0;
    iken i < 512 { kxm_y64(KXM_PD + i * 8, 0); i = i + 1; }
    değişken j: tam64 = 0;
    iken j < 512 { kxm_y64(KXM_PT + j * 8, 0); j = j + 1; }
    kxm_y64(KXM_PT, KMMU_CEVIRI_PA + 3);
    kxm_y64(KXM_PD + 128 * 8, KXM_PT + 3);
    değişken pml4: tam64 = kxm_cerceve(kxm_cr3_oku());
    değişken pdpt: tam64 = kxm_cerceve(kxm_o64(pml4));
    kxm_y64(pdpt + 8, KXM_PD + 3);
    kxm_cr3_yaz(kxm_cr3_oku());
}
```

Wait, the task says use `değişken i: tam64 = 0; iken i < 512 { ...; i = i + 1; }` - I'll reuse a variable name, but the spec seems to indicate two loops. Let me use `i` for the first and then reinit. Actually reusing `i` is fine in KEMGU (variables scoped). Let me check - the spec says both loops use `i`. Let me reinit.

Actually looking at the spec again: "KXM_PD'den başlayan 512 girdiyi ve KXM_PT'den başlayan 512 girdiyi 0 yap". I could write two loops. Reinitializing `i` should be fine.

Let me just use one variable and reset:
```
değişken i: tam64 = 0;
iken i < 512 { kxm_y64(KXM_PD + i * 8, 0); i = i + 1; }
i = 0;
iken i < 512 { kxm_y64(KXM_PT + i * 8, 0); i = i + 1; }
```

That's cleaner.

kmmu_ceviri_testi - same body as arm:
```
çıplak işlev kmmu_ceviri_testi() -> tam32 {
    kxm_y64(KMMU_CEVIRI_PA, 3735928559);
    değişken a: tam64 = kxm_o64(KMMU_CEVIRI_VA);
    kxm_y64(KMMU_CEVIRI_VA, 3405691582);
    değişken b: tam64 = kxm_o64(KMMU_CEVIRI_PA);
    eğer a == 3735928559 ve b == 3405691582 {
        ver 1;
    }
    ver 0;
}
```

kxm_bekle_yaz, kxm_bekle_oku, kxm_cr2_yaz, kxm_cr2_oku:

```
çıplak işlev kxm_bekle_yaz(v: tam64) {
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"leaq kdl_x86_pf_bekle(%rip), %r11
movq $0, (%r11)"#
            girdi("r", v)
            bozulan("~{r11}") bozulan("~{memory}")
        }
    }
}
```

Hmm, but the spec says the şablon contains a newline. In KEMGU there are no escapes so we write a literal newline in the raw string. Following the pattern in kmmu_bekle_yaz which uses actual newlines in the raw string.

Actually wait, looking at the arm example:
```
şablon: r#"adrp x9, kdl_fault_bekleniyor
add x9, x9, :lo12:kdl_fault_bekleniyor
str $0, [x9]"#
```

Yes, actual newlines inside raw strings. So I write actual newlines too.

```
çıplak işlev kxm_bekle_oku() -> tam64 {
    değişken x: tam64 = 0;
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"leaq kdl_x86_pf_bekle(%rip), %r11
movq (%r11), $0"#
            çıktı("=r", &x)
            bozulan("~{r11}") bozulan("~{memory}")
        }
    }
    ver x;
}
```

Same for cr2.

kxm_oku_faulting:
```
çıplak işlev kxm_oku_faulting(adr: tam64) -> tam64 {
    değişken x: tam64 = 0;
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"movq (%rdi), %rax"#
            girdi("{di}", adr)
            çıktı("={ax}", &x)
            bozulan("~{memory}")
        }
    }
    ver x;
}
```

kmmu_fault_testi:
```
çıplak işlev kmmu_fault_testi() -> tam32 {
    kxm_cr2_yaz(0);
    kxm_bekle_yaz(3);
    değişken cop: tam64 = kxm_oku_faulting(KMMU_FAULT_VA);
    değişken bekle: tam64 = kxm_bekle_oku();
    değişken cr2: tam64 = kxm_cr2_oku();
    eğer bekle == 0 ve cr2 == KMMU_FAULT_VA {
        ver 1;
    }
    ver 0;
}
```

Now file header comments. Constants:
- KXM_PD: 58720256 = 0x3800000 = 56 MB ✓
- KXM_PT: 58724352 = 0x3801000 = 56 MB + 4KB ✓
- KMMU_CEVIRI_VA: 1342177280 = 0x50000000 = 1280 MB (PD index 128, since PD covers 1GB per PML4/PDPT entry; 128 * 2MB = 256MB... hmm but PD[128] within the new PD at KXM_PD which starts at PDPT[1] = 1GB base. So VA = 1GB + 128 * 2MB = 0x40000000 + 0x10000000 = 0x50000000 = 1342177280 ✓)
- KMMU_CEVIRI_PA: 60817408 = 0x3A00000 = 58 MB ✓
- KMMU_FAULT_VA: 1610612736 = 0x60000000 (PD[256] within PDPT[1]'s PD... wait PDPT[1] maps 1-2GB, so VA 0x60000000 = 1.5GB → PD index 256. That PD index 256 is in the SAME PD that contains PD[128]? Wait no.

Hmm, actually PDPT[1] maps 1GB-2GB range. PD index within = (VA - 1GB) / 2MB. For VA = 0x50000000 = 1.25GB, PD index = 0.25GB / 2MB = 128. ✓
For VA = 0x60000000 = 1.5GB, PD index = 0.5GB / 2MB = 256. ✓

Good, both fall in the same PD at KXM_PD. So they share the PD. The PD is 512 entries, but we're using entries up to 256. Since we zeroed all 512, PD[256] will be 0 → not present → #PF. ✓

Comment:
"The heap is ~17MB, zamanlayıcı 48-50MB, MMU 56-58MB. This PD at 56MB covers PDPT[1] = 1-2GB VA space."

OK. Now I need to check the syntax for `ve` and `==`. From examples, yes: `eğer a == 3735928559 ve b == 3405691582`.

Also I should note: the spec says `mimari: x86_64`. And no arm64 asm.

For kxm_y64 with KMMU_CEVIRI_PA + 3 - the constant is a tam64 sabit, so this works.

Wait, careful with the % (modulo) - the spec says `g - (g % 4096)`. Since KEMGU has mod with divisor check (D-502 mentions division-by-zero @kdl_panik). But the spec explicitly says to use `g % 4096`, so 4096 is a constant, non-zero, fine.

Actually wait, does the mod operation need @kdl_panik in kem_os? Looking at the kanal code comment:
"`%` BİLEREK YOK: değişken bölenli `%` sıfıra-bölme kontrolü (D-502) → @kdl_panik ister"

This says "değişken bölen" (variable divisor). For a constant non-zero divisor, the compiler could elide the check. But we're told to use it, so let's use it. If there's a problem with kdl_panik not being available... but the spec explicitly says to write it. Hmm.

Actually re-reading: "değişken bölenli `%`" - variable divisor modulo. Constant 4096 might be fine. Let's follow the spec.

Let me write the full file now.

Also need proper "bozulan" for asm with memory writes. For kxm_y64 (no asm). For kxm_cr3_yaz, "~{memory}". For kxm_bekle_yaz and kxm_cr2_yaz: "~{r11}" and "~{memory}". For kxm_bekle_oku and kxm_cr2_oku: output is "=r", plus bozulan("~{r11}") bozulan("~{memory}").

Let me also double check the output constraint. Arm version uses `çıktı("=r", &x)`. I'll use the same.

Actually for kxm_oku_faulting, the spec says:
`çıktı("={ax}", &x)`
So use that exact constraint.

Full file:

```kemgu
// kem_mmu_hw.kem — x86_64 MMU çeviri + kurtarılabilir #PF donanım kancaları (D2b, [D-687]).
// ============================================================================
// arm64 ikizi: runtime/kem_mmu.kem (adrp/add + mrs ESR_EL1/FAR_EL1). Bu dosya aynı
// TEST ADLARINI (kmmu_ceviri_testi, kmmu_fault_testi) ve AYNI test mantığını x86
// donanımıyla sağlar; arm64 asm YASAK (AS001) → yalnız `mimari: x86_64`.
//
// RAM yerleşimi (kem_os): heap ~17 MB; zamanlayıcı/korunmuş 48–50 MB (0x3000000+);
// MMU tabloları 56–58 MB — KXM_PD (0x3800000) yeni PD, KXM_PT (0x3801000) 4KB-sayfa
// tablosu, KMMU_CEVIRI_PA (0x3A00000) identity-eşli boş RAM. PML4[0]→PDPT hazır
// (boot); bu dosya PDPT[1]'i kendi PD'mize bağlar: VA 1–2 GB (0x40000000–0x7FFFFFFF)
// → PD indeksi = (VA−0x40000000)/2MB. KMMU_CEVIRI_VA=0x50000000 → PD[128];
// KMMU_FAULT_VA=0x60000000 → PD[256] (boş → GERÇEK #PF).

sabit KXM_PD: tam64 = 58720256;           // 0x3800000 (56 MB) yeni PD (PDPT[1] için, 1..2 GB)
sabit KXM_PT: tam64 = 58724352;           // 0x3801000 4KB-sayfa tablosu
sabit KMMU_CEVIRI_VA: tam64 = 1342177280; // 0x50000000 (PD indeksi 128, PT indeksi 0)
sabit KMMU_CEVIRI_PA: tam64 = 60817408;   // 0x3A00000 (58 MB) identity-eşli boş RAM
sabit KMMU_FAULT_VA: tam64 = 1610612736;  // 0x60000000 (PD[256] boş → #PF)

// Ham 64-bit yaz (adr → *tam64). arm kmmu_y64 ikizi.
çıplak işlev kxm_y64(adr: tam64, deger: tam64) {
    değişken p: *tam64 = adr olarak *tam64;
    *p = deger;
}

// Ham 64-bit oku. arm kmmu_o64 ikizi.
çıplak işlev kxm_o64(adr: tam64) -> tam64 {
    değişken p: *tam64 = adr olarak *tam64;
    ver *p;
}

// CR3 oku (PML4 fiziksel tabanı) — `movq %cr3, $0`.
çıplak işlev kxm_cr3_oku() -> tam64 {
    değişken x: tam64 = 0;
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"movq %cr3, $0"#
            çıktı("=r", &x)
        }
    }
    ver x;
}

// CR3 yaz — `movq $0, %cr3`. Aynı değeri yazmak TLB'yi temizler (INVLPG-gerekmez hile).
çıplak işlev kxm_cr3_yaz(v: tam64) {
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"movq $0, %cr3"#
            girdi("r", v)
            bozulan("~{memory}")
        }
    }
}

// Tablo girdisinden çerçeve adresi: alt 12 bit (bayraklar) at → g - (g % 4096).
çıplak işlev kxm_cerceve(g: tam64) -> tam64 {
    ver g - (g % 4096);
}

// MMU kur (arm64 boot eşi): PD+PT temizle, PT[0]=PA|P|RW, PD[128]=PT|P|RW,
// PDPT[1]=PD|P|RW (mevcut PML4→PDPT üzerine), CR3 yeniden yaz (TLB flush).
çıplak işlev kdl_mmu_kur_x86() {
    değişken i: tam64 = 0;
    iken i < 512 { kxm_y64(KXM_PD + i * 8, 0); i = i + 1; }
    i = 0;
    iken i < 512 { kxm_y64(KXM_PT + i * 8, 0); i = i + 1; }
    kxm_y64(KXM_PT, KMMU_CEVIRI_PA + 3);            // PT[0] = PA | P|RW
    kxm_y64(KXM_PD + 128 * 8, KXM_PT + 3);          // PD[128] = PT | P|RW
    değişken pml4: tam64 = kxm_cerceve(kxm_cr3_oku());
    değişken pdpt: tam64 = kxm_cerceve(kxm_o64(pml4));  // PML4[0] → PDPT
    kxm_y64(pdpt + 8, KXM_PD + 3);                  // PDPT[1] = PD | P|RW
    kxm_cr3_yaz(kxm_cr3_oku());                     // TLB temizle
}

// 1 = non-identity çeviri donanımda çalışıyor (alias testleri tutarsa). Sentetik DEĞİL.
çıplak işlev kmmu_ceviri_testi() -> tam32 {
    kxm_y64(KMMU_CEVIRI_PA, 3735928559);            // 0xDEADBEEF → identity VA (=PA)
    değişken a: tam64 = kxm_o64(KMMU_CEVIRI_VA);    // non-identity VA'dan oku → alias mı?
    kxm_y64(KMMU_CEVIRI_VA, 3405691582);            // 0xCAFEBABE → non-identity VA (ters yön)
    değişken b: tam64 = kxm_o64(KMMU_CEVIRI_PA);    // identity VA'dan oku → alias mı?
    eğer a == 3735928559 ve b == 3405691582 {
        ver 1;
    }
    ver 0;
}

// .S kurtarma-vari: kdl_x86_pf_bekle (atlanacak #PF komut uzunluğu) — RIP-göreli yaz.
çıplak işlev kxm_bekle_yaz(v: tam64) {
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"leaq kdl_x86_pf_bekle(%rip), %r11
movq $0, (%r11)"#
            girdi("r", v)
            bozulan("~{r11}") bozulan("~{memory}")
        }
    }
}

// kdl_x86_pf_bekle oku (kurtarma gerçekleştiyse 0'a düşer; .S temizler).
çıplak işlev kxm_bekle_oku() -> tam64 {
    değişken x: tam64 = 0;
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"leaq kdl_x86_pf_bekle(%rip), %r11
movq (%r11), $0"#
            çıktı("=r", &x)
            bozulan("~{r11}") bozulan("~{memory}")
        }
    }
    ver x;
}

// kdl_x86_pf_cr2 (yakalanan #PF CR2 değeri) — RIP-göreli yaz.
çıplak işlev kxm_cr2_yaz(v: tam64) {
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"leaq kdl_x86_pf_cr2(%rip), %r11
movq $0, (%r11)"#
            girdi("r", v)
            bozulan("~{r11}") bozulan("~{memory}")
        }
    }
}

// kdl_x86_pf_cr2 oku (kurtarma sonrası yakalanan CR2).
çıplak işlev kxm_cr2_oku() -> tam64 {
    değişken x: tam64 = 0;
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"leaq kdl_x86_pf_cr2(%rip), %r11
movq (%r11), $0"#
            çıktı("=r", &x)
            bozulan("~{r11}") bozulan("~{memory}")
        }
    }
    ver x;
}

// Haritasız VA'yı oku → GERÇEK #PF (tek komut; .S RIP'i 3 bayt ilerletir). Dönen değer
// ÇÖP — çağıran atar. SABİT YAZMAÇ `movq (%rdi), %rax` = 48 8B 07 (3 bayt).
çıplak işlev kxm_oku_faulting(adr: tam64) -> tam64 {
    değişken x: tam64 = 0;
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"movq (%rdi), %rax"#
            girdi("{di}", adr)
            çıktı("={ax}", &x)
            bozulan("~{memory}")
        }
    }
    ver x;
}

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

Wait, I need to check the "cop" variable is unused - that's fine (arm version has same pattern).

Also I should double check "bozulan" for kxm_cr3_oku - the spec only mentions çıktı. OK.

Let me also verify the `%` in kxm_cerceve for constant divisor. That should be OK.

Let me finalize.
