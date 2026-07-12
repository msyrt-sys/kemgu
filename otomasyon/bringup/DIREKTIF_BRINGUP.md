# KEMGU REAL-OS BRING-UP — EXECUTOR SABİT DİREKTİFİ

Bu direktif her bring-up görevinde SABİTtir. Loop harness (`bringup-loop.sh`) seni her tur
güncel görevle çağırır. Görev spec'ini uygula, gate.sh geçerse commit+push, geçmezse commitleme.

## BAĞLAM
Real-OS bring-up = **AKTİVASYON** (gerçek işlevsellik çalıştır + gerçek testle doğrula), migration
DEĞİL. Kanıtlı-C referansları: `test/bare_metal/minifs_arm.c`/`crashfs_arm.c` (fs), `runtime/kdl_virtio_net.c`
+ `test/bare_metal/{net,arp,icmp}_arm.c` (net). Roadmap: `docs/BRINGUP_ROADMAP.md`.

## D-271 (B1) DESENİ — HER GÖREVDE UYGULA
- Sürücü KENDİ `.kem` dosyasında (`runtime/kem_<ad>.kem`). Build'de kem_os.kem ile **CAT** → tek
  derleme birimi → çıplak→çıplak çözülür, **T002 yok** (`dış işlev` extern YOK). (bkz `runtime/kem_virtio_blk.kem`.)
- **MMIO + DMA-RAM = çıplak VOLATILE deref** (`store/load volatile`). ÖNEMLİ: deref-write'ta DEĞER-genişliği
  pointee'yi belirler → 64-bit alan için `tam64` DEĞER ver (yoksa i32 yazar, üst 4 bayt çöp).
- **Bariyer = satıriçi_asm arm64**: `güvensiz { satıriçi_asm { mimari: arm64 şablon: r#"dsb sy"# bozulan("~{memory}") } }`.
  `bozulan` FULL LLVM-constraint alır (`~{memory}`, `~{cc}`). kem_os build `--mimari arm64` ile (D-269 P1).
- **DMA tampon = SABİT identity-RAM adresi + MANUEL hizalı offset** (B1: 0x43000000, EL0-üstü/128MB-backed/
  Normal-WB; alt-tamponlar 16-katı offset). Net için de aynı desen; **allocator/malloc YOK** (static-adres).
- kem_os.kem'e **additive [N] blok** ekle (çekirdek [1..5] gate'ini ETKİLEME — disksiz/netsiz boot hâlâ
  KEM-OS OK vermeli). Makefile `calistir_kem_os_arm`: yeni sürücüyü CAT'e ekle + gerekiyorsa QEMU cihazı
  (net için `-netdev user -device virtio-net-device`) + yeni marker assert.

## GÖREV SPEC'LERİ
- **b2-fs** → `[7] FS RW OK`: minifs saf-.kem, `vblk_*` (virtio-blk) sektör I/O üstünde (kanıtlı-C
  minifs_arm.c refactor). Gate: format → dosya oluştur → RASTGELE içerik YAZ → OKU → içerik+boyut EŞLEŞME.
- **virtio-net** → `[8] NET DEV OK`: POLLED virtio-net transport (.kem, CAT; kdl_virtio_net.c deseni —
  IRQ YOK, used-ring poll). Gate: feature-negotiate + RX/TX queue kur + DRIVER_OK.
- **net-arp** → `[9] NET ARP OK`: gateway (10.0.2.2) için ARP request GÖNDER → QEMU SLIRP reply AL →
  gateway MAC EŞLEŞME (oper=2 + spa doğru).
- **net-icmp** → `[10] PING CANLI`: gateway'e ICMP echo → reply → seq/id + checksum EŞLEŞME.

## GERÇEK GATE ZORUNLU (D-264 dersi)
Her fonksiyonel test DOĞRULANABİLİR iş yapmalı: rastgele-pattern byte-eşleşme / checksum-doğrulama /
protokol-alan-eşleşme. ASLA hardcoded marker yazma. **Sentetik-geçiş imkânsız olsun.** GERÇEK QEMU seri
çıktısını denetle (marker-PASS yetmez → garbling yok, gerçek değer doğru).

## DUR / HONEST-FLAG (KOMUTA.md'ye "DUR: <sebep>" yaz + dur, UYDURMA)
- Görev roadmap'ten BÜYÜK çıkarsa (queue/protokol beklenenden karmaşık).
- Yeni codegen/dil gap gerekiyorsa (mevcut primitiflerle ifade edilemiyor).
- Cross-file non-builtin gerekiyor VE CAT çözmüyorsa (T002).
- **FAZ-A bağımlılığı**: net POLLED olmalı (IRQ/kesme/vektör GEREKMEZ). IRQ gerekiyorsa DUR — FAZ-A.
- FIXPOINT korunamıyorsa (kesinlikle codegen'e dokunma; dokunulmamalı).

## KURALLAR
- ASLA yeni branch. Yalnız `os/c1-region-backing`. Her iş öncesi `git rev-parse HEAD == origin`.
- Dosyalar: `runtime/kem_<ad>.kem` + `test/ornekler/kem_os.kem` + `Makefile` + gerekirse gate cihazı.
- gate.sh `<id> <marker>` YEŞİL → commit+push (D-numarası merge-anında güncel main'e göre). KIRMIZI →
  commitleme, KOMUTA.md'ye durum yaz.
- Her görev sonunda KOMUTA.md'ye checkpoint JSON (görev/durum/kanıt).
