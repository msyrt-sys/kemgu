# Dalga 13 bulguları

- Taban `b1b9470`, D-760. UEFI kabuk birleşik birimi 5358 satır, self-host `--llvm` 23,54 sn. Taban UEFI kabuk kâhini USB/256 MiB'de 3.935.760 bayt, AHCI/256 MiB'de 3.804.592 bayt heap üst sınırı ölçtü.
- VARSAYIM: ABI v2, eski RDI/RDX düzenine üçüncü argüman olarak RSI'yi ekler. Mevcut PVH ring0 bilinmeyen çağrı davranışı ve arm64 ABI'si korunur. SysV çağrısından önce yığın hizalanır ve DF temizlenir. `iretq` çağıranın bayraklarını yükler.

## C13-001

Self-host, negatif `tam64` sabiti doğrudan dönerken `ret i64` operandını i32 üretir. Küçük tekrar:

```kem
sabit HATA: tam64 = -38;
çıplak işlev sonuc() -> tam64 { ver HATA; }
```

Aynı kaynakta `build/kemgu --llvm --mimari x86_64-none C13-001.kem` ve `build/codegen --llvm --mimari x86_64-none C13-001.kem` çalıştırıldı. Her iki çıktıda beklenen sonuç, `clang -target x86_64-unknown-none -x ir -c` ile derlenmesiydi. C çıktı derlendi. Self çıktıda `sub i32 0, 38` ve `ret i64 %1` oluştu, clang tip uyuşmazlığı verdi. Pozitif sabit ve `ver 0 - HATA;` her iki derleyicide geçiyor.
