# CODEX GÖREVİ 19 — ARM: GICv3 + KVM + çok çekirdek (SMP) · dosya sistemi dizin önbelleği · süreç yalıtımı

Dalga 1'in kuralları geçerlidir (`otomasyon/chatgpt/CODEX_GOREV.md` §2, §3, §5 — yeniden oku),
aşağıdaki **izin genişletmesi** dışında. Dalga 6'nın gerçek-QEMU kâhin kuralı da geçerlidir
(`CODEX_GOREV_6.md` §0). Dalga 18'in test altyapısı kuralları da geçerlidir: geçici dizin
(`test/calisma_dizini.sh`), kapı bütçesi (`test/kapi_butce.tsv`), kapı haritası (`test/kapi_haritasi.tsv`,
`python3 tools/kapi/kapi_haritasi.py --yaz`), paylaşım denetimi. Soru sorma, durma; takılırsan (60 dk)
kaydet ve geç.

## ⏱ Çalışma biçimi — bu dalga da uzundur
- Bu dalga **yirmi bölümdür, HEPSİ zorunlu ve sırayla yapılır.** Bölüm 1-7 ARM kesme denetleyicisi ve KVM,
  Bölüm 8-16 ARM SMP, Bölüm 17-20 dosya sistemi, yalıtım, regresyon ve belgelerdir. KVM bölümleri SMP
  bölümlerinin koşum süresini doğrudan kısaltır; sırayı bozma. **30+ saat normaldir.**
- Bir bölümü bitirince **durma, rapor yazıp bekleme, özet sunup onay isteme — sonraki bölüme geç.** "Bu kadar
  yeterli" deme; yirmi bölümün hepsi bitene ya da her biri için takılma notu yazılana kadar çalış.
- Her bölümün sonunda: (1) o bölümün kapıları + sabotajları, (2) ayrı commit
  (`SURUCU(codex): Dalga 19 bölüm N — ...` ya da `ALTYAPI(codex): Dalga 19 bölüm N — ...`), (3)
  `codex19_ilerleme.md`'ye kısa not (yapılan, ölçümler, sabotaj sonuçları, süre). Dalga yarıda kesilse bile
  biten bölümler kullanılabilir olmalı.
- Gerçekten takılırsan (60 dk ilerleme yok): `codex19_bulgular.md`'ye yaz, bölümün yapılabilen kısmını
  kapılı bırak ve **sonraki bölüme geç**.
- Her 5 bölümde bir (5, 10, 15) ara regresyon: `make -j10 --output-sync=target test_tumu`; kırmızı varsa
  bir sonraki bölüme geçmeden önce kökünü bul.
- **Her sabotajı gerçekten uygula, çıktısını yaz.** Uygulamadığın sabotaj varsa bunu AÇIKÇA yaz; "benzer bir
  sabotaj" ile değiştirme. Sabotaj sessiz kalırsa önce korpusu/kâhini düzelt (dalga 17 ve 18'de defalarca
  oldu: korunan şekli korpus üretmiyordu ya da ölçüm yanlış yerdeydi); fazlalıksa ikisini birden kaldırıp
  ayrıca ölç ve ikisini de yaz.
- **Draft PR'ı açtıktan sonra son commit'i değiştirip zorla gönderme**; ek değişiklik gerekiyorsa yeni commit.
- Draft PR'ı yalnız yirmi bölüm bittikten (ya da her biri için takılma notu yazıldıktan) sonra aç.

---

## 0. Hedef

**(A) ARM kesme denetleyicisi ve KVM (Bölüm 1-7).** Bu makine (DGX Spark) ARM64'tür ve `/dev/kvm` vardır:
ARM konukları KVM ile **gerçek çekirdeklerde** koşabilir (x86 konuklar her zaman TCG'dir). Claude ölçtü
(§1): ARM duman çekirdeği ve iki çekirdekli `smp_queue_arm` KVM'de koşuyor, ama KEMGU-OS ARM çekirdeği
zamanlayıcı fazında takılıyor. Kök: Spark'ın KVM'i **GICv2 öykünmesini desteklemiyor**, çekirdeğin kesme
sürücüsü ise **yalnız GICv2** biliyor. Bu dalga GICv3 sürücüsünü ekler (GIC sürümü DTB'den seçilir; aynı imaj
GICv2 TCG'de de, GICv3 TCG/KVM'de de koşar), ARM QEMU koşumlarını tek bir yürütücüye toplar (KVM varsa KVM,
yoksa TCG; CI'daki x86 makinelerde TCG), sabit süreli beklemeleri "son satır gelince çık"a çevirir ve ARM
kapılarının süresini ölçerek düşürür.

**(B) ARM SMP (Bölüm 8-16).** KEMGU-OS ARM bugün tek çekirdekte koşar. Bu dalga ikincil çekirdekleri PSCI
`CPU_ON` ile başlatır, her çekirdeğe yığın/vektör/MMU/GIC yeniden dağıtıcısı/sanal zamanlayıcı verir,
çekirdek kilitlerini, SGI tabanlı işlemciler arası kesmeleri ve yayınlı TLB geçersizlemeyi (`TLBI ...IS`)
ekler, fiziksel çerçeve ayırıcısını ve heap'i ARM'de de SMP-güvenli yapar, kullanıcı süreçlerini birden çok
çekirdekte zamanlar ve hepsini **bağımsız kâhinle** (QMP + seri + host hesabı) `-smp 1/2/4/8` × {TCG, KVM}
matrisinde kanıtlar. Dalga 18'in x86 SMP tasarımı (`belgeler/KEMGU_OS_SMP.md`) başvurudur; ARM'in farkları
(PSCI, GICv3 SGI, yayınlı TLBI, zayıf bellek sıralaması) açıkça ele alınır.

**(C) Dosya sistemi, yalıtım, regresyon (Bölüm 17-20).** FAT32 sürücüsünde dizin ayrıştırma önbelleği (D(1)),
süreç dosya tanımlayıcısı yalıtım kapısı (H9j/E3f), tam regresyon ve belgeler.

Kapsam DIŞI: ARM'de ACPI, ITS/LPI ve MSI (ITS DTB'de var ama kullanılmaz), GICv4, NUMA, CPU hotplug, büyük.KÜÇÜK
çekirdek farkına göre zamanlama, x86 değişiklikleri (Bölüm 17-18 ve paylaşılan sürücüler hariç), derleyici.

---

## 1. Bugünkü durum (Claude ölçtü — varsayma, yine de kendin doğrula)

### Makine ve KVM (2026-11-27)
- DGX Spark, **20 çekirdek** (10 Cortex-X925 + 10 Cortex-A725), QEMU 8.2.2, sayfa boyu 4 KB, glibc 2.39.
- `/dev/kvm`: `crw-rw----+ root kvm`, ACL `user:msyrt:rw-` → bu kullanıcı KVM kullanabilir (grup üyeliği yok,
  ACL ile). Spark'ın KVM'i **GICv2 öykünmesini DESTEKLEMEZ**:
  `qemu-system-aarch64 -M virt,gic-version=2 -accel kvm -cpu host` → `KVM does not support GICv2 emulation`.
  KVM'de `-M virt,gic-version=3` zorunludur (`-M virt` tek başına da v3 seçer).
- **QEMU `virt` DTB'si (Claude `dumpdtb` ile döktü, kendin tekrar dök):**

  | | GICv3 (`gic-version=3`, KVM) | GICv2 (`-M virt`, TCG varsayılanı) |
  |---|---|---|
  | `compatible` | `arm,gic-v3` | `arm,cortex-a15-gic` |
  | `reg` | GICD `0x08000000`+`0x10000`, GICR `0x080A0000`+`0xF60000` | GICD `0x08000000`+`0x10000`, GICC `0x08010000`+`0x10000` |
  | ek | `#redistributor-regions = 1`, ITS `0x08080000` (kullanılmaz) | v2m `0x08020000` (kullanılmaz) |
  | PSCI | `arm,psci-1.0`, `method = hvc`, `cpu_on = 0xC4000003` | aynı |
  | zamanlayıcı | `arm,armv8-timer`: PPI 13/14/11/10 (sanal = PPI 11 = **INTID 27**) | aynı (v2'de CPU maskesi `0xf04`) |
  | `cpu@N` | `reg = N` (MPIDR afinitesi), `enable-method = psci` | aynı |

- **Claude'un KVM denemeleri:**
  - `build/kernel.elf` (ARM duman çekirdeği) `-M virt -accel kvm -cpu host` → `Merhaba KEMGU - Bare Metal`, `42`.
  - `build/kem_os.img` (KEMGU-OS ARM) `-M virt,gic-version=3 -accel kvm -cpu host` → 34 `OK` satırı
    (`[41] KANAL OK`'a kadar), `[5] TIMER TIK OK` hiç gelmiyor. **Aynı imaj TCG + `gic-version=3`'te de aynı
    fazda takılıyor** (6 sn'de 8 OK) → kusur KVM değil, GIC sürücüsü.
  - `smp_queue_arm.elf` `-smp 2 -accel kvm -cpu host` → `CPU_ON conduit=HVC ret=0x0`, toplam 20540, `SMP QUEUE OK`.
    20 koşuda: KVM 14 OK / 6 "KISMI", TCG 19 OK / 1 "KISMI". "KISMI" bellek hatası DEĞİL: toplam doğru ama
    bir çekirdek 40 işin hepsini öbürü başlamadan bitiriyor (CPU_ON gecikmesi) — bkz. Bölüm 5.
  - D-490 sabotajı (`smp_queue_arm.c`'de `dc civac/ivac` + eşlik eden iki `dsb sy` → `nop`) KVM'de de
    yanlış toplam/asılma ÜRETMEDİ (temiz ve sabotajlıda aynı dağılım). Büyük olasılıkla KVM, MMU'su kapalı
    konuk erişimlerini önbellekli yapıyor (FEAT_S2FWB) → bariyerlerin var olduğu önbelleklenebilirlik
    uyuşmazlığı oluşmuyor. **KVM, D-490'ın fiziksel donanım doğrulamasının yerini TUTMAZ**; bunu belgede
    açıkça yaz, D-490'ı kapatılmış sayma (`belgeler/ARM64_Fiziksel_Donanim_Kontrol_Listesi.md`).

### ARM çekirdeği bugün
- Kesme + zamanlayıcı: `runtime/mimari/arm64/kem_zaman_hw.kem` (79 satır). **GICv2**: `KZ_GICD = 0x08000000`,
  `KZ_GICC = 0x08010000`; `kdl_kesme_kur`: GICD_CTLR=1, GICC_PMR=0xFF, GICC_CTLR=1, GICD_ISENABLER0 bit 27;
  `kdl_irq_isle`: GICC_IAR (+12) oku, GICC_EOIR (+16) yaz; zamanlayıcı **CNTV** (sanal: `cntv_tval_el0`,
  `cntv_ctl_el0`). Adresler `kmmu_gicd_taban`/`kmmu_gicc_taban` (`runtime/kem_mmu.kem` 169/174) ve DTB
  keşfinden (`runtime/kem_dtb.kem` `kdtb_gic_dist`/`kdtb_gic_cpu`; KEMGU-OS fazı `[22] DTB KESIF`).
- Önyükleme: `boot/start_aarch64.S` (415 satır): SP=`__stack_top`, EL2/EL1'e göre CPTR/VBAR, `kdl_mmu_kur`,
  `kem_heap_kur`, `main`. Tek çekirdek; ikincil giriş yok. KVM konuğu EL1'de başlar.
- Diğer arm64 katmanı: `runtime/mimari/arm64/{kem_bariyer,kem_gorev_hw,kem_kesif,kem_platform,kem_uart}.kem`;
  ortak: `runtime/{kem_zaman,kem_mmu,kem_dtb,kem_gorev,kem_heap}.kem`; çekirdek: `test/ornekler/kem_os.kem`.
- Başvuru (çağırma, kopyalama): `test/bare_metal/smp_arm.c`, `smp4_arm.c`, `smp_queue_arm.c` (PSCI HVC→SMC
  yedekli `CPU_ON`, ikincil giriş MMU KAPALI, EL1), `smp_atomic_arm.c`, `smp_barrier_arm.c`, `smp_mcs_arm.c`,
  `smp_rwlock_arm.c`, `smp_prodcons_arm.c`, `smp_compute_arm.c`.

### ARM kapıları bugün
- Makefile'da `qemu-system-aarch64` 150 kez geçer (bir kısmı `command -v` denetimi); **76'sı
  `timeout N qemu-system-aarch64` çağrısıdır**. Aynı satırda görünen seçeneklerden sayılan: 72
  `-M virt -cpu cortex-a72`, 10 `-smp 2`, 2 `-smp 4` (seçenekleri devam satırında olan çağrılar bu sayıma
  girmez — tam envanter Bölüm 1'in işi). Çoğu **sabit süre** bekler (`timeout 12` 24 yer, `timeout 20` 26
  yer, ayrıca 8/10/15/25/30/40/60 s): konuk işini bitirse de süre dolana kadar beklenir.
- `test_tumu` içinde ARM: `calistir_qemu_cekirdek` (önkoşulları `calistir_qemu_smoke`, `calistir_kem_os_arm`,
  `calistir_ring3_page_test_x86`), `calistir_kem_os_arm_ag`, `calistir_arm64_test`. Diğer ARM hedeflerinin çoğu
  `calistir_os_kernels` toplu hedefinde (takımda değil). `tests/qemu/ag_arm_kahin.py`, `ag_arm_derle.py`.
- CI (`.github/workflows/ci.yml`) x86_64 Linux + Windows'ta koşar: orada ARM konukları **TCG**'dir ve öyle
  kalmalı.

### Dosya sistemi (D(1)) — D-781 ölçümü
- `mv` tek komutta `dizin_coz` (`drivers/fs/fat32_dizin.kem:39`, LFN tamponu `bayt_sifir(520)` :42) **17 kez**
  çalışıyor (`cp` 10, `yaz` 7, `mkdir` 3). ⚠ D-779'da yazılan "181" YANLIŞTI (181 = tüm `bayt_sifir`
  çağrıları; geri kalanı USB G/Ç geçicileri: 100 TRB 16 B, 24 sektör 512 B, 19 CBW 31 B, 19 SCSI CDB 10 B).
- İki okuma yolu da `dizin_coz`a varır: `liste(s,c)` (`drivers/fs/fat32_sektor.kem:63`) ve
  `fsy_dizin` (`fat32_sektor_yaz.kem:76`) → `fsy_kayitlar` (:94) → `imaj_kayitlar` (`fat32_imaj.kem:93`).
- Sürücü sans-IO: `fat_sektor_al` (`fat32_sektor.kem:20`) eksik sektörde `s.gerekli=lba` + `hata("gerekli")`;
  çekirdek sektörü okuyup `fat_sektor_koy` ile ekler ve **işlemin tamamını baştan koşturur**
  (`kd_yaz`/`kd_liste` `test/ornekler/kem_os_x86_kabuk.kem`, `iken i < 257`). Host ölçümü (Claude, yeniden
  deneme öykünmesi `tests/drivers/fs/fat32_sektor_yaz_test.kem` deseniyle): taze imajda tek yazma planı
  6-7 koşu, koşu başına 1 `dizin_coz`.
- `FatSektor` her işlemde TAZE kurulur (`kd_fat` → `fat_sektor_yeni`); sektör önbelleği (`kayitlar`, en çok
  256) yalnız EKLENİR, yazma planı sektörü değiştirmeden önce KOPYALAR (`fsy_kopya`, `fsy_duzenle`). Her
  işlem aynı sektörleri diskten yeniden okur (`mv`de ~19 sektör G/Ç'si).
- `kd_icerik_surumu` (`kem_os_x86_kabuk.kem:536`) FAT yazma planında, ramfs yaz/taşı ve süreç dosya kapatmada
  artıyor; emsal önbellek: `ks_program_hazirla` (:769).

### Süreç yalıtımı (H9j/E3f)
- D-763 aynı sanal adreste farklı fiziksel çerçeveleri kanıtlıyor; **dosya tanımlayıcısı yalıtım kapısı
  YOK** (yol haritası H9j "kısmen"). Dosya tanımlayıcıları süreç bağlamında (`KsvBaglam.fds`,
  `test/ornekler/kem_os_x86_vfs_ortak.kem` `ksa_islem`, `ksv_fd_gecerli`).

### Test altyapısı (dalga 18 + D-782/D-783 sonrası)
- Tam takım `make -j10 --output-sync=target test_tumu_olcum`: 190 kapı, **19:56** (D-784, 2026-11-27).
  En yavaş 15 kapının hepsi x86 QEMU (ağ aktarımı, ekran/fare, PS/2, dosyalar).
- Self-host derleyici büyük birimlerde 23× hızlandı (D-782/D-783): çekirdek derlemesi artık darboğaz değil.
- Linux CI ~9 dk, Windows CI ~47 dk.

---

## 2. Ön koşul ve çalışma alanı
```bash
cd /home/msyrt/Desktop/kemgu && git fetch origin
git log origin/main --oneline | grep -E "D-784" | head -2   # yoksa 5 dk'da bir, en çok 2 sa; sonra DUR, kaydet
git worktree add ../kemgu-codex19 -b codex/arm-smp-dalga19 origin/main && cd ../kemgu-codex19
make -j8 build/kemgu build/codegen build/kdl_runtime.o
make -j10 --output-sync=target test_tumu_olcum          # TABAN: build/zaman_raporu.txt'yi codex19_bulgular.md'ye kopyala
date > otomasyon/chatgpt/codex19_ilerleme.md
```
Tek dal, tek draft PR. ⚠ Claude bu dalga sırasında derleyicide ve bölge tahliyesinde (E4k-A) çalışabilir;
**dalını dalga ortasında main ile birleştirme** — Claude entegrasyonda birleştirir. Tam takımı her zaman
`-j10 --output-sync=target` ile koş.

⚠ Disk denetimi önceki koşumlardan kalan geçici dizinleri de kırmızı sayar; taze worktree'de sorun olmaz.

---

## 3. Okunacaklar
- `LOOP.md` D-776..D-784 (özellikle D-778 dar eleman: konuk belleğini okuyan kâhinler yuva genişliğini
  `eleman_byte`'tan almalı; D-781 dizin_coz ölçümü; D-784 dalga 18 birleştirmesi).
- `otomasyon/chatgpt/codex18_bulgular.md` ve `codex18_ilerleme.md` (x86 SMP kararları, ölçüm yöntemleri).
- `belgeler/KEMGU_OS_SMP.md`, `KEMGU_OS_Mimari.md`, `KEMGU_OS_Syscall_ABI.md`, `KEMGU_OS_Sabotaj_Envanteri.md`,
  `KEMGU_Test_Altyapisi.md`, `KEMGU_OS_Donanim_Yol_Haritasi.md`, `ARM64_Fiziksel_Donanim_Kontrol_Listesi.md`,
  `belgeler/KEMGU_E4k_Uzun_Omur_Tasarimi.md` §9.6 (çok çekirdek kuralı).
- `runtime/mimari/arm64/*.kem`, `runtime/{kem_zaman,kem_mmu,kem_dtb,kem_gorev,kem_heap}.kem`,
  `boot/start_aarch64.S`, `linker/bare-metal-aarch64.ld` (yalnız oku), `test/ornekler/kem_os.kem`,
  Makefile'ın `calistir_kem_os_arm`, `calistir_qemu_cekirdek`, `calistir_kem_os_arm_ag`, `calistir_os_kernels`.
- Başvuru: `test/bare_metal/smp*_arm.c` (yukarıdaki liste).
- ARM ARM (DDI 0487) D1 (istisna seviyeleri), D8 (MMU, TLB bakımı, `TLBI ...IS`), B2 (bellek modeli);
  GICv3 mimarisi (IHI 0069): dağıtıcı, yeniden dağıtıcı (`GICR_WAKER`, SGI çerçevesi), sistem yazmaçları
  (`ICC_SRE_EL1`, `ICC_PMR_EL1`, `ICC_IGRPEN1_EL1`, `ICC_IAR1_EL1`, `ICC_EOIR1_EL1`, `ICC_SGI1R_EL1`);
  PSCI (DEN0022) `CPU_ON`.

---

## 4. İzin genişletmesi (yalnız bu dosyalarda)
**ARM (Bölüm 1-16):**
- `runtime/mimari/arm64/*.kem` (yeni `kem_gic*.kem`, `kem_smp*.kem` vb. dahil), `runtime/kem_zaman.kem`,
  `runtime/kem_dtb.kem`, `runtime/kem_mmu.kem` (yalnız GICR/çekirdek başı eşleme ve TLB bakımı),
  `runtime/kem_gorev.kem` (yalnız arm64 dalları), `test/ornekler/kem_os*.kem` (ARM çekirdekleri).
- `boot/start_aarch64.S`: YALNIZ ikincil çekirdek girişi (çekirdek başı yığın, VBAR, MMU açma, FP açma) ve
  yeni istisna vektör koçanları. Mevcut birincil önyükleme yolu davranışı DEĞİŞMEZ.
- Heap kilidi: dalga 18'in x86 düzeniyle aynı — kilitli sarmalayıcılar mimari dosyasında
  (`runtime/mimari/x86_64/kem_kilit_hw.kem` `smp_heap_ayir`/`smp_heap_iade`, `smp_bellek_kilit_al(64)`), birleşik
  çekirdek kaynağında `malloc`/`free` derleme anında `test/x86_heap_bagla.py` ile bunlara bağlanır. ARM için aynı
  desen (yeni `runtime/mimari/arm64/kem_kilit_hw.kem` + ARM bağlayıcısı ya da `x86_heap_bagla.py`'nin
  genelleştirilmesi). `runtime/kem_heap.kem`'in heap algoritması ve bölge kodu E4k'dır (Claude'da) — DOKUNMA.
- Yeni `tools/qemu_arm.sh` (ya da Python eşdeğeri) — ARM QEMU yürütücüsü (Bölüm 3); Makefile'ın ARM hedefleri,
  `test/*.sh`, `tests/qemu/*`, `test/kapi_butce.tsv`, `test/kapi_haritasi.tsv`.
- `.github/workflows/ci.yml`: YALNIZ ARM kapılarının TCG ile koşmaya devam ettiğini doğrulayan bir adım ya da
  ortam değişkeni (iş adlarını, kurulumu, tetikleyicileri DEĞİŞTİRME).
**Dosya sistemi ve yalıtım (Bölüm 17-18):**
- `drivers/fs/*.kem`, `tests/drivers/fs/*`, `test/ornekler/kem_os_x86_kabuk.kem` ve `kem_os_x86_vfs_ortak.kem`
  (yalnız `FatSektor` kullanımı ve yalıtım kapısının gerektirdiği yerler), `test/kullanici/*.kem`.
**Belgeler (Bölüm 20):** `belgeler/KEMGU_OS_*.md` (yeni `KEMGU_OS_ARM_SMP.md` dahil),
`ARM64_Fiziksel_Donanim_Kontrol_Listesi.md`, `KEMGU_OS_Donanim_Yol_Haritasi.md`, `KEMGU_Test_Altyapisi.md`.

**YASAK:** `src/`, `selfhost/`, `boot/efi/`, `linker/` (hepsi), runtime C dosyaları, `test/bare_metal/*.c`
(Bölüm 5'teki `smp_queue_arm.c` düzeltmesi HARİÇ — yalnız o dosyada, yalnız hazır bayrağı beklemesi),
`runtime/kem_dizi_genis.kem` ve `runtime/mimari/x86_64/kem_gc_yurut.kem` (metin runtime'ı, E4k-A'da Claude
değiştirecek). Derleyici hatası → `codex19_bulgular.md`'ye C19-xxx + küçük tekrar üretimi (C ve self-host
komutu, beklenen/gerçek sonuç), dolan. Muafiyet listelerine satır EKLEME.

---

## 5. Tasarım ilkeleri (önceki dalgalarınkiler geçerli, ek olarak)
1. **Aynı imaj, iki GIC.** GIC sürümü DTB'deki `compatible`'dan seçilir; derleme zamanı bayrağı ya da sabit
   adres yoktur. GICv2 TCG yolu (bugünkü bütün ARM kapıları) birebir yeşil kalır.
2. **KVM bir hızlandırıcıdır, kâhin değil.** Her ARM kapısı hem TCG'de (CI'da koşan yol) hem KVM'de geçer;
   KVM'ye özgü bir başarı TCG'de kırmızıysa kapı kırmızıdır. Hangi hızlandırıcının kullanıldığı her koşumun
   günlüğüne yazılır; `KEMGU_ARM_HIZ=tcg` KVM'yi zorla kapatır (KVM'siz makineleri ve CI'yı taklit eder).
3. **Bekleme olaya bağlanır, süreye değil** (dalga 18 ilkesi): "son satır gelince çık, en fazla T". KVM'de
   konuk çok hızlıdır; süreye bağlı bekleyen kâhinler yanlış sonuç verir — hepsini ölçerek düzelt.
4. **ARM bellek modeli zayıftır, x86 değil.** Paylaşılan her yapı için edinme/bırakma (`LDAR/STLR`,
   `LDAXR/STLXR`) ya da açık `DMB ISH` kullanılır ve gerekçesi yazılır. x86'da doğru olan bir kilit ARM'de
   yanlış olabilir; kilit stres kapıları **KVM'de gerçek çekirdeklerde** koşar (TCG zayıf sıralamayı
   modellemez, D-490).
5. **SMP'de tek doğru kaynak yok sayılmaz:** çekirdek sayısı DTB `cpus` düğümünden okunur ve QMP
   `query-cpus-fast` ile host'ta ayrıca doğrulanır; çekirdek başına sayaçlar QMP bellek okuması + seri
   günlüğün host'ta bağımsız hesabına dayanır (dalga 18 ile aynı).
6. **Tek çekirdekte davranış değişmez:** `-smp 1` (TCG, GICv2) ile bütün mevcut ARM kapıları birebir yeşil;
   heap ve boş çerçeve sayıları önceki dalgayla aynıdır (ölç, yaz).
7. **E4k §9.6 kuralına hazırlık:** başka bir çekirdeğin tutabileceği çekirdek nesneleri (süreç kaydı,
   kuyruklar) bölge (`Dizi`/`metin`) değil ham bellek ya da çerçeve/heap kilitli yapı olmalı — dalga 18'in
   x86 düzeni (süreç kaydı `kxm_o64/kxm_y64` ofsetleri) ARM'de de korunur. Bu kuralı bozan bir tasarım
   gerekiyorsa DUR, `codex19_bulgular.md`'ye yaz.
8. ABI geriye uyumlu büyür; `tests/qemu/surec_abi.py` her yeni çağrıyı kapsar.

---

## 6. İş listesi — yirmi bölüm, HEPSİ zorunlu, sırayla

### Bölüm 1 — ARM ölçüm tabanı ve KVM denemelerinin tekrarı
- Makefile'daki bütün `qemu-system-aarch64` çağrılarını envantere al (hedef, `-M/-cpu/-smp`, bekleme biçimi:
  sabit `timeout` mı, satır mı). `test_tumu` içindekileri ve `calistir_os_kernels` içindekileri ayrı say.
- §1'deki KVM denemelerini kendin tekrarla ve sayılarla yaz (duman, KEMGU-OS 34 OK / takılma fazı, GICv2
  reddi, `smp_queue_arm` dağılımı, `dumpdtb` çıktısı). `CNTFRQ_EL0` değerini KVM ve TCG'de ölç (çekirdek
  yazdırsın ya da QMP ile oku); zamanlayıcı aralığı buna bağlıdır.
- ARM kapılarının TCG süre tablosu (taban). Sabotaj: yok (ölçüm bölümü).

### Bölüm 2 — GIC soyutlaması + GICv3 sürücüsü (TCG'de)
- Yeni GIC katmanı: `kdl_kesme_kur`, `kdl_irq_isle` (IAR/EOI), PPI açma, öncelik maskesi, (Bölüm 11 için)
  SGI gönderme — iki arka uçla: v2 (bugünkü MMIO kodu) ve v3 (dağıtıcı `GICD_CTLR` ARE_NS + Grup 1 NS;
  çekirdeğin yeniden dağıtıcısı: `GICR_WAKER` ProcessorSleep temizle, ChildrenAsleep=0 bekle; SGI çerçevesi
  (`GICR + 0x10000`): `IGROUPR0` grup 1, `ISENABLER0` bit 27; CPU arayüzü sistem yazmaçlarıyla:
  `ICC_SRE_EL1.SRE=1`, `ICC_PMR_EL1=0xFF`, `ICC_IGRPEN1_EL1=1`, `ICC_IAR1_EL1`, `ICC_EOIR1_EL1`).
  Sistem yazmacı adlarını assembler tanımıyorsa genel kodlamayı kullan (`S3_0_C12_C12_5` = ICC_SRE_EL1 vb.)
  ve hangisini kullandığını yaz.
- Sürüm seçimi DTB'den (`compatible`); GICR tabanı ve çekirdek başı yeniden dağıtıcı adımı
  (`GICR_TYPER`'deki afinite ile MPIDR eşlemesi; sabit 0x20000 adımı VARSAYMA, TYPER.Last'a kadar tara).
  GICR bölgesi MMU'da Device eşlenmeli (kendin doğrula).
- Kapı: `calistir_kem_os_arm` **iki kez**: `-M virt` (v2) ve `-M virt,gic-version=3` (v3), TCG, 24 fazın hepsi;
  hangi sürümün seçildiği seride yazılsın ve kâhin bunu denetlesin.
- Sabotaj: (a) v3'te `GICR_WAKER` adımını kaldır → zamanlayıcı fazı kırmızı; (b) DTB seçimini hep v2 yap → v3
  koşumu kırmızı; (c) EOI'yi atla → ikinci tık gelmez, kırmızı.

### Bölüm 3 — ARM QEMU yürütücüsü (TCG/KVM seçimi tek yerde)
- `tools/qemu_arm.sh` (ya da Python): hızlandırıcıyı seçer (KVM: `/dev/kvm` okunup yazılabiliyor VE ana makine
  aarch64 → `-accel kvm -cpu host -M virt,gic-version=3`; değilse TCG `-cpu cortex-a72 -M virt` (v2)
  ya da açık istenirse `gic-version=3`), seri çıktıyı dosyaya yazar, **son satır deseni gelince QEMU'yu kapatır**
  (ya da konuk PSCI `SYSTEM_OFF` ile kapanır), en fazla T bekler, çıkış kodunu ve kullanılan hızlandırıcıyı
  yazar. `KEMGU_ARM_HIZ=tcg|kvm|auto` ile zorlanabilir.
- Makefile'daki ARM QEMU çağrılarını yürütücüye taşı (önce `test_tumu` içindekiler, sonra kalanlar). Her
  hedefin denetlediği satırlar değişmez; değişen tek şey bekleme biçimi ve hızlandırıcıdır.
- Kapı: her taşınan hedef `KEMGU_ARM_HIZ=tcg` ve `kvm` ile yeşil. Taşınmadan önce ve sonra süre tablosu.
- Sabotaj: yürütücüye yanlış son satır deseni ver → zaman aşımıyla kırmızı (sessiz yeşil DEĞİL); KVM yokken
  `kvm` zorla → açık hata.

### Bölüm 4 — ARM kapılarının KVM'ye taşınması ve süre ölçümü
- `calistir_qemu_cekirdek`, `calistir_kem_os_arm`, `calistir_kem_os_arm_ag`, `calistir_os_kernels`'in ARM
  hedefleri: KVM + GICv3 ile koş, kırmızıları kökünden düzelt (sürücü/kâhin), TCG'de de yeşil kalsın.
- Ölçüm: hedef başına TCG / KVM süresi; `test_tumu` duvar saati önce/sonra. `kapi_butce.tsv`'yi ölçümle güncelle.
- Sabotaj: KVM yolunda GIC seçimini bozan değişiklik (Bölüm 2 (b)) tam takımda yakalanmalı.

### Bölüm 5 — Hızlı konukta zamanlamaya bağlı kâhinler + `smp_queue_arm` KISMI
- KVM'de konuk TCG'den çok hızlıdır: süreye/tık sayısına bağlı her ARM kâhinini tara, olaya bağla.
- `test/bare_metal/smp_queue_arm.c`: çekirdek 0, çekirdek 1'in hazır bayrağını görmeden işe başlamasın
  (sınırlı bekleme, edinme/bırakma). Bu, "KISMI" sonucunu (TCG 1/20, KVM 6/20) kapatır. 100 koşuda 0 KISMI
  (TCG ve KVM) ölç ve yaz. Bariyer satırlarına (`dc civac/ivac`, `dsb sy`) DOKUNMA.
- Sabotaj: hazır bayrağı beklemesini kaldır → KVM'de KISMI yeniden görülür (100 koşuda ≥ 1).

### Bölüm 6 — ARM istisna seviyeleri, zamanlayıcı ve önyükleme KVM'de
- KVM konuğu EL1'de başlar; TCG `virt` EL1/EL2 (ölç). `boot/start_aarch64.S`'nin iki yolu da koşmalı.
- Zamanlayıcı: CNTV, `CNTFRQ_EL0`'a göre aralık; KVM'de tık oranı (QMP + seri, dalga 18 Bölüm 11 deseni).
- Kapı: KVM ve TCG'de tık oranı beklenen aralıkta (ölçülerek seçilen sınır).
- Sabotaj: aralığı `CNTFRQ` yerine sabit varsay → KVM'de oran kırmızı (ölç; sessizse kâhini düzelt).

### Bölüm 7 — D-490 ve KVM: ne kanıtlanır, ne kanıtlanmaz
- §1'deki D-490 sabotaj denemesini `-smp 2/4` KVM'de tekrarla (temiz/sabotajlı × 100). Sonucu
  `ARM64_Fiziksel_Donanim_Kontrol_Listesi.md`'ye "KVM'de neden kanıt değildir" notuyla yaz (FEAT_S2FWB
  olasılığı; ölçebiliyorsan `ID_AA64MMFR2_EL1` ana makinede/konukta).
- Yeni bir zayıf bellek kapısı EKLEME (D-490: kanıtlamayan kapı eklenmez). Sabotaj: yok (belge bölümü).

### Bölüm 8 — İkincil çekirdekleri başlatma (PSCI `CPU_ON`)
- DTB `cpus` düğümünden çekirdek sayısı ve MPIDR afiniteleri; PSCI yöntemi DTB'den (`method = hvc`; SMC
  yedeği). `boot/start_aarch64.S`'ye ikincil giriş: çekirdek başı yığın (DTB'deki çekirdek sayısına göre),
  `VBAR_EL1`, FP/SIMD açma, MMU'yu birincil çekirdeğin tablolarıyla açma (`MAIR/TCR/TTBR0` aynı), `TPIDR_EL1`
  = çekirdek başı yapı.
- Kapı: `-smp 1/2/4/8` (TCG ve KVM): her çekirdek "ÇEKİRDEK N ÇEVRİMİÇİ" + host QMP sayısı eşleşir; `-smp 1`
  bugünkü çıktıyla birebir.
- Sabotaj: ikincil yığın olarak birincilin yığınını ver → kırmızı (ya da bozulma); `CPU_ON` dönüş kodunu
  yok say → başlamayan çekirdek fark edilsin.

### Bölüm 9 — Çekirdek başı veri, GIC yeniden dağıtıcısı ve zamanlayıcı
- Her çekirdek kendi GICR'ını uyandırır, kendi PPI 27'sini açar, kendi CNTV'sini kurar; çekirdek başı tık
  sayaçları. GICv2 yolunda bankalı GICD/GICC kayıtları.
- Kapı: her çekirdekte tık oranı (QMP + seri, host hesabı), TCG ve KVM.
- Sabotaj: GICR'ı yalnız çekirdek 0 için uyandır → diğer çekirdeklerde tık yok, kırmızı.

### Bölüm 10 — Çekirdek kilitleri (ARM)
- Bilet kilidi (`LDAXR/STLXR` ya da LSE `LDADDAL` — ana makinenin `ID_AA64ISAR0_EL1.Atomic`'ine göre; seçimi
  yaz), `WFE/SEV` ile bekleme, kilit sırası numaraları ve ters sıra denetimi (dalga 18 ilkesi).
- Kapı: 2/4/8 çekirdekte sayaç stresi (KVM'de gerçek çekirdeklerde!), FIFO adaleti, kesme kapalı alma.
- Sabotaj: edinme/bırakma yerine düz `LDR/STR` → KVM'de sayaç kaybı (ölç; TCG'de sessiz kalabilir — bunu yaz).

### Bölüm 11 — İşlemciler arası kesmeler (SGI)
- GICv3: `ICC_SGI1R_EL1` (hedef listesi, afinite); GICv2: `GICD_SGIR`. Çağrı/yanıt kutusu, duruş SGI'ı.
- Kapı: her çekirdekten her çekirdeğe SGI, gecikme ölçümü (TCG/KVM ayrı), duruş SGI'ı tüm çekirdekleri durdurur.
- Sabotaj: SGI'da EOI'yi atla → ikinci SGI gelmez, kırmızı.

### Bölüm 12 — SMP-güvenli çerçeve ayırıcısı ve heap kilidi (ARM)
- Dalga 18'in x86 düzeninin ARM karşılığı (`smp_heap_ayir`/`smp_heap_iade` kilit sarmalı + derleme anında
  `malloc`/`free` bağlama, çerçeve ayırıcısı kilidi; §4). Bölge (`Dizi`/`metin`) tahsisi yapan kod yalnız BSP'de koşmalı (§5-7); AP'de bölge tahsisi varsa
  ölç ve yaz.
- Kapı: 4/8 çekirdekte eşzamanlı ayırma/iade stresi, sahiplik denetimi, sızıntı 0.
- Sabotaj: heap kilidini kaldır → KVM'de çift sahiplik/bozulma görülür.

### Bölüm 13 — TLB bakımı (ARM)
- ARM'de TLB vurgulama için IPI gerekmez: `TLBI VAE1IS`/`ASIDE1IS` + `DSB ISH` + `ISB` iç paylaşılabilir alana
  yayınlanır. Eşleme kaldırma ve çerçeve iadesi: yayınlı TLBI tamamlanmadan çerçeve iade edilmez.
- Kapı: bir çekirdek eşlemeyi kaldırır, diğer çekirdek eski çeviriyle erişemez (veri soyutu), TCG ve KVM.
- Sabotaj: `IS` yerine yerel `TLBI VAE1` → diğer çekirdekte eski çeviri görülür (KVM'de ölç).

### Bölüm 14 — Süreçlerin ARM çekirdekleri arasında zamanlanması
- Dalga 18 Bölüm 16'nın ARM karşılığı: çekirdek başı hazır maskesi, iş çalma, süreç kaydı tek sahipli el
  değiştirme (ham bellek, §5-7), BSP hizmeti (dosya/aygıt çağrıları BSP'de sırada).
- Kapı: 4 vCPU'da dört süreç dört farklı çekirdekte (QMP: çekirdek, PID, TTBR0), iş çalma kapalıyken tek çekirdek.
- Sabotaj: iş çalmayı kapat → yalnız çekirdek 0'da çocuk görülür, kırmızı.

### Bölüm 15 — Paralel kullanıcı programı ve hızlanma eğrisi
- Asal sayımı (dalga 18 Bölüm 17'deki program) ARM'de: N=1/2/4/8 süreç × vCPU=1/2/4/8, **KVM'de gerçek
  çekirdeklerde** ve TCG'de. Sonuç host'ta bağımsız hesapla doğrulanır.
- Kapı: doğru sonuç her hücrede; KVM'de 4 vCPU hızlanması ölçülerek seçilen bir eşiğin üstünde.
- Sabotaj: zamanlayıcıyı tek çekirdeğe kilitle → hızlanma eşiği kırmızı.

### Bölüm 16 — ARM SMP sağlamlığı
- Kilit zaman aşımı, tüm çekirdeklerde panik duruşu (SGI), çekirdek başı istisna yığınları, `-smp 1/2/4/8` ×
  {TCG, KVM} × {GICv2 (yalnız TCG), GICv3} matrisi.
- Kapı: matrisin her hücresi; ARM regresyonu (`calistir_os_kernels` ARM hedefleri) yeşil.
- Sabotaj: panikte duruş SGI'ını atla → diğer çekirdekler koşmaya devam eder, kırmızı.

### Bölüm 17 — FAT32 dizin ayrıştırma önbelleği (D(1))
- **D(1a), ZORUNLU:** `FatSektor`e küme numarasıyla anahtarlı ayrıştırma kaydı; `liste` ve `fsy_dizin`/
  `fsy_kayitlar` yolu AYNI kaydı kullanır. Gerekçe: kayıt sektör önbelleğinin içeriğinin saf fonksiyonudur;
  sektör önbelleği `FatSektor` ömrü içinde yalnız büyür ve yerinde değişmez → geçersizleme GEREKMEZ.
  Başarısız (`gerekli`) ayrıştırma önbelleğe girmez.
- **D(1b), yalnız ölçüm gösterirse:** işlemler arası sektör/ayrıştırma önbelleği, geçersizleme
  `kd_icerik_surumu` ile (emsal `ks_program_hazirla`). Önce her yazma ve bağlama/çıkarma yolunun sayacı
  artırdığını ÖLÇ.
- Kapı: `dizin_coz` gerçek ayrıştırma sayısı DETERMİNİSTİK (host sürücü testi, yeniden deneme öykünmesiyle;
  eşik ölçümden), `mv`/`cp`/`mkdir` için önce/sonra tablo; FAT/fsck kapıları yeşil.
- Sabotaj: önbellek anahtarını boz (hep ilk kayıt) → doğruluk kırmızı; önbelleği kapat → sayaç kapısı
  kırmızı; sektör önbelleğinde yerinde değişiklik yapan bir yazma planı → kilit testi kırmızı.

### Bölüm 18 — Süreç dosya tanımlayıcısı yalıtımı (H9j/E3f)
- Kapı: süreç A bir dosya açar (fd N); süreç B aynı fd numarasıyla okuma/yazma/kapama dener → EBADF (ya da B'nin
  kendi fd'si; asla A'nın dosyası). A'nın fd tablosu B'nin çıkışından etkilenmez. x86'da; ARM'de süreç modeli
  izin veriyorsa ARM'de de.
- Sabotaj: fd tablosunu süreçler arası paylaştır → kırmızı.

### Bölüm 19 — Regresyon (tam takım + CI taklidi)
- `make -j10 --output-sync=target test_tumu_olcum` (KVM'li) ve `KEMGU_ARM_HIZ=tcg` ile ikinci koşum (CI'yı
  taklit eder): ikisi de rc=0. `-j20` stres koşumu.
- Süre tablosu: taban (Bölüm 1) / son; ARM kapıları ayrı.

### Bölüm 20 — Belgeler, yol haritası, sabotaj envanteri
- Yeni `belgeler/KEMGU_OS_ARM_SMP.md` (GIC katmanı, PSCI, kilitler, SGI, TLB, zamanlama, ölçümler), yol haritası
  (H14 ARM SMP), `KEMGU_Test_Altyapisi.md` (ARM yürütücüsü, KVM/TCG), sabotaj envanteri, D-490 notu.

---

## 7. Kapılar (her bölümden sonra)
- O bölümün yeni kapıları + sabotajları (çıktılarıyla).
- `calistir_kem_os_arm` (GICv2 TCG ve GICv3) + `calistir_qemu_cekirdek` + `calistir_kem_os_arm_ag`.
- x86'ya dokunan bölümlerde (17-18) ilgili x86 kapıları; her 5 bölümde tam takım.
- `calistir_kapi_secici` (kapı haritası güncel mi) ve `calistir_kapi_butcesi`.

## 8. Bitirince
Draft PR `codex/arm-smp-dalga19`, gövde `codex19_ilerleme.md` (yirmi bölümün her biri için: yapılanlar,
kapılar, sabotaj sonuçları — uygulanmayan sabotaj varsa AÇIKÇA, süre). `codex19_bulgular.md`: taban ve son
süre tabloları (TCG / KVM, `-j10` / `-j20`), ARM QEMU envanteri, GIC sürüm matrisi, SMP ölçümleri (çekirdek
başlatma süresi, tık oranları, kilit stresi, SGI gecikmesi, TLB, hızlanma eğrisi), D(1) önce/sonra, yalıtım
kapısı, derleyici bulguları (C19-xxx), kalanlar ve VARSAYIM'lar. Auto-merge AÇMA, merge ETME; PR açıldıktan
sonra commit'leri yeniden yazma.

## 9. Bu dalganın DIŞINDA
- E4k (bölge tahliyesi), derleyici, metin runtime'ı: Claude.
- ARM ACPI, ITS/LPI/MSI, GICv4, NUMA, CPU hotplug, büyük.KÜÇÜK çekirdek ayrımına göre zamanlama, x86 SMP
  değişiklikleri, IPv6/TLS, pencere yöneticisi, ses, KEMFS, fiziksel makine koşumları (Mehmet).
