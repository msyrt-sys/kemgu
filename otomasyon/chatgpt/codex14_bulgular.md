# Dalga 14 ölçümleri

## Zamanlayıcı ve adres alanı

- Bir işlemcili x86_64 UEFI yolunda, kullanıcı kipinden gelen LAPIC kesmesinde 15 genel yazmaç ile RIP, CS, RFLAGS, RSP ve SS saklanıyor. Kesme çekirdek kipinde gelirse mevcut çağırana dönüyor. CR3 süreç kaydında tutuluyor. Çekirdeğe dönüşte eski CR3 ve CR0 geri yükleniyor.
- LAPIC saatinin hedef periyodu 10 ms. Süreç süresi için kullanıcı kipinde tamamlanan kesme dilimleri sayılıyor. Saat ve uyku davranışı dalga 13 ile uyumlu.
- 256 MiB OVMF konukta ilk ölçümde 49.761 çerçeve boştu. Bölüm 1 düzeninde bir canlı süreç 512 veri sayfası ve dört tablo sayfası tüketti. Biten çocuğun sonucu `bekle` için kayıtlı kalırken çerçeveleri genel ayırıcıya dönüyor.
- Klavye beklemesi için PS/2 IRQ1 halkası veya bekleyen xHCI aktarımının tamamlanma biti kullanılıyor. xHCI sürücüsü mevcut olay halkasını kullanıyor. Çekirdek hazır iş yokken LAPIC veya aygıt kesmesiyle uyanıyor.
- Yeni süreç çağrıları ve ikili kayıt biçimleri syscall ABI belgesinde 58–60 numaralarda yer alıyor.

### C14-001 — modülden içe alınan çıplak işlev

Aşağıdaki kaynak için C ön yüzü x86_64 LLVM IR üretti (rc=0). Self-host aynı çağrıyı E013 ile reddetti (rc=1). Her iki komut da depo kökünde çalıştırıldı.

```kem
kullan drivers::bellek::cerceve::{ CerceveAyirici, ayir_ardisik };
çıplak işlev c14_al(d: &değişken CerceveAyirici) -> tam64 {
    eşleş ayir_ardisik(d,1) { hiç => { ver 0; } değer(t) => { ver t; } }
}
işlev main() -> tam32 { ver 0; }
```

```sh
build/kemgu --llvm --mimari x86_64-none build/c14_import.kem
build/codegen --llvm --mimari x86_64-none build/c14_import.kem
```

Beklenen iki derleyicide de başarıydı. Self-host tanısı `E013 3 25`. Çağıran UEFI işlevi normal KEMGU işlevi olarak tanımlandı.

### USB klavye açılışındaki aralıklı hata

Dalga 13 süreç regresyonunun self-host serisinde sekizinci açılışta, ilk kullanıcı programı çalışmadan, seri uçta `KABUK HATA xhci interrupt IN yok` görüldü. Aynı kaynakların yeniden derlendiği 10 açılışın tamamı geçti. QMP bağlantısının kapanması bu çalışmada bir sonuçtu. Altta yatan neden doğrulanmadı.
