# Tur 19 — x86 kurtarılamaz istisna raporlayıcısı .kem'de (ZERO-C B1 x86 ikizi)

YALNIZ `runtime/mimari/x86_64/kem_mmu_hw.kem` dosyasının SONUNA ekle: tek bir
```duzenle:runtime/mimari/x86_64/kem_mmu_hw.kem bloğu; ESKİ kısım dosyanın son işlevi
`kmmu_wx_testi`nin TAMAMI (kesitte verildi, birebir), YENİ kısım aynı işlev + altına yeni kod.
Başka dosyaya DOKUNMA. Yalnız `mimari: x86_64` asm; tüm işlevler `çıplak işlev`.
Bağlamdaki arm64 kesiti (`kis_*` + `kdl_istisna_isle`) ikizdir: AYNI biçim, AYNI adlar
(`kis_metin`, `kis_satir`, `kis_hex_basamak`, `kis_onaltilik`, `kdl_istisna_isle`), x86 donanımı.
Kullanılacak mevcut x86 işlevleri (başka dosyalarda, aynı derleme biriminde):
`kem_uart_bayt(b: tam32)` (COM1, sınırlı LSR yoklamalı), `kem_pvh_bilgi() -> tam64`,
`kem_pvh_oku64(adr: tam64) -> tam64`. Yeniden tanımlama YOK.

## İşlevler
1. `kis_bayt(b: tam32)` — `kem_uart_bayt(b);` (arm'daki PL011 yerine COM1).
2. `kis_metin(s: metin)`, `kis_satir()`, `kis_hex_basamak(d: dtam64)`, `kis_onaltilik(d: dtam64)`:
   arm gövdeleri BİREBİR (kis_bayt üzerinden).
3. `kis_cr2_oku() -> tam64`: asm `movq %cr2, $0`, çıktı("=r", &x).
4. `kdl_istisna_isle(tip: tam64, a: tam64, b: tam64)` — `.S` isr_ortak çağırır (rdi=vektör,
   rsi=hata kodu, rdx=RIP). C'deki WEAK tanımı bu GÜÇLÜ tanım ezer. Gövde arm ile birebir:
   `adr = kis_cr2_oku()`; tek satır `ISTISNA tip=0x.. a=0x.. b=0x.. adr=0x..` (değerler
   `olarak dtam64`); `kis_satir()`; sonra sonsuz döngü: `iken 1 == 1 { güvensiz { satıriçi_asm
   { mimari: x86_64 şablon: r#"hlt"# bozulan("~{memory}") } } }`.
5. `kem_pvh_cmdline_esit(s: metin) -> tam32` — PVH çekirdek komut satırı TAM OLARAK s mi?
   - `değişken bi: tam64 = kem_pvh_bilgi(); eğer bi == 0 { ver 0; }`
   - `değişken c: tam64 = kem_pvh_oku64(bi + 24); eğer c == 0 { ver 0; }` (cmdline_paddr)
   - s'nin her baytı i için: komut satırı baytı = `kem_pvh_oku64(c + i)` değerinin en düşük
     baytı = `v - (v / 256) * 256` (v negatif olamaz değil — `v` yerine `kem_pvh_oku64` sonucunu
     `olarak dtam64` ile dtam64'e çevirip `% 256` kullan). `metin_bayt(s, i) olarak dtam64` ile
     eşit değilse `ver 0`.
   - s bittikten sonra komut satırının bir sonraki baytı 0 (NUL) olmalı; değilse `ver 0`.
   - `ver 1;`
Bloğun başına `// [D-690] kurtarılamaz istisna raporu (.kem) + PVH cmdline` başlık yorumu,
her işlev üstüne kısa Türkçe yorum.
