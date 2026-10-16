# `baremetal_diff` kapsamı — birimler, parçalar ve neden "atlandı" yok

> **D-675 (2026-10-12).** `test/baremetal_diff_harness.sh` (`make
> calistir_baremetal_diff`) aarch64 bare-metal yüzeyini C oracle ↔ self-host
> codegen arasında YAPISAL olarak karşılaştırır (define kümesi, üçlü, asm /
> volatile / inttoptr sayısı). Bu belge kapının neyi, hangi birimle ve hangi
> ölçümle kapsadığını kayda geçirir.

## Kusur: kapı geçiyor ama hiçbir şey ölçmüyordu

Onarım öncesi kapı, oracle'ın derleyemediği her birimi **"atlandı"** sayıyor ve
`rc=0` dönüyordu. Ölçüldü (2026-10-12, onarım ÖNCESİ, aynı sabotajlar):

| Sabotaj | Onarım ÖNCESİ | Onarım SONRASI |
|---|---|---|
| A — birleşik birimden `kem_uart` parçası düşürüldü (T002) | `rc=0` · "2/2 birim (9 atlandı)" — **YEŞİL** | `rc=2` · `🔴 BİRLEŞİK OS — C oracle DERLEYEMEDİ` |
| B — `kem_mmu.kem`'e tip hatası | `rc=0` · "2/2 birim (9 atlandı)" — **YEŞİL** | `rc=2` · aynı |
| C — sınıflandırılmamış yeni `runtime/*.kem` | (sessizce atlanırdı) | `rc=2` · `🔴 SINIFLANDIRILMAMIŞ` |
| D — birleşik birimde OLMAYAN sembollü dosya parça sayıldı | (ölçüm yoktu) | `rc=2` · `🔴 birleşik birimde EKSİK: C:@x86_net_tx_oku KEMGU:@x86_net_tx_oku` |

A ve B'de çekirdeğin tamamı (326 işlev) ölçülmeden kapı yeşildi. Bu,
ZERO-C audit'te "pure-.kem" iddiasını yanlış çıkaran körlükle aynı sınıftır
(D-446/D-486: var olan ama ölçmeyen kapı, olmayandan tehlikelidir).
Ayrıca D-671/D-672 `kem_heap` ve `kem_virtio_blk`'ı mimari başına dosyalara
bağladığında ikisi tek başına derlenmez olmuş ve **sessizce** "atlandı"ya
düşmüştü (5/5 → 3/3).

## Yeni kurallar

1. **Birleşik OS birimi** (Makefile `calistir_kem_os_arm` cat sırası) derlenemez
   ya da parçası eksikse → KIRMIZI.
2. **BIRIMLER** — tek başına ya da mimari bağımlılıklarıyla derlenen birimler.
   Derlenemezse → KIRMIZI.
3. **PARCALAR** — tek başına derlenemeyen dosyalar. Her birinin dosyadan okunan
   TÜM `işlev` ve `küresel değişken` adları, birleşik birimin **hem C hem
   KEMGU IR'ında** tanımlı olmalı (`define … @ad(` / `@ad = `). Eksik → KIRMIZI.
   Parça tek başına derleniyorsa → KIRMIZI (bayat sınıflandırma, D-534).
4. **Sınıflandırma bütünlüğü** — `runtime/*.kem` ve `runtime/mimari/arm64/*.kem`
   içindeki her dosya 2 ya da 3'te olmalı; değilse → KIRMIZI.

"Atlandı" sayacı kaldırıldı; özet satırı `0 atlandı` der ve bu bir iddia değil,
kodun yapısıdır.

## Birimler (ölçülen, 2026-10-12)

| Birim | Dosyalar | define (C = KEMGU) |
|---|---|---|
| BİRLEŞİK OS | 13 dosya (aşağıdaki tüm parçalar + birimler) | 326 · 48 asm · 101 volatile · 116 inttoptr |
| `kem_dtb` | `kem_dtb.kem` | 41 |
| `kem_kanal_abi` | `kem_kanal_abi.kem` | 1 |
| `kem_heap` | `kem_heap.kem` + `mimari/arm64/kem_uart.kem` | 32 — **geri alındı** |
| `kem_bariyer` | `mimari/arm64/kem_bariyer.kem` | 1 — yeni kural 3 yakaladı (tek başına derleniyordu) |
| `kem_virtio_blk` | `kem_virtio_blk.kem` + `mimari/arm64/kem_bariyer.kem` + `mimari/arm64/kem_platform.kem` | 17 — **geri alındı** |

`kem_heap` ve `kem_virtio_blk` istenen biçimde listeye geri döndü: tek başına
değil, D2b'nin getirdiği **mimari bağımlılıklarıyla birlikte** küçük birimler
olarak.

## Parçalar — neden tek başına derlenmiyor, nasıl kapsanıyor

Neden: `kemgu --check --mimari aarch64 <dosya>` → T002 (tanımsız sembol).
KEMGU'da üst düzey ileri bildirim YOK; çapraz dosya çağrısı tek derleme birimi
(CAT) gerektirir. İlk tanımsız kullanım ölçüldü:

| Parça | T002 | İlk tanımsız kullanım → bağımlılık | Birleşik birimde ölçülen |
|---|---|---|---|
| `kem_elf` | 45 | `vblk_oku` → kem_virtio_blk | 13 işlev |
| `kem_gorev` | 53 | `KMMU_L1` → kem_mmu | 58 işlev + 3 küresel |
| `kem_minifs` | 35 | `vblk_data_yaz` → kem_virtio_blk | 4 işlev |
| `kem_mmu` | 15 | `KZ_GICD` → kem_zaman | 44 işlev |
| `kem_zaman` | 13 | `kmmu_gicd_taban` → kem_mmu (**karşılıklı** mmu ↔ zaman) | 11 işlev + 2 küresel |
| `kem_virtio_net` | 85 | `VM_YUVA_SAYISI` → platform; `vblk_*` → blk | 10 işlev + 2 küresel |
| `mimari/arm64/kem_uart` | 1 | `kdl_mmio_yaz32` → kem_heap | 1 işlev |
| `mimari/arm64/kem_platform` | 0 | (tek başına derlenir ama **hiç işlev yok** — birim olsaydı boş define kümesi karşılaştırılırdı = ölçmeyen kapı, D-425) | 2 küresel |
| `test/ornekler/kem_os.kem` | 242 | `kmmu_uart_taban` → kem_mmu (çekirdeğin kendisi) | 93 işlev |

**Toplam: 234 işlev + 9 küresel** tek tek adıyla birleşik birimin C ve KEMGU
IR'ında doğrulandı. `sabit`ler IR'da sembol bırakmaz (satır içine açılır) →
sayılamaz; onları birleşik birimin derlenmesi kapsar.

Bayat sınıflandırma kuralı `grep '^define'` ister: hiç işlev tanımlamayan bir
parça (platform) tek başına derlense bile "birim" sayılmaz — bilinçli.

## Kapsam dışı (bilinçli, başka kapılarda)

- `runtime/mimari/x86_64/*.kem` — bu kapı aarch64 hedefler; x86 etiketli asm
  AS001 verir. x86 birleşik birimi `check_kapisi` (`--mimari x86_64`, 7 parça)
  ve `calistir_kem_os_x86` (QEMU microvm, Kanıt 1-10) tarafından ölçülür.
- **D-680:** x86 birleşik birimi (parça listesi Makefile `calistir_kem_os_x86` CAT
  satırından okunur) bölüm (5)'te C ↔ self-host karşılaştırılır: define kümesi,
  üçlü, asm sayısı + şablon/kısıt dizgileri, volatile, inttoptr. Sınıflandırma
  `runtime/mimari/x86_64/*.kem`'i de kapsar (CAT'te olmayan → KIRMIZI).
