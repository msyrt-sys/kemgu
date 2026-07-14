# KEMGU-OS — Kalan 5 Subsystem: Final-Faz Yol Haritası (KEŞİF)

> **Durum:** kem_os'un SKALER runtime'ı (allocator + metin + mmio + yetki) SAF-.kem, C=0
> (D-260→268). Bu belge kalan 5 subsystem'in (**kesme / zaman / mmu / görev / virtio**)
> saf-.kem'e göçünü kapsar. **Bu bir KEŞİFtir — kod göçü yapılmadı.** Her iddia gerçek
> kaynağa + gerçek clang/KEMGU çıktısına karşı doğrulandı (D-268 dersi: tahmin ≠ gerçek).
>
> Kaynak: `runtime/kdl_{kesme,zaman,mmu,gorev,virtio,virtio_net}.c`, `boot/start_aarch64.S`,
> `test/ornekler/kem_os.kem`, `src/llvm.h`, `src/llvm.c`. Doğrulama tarihi: 2026-07-12 (D-268 sonrası).

---

## 0. En Önemli İki Ampirik Bulgu

### Bulgu A — Gereken ASIL primitif ZATEN VAR, yalnız arch-kilitli (inline-asm)
KEMGU'da satıriçi asm **zaten tam kurulu**: `satıriçi_asm { mimari: … şablon: … çıktı/girdi/bozulan }`
sözdizimi parse edilir ve `call asm sideeffect "…","…"(…)` olarak emit edilir. Ampirik kanıt:

```
$ build/kemgu.exe --llvm test/snapshots/asm_round_trip.kem   # x86_64-tag'li
  %7 = call { i32, i32 } asm sideeffect "mov $2,$0\0A add $$2,$0 …", "=r,=r,r,~{cc}"(i32 %6)   ✓ EMIT
```

**AMA hedef mimari sabit-kodlu x86_64** (`src/llvm.h:38` `#define KEMGU_HEDEF_MIMARI "x86_64"`).
arm64-tag'li asm `AS001` ölümcül hatası ile **reddedilir** (`src/llvm.c` DUGUM_SATIRICI_ASM):

```
$ build/kemgu.exe --llvm test/snapshots/asm_arm64_ret.kem   # arm64-tag: mrs $0, CNTPCT_EL0
  hata[AS001]: satirici_asm mimari etiketi 'arm64' hedef mimariyle uyusmuyor (hedef: x86_64) — satir 6
  rc=1   ✗ RED
```

`asm_arm64_ret.kem`'in kendi yorumu bunu doğruluyor: *"arm64-tagli asm x86_64 triple altinda
REDDEDILMELI. **Hedefe-duyarli triple gelince (C8) bu dosya ARM64 hedefinde derlenecek.**"* — yani
aarch64 sysreg/barrier asm zaten TASARLANMIŞ; tek eksik hedefe-duyarlı triple (bilinen "C8'in işi").
Emit edilen `asm sideeffect` IR'ı hedef-agnostiktir; şablonu clang aarch64 backend'i assemble eder.
Dolayısıyla **P1 = yeni özellik değil, mevcut altyapının arch-kilidini açmak.**

### Bulgu B — Cross-file yalnız BUILTIN arabirimiyle çalışır (paralellik forkunun cevabı)
Ampirik test: bir çıplak fn başka dosyadaki builtin-OLMAYAN sembolü çağırınca:

```
$ build/kemgu.exe --check <çıplak fn: ver malloc(n)>
  hata[T002]: tanimsiz sembol   ← malloc bare-ad builtin DEĞİL, dosya-dışı çözülemez
```

`.kem`'de dosya-dışı builtin-olmayan fonksiyon çağrısı için forward-declare/extern **YOK**. Cross-file
link YALNIZ derleyici-bilinen builtin'ler için çalışır: kem_os.o `@kdl_dizi_olustur / @kdl_mmio_oku32 /
@kdl_yetki_olustur` çağırır (compiler `declare` emit eder), kem_heap.o bu sembolleri **tanımlar** →
link çözer. Bugünkü kem_os = tam olarak bu (kem_os.o + kem_heap.o + N subsystem.o, hepsi builtin/
boot-asm arabirimiyle). **Sonuç:** subsystem AYRI dosyada olabilir ANCAK yalnız builtin (Dizi/mmio/
yetki/bölge) + boot-asm-giriş + dosya-içi yardımcılar üzerinden konuşursa. Birbirinin builtin-olmayan
fonksiyonunu cross-file çağıran subsystem'ler (kesme↔zaman↔görev) ya AYNI dosyada olmalı ya da
**P3 (cross-file çıplak declare)** primitifini gerektirir.

---

## 1. Gereken Enabling Primitifler (birleşik, önceliklendirilmiş)

| # | Primitif | Durum | Neyi açar | Hangi subsystem'ler |
|---|----------|-------|-----------|---------------------|
| **P1** | **aarch64 satıriçi_asm (hedefe-duyarlı mimari/triple)** | Altyapı VAR (emit `asm sideeffect`), arch x86_64 sabit-kodlu (`llvm.h:38`); arm64 → AS001. Fix = "C8'in işi" (hedefe-duyarlı triple). | **MSR/MRS sysreg** (tümü) + **DSB/ISB/DMB bariyer** (mmu, virtio) + **TLBI** (mmu) + wfi/wfe (zaman/kesme) | **5/5** — TÜM irreducible asm bu tek primitifle örtülür |
| **P2** | **Veri hizalama denetimi** (statik/bölge tampon `aligned(N)`) | `.kem`'de hizalama attribute'u YOK. Workaround: bölgeden over-alloc + manuel `(p+15)&~15` (statik dizi için yetmez). | 16-bayt virtqueue ring'leri + 4KB sayfa tabloları | **virtio** (14× aligned16), **mmu** (2× aligned4096) |
| **P3** | **Cross-file çıplak declare/extern** (builtin-olmayan) | Ampirik: cross-file builtin-olmayan çağrı → T002. Yalnız builtin cross-file çalışır. | Ayrı-dosya subsystem'lerin birbirini çağırması → **PARALELLİK** | Yalnız coupled küme (kesme/zaman/görev) ayrı dosyalara bölünürse. Tek-dosya ise gereksiz. |
| P4 (ops.) | **naked/ham fonksiyon** (region-prologue'suz + özel epilog) | Yok. AMA context-switch + vektör-stub'lar ZATEN `boot/*.S`'de (C değil). | Context-switch / vektör-stub'ı .kem-asm'e taşımak | görev/kesme — **gerekli değil** (asm zaten .S; Yasa-4 = sıfır **C**, .S ayrı karar) |

**P1 baskındır**: tek başına 5 subsystem'in sysreg + bariyer + TLBI ihtiyacının tamamını açar. P2 dar
(2 subsystem), P3 yalnız paralellik-için, P4 opsiyonel (asm zaten .S).

**D-268 struct-ABI mayını — bu 5'te YOK:** 5 subsystem'in HİÇBİRİ struct-by-value döndürmez (hepsi
void/int/uint64_t/pointer; `KdlTCB`/`KdlYetki`/virtqueue struct'ları daima pointer'la geçer). AAPCS64
naïf-aggregate register-packing uyumsuzluğu (D-268) bu göçlerde tetiklenmez. ✓

---

## 2. Subsystem-başı Tablo

| Subsystem | İfade-edilebilir? | Gereken primitif | Bağımlılık (DAG) | Paylaşılan obje | Struct-sınır riski | Boyut / zorluk |
|-----------|-------------------|------------------|------------------|-----------------|--------------------|----------------|
| **virtio** (blk+net) | Mantık=int/ptr ✓; MMIO=builtin ✓; tek engel bariyer+hizalama | **P1** (dsb sy) + **P2** (aligned16 ring) | **YOK** — saf polling, IRQ yok, allocator yok (statik BSS) | bm_a64_virtio.o + _net.o; virtio kernel'leriyle paylaşılır → mmio-gibi guard/variant | **Yok** (struct sınırı geçmez) | ~16KB / **EN KOLAY** (IRQ/alloc/struct duvarı yok) |
| **mmu** | %90 düz bellek (tablo kurma) + %10 sysreg/bariyer | **P1** (MSR MAIR/TCR/TTBR0/SCTLR + DSB/ISB + TLBI vmalle1) + **P2** (aligned4096 tablo) | **YOK** — self-contained (dışa çağrı yok) | bm_a64_mmu.o; boot `bl kdl_mmu_kur` | Yok | 141 satır / **ORTA**, temiz (izole) |
| **zaman** | Timer=sysreg (MMIO değil!); GIC=MMIO builtin ✓ | **P1** (MRS CNTFRQ_EL0, MSR CNTV_TVAL/CTL_EL0, DAIF, wfi) | kesme (aynı dosyada birleşik); handler boot-asm vektöründen; **kem_os KULLANMIYOR** | bm_a64_zaman.o | Yok | 139 satır / **ORTA**, kesme ile coupled |
| **görev** | Politika=taşınabilir; context-switch=irreducible asm **(zaten `boot/start_aarch64.S`)** | **P1** (compiler bariyer — trivial) [+P4 ops.] | zaman+kesme (preempt yolu); `kdl_baglam_degis`(.S), `kdl_ttbr_degis`(.S/mmu) çağırır | bm_a64_gorev.o | Yok (KdlTCB daima ptr) | ~300 satır / **ORTA**; switch **.S KALIR** |
| **kesme** (fault+syscall) | Mantık büyük; MRS FAR_EL1/SPSR_EL1 | **P1** (MRS FAR_EL1/SPSR_EL1, wfe) [+P3 bölünürse] | görev, zaman, virtio(blk+net), yazdir_bare, linker-sym — **EN COUPLED** | bm_a64_kesme.o | Yok | 532 satır / **EN ZOR** (boyut+coupling) |

### Kritik nüans — bu 5 C dosyası bugün kem_os'tan ÇAĞRILMIYOR
5 okuyucunun tamamı doğruladı: `kem_os.kem`'de bu C dosyalarına **sıfır çağrı**. kem_os exception
mantığını **.kem'de yeniden yazıyor** (`kem_istisna_isle`), timer/mmu/görev/syscall'ı kullanmıyor.
C subsystem'ler kem_os'a **boot-asm** üzerinden bağlı (`start_aarch64.S` vektörleri → `kdl_irq_isle`;
`bl kdl_mmu_kur`) VEYA ölü-linkli VEYA başka kernel'ler (virtio/test) tarafından kullanılıyor.
**Sonuç:** bu subsystem'leri "göç ettirmek", önce onları kem_os'ta **AKTİFLEŞTİRMEYİ** (şu an dormant)
gerektirir — saf mekanik port'un ötesinde bir kapsam genişlemesi. Bu, göçün gerçek yükünün büyük
kısmıdır (özellikle kesme/görev/zaman kümesi).

---

## 3. Önerilen Final-Faz Sırası

### FAZ 0 — P1 (aarch64 satıriçi_asm) — HER ŞEYİ AÇAN TEK KAPI
Tek codegen değişikliği: `KEMGU_HEDEF_MIMARI/TRIPLE` hedefe-duyarlı yap (arm64 tag'i aarch64 build'de
kabul et). Hazır test var (`asm_arm64_ret.kem` — `mrs CNTPCT_EL0`). Kaynak yorumları bunu "C8'in işi"
olarak işaretliyor. **P1 olmadan HİÇBİR subsystem'in irreducible asm'i .kem olamaz** → en yüksek kaldıraç,
mutlak önkoşul. Gate: `asm_arm64_ret.kem` aarch64 hedefinde `mrs` emit + QEMU'da doğru değer okur.

### FAZ 1 — virtio (EN KOLAY) — P1 + P2, PARALEL-GÜVENLİ
Ayrı dosya, builtin+MMIO arabirimi, sıfır bağımlılık (polling, IRQ/alloc yok). P2 (aligned16) + dsb (P1)
eklenince mevcut `.kem` discovery sürücüsü tam virtqueue'ya genişler. Kendi golden-output gate'i
(virtio-blk oku/yaz + net RX/TX QEMU).

### FAZ 2 — mmu — P1 + P2, PARALEL-GÜVENLİ
Self-contained (cross-call yok) → virtio ile paralel gidebilir. %90 zaten düz bellek. Kalan: 4 sysreg
commit + bariyer + TLBI (P1) + aligned4096 tablo (P2). Gate: MMU-on boot ([1..5] korunur).

### FAZ 3 — zaman + kesme + görev (SERİ küme) — P1 [+ P3]
Bu üçü birbirinin builtin-OLMAYAN fonksiyonunu cross-call eder (kesme→görev/zaman; zaman→görev preempt).
Ya **TEK .kem dosyada** birleştir (kem_heap.kem stili) ya da **P3** (cross-file çıplak declare) ile
paralelleştir. Context-switch + vektör-stub'lar **.S kalır** (P4 almadıkça). kesme en büyük/en coupled →
**en son**. **ÖNKOŞUL:** kem_os'ta kesme/timer/scheduler'ı önce AKTİFLEŞTİR (şu an dormant) — bu fazın
gerçek yükü buradadır, saf port değil.

**Özet DAG:**
```
P1 (FAZ 0, zorunlu kapı)
 ├─► virtio   (P1+P2)  ┐  paralel
 ├─► mmu      (P1+P2)  ┘  (ayrı dosya, bağımsız)
 └─► [zaman + kesme + görev]  (P1 [+P3]) — SERİ küme, coupled; kem_os'ta önce AKTİFLEŞTİR
```

---

## 4. Dürüst Belirsizlikler / Bayraklar

- **Context-switch irreducible asm.** `kdl_baglam_degis` (SP-swap + `ret`-to-diff-context) ve IRQ
  trap-frame (`eret` + tüm-reg) **zaten `boot/start_aarch64.S`'de** (C değil). P1 ile bile bunlar
  normal bir `.kem` fonksiyonu olamaz (bölge prologue/epilogue çakışır) — .S kalır VEYA P4 (naked)
  gerekir. Yasa-4 "sıfır **C**" olduğundan .S'nin kalması muhtemelen kabul edilebilir; **Mehmet kararı**.
- **P2 hizalama tasarımı net değil.** Bölge-tabanlı over-alignment mı yeter yoksa gerçek bir
  `hizala(N)` attribute'u mu gerek — tasarım kararı (küresel statik ring'ler için bölge yolu belirsiz).
- **"Göç" ≠ salt port.** kem_os bu 5 subsystem'i BUGÜN kullanmıyor (dormant/boot-asm/başka-kernel).
  Gerçek final-faz = subsystem'leri kem_os'ta **aktifleştir** (canlı interrupt/timer/scheduler/virtio)
  + saf-.kem yap. Aktivasyon yükü mekanik port'tan büyük olabilir — özellikle kesme/görev/zaman.
- **x86_64 tarafı kapsam-dışı bırakıldı** bu analizde (kdl_mmu.c x86 paging boot'ta; timer/kesme x86
  dalları port/PIC/IDT). aarch64 birincil hedef; x86 ayrı P1-x86 (port I/O `outb`, `lidt`, `sti/hlt`)
  gerektirir — aynı arch-gate deseni.

---

## 5. Ampirik Kanıt Özeti (bu keşifte gerçekten koşturulan)

| İddia | Kanıt |
|-------|-------|
| aarch64 inline-asm bloklu | `asm_arm64_ret.kem` → `AS001 … hedef: x86_64`, rc=1 |
| x86 inline-asm emit çalışıyor | `asm_round_trip.kem` → `call {i32,i32} asm sideeffect …` |
| arch sabit-kodlu | `src/llvm.h:38` `KEMGU_HEDEF_MIMARI "x86_64"`; llvm.c AS001 memcmp |
| cross-file builtin-olmayan çağrı bloklu | çıplak `malloc(n)` → `T002 tanimsiz sembol` |
| cross-file builtin çağrı çalışıyor | mevcut kem_os = kem_os.o + kem_heap.o + N obje, QEMU boot ✓ (D-268) |
| timer = sysreg (MMIO değil) | kdl_zaman.c:58-64 `mrs cntfrq_el0` / `msr cntv_tval_el0` |
| mmu %90 düz bellek | kdl_mmu.c:38-57 tablo kurma; asm yalnız 59-68/136-137 (commit+bariyer) |
| context-switch .S'de | `boot/start_aarch64.S:347` `kdl_baglam_degis`; kdl_gorev.c:28 extern |
| virtio IRQ-suz polling | kdl_virtio.c used.idx spin; sıfır kesme çağrısı |
| 5/5 struct-return yok | tüm C fn'ler void/int/uint64_t/ptr döner (D-268 mayını yok) |
| kem_os bu 5'i çağırmıyor | 5 okuyucu: kem_os.kem'de sıfır call-site (boot-asm/dormant/başka-kernel) |
