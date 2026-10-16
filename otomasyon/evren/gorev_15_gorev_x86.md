## İş 15 — x86 preemptive görev değiştirme: IRQ0 koçanı SP-takası + x86 görev kancaları

Zamanlayıcı çekirdeği `runtime/kem_gorev_cekirdek.kem` (bağlamda) mimariden
bağımsız; arm64 kancaları `runtime/mimari/arm64/kem_gorev_hw.kem` (bağlamda, ÖRNEK).
Model: her timer IRQ'da `.S` koçanı TÜM GP yazmaçlarını yığına iter,
`kdl_irq_isle(sp)` çağırır, dönen SP'ye geçer (başka görevin çerçevesi),
yazmaçları geri alır, `iretq`. Yeni görev = SENTETİK çerçeve.

### ÇERÇEVE DÜZENİ (orkestratör kararı — iki parça BUNA uymak ZORUNDA)
İtme sırası: `rax, rbx, rcx, rdx, rsi, rdi, rbp, r8, r9, r10, r11, r12, r13, r14, r15`
(15 × 8 = 120 B). İtmeden sonra SP = r15 slotu. SP'den yukarı:
`[0]=r15 … [14]=rax` (ofset 0..112), `[15]=RIP @120`, `[16]=CS @128`,
`[17]=RFLAGS @136`, `[18]=RSP @144`, `[19]=SS @152` (CPU'nun ittiği iretq
çerçevesi). Toplam 160 B. CPU kesmede RSP'yi 16'ya hizalar → 5 itme (40 B) +
15 itme (120 B) = 160 → `call` öncesi SP 16-hizalı (SysV) ✓.

### Değişiklik 1 — `boot/start_x86_64.S` IRQ0 koçanı
Bağlamdaki `/* === IRQ0 (timer) stub (C3b) … */` yorumundan `iretq`'ya kadar olan
bloğun TAMAMINI (ÇAPA = o blok, birebir) şununla değiştir:
- Yorum: tam kayıt + SP takası (D-685); C kernel'leri için uyumluluk (aşağıdaki
  zayıf varsayılan SP'yi değiştirmez → davranış AYNI).
- `.global kdl_irq0_stub` / `kdl_irq0_stub:` → 15 `pushq` (yukarıdaki sırayla) →
  `movq $0, %rdi` / `call kdl_kesme_isle` (tik; C ya da .kem) →
  `movq %rsp, %rdi` / `call kdl_irq_isle` / `movq %rax, %rsp` (SP takası) →
  PIC EOI (`movb $0x20, %al` / `outb %al, $0x20`) → 15 `popq` (TERS sıra) → `iretq`.
- Hemen ardından ZAYIF varsayılan: `.weak kdl_irq_isle` / `kdl_irq_isle:` /
  `movq %rdi, %rax` / `ret` — yorum: `.kem` x86 güçlü tanımı (kem_preempt) ezer;
  C kernel'lerinde SP değişmez.

### Değişiklik 2 — YENİ `runtime/mimari/x86_64/kem_gorev_hw.kem`
Türkçe başlık (D2b, arm64 ikizi, çerçeve düzeni). İçerik:
- Sabitler (DOĞRULANMIŞ, ondalık; x86 RAM 48-50 MB — heap 17 MB üstü, DMA ~127 MB altı):
  `KG_PSP = 50331648` (0x03000000), `KG_OLU = 50334720` (0x03000C00),
  `KG_G1 = 50335744`, `KG_G2 = 50335752`, `KG_AKTIF = 50335760`,
  `KG_STK1 = 51380224` (0x03100000), `KG_STK2 = 52428800` (0x03200000). Hepsi `tam64`.
- `kg_gorev1_adr() -> tam64` / `kg_gorev2_adr()`: `satıriçi_asm { mimari: x86_64
  şablon: r#"leaq kem_gorev1(%rip), $0"# çıktı("=r", &x) }` (ve kem_gorev2).
- `kg_cerceve_kur(yigin_tepe: tam64, giris: tam64) -> tam64`:
  `ust = (yigin_tepe / 16) * 16`; `sp = ust - 160`; 0..14 slotları sıfırla
  (`kg_yaz64(sp + k*8, 0)`); `sp+120 = giris`; `sp+128 = 8` (CS); `sp+136 = 514`
  (RFLAGS 0x202, IF=1); `sp+144 = ust - 8` (görev RSP; `call` sonrası hizası);
  `sp+152 = 16` (SS); `ver sp;`
- `kg_alan_sifirla()` ve `kg_alan_gec(i: tam64)`: boş gövde (x86 ara hedefinde
  görev başına adres alanı yok; yorumla belirt).
- `kdl_irq_isle(sp: tam64) -> tam64 { ver kem_preempt(sp); }` — koçanın güçlü
  tanımı (arm64'te karşılığı kem_zaman_hw.kem'deki kdl_irq_isle).
asm'ler `güvensiz { }` içinde.

Blok yolları yalnız: `boot/start_x86_64.S`, `runtime/mimari/x86_64/kem_gorev_hw.kem`.
