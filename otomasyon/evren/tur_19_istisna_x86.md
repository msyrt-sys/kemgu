# Tur 19 — istisna_x86 — `deepseek-v4.1-flash`

- Tarih: 2026-10-16 15:16:53 · süre 205.4 sn · finish `stop` · reasoning_effort `low`
- usage: `{"prompt_tokens": 13766, "total_tokens": 20200, "completion_tokens": 6434, "prompt_tokens_details": {"cached_tokens": 0, "created_cache_tokens": 13760}, "completion_tokens_details": {"reasoning_tokens": 3683}}`
- Değişiklik blokları: 1 — duzenle:runtime/mimari/x86_64/kem_mmu_hw.kem
- İstem: [tur_19_istisna_x86_istem.md](tur_19_istisna_x86_istem.md)

## Yanıt

```duzenle:runtime/mimari/x86_64/kem_mmu_hw.kem
<<<<<<< ESKI
çıplak işlev kmmu_wx_testi() -> tam32 {
    kxm_nx_ac();
    değişken c: tam64 = kmmu_as_olustur(2);
    eğer c == 0 { ver 0; }
    değişken pt: tam64 = KMMU_AS_TABAN + 2 * KMMU_AS_ADIM + 12288;
    değişken pa: tam64 = KMMU_OZEL_PA + 2 * KMMU_2MB;
    kxm_y64(pa, 195);                                  // sayfa 0: `ret`
    kxm_y64(pa + KMMU_SAYFA, 195);                     // sayfa 1: `ret`
    kxm_y64(pt, KXM_NX_BIT + pa + 3);                  // sayfa 0 = VERİ (NX)
    kxm_y64(pt + 8, pa + KMMU_SAYFA + 3);              // sayfa 1 = KOD
    değişken k: tam64 = kxm_cr3_oku();
    kmmu_as_gec(c);
    kxm_cagir(KMMU_OZEL_VA + KMMU_SAYFA);              // KOD sayfası: fault yok, döner
    kxm_cr2_yaz(0);
    kxm_hata_yaz(0);
    kxm_bekle_yaz(KXM_RET_TAKLIDI);
    kxm_cagir(KMMU_OZEL_VA);                           // VERİ sayfası: komut getirme #PF
    değişken bekle: tam64 = kxm_bekle_oku();
    değişken cr2: tam64 = kxm_cr2_oku();
    değişken hk: tam64 = kxm_hata_oku();
    kmmu_as_gec(k);
    eğer (bekle == 0) ve (cr2 == KMMU_OZEL_VA) ve (hk == KXM_PF_NX_HATA) { ver 1; }
    ver 0;
}
=======
çıplak işlev kmmu_wx_testi() -> tam32 {
    kxm_nx_ac();
    değişken c: tam64 = kmmu_as_olustur(2);
    eğer c == 0 { ver 0; }
    değişken pt: tam64 = KMMU_AS_TABAN + 2 * KMMU_AS_ADIM + 12288;
    değişken pa: tam64 = KMMU_OZEL_PA + 2 * KMMU_2MB;
    kxm_y64(pa, 195);                                  // sayfa 0: `ret`
    kxm_y64(pa + KMMU_SAYFA, 195);                     // sayfa 1: `ret`
    kxm_y64(pt, KXM_NX_BIT + pa + 3);                  // sayfa 0 = VERİ (NX)
    kxm_y64(pt + 8, pa + KMMU_SAYFA + 3);              // sayfa 1 = KOD
    değişken k: tam64 = kxm_cr3_oku();
    kmmu_as_gec(c);
    kxm_cagir(KMMU_OZEL_VA + KMMU_SAYFA);              // KOD sayfası: fault yok, döner
    kxm_cr2_yaz(0);
    kxm_hata_yaz(0);
    kxm_bekle_yaz(KXM_RET_TAKLIDI);
    kxm_cagir(KMMU_OZEL_VA);                           // VERİ sayfası: komut getirme #PF
    değişken bekle: tam64 = kxm_bekle_oku();
    değişken cr2: tam64 = kxm_cr2_oku();
    değişken hk: tam64 = kxm_hata_oku();
    kmmu_as_gec(k);
    eğer (bekle == 0) ve (cr2 == KMMU_OZEL_VA) ve (hk == KXM_PF_NX_HATA) { ver 1; }
    ver 0;
}

// [D-690] kurtarılamaz istisna raporu (.kem) + PVH cmdline
// ============================================================================
// ZERO-C B1 x86 ikizi: arm64 kdl_istisna_isle (kis_* + CR2/FAR) ile AYNI biçim,
// AYNI adlar; x86 donanımı (COM1 UART, %cr2). C WEAK tanımını bu GÜÇLÜ .kem
// tanımı ezer. NaN durumu yok: tek satır rapor + hlt sonsuz.
// ============================================================================

// COM1'e tek bayt (arm'daki PL011 MMIO yerine kem_uart_bayt).
çıplak işlev kis_bayt(b: tam32) { kem_uart_bayt(b); }

// Metni COM1'e yaz.
çıplak işlev kis_metin(s: metin) {
    değişken n: tam32 = metin_uzunluk(s);
    değişken i: tam32 = 0;
    iken i < n { kis_bayt(metin_bayt(s, i) olarak tam32); i = i + 1; }
}

// Satır sonu.
çıplak işlev kis_satir() { kis_bayt(10); }

// dtam64'ü onaltılık basamaklarla yaz (özyinelemeli, arm ile birebir).
çıplak işlev kis_hex_basamak(d: dtam64) {
    eğer d > 0 {
        kis_hex_basamak(d / 16);
        değişken r: tam32 = (d - (d / 16) * 16) olarak tam32;
        eğer r < 10 { kis_bayt(r + 48); } değilse { kis_bayt(r + 87); }
    }
}

// 0 → "0", aksi hâlde onaltılık basamaklar.
çıplak işlev kis_onaltilik(d: dtam64) {
    eğer d == 0 { kis_bayt(48); } değilse { kis_hex_basamak(d); }
}

// CR2 (son #PF hatası adresi) oku.
çıplak işlev kis_cr2_oku() -> tam64 {
    değişken x: tam64 = 0;
    güvensiz { satıriçi_asm { mimari: x86_64 şablon: r#"movq %cr2, $0"# çıktı("=r", &x) } }
    ver x;
}

// .S isr_ortak çağırır (rdi=vektör, rsi=hata kodu, rdx=RIP); arm ikiziyle birebir:
// CR2 oku + "ISTISNA tip=0x.. a=0x.. b=0x.. adr=0x.." + satır + hlt sonsuz.
çıplak işlev kdl_istisna_isle(tip: tam64, a: tam64, b: tam64) {
    değişken adr: tam64 = kis_cr2_oku();
    // kis_onaltilik ISARETSIZ (dtam64) ister — adres/hata degerleri isaretsiz
    // yorumlanmali (tam64 negatif goruntusu onaltilik basamakta YANLIS olur).
    kis_metin("ISTISNA tip=0x"); kis_onaltilik(tip olarak dtam64);
    kis_metin(" a=0x"); kis_onaltilik(a olarak dtam64);
    kis_metin(" b=0x"); kis_onaltilik(b olarak dtam64);
    kis_metin(" adr=0x"); kis_onaltilik(adr olarak dtam64);
    kis_satir();
    iken 1 == 1 {
        güvensiz { satıriçi_asm { mimari: x86_64 şablon: r#"hlt"# bozulan("~{memory}") } }
    }
}

// PVH çekirdek komut satırı TAM OLARAK s mi? 1=evet, 0=hayır.
// bi+24 = cmdline_paddr (PVH boot bilgi yapısı); bayt-bayt karşılaştır + NUL sınırı.
çıplak işlev kem_pvh_cmdline_esit(s: metin) -> tam32 {
    değişken bi: tam64 = kem_pvh_bilgi();
    eğer bi == 0 { ver 0; }
    değişken c: tam64 = kem_pvh_oku64(bi + 24);      // cmdline_paddr
    eğer c == 0 { ver 0; }
    değişken n: tam32 = metin_uzunluk(s);
    değişken i: tam32 = 0;
    iken i < n {
        değişken ham: dtam64 = kem_pvh_oku64(c + (i olarak tam64)) olarak dtam64;
        eğer ham % 256 != (metin_bayt(s, i) olarak dtam64) { ver 0; }
        i = i + 1;
    }
    değişken son: dtam64 = kem_pvh_oku64(c + (n olarak tam64)) olarak dtam64;
    eğer son % 256 != 0 { ver 0; }                   // s'den sonra NUL olmalı
    ver 1;
}
>>>>>>> YENI
```

Açıklama: `kmmu_wx_testi` aynen korundu, altına x86 ikiz raporlayıcı eklendi. `kis_bayt` COM1 `kem_uart_bayt`'a delege eder; `kis_metin/satir/hex_basamak/onaltilik` arm64 kesitiyle birebir gövde. `kis_cr2_oku` yalnız `movq %cr2, $0` (x86_64 asm). `kdl_istisna_isle` arm ikiziyle aynı biçimde CR2 okur, tek satır rapor basar ve `hlt` ile sonsuza döner. `kem_pvh_cmdline_esit` cmdline_paddr'dan bayt-bayt karşılaştırma yapar; `% 256` ile en düşük bayt alınır (dtam64'e açık dönüşüm) ve son NUL kontrol edilir.

VARSAYIMLAR
- `kmmu_wx_testi` dosyanın son işlevidir; ardından başka içerik yok.
- `kem_uart_bayt`, `kem_pvh_bilgi`, `kem_pvh_oku64` çıplak işlevlerdir (aynı derleme biriminde; E013 sağlanır) ve yeniden tanımlanmaları istenmedi.
- `metin_uzunluk` / `metin_bayt` dil yerleşikleridir (arm64 kesitindeki kullanımla aynı).
- `%` sabit bölen (256) ile `@kdl_panik` gerektirmez; görev bunu açıkça istedi.

## Düşünme (reasoning)

Let me look. The instruction says ESKI block must be the entire `kmmu_wx_testi` function. YENI is the same + new code appended. This is a terminal append.

Wait, but there are other functions after `kmmu_wx_testi`? The kesit says satır 325-348, and it's the last function presumably. OK.

Let me write the new code.

For dtam64 usage: `değişken v: dtam64 = kem_pvh_oku64(c + (i olarak tam64)) olarak dtam64;` then `v - (v / 256) * 256`. Or use `% 256`. The task says use `% 256`. Actually wait, "kem_pvh_oku64 sonucunu `olarak dtam64` ile dtam64'e çevirip `% 256` kullan". OK.

Hmm but dtam64 is unsigned. `%` might need kdl_panik... The task explicitly says to use `% 256`, so go with it.

Wait but the kanal comment said `%` requires @kdl_panik. But task says explicitly to use `% 256`. Follow the task.

Actually, let me re-read: "komut satırı baytı = `kem_pvh_oku64(c + i)` değerinin en düşük baytı = `v - (v / 256) * 256` (v negatif olamaz değil — `v` yerine `kem_pvh_oku64` sonucunu `olarak dtam64` ile dtam64'e çevirip `% 256` kullan)."

Hmm confusing. It says the low byte is `v - (v / 256) * 256` (v negative cannot—wait). Then says: "instead of v, convert kem_pvh_oku64 result to dtam64 and use `% 256`". So use `% 256`.

Let me write:

```
değişken ham: dtam64 = kem_pvh_oku64(c + (i olarak tam64)) olarak dtam64;
değişken kb: dtam64 = ham % 256;
eğer kb != (metin_bayt(s, i) olarak dtam64) { ver 0; }
```

For the loop, need to iterate i from 0 to metin_uzunluk(s). 

```
değişken n: tam32 = metin_uzunluk(s);
değişken i: tam32 = 0;
iken i < n {
    değişken ham: dtam64 = kem_pvh_oku64(c + (i olarak tam64)) olarak dtam64;
    eğer ham % 256 != (metin_bayt(s, i) olarak dtam64) { ver 0; }
    i = i + 1;
}
değişken son: dtam64 = kem_pvh_oku64(c + (n olarak tam64)) olarak dtam64;
eğer son % 256 != 0 { ver 0; }
ver 1;
```

Wait `ham % 256` where ham is dtam64 — result dtam64. Comparison with dtam64, should be fine.

Actually in kemgu, `%` on dtam64... the kanal comment said variable-divisor `%` needs panic but constant divisor probably OK. Here constant 256. Should be fine.

Hmm but the task said use `% 256`. OK.

kis_hex_basamak: arm version:

```
çıplak işlev kis_hex_basamak(d: dtam64) {
    eğer d > 0 {
        kis_hex_basamak(d / 16);
        değişken r: tam32 = (d - (d / 16) * 16) olarak tam32;
        eğer r < 10 { kis_bayt(r + 48); } değilse { kis_bayt(r + 87); }
    }
}
```

Use same.

kis_cr2_oku: asm movq %cr2, $0. Wait arm version uses `mrs $0, far_el1` with çıktı("=r", &x). For x86: `movq %cr2, $0`.

Hmm the AT&T syntax for reading CR2 into register: `movq %cr2, %rax`. With $0 being the output register. So `movq %cr2, $0`.

Now let me check the `kis_far_oku` in arm uses güvensiz wrapper. For x86 kis_cr2_oku, the task says: "asm `movq %cr2, $0`, çıktı("=r", &x)". Should it be in güvensiz? The arm one is in güvensiz. Let me match:

```
çıplak işlev kis_cr2_oku() -> tam64 {
    değişken x: tam64 = 0;
    güvensiz { satıriçi_asm { mimari: x86_64 şablon: r#"movq %cr2, $0"# çıktı("=r", &x) } }
    ver x;
}
```

Task says just çıktı("=r", &x), no bozulan("~{memory}") mentioned. Match arm form.

For hlt loop: `iken 1 == 1 { güvensiz { satıriçi_asm { mimari: x86_64 şablon: r#"hlt"# bozulan("~{memory}") } } }`.

The arm version uses `iken 1 == 1` inside kdl_istisna_isle. OK.

Now the new block. Let me construct.

```
// [D-690] kurtarılamaz istisna raporu (.kem) + PVH cmdline

// COM1'e tek bayt (arm'daki PL011 yerine).
çıplak işlev kis_bayt(b: tam32) { kem_uart_bayt(b); }

// Metni COM1'e yaz.
çıplak işlev kis_metin(s: metin) {
    değişken n: tam32 = metin_uzunluk(s);
    değişken i: tam32 = 0;
    iken i < n { kis_bayt(metin_bayt(s, i) olarak tam32); i = i + 1; }
}

// Yeni satır.
çıplak işlev kis_satir() { kis_bayt(10); }

// dtam64'ü onaltılık basamaklarla yaz (özyinelemeli).
çıplak işlev kis_hex_basamak(d: dtam64) {
    eğer d > 0 {
        kis_hex_basamak(d / 16);
        değişken r: tam32 = (d - (d / 16) * 16) olarak tam32;
        eğer r < 10 { kis_bayt(r + 48); } değilse { kis_bayt(r + 87); }
    }
}

// 0 → "0", değilse onaltılık basamaklar.
çıplak işlev kis_onaltilik(d: dtam64) {
    eğer d == 0 { kis_bayt(48); } değilse { kis_hex_basamak(d); }
}

// CR2 (son #PF adresi) oku.
çıplak işlev kis_cr2_oku() -> tam64 {
    değişken x: tam64 = 0;
    güvensiz { satıriçi_asm { mimari: x86_64 şablon: r#"movq %cr2, $0"# çıktı("=r", &x) } }
    ver x;
}

// kdl_istisna_isle birebir (arm ikizi): CR2 oku + rapor + hlt sonsuz.
// .S isr_ortak çağırır (rdi=vektör, rsi=hata kodu, rdx=RIP); C WEAK tanımı bu GÜÇLÜ tanımı ezmez.
çıplak işlev kdl_istisna_isle(tip: tam64, a: tam64, b: tam64) {
    değişken adr: tam64 = kis_cr2_oku();
    kis_metin("ISTISNA tip=0x"); kis_onaltilik(tip olarak dtam64);
    kis_metin(" a=0x"); kis_onaltilik(a olarak dtam64);
    kis_metin(" b=0x"); kis_onaltilik(b olarak dtam64);
    kis_metin(" adr=0x"); kis_onaltilik(adr olarak dtam64);
    kis_satir();
    iken 1 == 1 {
        güvensiz { satıriçi_asm { mimari: x86_64 şablon: r#"hlt"# bozulan("~{memory}") } }
    }
}

// PVH çekirdek komut satırı TAM OLARAK s mi? 1=evet, 0=hayır.
çıplak işlev kem_pvh_cmdline_esit(s: metin) -> tam32 {
    değişken bi: tam64 = kem_pvh_bilgi();
    eğer bi == 0 { ver 0; }
    değişken c: tam64 = kem_pvh_oku64(bi + 24);
    eğer c == 0 { ver 0; }
    değişken n: tam32 = metin_uzunluk(s);
    değişken i: tam32 = 0;
    iken i < n {
        değişken ham: dtam64 = kem_pvh_oku64(c + (i olarak tam64)) olarak dtam64;
        eğer ham % 256 != (metin_bayt(s, i) olarak dtam64) { ver 0; }
        i = i + 1;
    }
    değişken son: dtam64 = kem_pvh_oku64(c + (n olarak tam64)) olarak dtam64;
    eğer son % 256 != 0 { ver 0; }
    ver 1;
}
```

Wait, do I need `kis_bayt` to call `kem_uart_bayt(b);`? The task says: `kis_bayt(b: tam32)` — `kem_uart_bayt(b);`. OK.

But wait — E013: çıplak işlev yalnız çıplak/extern çağırabilir. `kem_uart_bayt` — is it çıplak? Task says it's in another file. Should be çıplak. OK.

`metin_uzunluk` and `metin_bayt` — are these builtins? In kem_heap.kem, `kdl_metin_uzunluk` uses `metin_uzunluk` and `metin_bayt` which are called without prefix... Hmm actually in kem_heap.kem the çıplak functions `kdl_metin_uzunluk` call `metin_uzunluk`? Let me re-check.

Actually looking at kem_heap.kem: the fearures are named `kdl_metin_uzunluk` and `kdl_metin_bayt`. But in kem_panik it calls `metin_uzunluk(m)` and `metin_bayt(m, i)`. So these are different — `metin_uzunluk` / `metin_bayt` are builtins (dil seviyesi). Good, arm version also uses them.

Now for the ESKI block — must match exactly. Let me copy from the kesit.

The whole `kmmu_wx_testi`. OK.

Now, does the block end with the function or is there content after? The kesit shows satır 325-348 and ends with the function closing. I'll assume it's the last content.

Let me use `% 256` on `ham` (dtam64). Should be fine.

Actually, one more concern: the task says `v - (v / 256) * 256` alternative but then says use `% 256`. I'll use `% 256`.

Hmm actually re-reading more carefully: "komut satırı baytı = `kem_pvh_oku64(c + i)` değerinin en düşük baytı = `v - (v / 256) * 256` (v negatif olamaz değil — `v` yerine `kem_pvh_oku64` sonucunu `olarak dtam64` ile dtam64'e çevirip `% 256` kullan)". 

I think it's saying: the byte = low byte = v - (v/256)*256 (but "v negatif olamaz" got garbled). Instead use % 256.

OK let me finalize. I'll write `ham % 256` with ham: dtam64.

Wait but `%` on dtam64 might emit panic for zero divisor? No, constant 256, not variable. Fine.

Let me also double-check: `kem_pvh_oku64(c + (i olarak tam64))` — i is tam32, cast to tam64. c is tam64. OK.

For `son % 256 != 0` — comparing dtam64 with literal 0. OK.

Now let me make sure the ESKI block matches the given text exactly. The kesit:

```
çıplak işlev kmmu_wx_testi() -> tam32 {
    kxm_nx_ac();
    değişken c: tam64 = kmmu_as_olustur(2);
    eğer c == 0 { ver 0; }
    değişken pt: tam64 = KMMU_AS_TABAN + 2 * KMMU_AS_ADIM + 12288;
    değişken pa: tam64 = KMMU_OZEL_PA + 2 * KMMU_2MB;
    kxm_y64(pa, 195);                                  // sayfa 0: `ret`
    kxm_y64(pa + KMMU_SAYFA, 195);                     // sayfa 1: `ret`
    kxm_y64(pt, KXM_NX_BIT + pa + 3);                  // sayfa 0 = VERİ (NX)
    kxm_y64(pt + 8, pa + KMMU_SAYFA + 3);              // sayfa 1 = KOD
    değişken k: tam64 = kxm_cr3_oku();
    kmmu_as_gec(c);
    kxm_cagir(KMMU_OZEL_VA + KMMU_SAYFA);              // KOD sayfası: fault yok, döner
    kxm_cr2_yaz(0);
    kxm_hata_yaz(0);
    kxm_bekle_yaz(KXM_RET_TAKLIDI);
    kxm_cagir(KMMU_OZEL_VA);                           // VERİ sayfası: komut getirme #PF
    değişken bekle: tam64 = kxm_bekle_oku();
    değişken cr2: tam64 = kxm_cr2_oku();
    değişken hk: tam64 = kxm_hata_oku();
    kmmu_as_gec(k);
    eğer (bekle == 0) ve (cr2 == KMMU_OZEL_VA) ve (hk == KXM_PF_NX_HATA) { ver 1; }
    ver 0;
}
```

I'll use this as ESKI and the same + new code as YENI.

Note: instruction says "ESKİ kısım dosyanın son işlevi `kmmu_wx_testi`nin TAMAMI ... YENİ kısım aynı işlev + altına yeni kod." So YENI = same function then new code.

Let me write.
