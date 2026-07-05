# KEMGU-OS Readiness Envanteri — SALT-OKUNUR Zemin Tespiti

> **Amaç:** OS C0 (mimari) kararından önce, KEMGU'nun bare-metal/OS yeteneklerinin DÜRÜST envanteri. Orchestrator'ın mimari önerisine girdi.
> **Statü:** KARAR-GİRDİSİ — nihai mimari değil. NE VAR · NE KADAR TAM · NEYE BAĞLI · EN BÜYÜK RİSK.
> **Yöntem:** 4 alan paralel deep-read (`escape`/`llvm`/`codegen`/runtime/Makefile/boot/linker/drivers) + her alan adversarial doğrulama (8 ajan, 174 araç çağrısı). Tüm bulgular `file:line` sitasyonlu.
> **Kural:** oversell YASAK · charitable-yorum YASAK · kanıt yoksa "doğrulanmadı" denir.
> **⚠️ KRİTİK DÜRÜSTLÜK NOTU:** Bu envanter SALT-OKUNUR yapıldı — **hiçbir şey derlenmedi/çalıştırılmadı**. `build/` dizininde tek bir bare-metal artefakt (kernel.elf / *.o) YOK → bu ortamda kernel hiç build edilmemiş. Dolayısıyla en güçlü iddia bile **KAYNAK-YAPISI-KANITLI**'dır, **ÇALIŞMA-ZAMANI-KANITLI değil**. "Reçete var" ≠ "çalışır".

---

## 🟢 GÜNCELLEME (2026-06-30) — Bu envanterin bulguları ÇÖZÜLDÜ (FAZ C tamamlandı)

> Aşağıdaki envanter, OS işine başlamadan önceki AS-WAS durumu (salt-okunur) tespit etti.
> O zamandan beri **FAZ C** uygulandı ve bu envanterin saptadığı darboğazlar **çözüldü**.
> Tarihsel bağlam için orijinal analiz korunuyor; güncel gerçeklik bu kutuda.

**Bu envanterin "en büyük risk" dediği şey doğrulandı + çözüldü:**
- ⚠️→✅ Bare-metal yol **HEAD'de gerçekten KIRIKTI** (envanterin "reçete-tam ama doğrulanmadı" dediği yer): region codegen her fonksiyona koşulsuz `@kdl_bolge_*` emit ediyordu, link sağlamıyordu → `ld.lld: undefined`. **C1a düzeltti** (frame allocator + bölge backing, codegen'e dokunmadan).
- ⚠️→✅ Region runtime host-malloc'a bağlıydı → **bare-metal frame allocator** (`kdl_bare_heap.c`).
- ⚠️→✅ Exception/interrupt/timer **kategorik yoktu** → **C3a** (VBAR/IDT) + **C3b/C4** (GICv2/CNTV + PIC/PIT) eklendi.
- ⚠️→✅ "build/ boş, QEMU hiç koşmadı" → QEMU kuruldu (11.0.1); **12/12 QEMU boot kanıtı**.

**MİNİMAL OS GÖSTERİCİ TAM (her iki mimaride QEMU-kanıtlı) — `make calistir_os_kernels` = 12/12:**

| Yetenek | aarch64 (QEMU virt) | x86_64 (QEMU PVH) | Commit / D |
|---|---|---|---|
| Boot + UART konsol | ✅ | ✅ | C1a/C1-x86, D-105/107 |
| Region-bellek + heap dizi | ✅ | ✅ | C1b, D-106 |
| Exception (fault→teşhis+halt) | ✅ | ✅ | C3a, D-108 |
| IRQ + periyodik timer | ✅ | ✅ | C3b/C4, D-109 |
| Sistem çağrısı (SVC/int 0x80) | ✅ | ✅ | C6, D-110 |
| **Entegrasyon capstone (hepsi tek boot)** | ✅ | ✅ | Capstone, D-111 |

Branch `os/c1-region-backing` (7 commit). **Mimarinin öngörüsü doğrulandı:** aynı KEMGU region-confinement runtime (F4.2b/F4.3 backing) DEĞİŞMEDEN iki platformda boot ediyor (codegen'e dokunulmadı). Detay: `DECISIONS_LOG.md` D-105..D-111, [[project-os-c1-region-backing-track]].

**Beyond-minimal / flag'li (kapsam-dışı):** C5 virtio-blk codegen fix (`&Struct+sonuç<>` deep multi-subsystem segfault — derin C-track, izole edilemedi: 5 standalone repro tetiklemiyor, tam virtio cross-file bağlamı gerek); C7 scheduling (Mehmet: minimal-gösterici); MMU/sayfalama; self-host bare-metal.

---

## Yönetici Özeti — Readiness Skor Kartı

| Alan | Seviye | Durum | En kritik tek blok |
|---|---|---|---|
| **Boot + konsol (ARM64)** | ~%35-45 | Reçete-tam, bu ortamda **çalışan-boot-kanıtı YOK** | QEMU/build hiç koşmadı; `build/` boş |
| **Sürücüler (virtio-blk)** | ~%25 | Taslak + **host-mock**; 9 testten **7'si KIRIK** | `&Struct param + sonuç<>` codegen segfault + IRQ yok |
| **MMIO/UART/panik runtime (C)** | sağlam iskelet | Kaynak-tam, iki-mod (bare-metal/mock) | init'ler V1-no-op (firmware-init varsayımı) |
| **Bölge sistemi bare-metal (KRİTİK)** | ~%5 | **Port EDİLMEMİŞ** — yalnız TODO yorumu | **Frame allocator YOK + kdl_bolge.c koşulsuz `malloc`** |
| **Timer** | %0 | **KATEGORİK YOK** | sıfırdan |
| **Interrupt / exception vector / GIC** | %0 | **KATEGORİK YOK** (boot stub VBAR kurmuyor) | sıfırdan → preemption imkânsız |
| **Self-host → bare-metal** | ~%0 | `kemgu_self` bare-metal **DERLEYEMEZ** | sabit host triple + MMIO/asm emit yok |
| **Inline asm (`satıriçi_asm`)** | C-tarafı VAR | C-bootstrap gerçek `asm sideeffect` emit eder; self-host emit YOK | self-host emit eksik |

**Tek cümle:** KEMGU'nun "ilk konsol çıktısı veren bare-metal kernel" iskeleti (boot stub + linker + UART + libc'siz yazdır + panik) **uçtan-uca bağlanmış ve tutarlı**, ancak (1) bu ortamda **çalıştığı doğrulanmadı**, (2) dilin gerçek özelliklerini (dizi/yapı/bölge) kullanan herhangi bir kernel bugün bare-metal'de **linklenemez** çünkü **bölge runtime'ı bare-metal'e bağlanmamış**, ve (3) bir OS çekirdeğinin can damarları **timer + interrupt KATEGORİK olarak yok**.

---

## ADIM 1 — Bare-metal hedef + mevcut boot durumu

### NE VAR

| Öğe | Durum | Kanıt | Not |
|---|---|---|---|
| Hedef: `aarch64-unknown-none` | reçete-var, çalışan-çıktı-yok | `boot/start_aarch64.S`, `linker/bare-metal-aarch64.ld`, `Makefile:740-779` | **TEK tam kernel-link hedefi** |
| Hedef: `x86_64-unknown-none` | yalnız UART-object | `Makefile:831-856` | Boot stub YOK, linker script YOK → tam kernel hedefi **DEĞİL**; sadece 16550 sürücüsünün freestanding derlendiğini kanıtlayan nokta-test. (`BARE_METAL_DESTEK.md:20` `ld.lld -m elf_x86_64` der ama x86_64 `.ld` dosyası YOK → **aspirasyonel**) |
| Boot stub `_start` | reçete-var | `boot/start_aarch64.S:24-51` | SP=`__stack_top` → BSS temizle → `bl main` → `_halt` (wfe spin). **Vektör tablosu YOK, exception YOK, SMP YOK** |
| Linker script | reçete-var | `linker/bare-metal-aarch64.ld:21-69` | Yük adresi `0x40000000` (QEMU virt RAM), 16KB stack, `__bss_*`. **`_heap_start/_heap_end` YOK** |
| Kernel ELF pipeline | reçete-var | `Makefile:740-779` (merhaba), `667-706` (echo) | `kemgu --llvm \| clang -target aarch64 \| ld.lld`. İki demo: `kernel.elf` (hello) + `kernel_echo.elf` (RX→TX echo) |
| libc-yok kanıt-kapısı | reçete-var | `Makefile:699-704, 771-778` | Link sonrası `llvm-nm --undefined-only` ile `malloc\|free\|memcpy\|printf...` VARSA `exit 1`. (⚠️ liste `kdl_bolge_*` ARAMAZ — bkz. ADIM 3) |
| PROVENANCE | — | `Makefile:742` `./build/kemgu --llvm` | Üretici **C-bootstrap `kemgu.exe`**, self-host `kemgu_self` DEĞİL. "Self-host bare-metal kernel üretiyor" iddiası **GEÇERSİZ** |
| QEMU smoke | opsiyonel | `Makefile:708-731` | `qemu-system-aarch64 -M virt`, stdout'ta "Merhaba KEMGU"+"42" arar. **`test_tumu` dışında**; qemu yoksa sessiz "atlandı" |
| `build/kernel.elf` | **YOK** | Glob → "No files found" | `build/`'de kemgu/kemgu_self/codegen exe'leri var, **tek bare-metal artefakt yok** → bu ortamda hiç build edilmemiş |
| PL011 UART konsol | kaynak-var | `runtime/kdl_runtime_uart_pl011.c:136-186` | `init` **V1-no-op** (baud/CR yazmaz; QEMU/firmware-init'li board varsayımı), `putc`/`oku` FR-spin. Gerçek MMIO + mock iki-mod |
| Libc'siz yazdır katmanı | kaynak-var | `runtime/kdl_runtime_yazdir_bare.c:44-298` | malloc/snprintf YOK; stack-tampon base-10, UTF-8, hex, line-buffered input |
| `uart_merhaba.kem` / `uart_echo.kem` | kaynak-var | `test/ornekler/uart_merhaba.kem:31-35` | Aşırı minimal (3 satır gövde), yalnız extern runtime çağrıları |
| `kernel.kem` | yanıltıcı isim | `test/ornekler/kernel.kem:7-32` | **Gerçek kernel DEĞİL** — `tam64` bit-op (sahte LPAE) exit-kod=42 codegen snippet'i. UART/boot/MMU içermez |

### NE KADAR TAM
**~%35-45 — erken bring-up.** (+) Boot stub + linker + PL011 + libc'siz yazdır + 2 demo + tam `ld.lld` reçetesi mevcut ve tutarlı. (−) Bu ortamda **hiç derlenmedi/QEMU koşmadı**; `build/` bare-metal-boş. (−) QEMU smoke `test_tumu` dışı → otomatik regresyon güvencesi zayıf. (−) `init()` no-op → yalnız QEMU/firmware-init'li board'da çalışır, ham RPi'de değil. (−) Vektör tablosu / exception / MMU / SMP YOK. (−) "kernel" örnekleri gerçek kernel değil.

### NEYE BAĞLI
`clang` (IR→object cross), `ld.lld`, `llvm-nm/objdump` (sembol doğrulama), `build/kemgu.exe` (C-bootstrap; self-host değil), `qemu-system-aarch64` (opsiyonel), `-ffreestanding -nostdlib` (libc/heap yok, stack-yalnız).

---

## ADIM 2 — Sürücü + runtime asset'leri

### NE VAR

| Öğe | Durum | Kanıt | Not |
|---|---|---|---|
| virtio-blk yığını (KEMGU kaynağı) | taslak | `drivers/virtio/` **10 .kem** (~60KB) | Katmanlı: layout/status-FSM/feature-negotiation/MMIO-handshake/queue-bind/blk-init/config/read |
| `blk_oku` (sektör okuma) | host-mock | `drivers/virtio/virtio_blk_oku.kem:70,92-148` | **Gerçek 3-descriptor ring kurulumu** (sadece stub değil): NEXT/WRITE flag, avail-publish, used-poll. AMA cihaz tamamlaması **HOST-MOCK simülasyonu** (`:130-150`); gerçek HW/IRQ yolu D10 TODO |
| `blk_yaz` (yazma) | **YOK** | yalnız `docs/drivers/virtio-blk-oku.md:115` notu | Read-only sürücü |
| `blk_baslat` (init) | KIRIK | `virtio_blk.kem:57,259`; `Makefile.drivers:44` | Init testi **segfault**: `virtqueue_bagla &Virtqueue param codegen bug`; `DECISIONS_LOG:3342` "11 hata, --check geçmiyor" |
| `virtqueue_bagla` | kaynak-var | `virtqueue_bind.kem:51,82-115` | Register binding tam; **DMA allocation YOK** (`:48` TODO), **bellek bariyeri (dmb) YOK** (`:114` TODO) |
| MMIO primitive (oku/yaz 16/32/64) | kaynak-var | `runtime/kdl_runtime_mmio.c:34-61, 96-118` | İki-mod: bare-metal volatile + host-mock LE tampon. `<stdint.h>` dışı bağımlılık yok (`:28`) |
| Konsol PL011 (ARM) + 16550A (x86) | kaynak-var | `kdl_runtime_uart_pl011.c:136`, `kdl_runtime_uart_16550.c:137` | Simetrik API; `init` V1-no-op; cross-target vtable (`KdlUartSurucu`) |
| Panik handler | kaynak-var | `kdl_runtime_panik.c:38-67` | UART "PANIK:" + sonsuz halt (wfe/hlt); libc/heap yok |
| **Timer** (CNTV/CNTP/systick) | **YOK** | Grep → kod yok; `pl011.c:152` "irq/timer yok" | Zamanlama yalnız spin |
| **Interrupt** (GIC/IRQ/exception-vector/trap) | **YOK** | Grep → implementasyon yok; boot stub'da VBAR yok | virtio tamamlamayı IRQ yerine host-mock'la simüle ediyor |

### NE KADAR TAM
**ORTA-DÜŞÜK (iskelet + host-mock).** virtio-blk'da kayda değer **gerçek kod** var (MMIO handshake + 3-descriptor ring + status FSM + feature negotiation) ama (a) cihaz tarafı host-mock, (b) 9 testten yalnız 2'si (mmio_mock + blk_oku) yeşil-pipeline'da, 7'si KIRIK, (c) blk_yaz/DMA/bariyer yok. Timer + interrupt **kategorik yok** → preemptive çekirdek için temel eksik.

> **⚠️ Doğrulama uyarısı (çelişki):** `blk_oku` host-mock harness'ı `--llvm` yolunda "exit 0=GEÇTİ" iddia eder (`Makefile.drivers:34`) ama `DECISIONS_LOG:3342-3343` **aynı satırda** `virtio_blk_oku_test`'in `--check` modunda **14 hata** verdiğini söyler. İki mod arası doğrulanmamış çelişki var; "blk_oku yeşil" **kanıtlı değil**.

### NEYE BAĞLI
`clang -x ir`, `build/kemgu.exe`, `kdl_runtime.o + kdl_runtime_mmio.o`, `ld.lld/llvm-nm`, `-DKEMGU_BARE_METAL` vs mock bayrakları, mimari-bağlı inline asm (wfe/hlt + inb/outb). **Sürücü mantığı KEMGU'da, runtime primitive'leri C'de** (kasıtlı katmanlama).

---

## ADIM 3 — Bölge sistemi bare-metal uyumu (KRİTİK)

### NE VAR

| Öğe | Durum | Kanıt | Not |
|---|---|---|---|
| `kdl_bolge.c` arena allokatör | kaynak-tam, **host-bağlı** | `runtime/kdl_bolge.c:61-126` | bump-pointer, 64KB blok; **koşulsuz `malloc`/`free`** (`:53,62,113,116`), **`KEMGU_BARE_METAL` #ifdef YOK** (yalnız `:15-18` TODO yorumu) |
| Codegen ρ_yerel (C) | kaynak-tam | `src/llvm.c:5320-5328, 403-406` | **HER fn girişinde** (main dâhil) `call @kdl_bolge_olustur()`; her `ret` öncesi `@kdl_bolge_serbest` |
| Codegen ρ_yerel/ρ_iter (self-host) | kaynak-tam | `selfhost/codegen.kem:3539-3551, 3642-3647, 2942-2943` | Aynı IR'yi üretir → `@kdl_bolge_olustur/serbest`'e bağlı |
| `bölge_al(ρ,n)` intrinsic | kaynak-tam | `src/llvm.c:2824-2851` | ρ'yu **yok sayıp** doğrudan inline `@malloc` emit eder |
| `kdl_dizi_*` runtime (F4.2a) | kaynak-tam, host-bağlı | `runtime/kdl_runtime.c:543-718` | `kdl_bolge_ayir` + `memcpy`; OOB→`kdl_panik` |
| Global sızıntı havuzu | kaynak-tam | `runtime/kdl_runtime.c:38-46` | Hiç serbest edilmez (status-quo leak); host malloc |
| Bare-metal MMIO/panik/UART | bölge **içermez** | `kdl_runtime_mmio.c:28`, `kdl_runtime_panik.c:8` | Bare-metal link YALNIZ UART+yazdır+start.S+program.o; **`kdl_bolge.c`/`kdl_runtime.c` DAHİL DEĞİL** → bölge runtime'ı bare-metal binary'de YOK |
| **Frame / page allocator** | **YOK** | `kdl_bolge.c:15-18` (sadece yorum); linker'da `_heap_start/_heap_end` yok | Hiçbir fiziksel-bellek allokatörü yok |

### 🔴 EN YÜK-TAŞIYAN BULGU (adversarial doğrulamada ortaya çıktı)
Codegen, **main dâhil HER fonksiyona KOŞULSUZ** `@kdl_global_bolge_al` + `@kdl_bolge_olustur` çağrısı emit eder (`llvm.c:5308-5328`). Yani KEMGU kaynak "tahsis-içermese" bile (örn. `uart_merhaba.kem`) üretilen IR **bölge sembollerine referans verir**. Bu semboller bare-metal link'e dâhil değil (`Makefile:687-690`). → **Bare-metal hello-world'ün linklenebilmesi YALNIZCA `clang -O2`'nin kullanılmayan bölge çağrılarını dead-code-elimine etmesiyle açıklanabilir — ve bu DOĞRULANMADI** (bu ortamda build koşmadı). Ek: libc-yok grep'i (`Makefile:700`) yalnız `malloc\|free\|memcpy...` arar, `kdl_bolge_*` aramaz → bu güvenlik ağı bölge-sembol sızıntısını **tasarım gereği yakalamaz**; link zaten `ld.lld` undefined-symbol ile ondan önce çökerdi.

### NE KADAR TAM
**Host-tam, bare-metal-YOK.** Bölge mimarisi host'ta kurulu ve (CLAUDE.md iddiasına göre) ASan-temiz; **codegen seviyesinde bare-metal'e hazır** (ρ_yerel/ρ_iter IR mimari-bağımsız `@kdl_bolge_*` çağrıları). Ama **BACKING bağlanmamış**: `kdl_bolge.c` koşulsuz `malloc`, frame allocator yok, bare-metal link'e dâhil değil. **Sonuç: skaler-aritmetik dışında herhangi bir KEMGU programı bugün bare-metal'de linklenemez.** (`BARE_METAL_DESTEK.md:116-118` bunu doğrular: "Bump allocator — Mehmet onayında ... Şu an kapsam dışı.")

### NEYE BAĞLI
host libc `malloc`/`free` (`kdl_bolge.c:53,62,113,116`); `string.h memcpy` (`kdl_runtime.c:547` dizi-büyüt, metin yolları); üretilen IR sembolleri `@kdl_bolge_olustur/serbest/@kdl_global_bolge_al/@kdl_dizi_*/@malloc`.

### SIFIRDAN NE GEREKİR (orta ölçek, codegen'e DOKUNMADAN)
1. **Frame/page allocator** (~150-300 satır, 1 modül) + linker script heap bölgesi (`_heap_start/_heap_end`).
2. `kdl_bolge.c`'ye `KEMGU_BARE_METAL` #ifdef köprüsü: `malloc→frame_alloc`, `free→frame_free` (veya no-op + toplu reset).
3. `kdl_runtime.c`'nin bölge/dizi kısmının **freestanding ayrımı** (dosya tümüyle `<stdio/stdlib/string>`'e bağlı, monolitik).
4. Freestanding `memcpy/memset` (veya `-ffreestanding` builtin — ama libc-yok grep'i `memcpy`'yi de yasaklıyor).
5. Makefile bare-metal link'e bölge `.o` ekleme + sembol-yasak listesi güncelleme.

> **Önemli:** Codegen **değişmez** (ρ_yerel/ρ_iter/F4.2b/F4.3 IR aynı kalır) — yalnız sembollerin bare-metal implementasyonu link'e girer. Bu, bölge-confinement işinin (F4.2b/F4.3) bare-metal'de **kavramsal olarak çalışacağı** ama backing olmadan **test edilemeyeceği** anlamına gelir.

---

## ADIM 4 — Self-host derleyici freestanding kabiliyeti

### NE VAR

| Öğe | Durum | Kanıt | Not |
|---|---|---|---|
| Target triple | **sabit (host), parametrik DEĞİL** | `codegen.kem:3748` `"x86_64-pc-windows-gnu"` hardcoded; `src/llvm.h:39` `KEMGU_HEDEF_TRIPLE` | `datalayout` hiç emit edilmez. Bare-metal seçimi yalnız aşağı-akış `clang -target` override (kırılgan, doğrulanmadı) |
| `runtime_header_yaz` | koşulsuz libc declare | `codegen.kem:2120-2127` | `puts/malloc/free/memcpy` + ~60 `@kdl_*` declare KOŞULSUZ. (Yalnız declare — çağrılmadıkça undefined-symbol değil) |
| Self-host runtime (`kdl_runtime.o`) | host-libc bağlı | `kdl_runtime.c:24,27,63,71`; `Makefile:538` | `printf/malloc/fputs/stdio/stdlib`. Bare-metal runtime (`yazdir_bare` + UART) **kapsamlı değil** — yalnız yazdır ailesi; dizi/metin/dosya/bölge bare-metal karşılığı YOK |
| Bare-metal yolu | C-bootstrap'a kilitli | `Makefile:740-742, 593, 670` | Tüm bare-metal/ARM64 target'ları `build/kemgu.exe`; `kemgu_self` için bare-metal Makefile yolu **HİÇ YOK** |
| **MMIO emit (self-host)** | **YOK** | `llvm.c:2970-3044` (C emit var) vs `codegen.kem:3812-3813` (yalnız checker-adı) | `kemgu_self` MMIO çağrısı içeren kernel'i **derleyemez** |
| **Inline asm `satıriçi_asm`** | C-tarafı **VAR**, self-host emit YOK | `src/llvm.c:4849-4942` (`asm sideeffect` + AS001 arch-check); `codegen.kem:1346-1383` (parse-eder-atar, emit yok) | Türkçe keyword; tam clause grameri (mimari/şablon/çıktı/girdi/bozulan/çevrim). **C-bootstrap gerçek asm IR üretir** |
| AS001 mimari-koruma | C-tarafı var | `src/llvm.h:35-39`, `llvm.c:4858-4878` | asm arch-tag `KEMGU_HEDEF_MIMARI` ile uyuşmazsa derlemeyi **reddeder** ("yanlış hedefe sessizce bozuk IR YASAK") — bare-metal retarget güvenliği için ilgili |

### NE KADAR TAM
**TASLAK/KISMİ — host self-host TAM, bare-metal self-host YOK.** `kemgu_self` bugün bare-metal kernel **DERLEYEMEZ**: sabit host triple, koşulsuz host-libc header, MMIO + `satıriçi_asm` için **emit handler yok** (yalnız parse). C-bootstrap `kemgu` ile **saf-hesap** kernel'i bare-metal ELF'e gidebilir (reçete var, doğrulanmadı). Self-host **HEAP-uniform** (diziler her zaman heap) → bare-metal'de heap/region olmadan dizi kullanan kernel çalışamaz (ADIM 3 ile örtüşür).

### SIFIRDAN NE GEREKİR
1. Parametrik triple/`--hedef` bayrağı (codegen.kem + llvm.c) — `datalayout` emit.
2. Self-host **MMIO emit handler** (`kdl_mmio_*` volatile call üretimi).
3. Self-host **`satıriçi_asm` emit handler** (`asm sideeffect` üretimi; clause'ları şu an atıyor).
4. Koşullu `runtime_header` (bare-metal'de libc declare'larını bastır / freestanding set seç).
5. Bare-metal kapsamlı/no-heap runtime (ADIM 3 frame allocator + dizi/metin port).

---

## ADIM 5 — EN BÜYÜK BRING-UP RİSKİ ("temel çalışana kadar hiçbir şey çalışmaz")

### 🔴 Birincil darboğaz: BÖLGE RUNTIME'ININ BARE-METAL BACKING'İ
**Neden bu "her şeyi kilitleyen temel":** Codegen, main dâhil **her fonksiyona koşulsuz** bölge-çağrısı (`@kdl_bolge_olustur/@kdl_global_bolge_al`) emit eder. Şu an bunu kurtaran tek şey, kullanılmayan çağrıların `-O2` ile elenmesi (**doğrulanmadı**). Dilin gerçek özelliklerini kullanan **ilk program** (bir dizi, bir yapı, bir metin birleştirme, bir buffer) bölge backing'i çağıracak — ve o backing bare-metal'de **YOK** (frame allocator yok, `kdl_bolge.c` koşulsuz `malloc`, link'e dâhil değil). → **Bir sürücüyü/kernel'i KEMGU'da gerçekten yazamazsın** çünkü buffer/struct/dizi kullanan kod bare-metal'de **linklenmez**. Tüm aşağı-akış (sayfa tabloları, scheduler yapıları, sürücü buffer'ları) buna bağlı. Bu, **codegen'e dokunmadan çözülebilir** ama çözülene kadar bare-metal yalnız "skaler hello-world" seviyesinde kalır.

### 🟠 İkincil darboğaz: TIMER + INTERRUPT KATEGORİK YOK
Boot stub VBAR_EL1 kurmuyor; GIC/IRQ/exception-vector/trap/timer **hiçbiri yok**. Bu olmadan: preemptive scheduling imkânsız, gerçek cihaz tamamlaması (virtio IRQ) imkânsız (şu an host-mock), saat yok. Bu **greenfield-eklemeli** bir blok — birincil darboğaz gibi gizli bir bağımlılık değil; boot+konsol+bölge çalıştıktan SONRA inşa edilebilir, ama bir OS olması için **zorunlu** ve büyük.

### 🟡 Aktif takılma noktası: SÜRÜCÜ TRACK'İ
virtio sürücü track'i şu an **takılı**: 9 testten 7'si kırık, `&Struct param + sonuç<>` codegen segfault'una + çok-segment import eksiğine bağlı, ve host-mock (gerçek cihaz yok). `blk_oku`'nun çalışabilmesinin sebebi bu codegen bug'ını **by-value struct ile bypass etmesi**; `init`/`bind` bypass edemediği için kırık. Bu, **C-track codegen düzeltmesi** gerektiren somut bir correctness-blocker.

### Boot şu an takılı mı?
**Sert-takılı değil**, ama **bu ortamda kanıtsız**: hello-world/echo yolu reçete-tam ve (önceki oturumlara/`BARE_METAL_DESTEK.md`'ye göre) muhtemelen çalışıyor, ama `build/` boş ve QEMU hiç koşmadı → "boot edip Merhaba KEMGU/42 basıyor" **bu ortamda doğrulanmadı**. Bu bir **doğrulama boşluğu**, tasarım boşluğu değil. Tasarım darboğazı = bölge backing.

---

## Sentez — Mevcut zemin ne kadar / sıfırdan ne gerekir

### Mevcut zemin (gerçek, küçümsenmemeli)
- ARM64 bare-metal **alt-katman iskeleti uçtan-uca bağlı**: boot stub + linker + PL011/16550 UART + cross-target vtable + libc'siz yazdır + panik + iki demo kernel + libc-yok kanıt-kapısı + opsiyonel QEMU smoke.
- MMIO/UART/panik runtime'ı **iki-modlu** (bare-metal volatile / host-mock) ve volatile-doğru.
- Bölge sistemi **codegen seviyesinde mimari-bağımsız** (backing değişimiyle bare-metal'e hazır).
- virtio'da **gerçek protokol kodu** (descriptor ring + handshake), host-mock da olsa.
- **`satıriçi_asm` C-tarafı çalışır** (gerçek `asm sideeffect` + AS001 arch-koruma) — düşük-seviye kernel kodu için kritik primitif **zaten var** (yalnız self-host emit eksik).

### Sıfırdan ne gerekir (kabaca, kritik-yol sırasıyla)
1. **(TEMEL) Frame allocator + `kdl_bolge.c` bare-metal #ifdef + freestanding runtime ayrımı** — orta ölçek, codegen'e dokunmadan. **Bu olmadan dilin gerçek özellikleri bare-metal'de kullanılamaz.**
2. **(DOĞRULAMA) Bu ortamda gerçek build + QEMU smoke** — `build/` boş, mevcut iskeletin gerçekten boot ettiğini kanıtla (reçete var, kanıt yok).
3. **(SÜRÜCÜ) `&Struct param + sonuç<>` codegen segfault düzeltmesi** — virtio init/bind track'ini açar (C-track).
4. **(ÇEKİRDEK) Exception vector + GIC + timer** — greenfield, preemption + gerçek IRQ tamamlaması için. Büyük.
5. **(SELF-HOST, OPSİYONEL) `kemgu_self` bare-metal yolu** — parametrik triple + MMIO/asm emit + freestanding header. **Stratejik soru: OS v1 self-host-derlenmiş mi olmalı, yoksa C-bootstrap-derlenmiş kabul mü?** (C-bootstrap interim tamamen makul.)

### Mimari öneri girdisi (orchestrator için)
- **Kritik yol bölge backing'den geçer, boot'tan değil.** Boot iskeleti hazır; gerçek iş, dilin bellek modelini bare-metal'de canlandırmak. C0 mimarisi **frame allocator + bölge backing**'i ilk taş olarak konumlandırmalı.
- **Timer/interrupt'ı erken DESIGN-STOP yap** — bu kategorik-yok blok, GIC sürücüsü + exception model + (muhtemelen) MMU kararları gerektirir; mimari karar noktası, kademeli ekleme değil.
- **Self-host bare-metal'i v1 için kapsam-dışı tut** — C-bootstrap `kemgu` ile kernel derlemek tamamen yeterli; self-host bare-metal portu ayrı/sonraki bir hedef. Bu, ADIM 4'teki 3 bloğu (triple/MMIO-emit/asm-emit) kritik yoldan çıkarır.
- **`satıriçi_asm`'i ciddiye al** — MMU enable / register erişimi / trap için dil-içi mekanizma **zaten var** (C-tarafı). `BARE_METAL_DESTEK.md:195`'in "yeni keyword `asm` gerekir" notu **STALE** (satıriçi_asm o nottan sonra eklendi).

---

## Doğrulama notu + tespit edilen stale belgeler

**Kanıt seviyesi:** Bu envanterde **hiçbir şey derlenmedi/çalıştırılmadı** (salt-okunur kural). Tüm "kaynak-tam/çalışır" işaretleri **kaynak-yapısı-kanıtlı**dır; runtime davranışı (ASan-temizlik, test sayıları) CLAUDE.md/`DECISIONS_LOG` alıntısıdır, bu oturumda **doğrulanmadı**. 4 alan ayrıca adversarial doğrulamadan geçti; tüm yük-taşıyan iddialar dosyaya karşı denetlendi.

**Tespit edilen drift'ler (gerçeklik ≠ belge):**
- `BARE_METAL_DESTEK.md:195` inline-asm'i "gerekli gelecek keyword (`asm`)" sayar — ama `satıriçi_asm` **zaten var ve C-tarafı emit eder**. Belge bu noktada eski.
- `DECISIONS_LOG:3328` virtio'yu "6 dosya" sayar; güncel sayı **10 .kem**.
- Okuyucu ajanlar başta "inline asm YOK" ve "uart_* tahsis-içermez (libc-yok geçer)" dedi — **ikisi de yanlış**, doğrulama düzeltti (satıriçi_asm var; codegen koşulsuz bölge-çağrısı emit eder). Bu iki düzeltme bu envanterin gövdesine işlendi.

---

> **NET (tek paragraf):** KEMGU'nun bare-metal zemini, "konsola çıktı veren ilk kernel" seviyesinde **gerçek ve tutarlı bir iskelet** (boot+linker+UART+panik+libc'siz yazdır) içeriyor — küçümsenmemeli — ama (1) bu ortamda **çalıştığı kanıtlanmadı** (build/QEMU hiç koşmadı, `build/` boş), (2) asıl darboğaz boot değil **bölge runtime'ının bare-metal backing'i**: codegen her fonksiyona koşulsuz bölge-çağrısı emit ettiği için, dilin gerçek özelliklerini kullanan herhangi bir kernel bugün bare-metal'de **linklenemez** (frame allocator yok, `kdl_bolge.c` koşulsuz `malloc`), ve (3) bir OS'un can damarları **timer + interrupt kategorik olarak yok**. Kritik yol: **frame allocator + bölge backing → gerçek build/QEMU doğrulaması → sürücü codegen düzeltmesi → exception/GIC/timer**. Self-host bare-metal v1 için kapsam-dışı tutulabilir (C-bootstrap yeterli); `satıriçi_asm` düşük-seviye primitif olarak C-tarafı zaten hazır.
