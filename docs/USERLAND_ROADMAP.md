# USERLAND_ROADMAP — "shell'e boot eden kullanılabilir OS" (Model A)

**Mod:** READ-ONLY envanter/analiz. Kod/build/kernel değişmedi. Ampirik: proven-C kaynağı okundu,
`nm`/`objdump` ile gerçek link davranışı doğrulandı, küçük `/tmp` probe'ları (LLVM IR→.o) ile
linchpin test edildi (iş bitince silindi). Branch: `os/c1-region-backing` @ HEAD==origin.

---

## 🟢 LINCHPIN DEĞERLENDİRMESİ (ADIM 1) — GO

**Soru:** EL0'da gerçek `.kem` kodu (elle-assemble byte'lar değil) koşabilir mi?

**Cevap: EVET — pure linker-script + Makefile hack, SIFIR codegen/parser değişikliği.**

### Ampirik kanıt
`clang -x ir <ll> -c -ffunction-sections -fdata-sections` (LLVM IR girdisiyle bile) her fonksiyonu/
global'i kendi adıyla section'a böler:
```
define i32 @usr_topla(...)  →  .text.usr_topla
@usr_g = global i64 0        →  .bss.usr_g
```
Bu, `.kem`'in `metin`/`küresel değişken` gibi başka hiçbir dil özelliğine dokunmadan, **isim-öneki
bazlı linker-script glob'uyla** belirli fonksiyon/global'leri `.user`/`.user_data` section'ına
yönlendirmeyi mümkün kılar (mevcut `*(.user) *(.user.*)` glob'una `*(.text.kul_*)` gibi bir desen
eklemek yeterli). `.kem` parser/codegen'inde `section` attribute'u YOK (grep=0 sonuç) ama **gerekmiyor**.

### Adresleme (1c) — sorun yok
Global erişimi `adrp`+`ldr`/`str` (PC-relative, ±4GB) — mutlak adresleme YOK. `.user` birimi hangi VA'ya
(0x42000000) yerleştirilirse yerleştirilsin, kendi içindeki çapraz-referanslar doğru çözülür (linker
relocation, derleme zamanında VA bilinmesine gerek yok).

### Tier sorusu (1d) — YENİ gap DEĞİL, mevcut disiplin
Normal (çıplak-olmayan) `.kem` fn'leri region-prologue (capability param `ρ`) alır — EL0'da bu
KURULAMAZ (kernel-capability EL0'a AP=00 nedeniyle kapalı). **Userland `.kem` kodu ÇIPLAK olmalı** —
A1-A5+ZeroC'de zaten kurulu tier (trap-handler'lar, MMIO sürücüleri, scheduler hepsi çıplak). Yeni
bir yürütme modeli GEREKMİYOR. Çıplak fn çıplak fn çağırabilir (call-rule zaten izin veriyor) + builtin
(`metin_uzunluk` vb., çıplak `kdl_metin_*`'e çözülüyor) kullanılabilir.

### Tek gerçek kısıt (authoring constraint, blocker DEĞİL)
`.kem` string literalleri (`metin_literal`) **isim-öneksiz** `@.str.N` global'i olarak emit ediliyor
(global sayaç, tüm dosya çapında) — prefix-bazlı routing bunları YAKALAYAMAZ (userland-mi-kernel-mi
ayırt edilemez). **Çözüm:** userland `.kem` kodu `metin` literal DEĞİL, adlandırılmış `küresel değişken`
bayt-dizisi kullanmalı (örn. `küresel değişken kul_cmd_ls: Dizi<tam8> = [...]`) — bu ZATEN adlandırılmış
global olarak emit oluyor (kem_gorev.kem'in `küresel değişken kem_paktif` deseni), prefix-routing'e uygun.
Proven-C'nin `__attribute__((section(".user_data"))) static char CMD_LS[] = "ls";` deseninin birebir
`.kem` karşılığı.

### Önerilen yol (implementasyon, bu raporda YAPILMADI)
1. Makefile: kem_os'un `.ll→.o` derleme adımına `-ffunction-sections -fdata-sections` ekle (yalnız
   kem_os hedefi).
2. `linker/bare-metal-aarch64.ld`: `.user`/`.user_data` glob'larına isim-öneği desenleri ekle
   (örn. `kul_` — Türkçe "kullanıcı"). `*(.text.kul_*)` → `.user`, `*(.bss.kul_*) *(.data.kul_*)` →
   `.user_data`.
3. Userland `.kem` kaynağı: tüm fn/global adları `kul_` öneğiyle, TAMAMI çıplak, `metin` literal YOK
   (adlandırılmış bayt-dizisi kullan), yalnız syscall (satıriçi_asm `svc #0`) ile kernel'e geçer.

**Ölçek/risk:** Küçük — Makefile+linker-script değişikliği + yeni bir `.kem` dosyası (yazım-disiplini
gerektirir, dil-değişikliği DEĞİL). Codegen-gap AİLESİ (D-276 küresel-internal-linkage gibi) burada
**tetiklenmiyor** çünkü section-routing linker seviyesinde çözülüyor, `.kem`'in kendi linkage modeline
dokunmadan.

---

## Bileşen Envanteri

| # | Bileşen | Proven-C durumu | `.kem` ihtiyacı | Bağımlılık | Enabling-primitif | Gerçek gate | Model A? |
|---|---|---|---|---|---|---|---|
| 1 | **Linchpin** (EL0 `.kem` yerleştirme) | `.user`/`.user_data` linker section + `kdl_el0_calistir` (.S, DEĞİŞMEZ) | Makefile flag + linker-glob (yukarı bak) | — | Her şeyin önkoşulu | Basit çıplak `.kem` fn EL0'da koşup syscall(1) çağırıp döner mi | Evet |
| 2 | **UART-RX** (`sys 26 read_satir`) | `kdl_kesme.c:478-495`, PL011 FR.RXFE=bit4 poll, bounded (8M iter deadlock-guard), user-buf yaz-doğrula | Yeni çıplak `kis_rx_bayt`/`kis_satir_oku` (B1'in `kis_bayt` TX'inin ikizi) + `.kem kdl_syscall_isle`'a num=26 case | UART primitifleri (B1'de VAR) | Linchpin GEREKMEZ (kernel-taraflı, EL1'de zaten çıplak) | Tuşla satır gönder → shell echo eder | Evet |
| 3 | **Syscall ABI genişletme** | 27 numara (`kdl_kesme.c:294-497`) — write/write_num/satir/artir/gettick/getpid/spawn/exit/durum/dosya×7/kanal×2/net×2/read_satir/saat | `.kem kdl_syscall_isle` (kem_gorev.kem) ŞU AN yalnız num=7 destekliyor (A5 testi) — 26 numara EKSİK | Alt-sistemler (aşağı bak) | — | Her syscall bağımsız test edilebilir (num→beklenen davranış) | Evet (tümü) |
| 4 | **Shell (userland REPL)** | `kemgu_shell_el0.c` (419 satır) — GERÇEK interaktif (`sys(26)` ile canlı satır okur) + gömülü deterministik komut dizisi (gate kanıtı, girişe bağımlı değil) | Userland `.kem` REPL fn'i (tokenize+dispatch, linchpin+syscall'a bağlı) | Linchpin (1) + Syscall (3) | — | `[X] SHELL EL0 OK` marker (gömülü dizi işlendi) + canlı girdi echo | Evet |
| 5 | **Spawn/wait wiring** | `sys(12)=spawn(entry)` → `kdl_surec_spawn`/`kdl_preempt_gorev_olustur_el0`; `sys(13)=exit`→`kdl_gorev_bitir`; `sys(14)=durum`→`kdl_gorev_durum` | `kem_gorev_olustur_el0` (A5) + `KG_OLU` (B2) **ZATEN VAR** — yalnız syscall num=12/13/14 wiring EKSİK | kem_gorev.kem (VAR) | Linchpin (userland fn adresi spawn hedefi) | shell'den `calistir <prog>` → ayrı EL0 görev koşar → exit → shell devam | Evet |
| 6 | **Program modeli** | Gömülü `.user` fn'ler (örn. `prog_hesap`, `prog_selam`) — AYRI ELF/disk YOK, spawn hedefi = zaten-linkli fn adresi | Userland `.kem` "program" = ayrı çıplak fn, aynı `.user` birimine derlenir | Linchpin + Spawn | — | `calistir hesap` → sonuç dosyaya yazılır (IPC, minifs üstünden) | **Model A tam kapsamı** |
| 7 | **Dosya syscall'ları** (15-21) | `kdl_dosya_ac/bul` + `kdl_dosyalar[]` tablo (C, ayrı basit array-FS, minifs'ten FARKLI) | kem_os'ta ZATEN `kem_minifs.kem` var (mfs_format/dosya_yaz/dosya_oku, FAZ-B2/D-272) — syscall'ları BUNA bağla (proven-C'nin kendi dosya tablosunu DEĞİL, mevcut minifs'i sar) | kem_minifs.kem (VAR) | — | `yaz`/`oku`/`ls`/`sil` komutları minifs üzerinden çalışır | Evet |
| 8 | **Net syscall'ları** (24-25) | `kdl_virtio_net_gonder/al` sarmalayıcı | `kem_virtio_net.kem`'in `vnet_gonder`/`vnet_al` ZATEN VAR (FAZ-C) — syscall wiring EKSİK | kem_virtio_net.kem (VAR) | — | `ping`/`arpscan` komutları çalışır | Model A+ (opsiyonel, shell çekirdek-gate'ine gerekli değil) |
| 9 | **getpid/gettick** (10-11) | `kdl_tik_al()`, `kdl_aktif_gorev()` | `kem_zaman.kem`'in `kem_tik` + `kem_gorev.kem`'in `kem_paktif` ZATEN küresel — çıplak accessor sarmalayıcı yeterli | VAR | — | trivial | Evet |

---

## Bring-up Sırası (DAG)

```
1. LINCHPIN (Makefile+linker-script)         ◄── HER ŞEYİN ÖNKOŞULU
      │
      ├─→ 2. syscall-basit (write/satir/artir/gettick/getpid — mevcut .kem primitiflerin sarımı)
      │        gate: minimal EL0 .kem fn syscall(1) çağırır → "SYSCALL OK" (proven-C num=1 birebir)
      │
      ├─→ 3. UART-RX primitifi (kis_rx_bayt) + syscall num=26
      │        gate: EL0 fn sys(26) çağırır → gerçek tuş-girdisi echo eder
      │
      ├─→ 4. dosya syscall'ları (15-21) → kem_minifs.kem sarımı
      │        gate: EL0'dan yaz/oku/ls/sil çalışır (minifs üstünden gerçek disk round-trip)
      │
      ├─→ 5. spawn/exit/durum syscall (12/13/14) → kem_gorev_olustur_el0 + KG_OLU sarımı
      │        gate: EL0'dan spawn edilen İKİNCİ bir EL0 görev koşar + exit eder + orijinal görev
      │               bunu (durum) görür
      │
      └─→ 6. SHELL (userland .kem REPL) — 2+3+4+5'in TÜMÜNE bağlı, EN SON
               gate: boot → shell prompt → komut oku (gömülü dizi + canlı) → dispatch → sonuç
               "KULLANILABİLİR OS" MILESTONE = boot→shell→[yaz/oku/ls/calistir]→çıktı UÇTAN-UCA
```

**Sıralama gerekçesi:** 2-5 birbirinden bağımsız (paralel yapılabilir, her biri kendi syscall
numaralarını mevcut `.kem` alt-sistemlere bağlıyor — çoğu "sarmalayıcı" işi, yeni alt-sistem DEĞİL).
Shell hepsine bağlı olduğu için EN SON. `calistir` (program spawn) shell'in İÇİNDE test edilir (ayrı
gate gerekmez, shell gate'i onu da kapsar).

---

## Model A vs Model B sınırı

**Model A (bu rapor kapsamı):** Gömülü `.user`-birim `.kem` programları (derleme-zamanı linkli),
spawn = zaten-var-olan fn adresini `kem_gorev_olustur_el0`'a ver. Disk sadece VERİ (minifs dosyaları)
için — PROGRAM YÜKLEME için DEĞİL. Tek adres-uzayı (TTBR-swap YOK — proven-C'nin `kemgu_shell_el0.c`
yorumu bunu AÇIKÇA doğruluyor: "Boot sayfa-tablosu altında koşar... TTBR-swap YOK"). Bu, kem_os'un A4/A5
tasarımıyla ZATEN UYUMLU (aynı basitlik seviyesi).

**Model B (SONRAKİ tier, bu raporun DIŞI):** Diskten ELF yükleme — ELF header parse, program-header
segment mapping, per-program sayfa tablosu (TTBR-swap AÇIK — proven-C'de VAR ama kem_os'un mevcut
scheduler'ı [A4/A5] bunu henüz KULLANMIYOR), dinamik relocation/PIE, `kdl_surec_spawn`'ın TAM
(multi-page-table) versiyonu. Proven-C'de kısmi emsal var (D3-çoklu `kdl_surec_kur`/`kdl_task_l1[]`)
ama kem_os hiç kullanmıyor — Model B ayrı, daha büyük bir kampanya.

---

## Dürüst belirsizlik/risk

- **Linchpin küçük risk:** linker-glob deseni yanlış eşleşirse (örn. bir kernel fn'i yanlışlıkla
  `kul_`-prefiksli adlandırılırsa) YANLIŞLIKLA `.user`'a düşer → AP=01 sayfada kernel kod = güvenlik
  regresyonu DEĞİL (hâlâ EL1 çalışır, salt yanlış-yerleşim) ama disiplin gerektirir (isimlendirme
  konvansiyonuna sıkı uyum, review'da kontrol edilmeli).
- **String-literal kısıtı** userland `.kem` yazımını biraz hantallaştırır (her sabit metin için ayrı
  `küresel değişken` bayt-dizisi tanımlamak, `metin_literal` sözdiziminin rahatlığı yok) — authoring
  overhead, dil-gap DEĞİL.
- **Spawn/exec DAG'ı beklenenden HAFİF çıktı:** `kem_gorev_olustur_el0`+`KG_OLU` A4/A5/B2'de ZATEN VAR
  — Model A'nın "yeni iş" kısmı ÇOĞUNLUKLA syscall-numarası sarmalayıcıları (mevcut `.kem` fn'leri
  syscall dispatch tablosuna bağlamak), YENİ alt-sistem DEĞİL. Bu, ilk bakışta beklenenden DÜŞÜK risk.
- **UART-RX bounded-poll deseni** proven-C'de zaten "best-effort... boot-window FIFO overrun canlı
  girişi kısmen düşürebilir" notuyla geliyor — kem_os'ta AYNI karakteristik beklenmeli (gate, canlı
  girdiye değil gömülü deterministik diziye dayanmalı — proven-C'nin kendi gate stratejisi).
- **x86_64:** Bu rapor yalnız aarch64 içindir; x86_64 userland ayrı borç (kapsam dışı).

---

## Özet — Go/No-Go

**LINCHPIN: GO.** Hard blocker YOK. Küçük linker-script+Makefile hack + authoring-disiplini (çıplak +
adlandırılmış-bayt-dizisi). Yeni dil/codegen özelliği GEREKMİYOR.

**Kalan iş çoğunlukla "sarmalayıcı":** kem_minifs/kem_virtio_net/kem_gorev/kem_zaman zaten var — syscall
dispatch tablosuna bağlamak asıl iş. Gerçek YENİ iş: (a) linchpin routing, (b) UART-RX çıplak primitifi,
(c) userland `.kem` shell REPL kaynağı (proven-C'nin `kemgu_shell_el0.c`'sinin `.kem` yeniden
gerçekleştirmesi).

**"Kullanılabilir OS" milestone gate'i:** boot → shell prompt → `yaz`/`oku`/`ls` (dosya) →
`calistir <prog>` (spawn) → sonuç → uçtan-uca, tek QEMU boot'ta, saf-.kem.
