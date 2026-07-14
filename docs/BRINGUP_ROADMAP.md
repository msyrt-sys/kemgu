# KEMGU-OS — REAL-OS v1 BRING-UP YOL HARİTASI (KEŞİF)

> **Bu bir KEŞİF/envanterdir — kod yazılmadı; kem_os/runtime/Makefile değiştirilmedi.** Her iddia
> gerçek `nm` / kaynak / QEMU-gate çıktısına karşı doğrulandı (D-268 dersi: tahmin ≠ gerçek).
> Doğrulama: 2026-07-12 (D-269 sonrası). Kaynak: `test/bare_metal/*.c` (103 kernel), `runtime/kdl_*.c`,
> `boot/start_aarch64.S`, `test/ornekler/kem_os.kem`, Makefile gate assertion'ları, DECISIONS_LOG D-232→242.

---

## 0. TEMEL ÇERÇEVE — "bring-up" = kanıtlı-C entegre OS'u saf-.kem yeniden gerçekleştirmek

**Kanıtlı-C gerçeği (D-232→240):** `test/bare_metal/kemgu_os_arm.c` (767 satır) TEK QEMU boot'ta gerçek bir
OS çalıştırıyor. Master gate (`calistir_kemgu_os_arm`, Makefile:3244) şunların HEPSİNİ assert eder:
`KEMGU-OS OK` + `PAGEFAULT OK`(MMU) + `SCHEDULER OK`(preemptive) + `USERSPACE INIT EL0` + `IZOLASYON OK`
(EL0 izolasyon) + `SHELL EL0 OK` + `sonuc=42`(syscall) + `kanal-toplam=300`(IPC) + `DISK RW OK`(virtio-blk)
+ `RTC OK` + `PING: CANLI`(net). → **v1 hedefinin tamamı C'de KANITLI.**

**Mevcut saf-.kem gerçeği:** `test/ornekler/kem_os.kem` = LİNEER [1..5] iskelet (BOOT→HEAP→MMIO→HESAP→
sentetik-EXC), `main` düz akış, `ver 0`. Skaler runtime (allocator+metin+mmio+yetki) saf-.kem (D-260→268).
AMA: scheduler YOK, timer/IRQ YOK, EL0/syscall YOK, gerçek fault-routing YOK ([5] SENTETİK — 3 hard-coded
ESR literali decode eder, vektör tablosuna bağlı DEĞİL), net YOK, gerçek FS YOK. MMU boot'ta CANLI ama
sayfa-tablosunu C `kdl_mmu.c` kuruyor.

**Bring-up v1 = her subsystem'i kem_os'ta AKTİFLEŞTİR (gerçek işlevsellik) + runtime'ı saf-.kem yap +
gerçek fonksiyonel gate ile doğrula.** MİGRASYON değil (sembol yer değiştirme), AKTİVASYON.

**Kritik sınır — .S ≠ C:** irreducible AArch64 asm (vektör tablosu, trap-frame, context-switch, `eret`)
`boot/start_aarch64.S`'de. Yasa-4 "sıfır **C**" → .S (assembly) KALIR (kem_os aynı boot objesini linkler).
Bunları .kem'e almak **P4** (naked-.kem + asm gövde) gerektirir ama **v1 için GEREKMEZ** (asm aynı, fayda
yok, risk var). P4 yalnız "sıfır-asm-da" mutlak hedefinde gündeme gelir — v1 kapsamı dışı.

---

## 1. SUBSYSTEM TABLOSU (9) — hepsi ampirik doğrulandı

| Subsystem | Mevcut durum (C kanıt / kem_os) | Aktivasyon gereği | Bağımlılık | Primitif | Gerçek fonksiyonel gate | .S/P4 |
|-----------|--------------------------------|-------------------|------------|----------|-------------------------|-------|
| **MMU** | C: `PAGEFAULT OK` (kemgu_os_arm.c:618 map→erişim→fault→FAR doğrula→kurtar) + `d1_arm` çeviri-izolasyon. **kem_os: CANLI@boot** (C kdl_mmu.c kuruyor; `bl kdl_mmu_kur` start.S:75) | Sayfa-tablo kurma (C-bellek-ops → .kem taşınabilir) + commit .kem'de | boot(bl kdl_mmu_kur); fault→kesme | **P1** (msr mair/tcr/ttbr/sctlr+tlbi) **P2** (raw ptr tablo) | map page→VA eriş→çeviri; unmapped→fault→FAR==adr→kurtar | commit msr .kem-asm; _start .S |
| **kesme** (vektör+fault+syscall dispatch) | C: fault-handler+syscall-dispatch (kdl_kesme.c). **kem_os: [5] SENTETİK** (vektöre bağlı değil; gerçek trap YOK) | Gerçek trap → .kem fault-hook (vektör → .kem); kdl_exc_ortak .S kalır | boot VBAR | **P1**(FAR/SPSR msr) **P4→.S**(vektör/trap-frame) cross-file | Gerçek data-abort → .kem handler karar verir (sentetik değil) | kdl_exc_ortak/vektör .S KALIR |
| **zaman** (timer+IRQ dispatch) | C: `TIMER OK tik=N` (capstone), tick. **kem_os: YOK** (timer/IRQ hiç kurulmaz) | kdl_kesme_kur(GIC)+kdl_timer_baslat(CNTV+daifclr) çağır | kesme(vektör) | **P1** (CNTV_*/DAIF msr — temiz .kem-asm) | timer IRQ tik ilerliyor (uptime artışı) | IRQ dispatch kdl_irq_ortak .S |
| **görev** (scheduler) | C: `PREEMPT OK` (preempt_arm.c: 2 görev, yield-suz, timer zorlar) + priority/sleep/multiproc. **kem_os: YOK** | 5 extern çağrı (baslat/olustur/kesme_kur/timer/ac) + ≥2 görev wire | zaman(timer)+kesme(vektör); switch→.S | **P1** **P4→.S**(kdl_baglam_degis+kdl_irq_ortak SP-swap) cross-file | 3 görev round-robin interleave (timer preemption) → KEM-PREEMPT OK | context-switch .S KALIR |
| **virtio-blk** | C: `DISK RW OK` (virtio_rw_arm.c sektör7 yaz→oku→32B eşleşme). **kem_os: DORMANT** (yalnız MAGIC-register probe; C sürücü kesme dead-dep ile linkli) | queue-setup + desc-ring + sektör I/O (C volatile-MMIO+dsb; privileged YOK) | virtio-mmio; (dosya yolu kesme'den) | **P1**(dsb) **P2**(aligned ring, manuel over-align). alloc YOK, IRQ YOK (polling) | sektör yaz→oku→byte eşleşme | yok (inline dsb) |
| **syscall/userspace (EL0)** | C: `USERSPACE OK toplam=55` + `IZOLASYON OK`(EL0 kötü-erişim→öldür) + syscall-ret=42. **kem_os: YOK** | .user-section EL0 entry + SVC dispatch + izolasyon; mekanizma .S'de HAZIR | kesme(vektör)+MMU(user VA AP=01) | **P1**(SPSR/ELR/SP_EL0 msr) **P4→.S**(vektör/eret) cross-file | EL0 görev SVC round-trip + kötü-erişim→izolasyon-öldür | kdl_el0_calistir/eret .S KALIR |
| **virtio-net** | C: TX+RX 180 LOC sürücü; .kem: `KEM NET OK`/`KEM MAC OK` (yalnız cihaz-keşif+MAC). **kem_os: YOK** | queue-setup (RX q0+TX q1+DRIVER_OK); .kem keşif VAR, queue makinesi portlanacak | virtio-mmio (polling, IRQ YOK) | **P1**(dsb) **P2**(aligned ring) alloc YOK, P3 YOK | ARP request→reply round-trip | yok (inline dsb) |
| **net-stack** (ARP/IP/ICMP/UDP/DHCP/DNS/HTTP) | C: `ARP REPLY OK`/`PING OK`/`DHCP OK`/`DNS REPLY OK`/`HTTP GET OK`. **SIFIRDAN — runtime kütüphanesi YOK** (her protokol test-kernel'de inline). **kem_os+.kem: YOK** | .kem'de yeniden yaz (min ARP+ICMP ~200 LOC; tam parite ~1500+ LOC) | virtio-net | **P1**(dsb) **P2**. alloc YOK (static buf+inline byte-loop→memcpy YOK→**T002 YOK**) | ARP→reply, ICMP ping→pong, DHCP lease | yok |
| **dosya-sistemi** | C: RAM-FS (`FILE OK`/`LS count=2`) + gerçek blok-FS `MINIFS OK`/`CRASHFS OK`/`FS JOURNAL OK` (inode+bitmap+WAL, ~300-500 LOC/kernel, virtio-blk üstünde). **kem_os: YOK** | Test-kernel FS'i runtime modülüne refactor → .kem (mekanik) | virtio-blk | **P1/P2** (virtio üstünden). alloc YOK (static buf+byte-loop→**T002 YOK**) | dosya yaz→oku→içerik eşleşme; crash→journal-kurtar | yok |

---

## 2. BAĞIMLILIK DAG + BRING-UP SIRASI

```
boot/start_aarch64.S  [P4-irreducible, .S KALIR: _start, VBAR, MMU-enable, vektör tablosu,
                       kdl_exc_ortak, kdl_irq_ortak(SP-swap), kdl_baglam_degis, kdl_el0_calistir]
   │  (kem_os aynı boot objesini linkler → BEDAVA miras)
   ▼
FAZ A — ÇEKİRDEK MEKANİZMA (kem_os'u iskeletten canlıya çevir)
   A1. MMU gerçek-gate        (P1+P2)  — zaten CANLI@boot; .kem page-table build + PAGEFAULT OK gate
   A2. kesme gerçek-trap      (P1,.S)  — [5] sentetik → vektör-bağlı gerçek fault-hook
   A3. zaman timer-IRQ        (P1)     — kdl_kesme_kur+kdl_timer_baslat → tik canlı
       └─ A4. görev preemptive (P1,.S) — 3-görev interleave → KEM-PREEMPT OK   ◄ EN ZOR entegrasyon
   A5. syscall/EL0            (P1,.S)  — EL0 görev SVC round-trip + IZOLASYON OK

FAZ B — DEPOLAMA (polling, IRQ-suz, alloc-suz, .S-suz → EN TEMİZ)
   B1. virtio-blk sektör I/O  (P1+P2)  — DISK RW OK
       └─ B2. dosya-sistemi   (refactor) — minifs yaz→oku→eşleşme; crashfs crash-kurtar

FAZ C — AĞ (polling, IRQ-suz, alloc-suz; net-stack SIFIRDAN → EN BÜYÜK)
   C1. virtio-net queue       (P1+P2)  — cihaz + RX/TX queue
       └─ C2. net-stack       (sıfırdan) — ARP→reply → ICMP ping → DHCP lease → DNS → HTTP
```

**Önerilen sıra gerekçesi:** FAZ A çekirdek (birbirine bağlı, seri; A4 en zor — timer→IRQ→preempt→
context-switch zinciri). FAZ B ve FAZ C **A'dan bağımsız** (virtio polling; kesme/IRQ GEREKMEZ) →
A tamamlanınca **paralel** gidebilir. B mekanik (kanıtlı-C refactor); C en büyük (net-stack sıfırdan).

---

## 3. ENABLING PRİMİTİFLER — birleşik + İLK-gereksinim

| Primitif | Durum | İLK nerede gerekli | Kapsam |
|----------|-------|--------------------|--------|
| **P1** (aarch64 satıriçi_asm) | ✅ **YAPILDI (D-269, `--mimari arm64`)** | A1 (MMU msr commit) + A3 (timer/DAIF msr) | TÜM (msr/mrs + dsb bariyer). Baskın, hazır. |
| **P2** (veri hizalama) | Manuel over-align (allocator 16-align deseni) | B1 (virtio-blk 16B ring) + A1 (MMU 4KB tablo) | virtio-blk/net ring (16B) + MMU sayfa-tablo (4KB). **RİSK:** 4KB base-align manuel over-align ile mümkün mü — DOĞRULA (16B kanıtlı, 4KB büyük). |
| **P3** (cross-file çıplak declare) | Yok (T002) | Belirsiz — virtio/net/fs TETİKLEMİYOR (static buf, alloc-suz, memcpy-suz) | Yalnız .kem OS çok-dosyalı olur + modüller birbirinin non-builtin fn'ini çağırırsa. Modül-sınırı başına DOĞRULA. |
| **P4** (naked-.kem + asm gövde) | Yok | **v1'de GEREKMEZ** (.S kalır) | Yalnız vektör/trap-frame/context-switch/eret'i .kem'e almak (Yasa-4 "sıfır-asm" mutlak hedefi) → v1 sonrası. |

**Kısa yol:** P1 hazır → FAZ A/B/C'nin msr+dsb ihtiyacı karşılandı. P2 (manuel over-align) FAZ B'de İLK
gerekli + A1 MMU 4KB'de doğrulanmalı. P3/P4 v1 için GEREKMEZ (tetiklenmiyor / .S kalıyor).

---

## 4. HER AŞAMANIN GERÇEK FONKSİYONEL GATE'İ (D-268 dersi: sembol yokluğu değil, gerçek iş)

| Aşama | Gerçek gate (kem_os'ta ÇALIŞTIRILABİLİR) | Kanıtlı-C referansı |
|-------|-------------------------------------------|---------------------|
| A1 MMU | Sayfa eşle → VA eriş → unmapped adres → data-abort → FAR==adr → kurtar → `PAGEFAULT OK` | kemgu_os_arm.c:618 |
| A2 kesme | Gerçek fault vektörden .kem-handler'a iner (3 sentetik literal DEĞİL) | start.S kdl_exc_ortak |
| A3 zaman | Timer IRQ tik sayacı ilerliyor (uptime++), IRQ canlı | capstone `TIMER OK` |
| A4 görev | 3 görev yield-suz, timer preempt eder, çıktıda `[A][B][C]` interleave → `KEM-PREEMPT OK` | preempt_arm.c `PREEMPT OK` |
| A5 syscall/EL0 | EL0 görev SVC(yaz)→çekirdek→dönüş; kötü-EL0-erişim→izolasyon-öldür → `IZOLASYON OK` | userspace_arm/d2_arm |
| B1 virtio-blk | Sektör 7 yaz → 0xEE ile doldur → oku → 32 byte eşleşme → `DISK RW OK` | virtio_rw_arm.c |
| B2 fs | Dosya yaz "hello" → oku → "hello"; (crashfs) crash → journal-replay kurtar | minifs_arm `MINIFS OK` |
| C1 virtio-net | RX q0 + TX q1 + DRIVER_OK; broadcast ARP request | virtio_net_selfhost `KEM NET OK` |
| C2 net-stack | ARP request → reply (oper=2, spa doğru) → `ARP REPLY OK`; sonra ICMP ping→pong; DHCP lease | arp_arm/icmp_arm/dhcp_arm |

---

## 5. DÜRÜST BELİRSİZLİK / RİSK

1. **net-stack SIFIRDAN + BÜYÜK.** `runtime/` net kütüphanesi YOK — her protokol (ARP/IP/ICMP/UDP/DHCP/
   DNS/HTTP/TCP) test-kernel'de inline, byte-byte. Yeniden kullanılabilir varlık = byte-layout referansları
   + 180-LOC virtio-net sürücü tasarımı. **v1-minimal = ARP+ICMP (~200 .kem LOC); tam parite ~1500+ LOC.**
   Bu, tek subsystem'de en büyük emek. v1 için minimal (ARP→ping) önerilir; tam net-stack ayrı faz.

2. **FAZ A entegrasyon-ağır, seri, EN ZOR.** kem_os LİNEER iskelet — timer→IRQ→preempt→context-switch→EL0
   zincirini AKTİFLEŞTİRMEK "mekanik port" DEĞİL. Runtime + .S HAZIR/kanıtlı ama kem_os'a wire edilmemiş.
   [5] sentetik-exc'i gerçek-vektör-trap'e çevirmek (A2) A3/A4/A5'in önkoşulu. Bu faz "aktivasyon-ağır".

3. **.S/P4 sınırı — v1'de .S kalır, ama "sıfır-asm" mutlak hedefiyle gerilim.** vektör/trap-frame/context-
   switch/eret irreducible asm. Yasa-4 "sıfır C" bunları kapsamaz (asm≠C) → v1'de .S. AMA "OS TAMAMEN .kem"
   yorumu asm'i da isterse P4 gerekir (fayda yok + risk) — **Mehmet kararı**. Öneri: v1'de .S kalsın.

4. **P2 4KB-align belirsiz.** MMU sayfa-tabloları 4KB-hizalı base ister. Allocator 16-align kanıtlı; 4KB
   manuel over-align (küresel static + `(p+4095)&~4095`) ile mümkün görünüyor ama DOĞRULANMADI. A1'de test et.

5. **P3 cross-file modül-sınırında belirsiz.** virtio/net/fs kendi içinde T002 tetiklemiyor (static, alloc-suz).
   AMA .kem OS çok-dosyalı olursa (subsystem başına dosya) + modüller birbirinin non-builtin fn'ini çağırırsa
   T002 duvarı. Şu an tek-dosya (kem_heap.kem) deseni bunu aşıyor ama ölçeklenmiyor. Modül-mimarisi kararı
   gerekebilir (P3 primitifi VEYA tek-dev-dosya). İLK gerçek ihtiyaç FAZ A4/A5'te (görev/EL0 birden çok .S
   externi + kendi tasklarını çağırır) net görülecek.

6. **Sentetik-gate tuzağı (D-264 dersi tekrar).** Her aşama GERÇEK fonksiyonel gate ile doğrulanmalı — sembol
   varlığı/yokluğu VEYA marker-PASS yetmez. kem_os'un mevcut [5]'i tam da bu tuzağın örneği (sentetik ESR
   decode "EXC OK" der ama gerçek trap yok). Bring-up'ta her subsystem QEMU'da GERÇEK iş yapmalı + gerçek
   çıktı denetlenmeli.

---

## 6. AMPİRİK KANIT ÖZETİ (bu keşifte gerçekten koşturulan/okunan)

| İddia | Kanıt |
|-------|-------|
| Entegre C OS tek boot'ta tüm subsystem'ler | kemgu_os_arm.c (767 satır) + master gate 13 assert (Makefile:3244-3266) |
| kem_os LİNEER iskelet, scheduler/timer/EL0 YOK | kem_os.kem main [1..5] düz akış; 5 okuyucu doğruladı sıfır kdl_preempt/kesme_kur/el0 çağrısı |
| kem_os [5] sentetik (vektöre bağlı değil) | kem_os.kem:310-315 3 hard-coded ESR literali; yorum "gerçek fault-routing SONRAKİ adım" |
| MMU CANLI@boot ama C-kuruluyor | start.S:75 `bl kdl_mmu_kur` main-öncesi; KEM_OS_A64_OBJS'de bm_a64_mmu.o |
| virtio-blk kem_os'ta DORMANT (magic-only) | kem_os.kem yalnız mmio_oku32(MAGIC); C sürücü kesme dead-dep ile linkli |
| Preemptive scheduling PROVEN | preempt_arm.c `PREEMPT OK` (yield-suz, timer zorlar); +priority/sleep/multiproc |
| virtio-blk RW PROVEN | virtio_rw_arm.c `DISK RW OK` (sektör yaz→oku→32B) |
| net-stack sıfırdan (runtime lib YOK) | runtime/ yalnız L2 sürücü; ARP..HTTP her test-kernel'de inline |
| gerçek blok-FS PROVEN (inode+WAL) | minifs_arm `MINIFS OK`, crashfs_arm `CRASHFS OK` |
| .S/P4 irreducible surface | start.S: vektör(.balign 0x800), kdl_exc_ortak, kdl_irq_ortak(SP-swap+eret), kdl_baglam_degis, kdl_el0_calistir |
| P1 hazır | D-269 `--mimari arm64` → `mrs x8, CNTPCT_EL0` assemble oldu |
| net/fs/virtio T002 tetiklemiyor | tümü static buf + inline byte-loop; memcpy/malloc çağrısı YOK |
