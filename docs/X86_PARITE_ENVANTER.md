# x86_64 Parite Envanteri — KEMGU-OS saf-`.kem` runtime

> **GÜNCEL DURUM (2026-10-12):** §7'deki kuyruğun 1-7. adımlarının x86 tarafı
> TAMAMLANDI (D-670…D-674, `os/x86-parite`). Kalan parite borcu: **4 dosyada 47
> gerçek `satıriçi_asm` bloğu** (§6 — sayım DÜZELTİLDİ). Kapı onarımı D-675,
> temizlik D-676. Ayrıntı ve kanıtlar §0.
>
> **İlk durum:** salt-okunur envanter (2026-10-11). Kod yazılmadı. Her iddia ya
> kaynaktan okundu ya da bu makinede ölçüldü; ölçülenler **(ölçüldü)** diye
> işaretli. Kapsam: `runtime/kem_heap.kem`, `runtime/kem_virtio_blk.kem`,
> `runtime/kem_minifs.kem`, `runtime/kem_virtio_net.kem`, `boot/*.S`.
> Kapsam dışı ama zorunlu borç ayrı başlıkta (§6).

---

## 0. Tamamlanan işler (2026-10-12)

Her biri taze klonda `otomasyon/bringup/gate_linux.sh` ile kapılandı:
aarch64 QEMU [1..10] regresyonsuz · x86 işlevsel kanıt (`calistir_kem_os_x86`,
gerçek objdump + QEMU değer denetimi) · `test_tumu` · FIXPOINT stage1 == stage2
(85312 satır) birebir. Hepsinde sabotaj kırmızı verdi.

| | İş | D | Kanıt (özet) |
|---|---|---|---|
| ✅ | UYARI 1 — x86 boot `kem_heap_kur` çağırıyor | D-670 | öncesi QEMU `0` (sessiz), sonrası `55`; objdump `callq <kem_heap_kur>` |
| ✅ | §4 D2b — taşınabilir DMA bariyeri (`runtime/mimari/<arch>/kem_bariyer.kem`) | D-671 | x86 `mfence` çalıştı (`X86 DMA BARIYER OK`), arm64 objdump `dsb sy` |
| ✅ | UYARI 2 — mimari başına UART, `kem_panik` x86 COM1 yolu | D-672 | QEMU `X86 PANIK YOLU OK`; `kem_uart_bayt` içinde gerçek `inb`/`outb` |
| ✅ | §3.2/3.4/3.5 — DMA haritası mimari başına + x86 3-4 GB UC MMIO | D-673 | DMA yaz/oku birebir; LAPIC sürüm `0x50014`; `movl $0xc000009b`; eski taban → #PF @ `0x43000110` |
| ✅ | §5 — virtio-mmio microvm (`0xFEB00000`, 24 yuva) + `force-legacy=false` | D-674 | sektör 7 yaz→sıfırla→oku 512 B birebir (imaj ana makineden de okundu); `mac_w0 0x12005452` |

Seçilen yol: §4'teki **D2b** (mimari başına dosya + Makefile CAT seçimi) —
derleyiciye (`src/*.c`, `selfhost/*`) DOKUNULMADI. D1 (bare-metal x86 üçlüsü)
ve D3 (yerleşik bariyer) hâlâ öneri; gerekmediler.

---

## ⚠ UYARI 1 — SESSİZ ÇÖKME: x86 boot'u allocator'ı hiç kurmuyor — ✅ D-670

`boot/start_aarch64.S:89-98` `main`'den önce `bl kem_heap_kur(__heap_start,
__heap_end)` çağırır. **`boot/start_x86_64.S` bunu ÇAĞIRMAZ** (`grep
kem_heap_kur boot/start_x86_64.S` → 0). Bugün x86 kernel'leri C
`kdl_bare_heap.c`'yi bağladığı için (lazy `kdl_heap_init`) görünmüyor; saf-`.kem`
allocator x86'ya taşındığı an:

```
kem_bump = 0, kem_son = 0           (küresel başlangıç değerleri)
malloc(n): serbest liste boş → bump: p + toplam > kem_son (0) → ver 0   ← HER çağrı null
kdl_bolge_olustur → malloc(16) = 0 → ver 0
kdl_bolge_ayir(0, n)    → ver 0            (null korumalı, sessiz)
kdl_dizi_olustur(0, 4)  → ver 0
kdl_dizi_ekle_tam(.., 0, x) → ver;         (no-op)
kdl_dizi_al_tam(0, i)   → ver 0            ← "doğru görünen" sıfır
kdl_dizi_boyut(0)       → ver 0
```

Zincirin **hiçbir halkası gürültülü değil**: her null-korumalı yol sessizce 0
döner, program çökmez, yanlış değerle sürer.

**D-264 ile aynı sınıf.** D-264'te kapı `KEM-OS OK` marker'ını gördü ve geçti,
oysa çıktı bozuktu (*"gate marker-PASS ama çıktı BOZUK olabilir"*). Burada da bir
"boot oldu + marker basıldı" kapısı bu tuzağı **yakalayamaz**: marker'ı basan kod
allocator'a dokunmuyorsa yeşil kalır, dokunuyorsa sıfırlarla devam eder.
**Ön koşul:** x86 kem_os kapısı ilk günden en az bir tahsis sonucunu DEĞERİYLE
denetlemeli (ör. `[2] HEAP/55` fazının x86 ikizi, beklenen toplam ile).

## ⚠ UYARI 2 — PANİK SESSİZLİĞİ: ilk hata hem sessiz hem teşhissiz — ✅ D-672

`kem_panik` (`kem_heap.kem:441-450`) mesajı `kdl_mmio_yaz32(150994944, …)` ile
yazar: `150994944 = 0x09000000` = QEMU aarch64 `virt` PL011 UART0. x86'da o
adreste UART **yok**:

- x86 `pc` makinesinde `0x09000000` (144 MB) ya RAM'dir (`-m` ≥ 160M ise →
  panik mesajı **RAM'i sessizce bozar**) ya da arka plansız adrestir (yazma
  yutulur). İkisinde de konsola hiçbir şey çıkmaz.
- Ardından `iken doğru { }` → kernel donar, **mesajsız**.

`kem_panik`i çağıranlar: kanal sınır ihlali (`kem_kanal_hucre`) ve görev
(D-624). Yani x86'da UYARI 1'in tetiklediği ilk gerçek hata, **teşhis
kanalının kendisi** ölü olduğu için zaman aşımından başka iz bırakmaz. x86
karşılığı COM1 port-I/O'dur: `outb %al, %dx` @ `0x3F8` (mevcut C referansı:
`runtime/kdl_runtime_uart_16550.c:39,130`; `.kem` çıplak + `satıriçi_asm`
biçiminde derlenip gerçek `out` komutuna indiği **ölçüldü**).

---

## 3. Dosya başına tablo

### 3.1 `runtime/kem_heap.kem` (498 satır, 31 `çıplak işlev`)

| | Bulgu | x86_64 durumu |
|---|---|---|
| **(a) `satıriçi_asm`** | **YOK** | IR iki mimaride `target triple` ve kullanılmayan bir `declare @llvm.x86.sse2.lfence` dışında **birebir aynı** (ölçüldü). |
| **(b) sabit adres** | `kem_panik`: `150994944` (PL011 `0x09000000`) ×2 | **Mimariye bağlı** → UYARI 2. COM1 port-I/O'ya geçmeli. |
| | `kdl_mmio_oku32/yaz32`: adres argümanla gelir | Mimariden bağımsız, ama x86'da MMIO'nun **UC** eşlenmesi gerekir (§3.5). |
| | Heap penceresi: `kem_heap_kur(taban, son)` linker sembolünden | Mimariden bağımsız; x86 boot çağırmıyor → UYARI 1. |
| **(c) hizalama** | `malloc` başlık 16 B + `((n+31)/16)*16`; bölge bump `((x+15)/16)*16` | x86_64 SysV `alignof(max_align_t) = 16` → AAPCS64 ile aynı, **değişiklik gerekmez**. |
| | `KdlDizi` {ptr@0, i32@8, i32@12, i32@16} 24 B; `KdlBolge`/`KdlBolgeBlok` offset'leri; yetki 16 B {i64@0,i16@8,i16@10,i8@12} | Doğal hizalı; x86_64 SysV yerleşimi aynı. Yetki **açık OUT-PTR** (D-268) → sret/dönüş-yazmacı farkından etkilenmez. |
| | Kanal hücreleri 8 B | Değişiklik gerekmez. |

### 3.2 `runtime/kem_virtio_blk.kem` (173 satır)

| | Bulgu | x86_64 durumu |
|---|---|---|
| **(a) `satıriçi_asm`** | 1 blok: `vblk_dsb()` → `mimari: arm64` `dsb sy` + `~{memory}`. Çağrı yeri: `vblk_gonder` içinde **3**; `kem_virtio_net` içinde **5** (toplam 8). | **AS001 ile derlemeyi DURDURUR** (`--mimari x86_64`, ölçüldü). Karşılık: `mfence` + `~{memory}` (güvenli tercih). x86-TSO'da mağazalar sırayla görünür; tutarlı WB DMA RAM için derleyici bariyeri (`""` + `~{memory}`) asgari, notify öncesi tam sıralama için `mfence`. |
| **(b) sabit adres** | `vblk_dma = 1124073472` (`0x43000000`): "identity-map RAM, EL0 üstü" | aarch64 `virt` RAM'i `0x40000000`'da başlar → bu adres "RAM + 48 MB". x86'da RAM 0'dan başlar ve boot **yalnız 0..1 GB'ı** eşler (512 × 2 MB, `start_x86_64.S:47-56`) → `0x43000000` **eşlemenin DIŞINDA** = sayfa hatası. Ayrıca QEMU x86 varsayılan `-m 128M` → arka plansız. |
| | Tarama tabanı `167772160` (`0x0a000000`), 32 yuva × `0x200` | **aarch64 `virt`'e özgü.** x86 `pc` makinesinde virtio-mmio **YOK** (mtree'de 0, ölçüldü). Yalnız `-M microvm`: `0xFEB00000`'dan, `0x200` adımlı **24 yuva** (ölçüldü). |
| | Register offset'leri (VirtIO-MMIO v2) | Taşıma-bağımsız (virtio spesifikasyonu). Ancak QEMU'da virtio-mmio varsayılanı **`force-legacy = true`** (v1) — hem aarch64 `virt` (32 yuva) hem x86 `microvm` (24 yuva) ölçüldü. Sürücü `VERSION != 2 → -1`. aarch64 Makefile bunu `-global virtio-mmio.force-legacy=false` ile çözüyor; x86 hedefi de aynı bayrağı istemeli. |
| **(c) hizalama** | DMA alt-tampon offset'leri `0/128/160/256/272/784`, hepsi 16'nın katı (D-271 "P2 manuel over-align") | virtio split-ring şartı (desc 16, avail 2, used 4) **mimariden bağımsız** → x86'da da yeterli. ⚠ virtio-**PCI legacy**'ye geçilirse kuyruk **4096-hizalı** olmalı; bu düzen yetmez. |

### 3.3 `runtime/kem_minifs.kem` (74 satır)

| | Bulgu | x86_64 durumu |
|---|---|---|
| **(a) `satıriçi_asm`** | YOK | — |
| **(b) sabit adres** | YOK (yalnız `vblk_*` üzerinden) | Kendi başına mimariden bağımsız; **geçişli olarak** `kem_virtio_blk`'in tüm sorunlarını miras alır. Tek başına derlemede T002 (vblk sembolleri) — CAT birimi gerekir (tasarım gereği). |
| **(c) hizalama** | 512 B sektör, bayt düzeyi LE serileştirme (`*256`, `*65536`) | Endian varsayımı açık ve elle yapılmış → değişiklik gerekmez. |

### 3.4 `runtime/kem_virtio_net.kem` (185 satır)

| | Bulgu | x86_64 durumu |
|---|---|---|
| **(a) `satıriçi_asm`** | Kendi bloğu YOK; `vblk_dsb()` **5** çağrı yeri (kur 1, gönder 3, al 1) | `vblk_dsb` düzelince otomatik düzelir; CAT sırası (blk önce) korunmalı. |
| **(b) sabit adres** | `vnet_dma = 1182793728` (`0x46800000`) — dosyanın kendi notu: "AH-1'de DTB'yi EZDİĞİ için taşındı; SINIF OLARAK KALICI ÇÖZÜM DEĞİL" | x86'da da 1 GB eşlemenin DIŞINDA. |
| | Tarama tabanı `0x0a000000` ×32 | blk ile aynı (§3.2). |
| | MAC config `base + 0x100` | virtio-**mmio**'ya özgü; virtio-PCI'da config bir capability ile bulunur. |
| **(c) hizalama** | RX ring `0/128/160`, RX tamponları `256 + i*2048`, TX `16640/16768/16800/16896` — hepsi 16'nın katı | Yeterli (§3.2 ile aynı gerekçe). |

### 3.5 `boot/start_aarch64.S` (415) ↔ `boot/start_x86_64.S` (278)

| İşlev | aarch64 | x86_64 | Boşluk |
|---|---|---|---|
| Firmware boot bilgisini sakla | `x0` → `kdl_dtb_adres` (AH-1) | **YOK** — PVH ABI'de `%ebx` = `hvm_start_info` fiziksel adresi; dosyada `ebx` hiç geçmiyor | Bellek haritası/cihaz keşfi imkânsız; her adres sabit kalmak zorunda. |
| FP/SIMD aç | `cptr_el2.TFP`, `cpacr_el1.FPEN` | **YOK** (CR4.OSFXSR/OSXMMEXCPT kurulmuyor); C `-mgeneral-regs-only` ile derleniyor | `.kem` IR'ı float/`vektör` içerirse x86'da derleme reddi ya da `#UD`. |
| Vektör tablosu | `vbar_el1/el2` ← `kdl_vektor_tablosu` (16×0x80) | IDT C'de (`kdl_idt_kur`), 32 ISR stub + `kdl_isr_tablo` | Var. |
| BSS sıfırla | var | var (`rep stosb`) | — |
| MMU | `bl kdl_mmu_kur` (identity, RAM Normal-WB, cihaz Device) | Sayfalama boot'ta: **0..1 GB, 2 MB sayfa, bayrak `0x83`** (present\|write\|PS, **PCD/PWT YOK = WB önbellekli**) | 1 GB üstü (`0x43000000`, `0x46800000`, microvm `0xFEB00000`) eşlenmemiş; eşlenenler de **önbellekli** → MMIO için UC (PCD\|PWT) ayrı eşleme gerekir. |
| `kem_heap_kur` | `bl kem_heap_kur` | **YOK** | → UYARI 1. |
| Fault kurtarma + EL0 kill | `kdl_fault_bekleniyor/_yakalanan`, `kdl_el0_kill_aktif` `.data`'da; `esr/far/elr` EL-duyarlı | ✅ D-687 `kdl_x86_pf_bekle` (#PF, RIP += uzunluk; D-689 ret taklidi) + **D-699** ring3 istisnasında çekirdek bağlamına dönüş (EL0 kill ikizi: kullanıcı çerçevesi atılır) | Beklenmeyen çekirdek fault'u yine `kdl_istisna_isle` (D-690). |
| Kullanıcı moduna in | `kdl_el0_calistir` (`eret`, SPSR=EL0t) | ✅ **D-699** `kdl_x86_ring3_gir` (`iretq`, CS=0x1B / SS=0x23, RFLAGS IF=0; TSS.RSP0 = giriş yığını) + `isr_ortak` ring3 dönüşü (bekle bayrağı **ve** kesilen CS RPL 3 → `kdl_x86_r3_don`); GDT `.data`'da (0x18/0x20 DPL3, 0x28 TSS); IDT 0x80 kapısı DPL3 (`0xEE`) | Mantık `.kem`: `runtime/mimari/x86_64/kem_ring3_hw.kem` (TSS tanımlayıcısı + `ltr`, U/S her tablo seviyesinde, kullanıcı kodu). Kanıt 26 `X86 RING3 OK`. |
| Syscall | SVC yolu, frame önce kaydedilir (D-126) | `kdl_syscall_stub` (`int 0x80`, `rax`=num) | ABI farklı (yazmaç eşlemesi) — kem tarafı ayrı katmanlı olmalı. |
| IRQ / preempt | `kdl_irq_ortak` 272 B tam frame + SP swap | `kdl_irq0_stub` yalnız caller-saved + PIC EOI, **SP swap yok** | Preempt `.S`'te yok; tek örneği test kernel'inin kendi C+inline-asm stub'ı (`test/bare_metal/preempt_x86.c`). |
| Bağlam değiştir | x19-x30 + sp | rbx,rbp,r12-r15 + rsp | Var, yerleşimler farklı (`kem_gorev` TCB düzeni buna bağlı). |
| Durma | `wfe` döngüsü | `hlt` döngüsü | — |

**(a) asm bloğu:** `.S` dosyaları zaten mimariye özgü; `satıriçi_asm` içermez.
**(b) sabit adres:** aarch64 yükleme `0x40000000` (`linker/bare-metal-aarch64.ld`),
EL0 `0x42000000`; x86 yükleme `0x100000`. İkisinde de 16 KB yığın + 16 MB heap.
**(c) hizalama:** aarch64 vektör tablosu `0x800` hizalı (VBAR şartı), frame'ler
16'nın katı (`sub sp, #(16*10)`, `#272`); x86 sayfa tabloları `4096` hizalı,
GDT 8. İkisinde de mimarinin kendi şartları — `.kem` tarafına sızan bir
16-bayt varsayımı yok.

---

## 4. `--mimari` (D-269) neden yetmiyor

D-269 bayrağı bilinçli olarak **dar** tasarlandı: yalnız (1) AS001 asm etiket
kontrolü, (2) yayılan `target triple`. Kendi kaydı: *"Bu primitif subsystem
göçünü AÇAR ama göç YAPMAZ."* Ölçülenler:

1. **AS001 sert hatadır, seçici değildir.** `kem_virtio_blk.kem` `--mimari
   x86_64` ile **rc=1** (`hata[AS001] … satirici_asm mimari etiketi hedef
   mimariyle uyusmuyor`). Aynı dosyada iki mimarinin bloğunu yan yana tutup
   hedefe göre birini seçen bir mekanizma YOK; dilde `hedef_mimari()` benzeri
   bir yerleşik ya da koşullu derleme YOK (`hedef_mimari` yalnız C içi
   `llvm_hedef_mimari()`).
2. **Üçlü bare-metal'e uygun değil.** `--mimari x86_64` →
   `x86_64-pc-windows-gnu` (`src/ana.c:704`, `selfhost/codegen.kem:14563`).
   Bugün x86 kernel hedefleri `kemgu`'yu **bayraksız** çağırıp
   `clang -target x86_64-unknown-none -Wno-override-module` ile üçlüyü ezerek
   çalışıyor (`Makefile:1896-1900`). Modül üçlüsüne saygı duyan herhangi bir araç
   (doğrudan `llc`, LTO) **Win64 çağrı sözleşmesi** üretir; `start_x86_64.S`
   ise SysV (`rdi/rsi/rdx`) varsayar → `kdl_istisna_isle`, `kdl_syscall_isle`
   argümanları yanlış yazmaçlardan okunur.
3. **Asm dışında IR mimariden bağımsız** (ölçüldü: `kem_heap`, virtio birleşik
   birim — fark yalnız üçlü ve kullanılmayan `lfence` declare'i). Yani ABI/
   datalayout tarafında yeni iş GEREKMİYOR; eksik olan **seçim**dir.

### Gereken derleyici işi (öneri, uygulanmadı)

| # | İş | Gerekçe |
|---|---|---|
| D1 | `--mimari x86_64-none` (ya da `--hedef bare-metal`) → `x86_64-unknown-none-elf`; aarch64'teki `aarch64-unknown-none-elf` ikizi. C + self-host paritesiyle (D-407). | Win64 sözleşme riskini kaldırır; `-Wno-override-module`'a güvenmeyi bitirir. |
| D2 | **Mimari seçimli asm:** ya (a) AS001'i "etiket uyuşmazsa bloğu ATLA, hiçbir blok seçilmezse hata" kuralına çevirmek, ya da (b) `vblk_dsb` gibi yardımcıları mimari başına ayrı dosyaya (`kem_bariyer_arm64.kem` / `_x86_64.kem`) bölüp Makefile CAT listesinde seçmek. | (b) derleyici değişikliği istemez ve bugünkü CAT deseniyle uyumludur; (a) tek dosyada parite sağlar ama sessiz atlama riski taşır — "hiç blok yayılmadı" durumu GÜRÜLTÜLÜ olmalı (D-416'nın sessiz-düşen-asm dersi). |
| D3 | **Taşınabilir DMA bariyeri yerleşiği** (ör. `dma_bariyeri()`): arm64 → `dsb sy`, x86 → `mfence`. | Emsal VAR: D-468 `sabitsüre` bariyerini mimariye göre (`csdb`/`lfence`) yayıyor ve `ct_bariyer` kapısı her iki mimaride sayıyor. Aynı desen + aynı türden yapısal kapı. Sürücülerden `satıriçi_asm`'yi tamamen kaldırır. |
| D4 | **Platform sabitleri modülü** (ör. Makefile'ın ürettiği `hedef_platform.kem`: UART türü/adresi, virtio tarama tabanı/yuva sayısı, DMA tabanı). | KEMGU'da önişlemci yok; `selfhost/konak.kem` (D-469 onarımı) tam bu deseni kullanıyor. Sabit kodlu `0x09000000`/`0x0a000000`/`0x43000000`'ı tek yere toplar. |
| D5 | Port-I/O yolu: ya `satıriçi_asm` (yeterli — ölçüldü) ya da `port_yaz8/port_oku8` yerleşiği. | Asm yolu bugün çalışıyor; yerleşik yalnız D3 gibi tek kapıdan denetim isteniyorsa. |

---

## 5. virtio-mmio farkı (özet)

| | aarch64 QEMU `virt` | x86 QEMU `pc` (varsayılan) | x86 QEMU `microvm` |
|---|---|---|---|
| virtio-mmio | `0x0a000000`, 32 × `0x200` | **YOK** (ölçüldü) | `0xFEB00000`, 24 × `0x200` (ölçüldü) |
| Varsayılan sürüm | legacy (`force-legacy=true`, ölçüldü) → Makefile `=false` geçiyor | — | legacy (`force-legacy=true`, ölçüldü) → aynı bayrak gerekli |
| DMA `0x43000000` / `0x46800000` | RAM + 48 / 104 MB, eşlenmiş | 1 GB eşlemenin dışında | 1 GB eşlemenin dışında; `0xFEB00000` da eşlenmemiş |
| Seri konsol | PL011 MMIO `0x09000000` | 16550 port-I/O `0x3F8` | 16550 port-I/O |

**Karar noktası (Mehmet):** x86 yolu (i) `microvm` + mevcut virtio-mmio
sürücüsü (en az iş: tarama tabanı + bariyer + eşleme), ya da (ii) `pc`/`q35`
+ yeni virtio-PCI sürücüsü (gerçek donanıma daha yakın; `pci_enum_test_x86`
C'de var, `.kem`'de yok). (i) parite için yeterli, (ii) ayrı bir sürücü
projesidir.

---

## 6. Kapsam dışı ama zorunlu: parite borcunun gerçek boyutu

kem_os CAT birimi bu envanterin dört dosyasından fazlasını içerir.

⚠ **SAYIM DÜZELTMESİ (2026-10-12):** ilk sürümdeki sayılar `grep -c
'satıriçi_asm'` ile alınmıştı ve **yorum satırlarını da sayıyordu** (28/14/7/1,
"51"). Gerçek blok = yorum ve ham dizgi dışındaki `satıriçi_asm {` açılışı.
Sağlama: aşağıdaki aarch64 blokları (46 + 1 + 1) = **48** = birleşik OS IR'ındaki
`asm sideeffect` sayısı = `baremetal_diff` "48 satıriçi asm". Üç bağımsız ölçüm
birebir.

| Dosya | gerçek asm blok | Not |
|---|---|---|
| `runtime/kem_gorev.kem` | ~~27~~ **25** | ✅ D-684/685 — preemptive zamanlayıcı çekirdeği mimariden bağımsız (`kem_gorev_cekirdek.kem`), arm64 kancaları + 2 `adrp` `mimari/arm64/kem_gorev_hw.kem`'de; x86 preempt GERÇEK (iki görev, SP takaslı IRQ0 koçanı). Kalan 25: EL0/`kul_*`/MB-1b/trampolin → ring3 fazı. **D-699:** x86 ring3 girişi/dönüşü + ring3 `int 0x80` yazıldı (`mimari/x86_64/kem_ring3_hw.kem`, 5 x86 asm); arm EL0 blokları arm donanımına özgü kalır |
| `runtime/kem_mmu.kem` | **14** | Sayfa tablosu, TLBI, MSR/MRS, fault — x86 sayfalama modeli bambaşka |
| `runtime/kem_zaman.kem` | ~~5~~ **0** | ✅ D-683 — donanım `runtime/mimari/<arch>/kem_zaman_hw.kem`'de (arm64 GIC/CNTV 5 blok; x86 PIC+PIT `.kem`, gerçek IRQ0 tik). Açık: x86'da preempt kancası yok (kem_gorev ile) |
| `runtime/kem_dtb.kem` | ~~1~~ **0** | ✅ D-682 — `kdtb_adres` `runtime/mimari/<arch>/kem_kesif.kem`'e taşındı (arm64 1 blok; x86 → 0, PVH keşfi D-678) |
| `runtime/mimari/arm64/kem_bariyer.kem` | 1 | ✅ D-671 — x86 ikizi var (`mfence`) |
| `runtime/mimari/arm64/kem_uart.kem` | 0 | ✅ D-672 — PL011 MMIO (asm yok); x86 ikizi 2 blok (`inb`/`outb`) |
| `runtime/kem_heap/virtio_blk/virtio_net/minifs/elf.kem` | 0 | mimariden bağımsız (D-671…D-674 sonrası) |
| `test/ornekler/kem_os.kem` | 0 | Ama fazları aarch64 davranışını ölçüyor |

**KALAN PARİTE BORCU: ~~47~~ → ~~46~~ → ~~41~~ → ~~39~~ → 35 gerçek blok (D-694: kem_gorev 21 EL0/ring3 + kem_mmu 14 — kem_mmu'nun x86 KARŞILIKLARI D-687…D-691'de yazıldı, arm blokları arm donanımına özgü kalır) (D-685/D-686 sonrası — D-686 asm eklemedi, servisleri taşıdı: `kem_gorev` 25 ring3/EL0, `kem_mmu` 14)** — `kem_gorev` 27 +
`kem_mmu` 14 + `kem_zaman` 5 + `kem_dtb` 1. (İlk üçü kapsam dışı tutuldu ve
ayrıca konuşulacak; `kem_dtb` envanterin ilk sürümünde "üç dosya"ya
sayılmamıştı ama aarch64'e özgüdür.)

---

## 7. Önerilen iş sırası (bağımlılık sırasına göre) — durum 2026-10-12

| # | Adım | Durum |
|---|---|---|
| 1 | Gözlemlenebilirlik — x86 COM1 çıplak UART + `kem_panik` x86 yolu | ✅ D-672 |
| 2 | Boot yükümlülükleri — `kem_heap_kur` + değer denetleyen heap fazı | ✅ D-670 |
| 2b | PVH `%ebx` (`start_info`) saklama | ✅ D-678 — `kdl_pvh_bilgi` (.data); magic + memmap okunuyor (QEMU: 127 MB RAM) |
| 3 | D1 — bare-metal x86 üçlüsü (`x86_64-unknown-none-elf`) | ❌ **YAPILMADI** — `clang -target … -Wno-override-module` geçersiz kılması bugün yetiyor; Win64 sözleşme riski (§4.2) kayıtlı duruyor. Derleyici işi → karar senin |
| 4 | DMA bariyeri | ✅ D-671 — **D2b** (mimari başına dosya), D3 yerleşiği DEĞİL |
| 5 | Platform sabitleri | ✅ kısmi D-673/D-674 — `runtime/mimari/<arch>/kem_platform.kem` (DMA + virtio tabanı/yuva) el yazımı; UART ayrı dosya (`kem_uart.kem`). Makefile'ın ürettiği tek modül (D4 biçimi) değil |
| 6 | x86 sayfalama — UC MMIO penceresi + DMA ilk 1 GB içinde | ✅ D-673 |
| 6b | DMA tabanını `start_info` haritasından türet | ✅ D-679 — `kem_platform_kur()`: 1 GB altı en yüksek RAM tepesi − 256 KB (QEMU: 130804 KB; RAM `0x7FFD000`'de biter). PVH yoksa sabit yedek |
| 7 | microvm + `force-legacy=false` → virtio-blk / minifs / virtio-net | ✅ D-674 + D-677 (minifs: format→yaz→oku, toplam 5071) |
| 8 | **Kapsam dışı ağır borç**: `kem_zaman` (5) → `kem_mmu` (14) → `kem_gorev` (27); + `kem_dtb` (1) | 🟡 **KISMEN — 39 blok kaldı** (D-683 kem_zaman 0, D-682 kem_dtb 0, D-684/D-685 preempt çekirdeği ortak + x86 SP takası, D-686 çekirdek modu syscall ortak servisler + x86 `int 0x80`; D-687 x86 MMU ilk adım: `.kem` 4KB sayfa tablosu (alias çeviri) + kurtarılabilir #PF (CR2) — `runtime/mimari/x86_64/kem_mmu_hw.kem`, 7 x86 asm bloğu; D-688 süreç başına adres alanı (PML4+PDPT+PD+PT kopyası, CR3 değişimi, izolasyon kanıtı); D-689 W^X: EFER.NXE + NX biti, veri sayfasını çalıştırmak komut getirme #PF (hata 17); D-690 kurtarılamaz istisna raporu `.kem` (`kdl_istisna_isle` güçlü, C zayıfı ezilir; ikinci QEMU koşumu `-append kem_istisna` ile gerçek #PF); D-691 çekirdek W^X (0..2 MB 4KB PT: .text+.rodata RO, geri kalan RW+NX, CR0.WP; kendi koduna yazma #PF hata 3); D-692 mimariden bağımsız algoritma fazları [25][26][29..40] ortak dosyaya taşındı → x86'da 14 faz aynı kodla koşuyor; D-693 [27] görev kapasitesi + [41] kanal ortak parça dosyasına taşındı → x86'da da ; D-694 dil görevi (görev_başlat/birleştir) ortak çekirdekte, mimari kancaları (IRQ, çağrı, trampolin adresi, arg0 ofseti) arm64 + x86 → [42] x86'da da; `kem_gorev.kem` asm 25 → 21; kalan: `kem_gorev` 25 ring3/EL0 + `kem_mmu` 14 — ring3 fazı kullanıcı kararı); D-699 x86 ring3: GDT DPL3 kod/veri + 64-bit TSS (RSP0) + IDT 0x80 DPL3, süreç adres alanı #3 özel sayfası U/S ile, `iretq` → ring3 `int 0x80` getpid (çağıran CS 0x1B) → `hlt` #GP ve gözetmen sayfa okuması #PF (hata 5) ile çekirdeğe kontrollü dönüş — Kanıt 26 `X86 RING3 OK`. `kem_gorev`, `kem_mmu`'ya (adres alanı) ve `kem_zaman`'a (preempt tik) bağlıdır; `kem_mmu` ↔ `kem_zaman` karşılıklı bağımlı (docs/BAREMETAL_DIFF_KAPSAM.md) |
| 9 | Parite kapısı | ✅ kısmi — `calistir_kem_os_x86` (Kanıt 1-10, değer denetimli) + `check_kapisi` birleşik x86 birimi. ✅ D-680 `baremetal_diff` x86 ikizi: birleşik x86 birimi C ↔ self-host (72 işlev, 4 asm şablon+kısıt, 79 volatile, 83 inttoptr) birebir; parça listesi Makefile'dan okunur. x86 birimi `kem_os.kem`'i içermiyor (kanıt çekirdeği `kem_os_x86.kem`) |

---

### Ölçüm notları

- `kem_virtio_blk.kem --mimari x86_64` → rc=1, AS001 satır 65.
- `kem_heap.kem` arm64 ↔ x86_64 IR farkı: yalnız `target triple` +
  `declare void @llvm.x86.sse2.lfence()`.
- `virtio_blk + minifs + net` (asm satırı çıkarılmış) arm64 ↔ x86_64: yalnız
  üçlü farkı; 7 `volatile` erişim yardımcısı korunuyor.
- QEMU 8.2.2 `info mtree` / `info qtree`: `microvm` virtio-mmio `0xFEB00000`,
  24 yuva, `force-legacy = true`; `pc` virtio-mmio 0; `virt` 32 yuva
  `force-legacy = true`.
- Referans `çıplak` x86 UART işlevi: `define void @…(i32 %b)` (ρ yok),
  `call void asm sideeffect "outb %al, %dx", "{ax},{dx}"`, nesne kodda `ee
  outb %al,%dx`.

- **D-705 (2026-10-17):** kullanıcı-işaretçili sistem çağrıları (5 yaz, 17 dosya_yaz, 18 dosya_oku) + D-610 işaretçi doğrulaması `kem_gorev.kem`'den ortak çekirdeğe taşındı; kullanıcı penceresi mimari kancası (arm64 0x42000000, x86 0x54000000). x86 ring3'ten gerçek virtio diske yaz/oku = arm64 [13] ikizi (Kanıt 28).
- **D-712 (2026-10-17):** x86 ring3'ten sistem çağrısı 26 (UART satır okuma, D-706 ortak `kis_satir_oku` + 16550 kancaları) = arm64 [12] UART RX ikizi. Normal koşum EOF yolunu, ikinci koşum (`-chardev file,input-path=`) satır okuma + ring3 yankısını kanıtlar (Kanıt 29).
