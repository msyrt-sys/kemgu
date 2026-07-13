# ZERO_C_AUDIT — kem_os "yalnız .S" iddiasının link-düzeyi denetimi (Law-4)

> ## 🔄 GÜNCELLEME — ZERO-C KAMPANYASI FAZ 0 (D-282, `-ffunction-sections`+`--gc-sections`)
> Nihai `.text`: **0x8b64 (35 684 B) → 0x5444 (21 572 B)**. **CANLI-C: 14 888 B → 744 B (%95 düşüş).**
> `--gc-sections` (kem_os link) + `-ffunction-sections -fdata-sections` (BM_A64_CF) ile:
> - **3 ölü nesne** (heap_kemmalloc/bolge_kemregion/mmio_kem) düştü ✅
> - **Override-edilen ölü weak `kdl_syscall_isle` gövdesi** düştü → onun transitif çektiği
>   **virtio, virtio_net, mmu_kem, zaman_kem, panik TAMAMEN düştü** ✅ (beklenenden fazla kazanç).
> - **KALAN CANLI-C (7 fn, 744 B)** = `.S`→exception/izolasyon report zinciri:
>   `kesme`: `kdl_istisna_isle`, `kdl_el0_izolasyon_isle` (.S kökleri) →
>   `yazdir`: `kdl_yazdir_metin/satir/onaltilik` → `uart`: `kdl_uart_pl011_putc` ; `gorev`: `kdl_gorev_bitir`.
> - **KALAN C-data:** `kdl_fault_bekleniyor/yakalanan` + `kdl_el0_kill_aktif` (kesme.o, .bss/.data).
> - **Kapı:** [1..10]+[5] yeşil (gerçek fault/EL0), FIXPOINT birebir 33371, test_tumu exit 0. Taze-clone teyidi altta.
> - **Sıradaki (FAZ 2 DAG):** (B1) `kdl_istisna_isle`→çıplak .kem + .kem-UART ⇒ yazdir+uart düşer.
>   (B2) `kdl_el0_izolasyon_isle`+`kdl_el0_kill_aktif`→çıplak .kem ⇒ gorev düşer. (B3) fault-scratch
>   global'leri → `.S`-data (`.quad`). Sonra: nihai ELF `.text` = yalnız .kem + .S → lenient-Law-4 doruk.
>
> _(Aşağısı FAZ 0 ÖNCESİ ilk denetim — tarihsel referans; sayılar D-281 @d2bc787 durumu.)_

---


**Mod:** READ-ONLY denetim (kaynak/build değişmedi; yalnız `-Map` inceleme re-link + `nm`).
**Hedef:** aarch64 `kem_os.elf` nihai link'inin, indirgenemez `boot/*.S` DIŞINDA C-derlenmiş
kod içerip içermediğini NİHAİ-ELF sembol kökeniyle kanıtla/çürüt. (`@kdl_` IR-grep DEĞİL — o
yanıltıcı; metrik = link çözümü.)
**Branch:** `os/c1-region-backing` @ `d2bc787` (HEAD==origin). **Build:** `make clean` + temiz.
**ELF:** `build/kem_os.elf`, `-Map` re-link byte-identik (Map sadık).

---

## ⛔ KARAR: HAYIR — kem_os `boot/*.S` dışında C-SIZ DEĞİL.

Nihai ELF'in `.text`'i (0x8b64 = 35 684 B) **~%42 C-derilmiş kod** içerir. "Yalnız .S" iddiası
**ÇÜRÜK**. Bu, batch kapılarının (IR-grep `define/call @.kem`) ATLADIĞI gerçek: batch'ler her
alt-sistemin `.kem`-*tanımlı+çağrılı* olduğunu doğruladı, ama nihai ikilideki **residual C
nesnelerini** hiç denetlemedi.

### `.text` köken dağılımı (Map, bayt)
| Köken | Bayt | ~% | Nesneler |
|---|---|---|---|
| **.kem** | 17 268 | 48.4 | kem_os.o (15 424) + kem_heap.o (1 844) |
| **C (canlı)** | 14 808 | 41.5 | yazdir 5136, kesme 5052, gorev 1416, virtio_net 1228, virtio 1060, mmu_kem 448, panik 220, uart 140, zaman_kem 108 |
| **C (ölü-ama-var)** | 80 | 0.2 | bolge_kemregion 36, mmio_kem 32, heap_kemmalloc 12 |
| **.S (indirgenemez)** | 2 752 | 7.7 | bm_a64_start.o (_start + .text) |
| _(hizalama pad)_ | ~776 | 2.2 | inter-object align |

**Toolchain/libc:** YOK (`-nostdlib` temiz — "Libc sembol kontrol: yok" ✓). Beklendiği gibi.

---

## ADIM 1 — Link nesne envanteri (15 nesne)

Link satırı: `ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld -o kem_os.elf <15 .o>`
(**`--gc-sections` YOK** → ölü kod strip EDİLMEZ; kritik, aşağıya bak).

| Nesne | Kaynak | Sınıf | .text B | Durum |
|---|---|---|---|---|
| kem_os.o | kem_os_comb.kem | **.kem** | 15 424 | canlı |
| bm_a64_kem_heap.o | kem_heap.kem | **.kem** | 1 844 | canlı |
| bm_a64_start.o | boot/start_aarch64.S | **.S** | 2 752 | canlı (indirgenemez) |
| bm_a64_uart.o | kdl_uart.c | **C** | 140 | **canlı** |
| bm_a64_yazdir.o | kdl_yazdir.c | **C** | 5 136 | **canlı** |
| bm_a64_panik.o | kdl_panik.c | **C** | 220 | **canlı** |
| bm_a64_kesme.o | kdl_kesme.c | **C** | 5 052 | **canlı (kök)** |
| bm_a64_zaman_kem.o | kdl_zaman.c (-DKEM) | **C** | 108 | **canlı** |
| bm_a64_mmu_kem.o | kdl_mmu.c (-DKEM) | **C** | 448 | **canlı** |
| bm_a64_gorev.o | kdl_gorev.c | **C** | 1 416 | **canlı** |
| bm_a64_virtio.o | kdl_virtio.c | **C** | 1 060 | **canlı** |
| bm_a64_virtio_net.o | kdl_virtio_net.c | **C** | 1 228 | **canlı** |
| bm_a64_heap_kemmalloc.o | kdl_bare_heap.c (-DKEM) | **C** | 12 | **ölü** (0 ref) |
| bm_a64_bolge_kemregion.o | kdl_bolge*.c (-DKEM) | **C** | 36 | **ölü** (0 ref) |
| bm_a64_mmio_kem.o | kdl_mmio.c (-DKEM) | **C** | 32 | **ölü** (0 ref) |

→ **12 C-türevi nesne** linkte. 9 canlı, 3 ölü-ama-var.

---

## ADIM 2 — Nihai-ikili sembol kökeni (asıl test)

Referans grafiği: TÜM linkli nesnelerin `U` (tanımsız) sembolleri toplandı (52 benzersiz),
her C nesnesinin tanımladığı semboller bu kümede aranarak canlı/ölü belirlendi.

| C nesnesi | tanım | referanslı | durum |
|---|---|---|---|
| uart | 6 | 2 | CANLI |
| yazdir | 18 | 5 | CANLI |
| panik | 1 | 1 | CANLI |
| kesme | 18 | 5 | CANLI |
| zaman_kem | 5 | 1 | CANLI |
| mmu_kem | 4 | 2 | CANLI |
| gorev | 31 | 4 | CANLI |
| virtio | 10 | 2 | CANLI |
| virtio_net | 13 | 3 | CANLI |
| heap_kemmalloc | 1 | 0 | **ÖLÜ** |
| bolge_kemregion | 4 | 0 | **ÖLÜ** |
| mmio_kem | 4 | 0 | **ÖLÜ** |

**Ayrım (denetim şeması):** durum bir KARIŞIM:
- **(A) CANLI residual C** — 9 nesne, nihai ELF'te tanımlı+referanslı → **NET Law-4 ihlali**.
- **(B) ÖLÜ-ama-var C** — 3 nesne, sembolleri `.kem` (kem_heap) tarafından override, HİÇ referans
  yok, ama `--gc-sections` OLMADIĞI için ikilide HÂLÂ var → "ikili temiz-değil; nesne düşürülmeli".
- **(C) tam temiz** — DEĞİL.

---

## ADIM 3 — Weak-override / guard doğrulama (batch replacement'ları TUTTU mu?)

`.kem`-replace'ların nihai linkte KAZANDIĞI **doğrulandı**:

| Sembol | Yöntem | Nihai ELF tanımı | Sonuç |
|---|---|---|---|
| `kdl_syscall_isle` | weak (C) / strong (.kem) | **@0x400006f8** (kem_os aralığı 0x98–0x3cd8) | ✅ .kem KAZANDI (C weak ölü-var) |
| `kdl_mmu_kur` | guard (-DKEM variant) | kem_os (.kem) | ✅ |
| `kdl_irq_isle` | guard (variant) | kem_os (.kem) | ✅ |
| `kdl_kesme_kur`/`kdl_timer_baslat` | guard (variant) | kem_os (.kem) | ✅ |
| `malloc`/`free`/`memcpy`/`memset` | K1 guard | (yok — kem_heap sağlar) | ✅ |
| `kdl_bolge_*`/`kdl_dizi_*`/`kdl_metin_*`/`kdl_mmio_*`/`kdl_yetki_*` | isim/K-göç | **kem_heap (.kem)** | ✅ |

→ Replacement'ların HEPSİ tuttu. **ANCAK** override edilen C tanımları (weak `kdl_syscall_isle`,
guard-variant'ların kalan gövdeleri) `--gc-sections` yokluğunda ikilide ÖLÜ-VAR kalıyor.

---

## ADIM 4 — `.S` sınırı (indirgenemez küme) + CANLI-C kök nedeni

**`bm_a64_start.o` (asm, C-DEĞİL — teyit: `.S`'ten derlendi) nihai ELF'e sağladığı semboller:**
`_start`, `_halt`, `kdl_vektor_tablosu` (VBAR), `kdl_exc_ortak` (sync trap-frame + fault-recovery),
`kdl_irq_ortak` (IRQ trap-frame + SP-swap), `kdl_baglam_degis` (cooperative ctx-switch),
`kdl_el0_calistir` (EL0 eret). → İndirgenemez asm substrat, Law-4-savunulabilir (asm≠C).

**CANLI-C'nin KÖKÜ = `.S`'in referansladığı C sembolleri.** `bm_a64_start.o`'nun `U` (tanımsız)
sembolleri arasında `.kem`'e çözülenler (kdl_mmu_kur, kdl_irq_isle, kdl_syscall_isle, kem_heap_kur,
main ✅) YANINDA **C'ye çözülen 5 sembol**:

| `.S`'in çağırdığı C sembolü | Kaynak | Rol |
|---|---|---|
| `kdl_istisna_isle` | kdl_kesme.c | EL1 kurtarılamaz-fault → yazdır+halt (`bl` fault-yolu) |
| `kdl_el0_izolasyon_isle` | kdl_kesme.c | EL0 izolasyon-ihlali → süreç öldür |
| `kdl_el0_kill_aktif` | kdl_kesme.c (data) | EL0-kill opt-in bayrağı |
| `kdl_fault_bekleniyor` | kdl_kesme.c (data) | fault-recovery scratch (D-276 belgeli sınır) |
| `kdl_fault_yakalanan` | kdl_kesme.c (data) | fault-recovery scratch (D-276 belgeli sınır) |

Bu 5 referans **`bm_a64_kesme.o`'yu (C) canlı çeker** → `kdl_kesme.c` bir HUB'dır ve transitif
olarak şunları çeker:
- `kdl_istisna_isle` → `kdl_yazdir_*` (**yazdir.o**) → `kdl_uart_*` (**uart.o**)  [C fault-report yolu]
- `kdl_el0_izolasyon_isle` → `kdl_gorev_bitir` (**gorev.o**)
- `kdl_kesme.c` dosya/net syscall glue (D-143/D-176) → **virtio.o + virtio_net.o**
- syscall/spawn/tik glue → **mmu_kem.o (kdl_surec_*) + zaman_kem.o (kdl_tik_al)**
- `kdl_panik` (**panik.o**)

**KRİTİK:** `kdl_istisna_isle` (C) hâlâ C `kdl_yazdir`/`kdl_uart` kullanır → kem_os'un KENDİ
çıktısı `.kem`-UART'a portlanmış olsa da (D-245), **fault/exception-report yolu HÂLÂ C-UART'tır**.
Bu, batch'lerin görmediği canlı ikinci çıktı-yolu.

---

## ADIM 5 — kdl_virtio.c özel durumu

Önceki sınır-notu: "kdl_virtio.o ölü-linkli, C-kesme üzerinden". **Güncel NET durum:**
- **Linkte:** EVET (`bm_a64_virtio.o` + `bm_a64_virtio_net.o`).
- **Canlı mı:** **CANLI-var** (referans grafiği: virtio 2 ref, virtio_net 3 ref).
- **Zincir:** `kdl_kesme.c` (dosya/net syscall glue, D-143/176) referanslıyor. C-kesme→virtio
  zinciri **KOPMADI** — aksine `kdl_kesme.c` artık `.S`→`kdl_istisna_isle` ile CANLI olduğundan,
  virtio/net de transitif olarak canlı-var. (Not: kem_os'un GERÇEK disk/fs/net'i `.kem` sürücüden
  — vblk_*/vnet_* kem_os.o'da; bu C virtio.o AYRI + kullanılmıyor ama linkli+referanslı.)

---

## ADIM 6 — Residual C'yi kaldırmak ne gerektirir (AYRI gated iş — bu denetimde YAPILMADI)

1. **`--gc-sections` + `-ffunction-sections`** → 3 ölü nesne (heap_kemmalloc/bolge_kemregion/
   mmio_kem) + override-edilmiş weak/guard gövdeleri ikiliden düşer. (En kolay; canlı C'ye dokunmaz.)
2. **`.S`'in C-referanslarını `.kem`'e çevir** → CANLI-C kökünü keser:
   - `kdl_istisna_isle` → `.kem` (EL1 fault-report; C yazdir yerine `.kem`-UART) → yazdir.o + uart.o düşer.
   - `kdl_el0_izolasyon_isle` + `kdl_gorev_bitir` → `.kem` → gorev.o düşer.
   - fault-scratch globals (`kdl_fault_*`) → `.kem` küresel-external (codegen gap, [[project_kem_kuresel_internal_linkage_gap]]).
3. **kesme.c syscall/fs/net glue** → `.kem` VEYA link'ten çıkar (kem_os `.kem` sürücü kullanıyor) →
   virtio/net/mmu_kem/zaman_kem transitif referansları düşer.
4. Link OBJS listesinden gereksiz C nesnelerini düşür (Makefile KEM_OS_A64_OBJS budama).

---

## Taze-clone doğrulaması

`/tmp`'ye taze clone (`d2bc787`) + `make clean` + temiz build + aynı denetim → **aynı sonuç**:
aynı 15 nesne, aynı 9-canlı/3-ölü C sınıflandırması, `kdl_syscall_isle` yine kem_os aralığında.
(Bayat-obj gerçek link'i maskelemiyor.)

## x86_64 notu
Bu denetim **aarch64** kem_os içindir. x86_64 parite AYRI Law-4 borcu (bu denetimin kapsamı dışı).

---

### Özet
kem_os aarch64 nihai ikilisi: **.kem %48 + C %42 + .S %8**. "Yalnız .S" **YANLIŞ**. Residual C =
9 canlı nesne (kök: `.S`→`kdl_istisna_isle`/`kdl_el0_izolasyon_isle`/fault-scratch → kdl_kesme.c hub
→ yazdir/uart/gorev/virtio/net/mmu/zaman) + 3 ölü-var nesne (`--gc-sections` yok). Batch replacement'ları
(guard/weak/isim) HEPSİ tuttu ama override-edilen C ikilide ölü-var kaldı. Toolchain/libc yok.
Law-4 tam-uyum için: `--gc-sections` (ölüleri düşür) + `.S`-C-referanslarını `.kem`'e göç (canlı kökü kes).
